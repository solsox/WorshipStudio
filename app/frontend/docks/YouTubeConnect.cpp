/******************************************************************************
    Copyright (C) 2026 by WorshipStudio Contributors

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
******************************************************************************/

#include "YouTubeConnect.hpp"
#include "WorshipOutput.hpp"

#include <Idian/Utils.hpp>

#include <obs-frontend-api.h>

#include <cstring>
#include <vector>

#include <QDialogButtonBox>
#include <QHBoxLayout>
#include <QIcon>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QPainter>
#include <QPointer>
#include <QPushButton>
#include <QRegularExpression>
#include <QSlider>
#include <QTcpServer>
#include <QTcpSocket>
#include <QTimer>
#include <QUrlQuery>
#include <QVBoxLayout>

#include "moc_YouTubeConnect.cpp"

namespace {
constexpr int IdRole = Qt::UserRole;
constexpr int TitleRole = Qt::UserRole + 1;
const QSize Thumb(240, 135);

/* Our own mark inside the settings of the browser source, so the window
 * recognizes its videos and the saved scenes can be pointed at the player
 * page again when the app starts with another port. */
const char *const MarkId = "ws_youtube";
const char *const MarkTitle = "ws_youtube_titulo";

constexpr qsizetype MaxRequest = 8192;
constexpr quint16 FirstPort = 8790;

QIcon placeholder()
{
	QPixmap pixmap(Thumb);
	pixmap.fill(QColor(0x18, 0x17, 0x16));
	return QIcon(pixmap);
}

/* The page with the player: YouTube inside, no controls, driven by the
 * "ws_youtube" events the window sends through the browser source. */
const char *const PlayerPage = R"HTML(<!doctype html>
<html lang="es"><head><meta charset="utf-8"><title>WorshipStudio</title>
<style>
html,body{margin:0;padding:0;width:100%;height:100%;background:#000;overflow:hidden}
#reproductor,iframe{position:absolute;inset:0;width:100%;height:100%;border:0}
#aviso{position:absolute;inset:0;display:none;align-items:center;justify-content:center;
 color:#fff;background:#000;font:600 28px/1.4 -apple-system,Segoe UI,Roboto,sans-serif;
 text-align:center;padding:8%}
</style></head><body>
<div id="reproductor"></div>
<div id="aviso"></div>
<script>
var VIDEO = "__ID__", BASE = "__BASE__";
var player = null, listo = false, cola = [], estado = -1;
function aviso(texto) {
	var box = document.getElementById("aviso");
	box.textContent = texto || "";
	box.style.display = texto ? "flex" : "none";
}
function onYouTubeIframeAPIReady() {
	player = new YT.Player("reproductor", {
		width: "100%", height: "100%", videoId: VIDEO,
		playerVars: {autoplay: 0, controls: 0, rel: 0, modestbranding: 1, playsinline: 1,
			     iv_load_policy: 3, fs: 0, disablekb: 1},
		events: {
			onReady: function () {
				listo = true;
				aviso("");
				while (cola.length)
					ejecutar(cola.shift());
				informar();
			},
			onStateChange: function (e) {
				estado = e.data;
				if (estado === 1 || estado === 3)
					aviso("");
				informar();
			},
			onError: function () { aviso("Este video no se puede ver dentro de la app. Prueba con otro."); }
		}
	});
}
function ejecutar(d) {
	if (!d || !d.cmd)
		return;
	if (!listo || !player) {
		cola.push(d);
		return;
	}
	switch (d.cmd) {
	case "play": player.playVideo(); break;
	case "pause": player.pauseVideo(); break;
	case "seek":
		var t = player.getCurrentTime() + (d.segundos || 0);
		player.seekTo(t < 0 ? 0 : t, true);
		break;
	case "restart": player.seekTo(0, true); player.playVideo(); break;
	case "volume": player.setVolume(d.valor); player.unMute(); break;
	case "mute": if (d.valor) player.mute(); else player.unMute(); break;
	}
	informar();
}
window.addEventListener("ws_youtube", function (e) { ejecutar(e.detail); });
function informar() {
	if (!listo || !player)
		return;
	var t = 0, d = 0;
	try { t = player.getCurrentTime() || 0; d = player.getDuration() || 0; } catch (err) { return; }
	var url = BASE + "/estado?v=" + encodeURIComponent(VIDEO) + "&t=" + t.toFixed(1) +
		  "&d=" + d.toFixed(1) + "&s=" + estado;
	fetch(url, {cache: "no-store"}).catch(function () {});
}
setInterval(informar, 700);
setTimeout(function () { if (!listo) aviso("No se pudo conectar con YouTube. Revisa la conexión a internet."); }, 12000);
var tag = document.createElement("script");
tag.src = "https://www.youtube.com/iframe_api";
document.head.appendChild(tag);
</script></body></html>)HTML";

/* The scene being edited (or the user's last one when the screen is showing
 * a panel full screen), like the other "Agregar fuente" entries. */
OBSSource sceneTarget()
{
	const bool studio = obs_frontend_preview_program_mode_active();
	OBSSourceAutoRelease current = studio ? obs_frontend_get_current_preview_scene() : obs_frontend_get_current_scene();
	OBSSource target = current.Get();
	if (target && WorshipOutput::IsInternalScene(QString::fromUtf8(obs_source_get_name(target))))
		target = WorshipOutput::LastUserScene();
	return target;
}

QString freeName(const QString &base, obs_source_t *mine)
{
	QString name = base;
	for (int i = 2;; ++i) {
		OBSSourceAutoRelease taken = obs_get_source_by_name(name.toUtf8().constData());
		if (!taken || taken.Get() == mine)
			return name;
		name = QStringLiteral("%1 %2").arg(base).arg(i);
	}
}

QString videoIdOf(obs_source_t *source)
{
	if (!source || strcmp(obs_source_get_id(source), "browser_source") != 0)
		return {};
	OBSDataAutoRelease settings = obs_source_get_settings(source);
	return QString::fromUtf8(obs_data_get_string(settings, MarkId));
}

/* The video source of the scene, found by our mark: the window and the phone
 * both drive the same one. */
OBSSource findVideoSource()
{
	struct Found {
		OBSSource source;
	} found;
	obs_enum_sources(
		[](void *param, obs_source_t *source) {
			auto *data = static_cast<Found *>(param);
			if (!data->source && !videoIdOf(source).isEmpty())
				data->source = OBSSource(source);
			return true;
		},
		&found);
	return found.source;
}

QString clock(double seconds)
{
	if (seconds < 0 || seconds != seconds) /* negative or NaN */
		seconds = 0;
	const int total = (int)(seconds + 0.5);
	const int hours = total / 3600;
	const QString rest = QStringLiteral("%1:%2")
				     .arg(hours ? (total / 60) % 60 : total / 60, hours ? 2 : 1, 10, QLatin1Char('0'))
				     .arg(total % 60, 2, 10, QLatin1Char('0'));
	return hours ? QStringLiteral("%1:%2").arg(hours).arg(rest) : rest;
}
} // namespace

/* ------------------------------------------------------------------ server */

YouTubePlayerServer &YouTubePlayerServer::instance()
{
	static YouTubePlayerServer server;
	return server;
}

bool YouTubePlayerServer::ensureRunning()
{
	if (server && server->isListening())
		return true;
	if (!server)
		server = new QTcpServer(this);
	/* Only this computer: the page is for the browser source of the app. */
	for (quint16 candidate = FirstPort; candidate < FirstPort + 10; ++candidate) {
		if (server->listen(QHostAddress::LocalHost, candidate)) {
			listenPort = candidate;
			connect(server, &QTcpServer::newConnection, this, &YouTubePlayerServer::onConnection);
			blog(LOG_INFO, "[WorshipStudio] Reproductor de YouTube en el puerto %u", (unsigned)listenPort);
			return true;
		}
	}
	return false;
}

QString YouTubePlayerServer::pageUrl(const QString &videoId) const
{
	return QStringLiteral("http://127.0.0.1:%1/reproductor?v=%2").arg(listenPort).arg(videoId);
}

void YouTubePlayerServer::onConnection()
{
	while (server && server->hasPendingConnections()) {
		QTcpSocket *socket = server->nextPendingConnection();
		connect(socket, &QTcpSocket::readyRead, this, [this, socket]() { onReadyRead(socket); });
		connect(socket, &QTcpSocket::disconnected, socket, &QObject::deleteLater);
	}
}

void YouTubePlayerServer::onReadyRead(QTcpSocket *socket)
{
	QByteArray buffer = socket->property("buffer").toByteArray() + socket->readAll();
	if (buffer.size() > MaxRequest) {
		socket->disconnectFromHost();
		return;
	}
	if (buffer.indexOf("\r\n\r\n") < 0) {
		socket->setProperty("buffer", buffer);
		return;
	}
	const QList<QByteArray> requestLine = buffer.left(buffer.indexOf("\r\n")).split(' ');
	if (requestLine.size() < 2) {
		socket->disconnectFromHost();
		return;
	}

	int status = 200;
	QByteArray contentType;
	const QByteArray body = respond(QString::fromUtf8(requestLine.at(1)), &status, &contentType);
	QByteArray response = "HTTP/1.1 " + QByteArray::number(status) + (status == 200 ? " OK" : " Not Found") + "\r\n";
	response += "Content-Type: " + contentType + "\r\n";
	response += "Content-Length: " + QByteArray::number(body.size()) + "\r\n";
	response += "Cache-Control: no-store\r\nConnection: close\r\n\r\n";
	response += body;
	socket->write(response);
	socket->disconnectFromHost();
}

QByteArray YouTubePlayerServer::handleForTest(const QString &target)
{
	int status = 0;
	QByteArray type;
	return respond(target, &status, &type);
}

QByteArray YouTubePlayerServer::respond(const QString &target, int *status, QByteArray *contentType)
{
	const QUrl url(target);
	const QUrlQuery query(url);
	const QString path = url.path();
	const QString video = YouTubeConnect::VideoId(query.queryItemValue(QStringLiteral("v")));

	if (path == QStringLiteral("/reproductor")) {
		*contentType = "text/html; charset=utf-8";
		QByteArray page(PlayerPage);
		page.replace("__ID__", video.toUtf8());
		page.replace("__BASE__", QStringLiteral("http://127.0.0.1:%1").arg(listenPort).toUtf8());
		return page;
	}
	if (path == QStringLiteral("/estado")) {
		*contentType = "text/plain; charset=utf-8";
		progressSeen = QJsonObject{{QStringLiteral("video"), video},
					   {QStringLiteral("tiempo"), query.queryItemValue(QStringLiteral("t")).toDouble()},
					   {QStringLiteral("duracion"), query.queryItemValue(QStringLiteral("d")).toDouble()},
					   {QStringLiteral("estado"), query.queryItemValue(QStringLiteral("s")).toInt()}};
		emit progress(video, query.queryItemValue(QStringLiteral("t")).toDouble(),
			      query.queryItemValue(QStringLiteral("d")).toDouble(),
			      query.queryItemValue(QStringLiteral("s")).toInt());
		return "ok";
	}
	*status = 404;
	*contentType = "text/plain; charset=utf-8";
	return "WorshipStudio";
}

/* ------------------------------------------------------------------ window */

QString YouTubeConnect::VideoId(const QString &text)
{
	const QString trimmed = text.trimmed();
	static const QRegularExpression bare(QStringLiteral("^[A-Za-z0-9_-]{11}$"));
	if (bare.match(trimmed).hasMatch())
		return trimmed;
	static const QRegularExpression inside(
		QStringLiteral("(?:v=|v%3D|youtu\\.be/|/embed/|/live/|/shorts/|/v/)([A-Za-z0-9_-]{11})"));
	const QRegularExpressionMatch match = inside.match(trimmed);
	return match.hasMatch() ? match.captured(1) : QString();
}

void YouTubeConnect::Run(QWidget *parent)
{
	static QPointer<YouTubeConnect> window;
	if (!window)
		window = new YouTubeConnect(parent);
	window->show();
	window->raise();
	window->activateWindow();
	window->searchEdit->setFocus();
	window->searchEdit->selectAll();
}

void YouTubeConnect::Install()
{
	/* When a scene collection opens, its videos point at the address the
	 * player page had the day they were saved: port them to this run. */
	obs_frontend_add_event_callback(
		[](enum obs_frontend_event event, void *) {
			if (event != OBS_FRONTEND_EVENT_FINISHED_LOADING &&
			    event != OBS_FRONTEND_EVENT_SCENE_COLLECTION_CHANGED)
				return;
			struct Found {
				std::vector<OBSSource> sources;
			} found;
			obs_enum_sources(
				[](void *param, obs_source_t *source) {
					if (!videoIdOf(source).isEmpty())
						static_cast<Found *>(param)->sources.push_back(OBSSource(source));
					return true;
				},
				&found);
			if (found.sources.empty())
				return;
			if (!YouTubePlayerServer::instance().ensureRunning())
				return;
			for (const OBSSource &source : found.sources) {
				OBSDataAutoRelease settings = obs_source_get_settings(source);
				const QString id = QString::fromUtf8(obs_data_get_string(settings, MarkId));
				const QString url = YouTubePlayerServer::instance().pageUrl(id);
				if (QString::fromUtf8(obs_data_get_string(settings, "url")) == url)
					continue;
				obs_data_set_string(settings, "url", url.toUtf8().constData());
				obs_source_update(source, settings);
			}
		},
		nullptr);
}

YouTubeConnect::YouTubeConnect(QWidget *parent)
	: QDialog(parent, Qt::Window),
	  network(new QNetworkAccessManager(this))
{
	setWindowTitle(tr("Conexión con YouTube"));
	setAttribute(Qt::WA_DeleteOnClose, true);
	resize(880, 680);

	auto *layout = new QVBoxLayout(this);
	layout->setSpacing(10);

	searchEdit = new QLineEdit(this);
	searchEdit->setPlaceholderText(tr("Busca un video o pega el enlace de YouTube"));
	searchEdit->setClearButtonEnabled(true);
	auto *searchButton = new QPushButton(tr("Buscar"), this);
	auto *top = new QHBoxLayout();
	top->addWidget(searchEdit, 1);
	top->addWidget(searchButton);
	layout->addLayout(top);

	results = new QListWidget(this);
	results->setViewMode(QListView::IconMode);
	results->setIconSize(Thumb);
	results->setGridSize(Thumb + QSize(16, 54));
	results->setWordWrap(true);
	results->setResizeMode(QListView::Adjust);
	results->setMovement(QListView::Static);
	results->setSelectionMode(QAbstractItemView::SingleSelection);
	layout->addWidget(results, 1);

	status = new QLabel(tr("Escribe el nombre del video y toca Buscar."), this);
	status->setTextFormat(Qt::RichText);
	idian::Utils::addClass(status, "text-muted");
	layout->addWidget(status);

	showButton = new QPushButton(tr("Mostrar en pantalla"), this);
	idian::Utils::addClass(showButton, "button-primary");
	showButton->setEnabled(false);
	auto *showRow = new QHBoxLayout();
	showRow->addStretch(1);
	showRow->addWidget(showButton);
	layout->addLayout(showRow);

	/* The player: no bar to drag, so nobody loses the place by mistake. */
	nowLabel = new QLabel(tr("Todavía no hay ningún video en la escena."), this);
	QFont nowFont = nowLabel->font();
	nowFont.setBold(true);
	nowLabel->setFont(nowFont);
	nowLabel->setWordWrap(true);
	layout->addWidget(nowLabel);

	restartButton = new QPushButton(tr("↺ Desde el inicio"), this);
	backButton = new QPushButton(tr("− 10 seg"), this);
	playButton = new QPushButton(tr("▶ Reproducir"), this);
	idian::Utils::addClass(playButton, "button-primary");
	forwardButton = new QPushButton(tr("+ 10 seg"), this);
	timeLabel = new QLabel(QStringLiteral("0:00 / 0:00"), this);
	muteButton = new QPushButton(tr("Silenciar"), this);
	muteButton->setCheckable(true);
	volume = new QSlider(Qt::Horizontal, this);
	volume->setRange(0, 100);
	volume->setValue(100);
	volume->setMaximumWidth(120);
	volume->setToolTip(tr("Volumen del video"));

	auto *controls = new QHBoxLayout();
	controls->setSpacing(6);
	controls->addWidget(restartButton);
	controls->addWidget(backButton);
	controls->addWidget(playButton);
	controls->addWidget(forwardButton);
	controls->addWidget(timeLabel);
	controls->addStretch(1);
	controls->addWidget(muteButton);
	controls->addWidget(volume);
	layout->addLayout(controls);

	auto *buttons = new QDialogButtonBox(QDialogButtonBox::Close, this);
	fitButton = buttons->addButton(tr("Ajustar a la pantalla"), QDialogButtonBox::ActionRole);
	removeButton = buttons->addButton(tr("Quitar de la escena"), QDialogButtonBox::ActionRole);
	layout->addWidget(buttons);

	auto *help = new QLabel(tr("El audio del video sale por la transmisión y por la grabación. Esta ventana no "
				   "tiene barra para arrastrar: el video se mueve de 10 en 10 segundos."),
				this);
	help->setWordWrap(true);
	idian::Utils::addClass(help, "text-muted");
	layout->addWidget(help);

	connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::close);
	connect(searchButton, &QPushButton::clicked, this, [this]() { search(searchEdit->text()); });
	connect(searchEdit, &QLineEdit::returnPressed, this, [this]() { search(searchEdit->text()); });
	connect(results, &QListWidget::itemDoubleClicked, this, [this]() { showSelected(); });
	connect(results, &QListWidget::itemSelectionChanged, this,
		[this]() { showButton->setEnabled(!results->selectedItems().isEmpty()); });
	connect(showButton, &QPushButton::clicked, this, [this]() { showSelected(); });
	connect(playButton, &QPushButton::clicked, this, &YouTubeConnect::togglePlay);
	connect(backButton, &QPushButton::clicked, this, [this]() { seek(-10); });
	connect(forwardButton, &QPushButton::clicked, this, [this]() { seek(10); });
	connect(restartButton, &QPushButton::clicked, this, [this]() { command(QStringLiteral("restart")); });
	connect(muteButton, &QPushButton::toggled, this, [this](bool on) {
		muted = on;
		muteButton->setText(on ? tr("Con sonido") : tr("Silenciar"));
		command(QStringLiteral("mute"), {{QStringLiteral("valor"), on}});
	});
	connect(volume, &QSlider::valueChanged, this, [this](int value) {
		command(QStringLiteral("volume"), {{QStringLiteral("valor"), value}});
	});
	connect(fitButton, &QPushButton::clicked, this, &YouTubeConnect::fitToScreen);
	connect(removeButton, &QPushButton::clicked, this, &YouTubeConnect::removeFromScene);

	connect(&YouTubePlayerServer::instance(), &YouTubePlayerServer::progress, this,
		[this](const QString &videoId, double time, double duration, int reported) {
			if (videoId != liveId)
				return;
			state = reported;
			timeLabel->setText(QStringLiteral("%1 / %2").arg(clock(time), clock(duration)));
			playButton->setText(state == 1 ? tr("⏸ Pausar") : tr("▶ Reproducir"));
		});

	adoptExistingSource();
	updatePlayer();
}

void YouTubeConnect::keyPressEvent(QKeyEvent *event)
{
	const bool typing = qobject_cast<QLineEdit *>(focusWidget()) || qobject_cast<QSlider *>(focusWidget());
	if (!typing && (event->key() == Qt::Key_Right || event->key() == Qt::Key_Left)) {
		seek(event->key() == Qt::Key_Right ? 10 : -10);
		return;
	}
	if (!typing && event->key() == Qt::Key_Space) {
		togglePlay();
		return;
	}
	QDialog::keyPressEvent(event);
}

void YouTubeConnect::search(const QString &text)
{
	const QString query = text.trimmed();
	if (query.isEmpty())
		return;

	/* A pasted link goes straight to the screen. */
	const QString pasted = VideoId(query);
	if (!pasted.isEmpty()) {
		showVideo(pasted, tr("Video de YouTube"));
		return;
	}

	const int token = ++searchToken;
	results->clear();
	showButton->setEnabled(false);
	status->setText(tr("Buscando en YouTube..."));

	QPointer<YouTubeConnect> self(this);
	SearchVideos(this, query, [self, token](const QJsonArray &videos) {
		if (!self || token != self->searchToken)
			return;
		self->showResults(videos);
	});
}

namespace {
/* Every "videoRenderer" of the answer, wherever YouTube put it. */
void collectVideos(const QJsonValue &value, QJsonArray *into)
{
	if (value.isObject()) {
		const QJsonObject object = value.toObject();
		const QJsonValue video = object.value(QStringLiteral("videoRenderer"));
		if (video.isObject())
			into->append(video);
		for (auto it = object.constBegin(); it != object.constEnd(); ++it)
			collectVideos(it.value(), into);
	} else if (value.isArray()) {
		const QJsonArray children = value.toArray();
		for (qsizetype i = 0; i < children.size(); ++i)
			collectVideos(children.at(i), into);
	}
}

QString firstText(const QJsonObject &object)
{
	const QJsonArray runs = object.value(QStringLiteral("runs")).toArray();
	if (!runs.isEmpty())
		return runs.first().toObject().value(QStringLiteral("text")).toString();
	return object.value(QStringLiteral("simpleText")).toString();
}

/* The results page carries its data in "var ytInitialData = {...};". */
QJsonArray videosOfPage(const QByteArray &page)
{
	const QByteArray opening = "var ytInitialData = ";
	const qsizetype start = page.indexOf(opening);
	qsizetype end = start < 0 ? -1 : page.indexOf("};", start);
	if (start < 0 || end < 0)
		return {};
	end += 1;
	const QJsonObject root =
		QJsonDocument::fromJson(page.mid(start + opening.size(), end - start - opening.size())).object();
	QJsonArray found;
	collectVideos(QJsonValue(root), &found);

	QJsonArray videos;
	QStringList seen;
	for (qsizetype i = 0; i < found.size(); ++i) {
		const QJsonObject video = found.at(i).toObject();
		const QString id = video.value(QStringLiteral("videoId")).toString();
		if (id.isEmpty() || seen.contains(id))
			continue;
		seen << id;
		videos.append(QJsonObject{
			{QStringLiteral("id"), id},
			{QStringLiteral("titulo"), firstText(video.value(QStringLiteral("title")).toObject())},
			{QStringLiteral("canal"), firstText(video.value(QStringLiteral("ownerText")).toObject())},
			{QStringLiteral("duracion"), firstText(video.value(QStringLiteral("lengthText")).toObject())}});
	}
	return videos;
}
} // namespace

void YouTubeConnect::SearchVideos(QObject *context, const QString &query,
				  std::function<void(const QJsonArray &)> done)
{
	QUrl url(QStringLiteral("https://www.youtube.com/results"));
	QUrlQuery params;
	params.addQueryItem(QStringLiteral("search_query"), query);
	params.addQueryItem(QStringLiteral("hl"), QStringLiteral("es"));
	url.setQuery(params);

	QNetworkRequest request(url);
	request.setRawHeader("User-Agent", "Mozilla/5.0 (Macintosh; Intel Mac OS X 10_15_7) AppleWebKit/537.36 "
					   "(KHTML, like Gecko) Chrome/126.0.0.0 Safari/537.36");
	request.setRawHeader("Accept-Language", "es-419,es;q=0.9,en;q=0.8");
	/* Without this YouTube answers with the cookies page in some countries. */
	request.setRawHeader("Cookie", "SOCS=CAI; CONSENT=YES+cb");
	request.setTransferTimeout(20000);
	request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);

	auto *network = new QNetworkAccessManager(context);
	QNetworkReply *reply = network->get(request);
	QObject::connect(reply, &QNetworkReply::finished, context, [reply, network, done]() {
		const QJsonArray videos = reply->error() == QNetworkReply::NoError ? videosOfPage(reply->readAll())
										  : QJsonArray();
		reply->deleteLater();
		network->deleteLater();
		done(videos);
	});
}

void YouTubeConnect::showResults(const QJsonArray &videos)
{
	const int token = searchToken;
	for (qsizetype index = 0; index < videos.size(); ++index) {
		const QJsonObject video = videos.at(index).toObject();
		const QString id = video.value(QStringLiteral("id")).toString();
		const QString title = video.value(QStringLiteral("titulo")).toString();
		const QString channel = video.value(QStringLiteral("canal")).toString();
		const QString length = video.value(QStringLiteral("duracion")).toString();

		auto *item = new QListWidgetItem(placeholder(), title);
		item->setToolTip(channel.isEmpty() ? title : QStringLiteral("%1\n%2").arg(title, channel));
		item->setData(IdRole, id);
		item->setData(TitleRole, title);
		item->setTextAlignment(Qt::AlignHCenter | Qt::AlignTop);
		results->addItem(item);

		QPointer<YouTubeConnect> self(this);
		QNetworkReply *picture = network->get(
			QNetworkRequest(QUrl(QStringLiteral("https://i.ytimg.com/vi/%1/mqdefault.jpg").arg(id))));
		connect(picture, &QNetworkReply::finished, this, [self, picture, item, token, length]() {
			picture->deleteLater();
			if (!self || token != self->searchToken)
				return;
			QImage image = QImage::fromData(picture->readAll());
			if (image.isNull())
				return;
			image = image.scaled(Thumb, Qt::KeepAspectRatioByExpanding, Qt::SmoothTransformation);
			image = image.copy((image.width() - Thumb.width()) / 2, (image.height() - Thumb.height()) / 2,
					   Thumb.width(), Thumb.height());
			if (!length.isEmpty()) {
				QPainter painter(&image);
				painter.setRenderHint(QPainter::Antialiasing);
				QFont font = painter.font();
				font.setBold(true);
				painter.setFont(font);
				const QRect box = painter.fontMetrics().boundingRect(length).adjusted(-6, -3, 6, 3);
				const QRect at = box.translated(Thumb.width() - box.width() - 8,
								Thumb.height() - box.height() - 8 - box.y());
				painter.setPen(Qt::NoPen);
				painter.setBrush(QColor(0, 0, 0, 190));
				painter.drawRoundedRect(at, 4, 4);
				painter.setPen(Qt::white);
				painter.drawText(at, Qt::AlignCenter, length);
			}
			item->setIcon(QIcon(QPixmap::fromImage(image)));
		});
	}

	status->setText(!videos.isEmpty() ? tr("Toca dos veces el video para ponerlo en la escena. Queda en pausa "
					       "hasta que toques Reproducir.")
					  : tr("Sin resultados. Prueba con otras palabras o pega el enlace del video."));
}

void YouTubeConnect::showSelected()
{
	const QList<QListWidgetItem *> items = results->selectedItems();
	if (items.isEmpty())
		return;
	showVideo(items.first()->data(IdRole).toString(), items.first()->data(TitleRole).toString());
}

OBSSource YouTubeConnect::liveSource() const
{
	return findVideoSource();
}

/* A video already on screen, from the phone or from a previous time the
 * window was open. */
void YouTubeConnect::adoptExistingSource()
{
	OBSSource source = findVideoSource();
	if (!source)
		return;
	OBSDataAutoRelease settings = obs_source_get_settings(source);
	liveId = QString::fromUtf8(obs_data_get_string(settings, MarkId));
	liveTitle = QString::fromUtf8(obs_data_get_string(settings, MarkTitle));
}

namespace {
/* Creates the browser source of the video in the scene being edited, or
 * points the one already there at the new video. */
OBSSource putVideoOnScene(const QString &id, const QString &title, OBSSource existing, QString *problem)
{
	if (!YouTubePlayerServer::instance().ensureRunning()) {
		*problem = QCoreApplication::translate("YouTubeConnect",
						       "No se pudo abrir el reproductor en esta computadora.");
		return OBSSource();
	}
	const QString url = YouTubePlayerServer::instance().pageUrl(id);
	const QString name = QCoreApplication::translate("YouTubeConnect", "YouTube - %1").arg(title.left(60));

	if (existing) {
		/* Same source, another video: it loads the new page. */
		OBSDataAutoRelease settings = obs_source_get_settings(existing);
		obs_data_set_string(settings, "url", url.toUtf8().constData());
		obs_data_set_string(settings, MarkId, id.toUtf8().constData());
		obs_data_set_string(settings, MarkTitle, title.toUtf8().constData());
		obs_source_update(existing, settings);
		obs_source_set_name(existing, freeName(name, existing).toUtf8().constData());
		return existing;
	}

	OBSSource target = sceneTarget();
	obs_scene_t *scene = target ? obs_scene_from_source(target) : nullptr;
	if (!scene) {
		*problem = QCoreApplication::translate("YouTubeConnect", "Primero crea una escena en el panel Escenas.");
		return OBSSource();
	}
	OBSDataAutoRelease settings = obs_data_create();
	obs_data_set_string(settings, "url", url.toUtf8().constData());
	obs_data_set_int(settings, "width", (int)WorshipOutput::CanvasWidth());
	obs_data_set_int(settings, "height", (int)WorshipOutput::CanvasHeight());
	/* The sound of the video goes through the OBS mixer. */
	obs_data_set_bool(settings, "reroute_audio", true);
	obs_data_set_bool(settings, "restart_when_active", false);
	obs_data_set_string(settings, MarkId, id.toUtf8().constData());
	obs_data_set_string(settings, MarkTitle, title.toUtf8().constData());

	OBSSourceAutoRelease created =
		obs_source_create("browser_source", freeName(name, nullptr).toUtf8().constData(), settings, nullptr);
	if (!created) {
		*problem = QCoreApplication::translate("YouTubeConnect", "No se pudo crear la fuente del video.");
		return OBSSource();
	}
	obs_scene_add(scene, created);
	/* Auto ajuste: el video entra completo en la pantalla, sin recortes. */
	WorshipOutput::FitItem(scene, created, 0.0f, 0.0f, 1.0f, 1.0f, OBS_ALIGN_CENTER, OBS_BOUNDS_SCALE_INNER);
	return created.Get();
}
} // namespace

bool YouTubeConnect::Show(const QString &videoId, const QString &title)
{
	const QString id = VideoId(videoId);
	if (id.isEmpty())
		return false;
	QString problem;
	return putVideoOnScene(id, title, findVideoSource(), &problem) != nullptr;
}

void YouTubeConnect::Command(const QString &cmd, double value)
{
	OBSSource source = findVideoSource();
	if (!source)
		return;
	QJsonObject payload{{QStringLiteral("cmd"), cmd}};
	if (cmd == QStringLiteral("seek"))
		payload.insert(QStringLiteral("segundos"), value);
	else if (cmd == QStringLiteral("volume") || cmd == QStringLiteral("mute"))
		payload.insert(QStringLiteral("valor"), cmd == QStringLiteral("mute") ? QJsonValue(value != 0)
										     : QJsonValue(value));
	const QByteArray json = QJsonDocument(payload).toJson(QJsonDocument::Compact);

	proc_handler_t *handler = obs_source_get_proc_handler(source);
	if (!handler)
		return;
	calldata_t data;
	calldata_init(&data);
	calldata_set_string(&data, "eventName", "ws_youtube");
	calldata_set_string(&data, "jsonString", json.constData());
	proc_handler_call(handler, "javascript_event", &data);
	calldata_free(&data);
}

QJsonObject YouTubeConnect::Status()
{
	OBSSource source = findVideoSource();
	if (!source)
		return {};
	OBSDataAutoRelease settings = obs_source_get_settings(source);
	const QString id = QString::fromUtf8(obs_data_get_string(settings, MarkId));
	QJsonObject status{{QStringLiteral("video"), id},
			   {QStringLiteral("titulo"), QString::fromUtf8(obs_data_get_string(settings, MarkTitle))},
			   {QStringLiteral("tiempo"), 0},
			   {QStringLiteral("duracion"), 0},
			   {QStringLiteral("estado"), -1}};
	const QJsonObject seen = YouTubePlayerServer::instance().lastProgress();
	if (seen.value(QStringLiteral("video")).toString() == id) {
		status.insert(QStringLiteral("tiempo"), seen.value(QStringLiteral("tiempo")));
		status.insert(QStringLiteral("duracion"), seen.value(QStringLiteral("duracion")));
		status.insert(QStringLiteral("estado"), seen.value(QStringLiteral("estado")));
	}
	return status;
}

void YouTubeConnect::showVideo(const QString &videoId, const QString &title)
{
	const QString id = VideoId(videoId);
	if (id.isEmpty())
		return;

	QString problem;
	OBSSource source = putVideoOnScene(id, title, liveSource(), &problem);
	if (!source) {
		status->setText(problem);
		return;
	}
	live = OBSGetWeakRef(source);
	liveId = id;
	liveTitle = title;
	state = -1;
	if (muted)
		command(QStringLiteral("mute"), {{QStringLiteral("valor"), true}});
	if (volume->value() != 100)
		command(QStringLiteral("volume"), {{QStringLiteral("valor"), volume->value()}});
	timeLabel->setText(QStringLiteral("0:00 / 0:00"));
	updatePlayer();
	status->setText(tr("\"%1\" ya está en la escena, en pausa. Toca Reproducir cuando quieras.").arg(title));
}

void YouTubeConnect::command(const QString &cmd, const QJsonObject &extra)
{
	if (cmd == QStringLiteral("seek"))
		Command(cmd, extra.value(QStringLiteral("segundos")).toDouble());
	else if (cmd == QStringLiteral("volume"))
		Command(cmd, extra.value(QStringLiteral("valor")).toDouble());
	else if (cmd == QStringLiteral("mute"))
		Command(cmd, extra.value(QStringLiteral("valor")).toBool() ? 1 : 0);
	else
		Command(cmd);
}

void YouTubeConnect::seek(double seconds)
{
	command(QStringLiteral("seek"), {{QStringLiteral("segundos"), seconds}});
}

void YouTubeConnect::togglePlay()
{
	const bool playing = state == 1;
	command(playing ? QStringLiteral("pause") : QStringLiteral("play"));
	state = playing ? 2 : 1;
	playButton->setText(playing ? tr("▶ Reproducir") : tr("⏸ Pausar"));
}

void YouTubeConnect::fitToScreen()
{
	OBSSource source = liveSource();
	OBSSource target = sceneTarget();
	obs_scene_t *scene = target ? obs_scene_from_source(target) : nullptr;
	if (!source || !scene)
		return;
	if (!obs_scene_find_source(scene, obs_source_get_name(source))) {
		obs_scene_add(scene, source);
		status->setText(tr("El video se agregó también a esta escena."));
	}
	WorshipOutput::FitItem(scene, source, 0.0f, 0.0f, 1.0f, 1.0f, OBS_ALIGN_CENTER, OBS_BOUNDS_SCALE_INNER);
}

void YouTubeConnect::removeFromScene()
{
	OBSSource source = liveSource();
	if (!source)
		return;
	command(QStringLiteral("pause"));
	obs_source_remove(source);
	live = OBSWeakSource();
	liveId.clear();
	liveTitle.clear();
	state = -1;
	timeLabel->setText(QStringLiteral("0:00 / 0:00"));
	updatePlayer();
	status->setText(tr("El video salió de las escenas."));
}

void YouTubeConnect::updatePlayer()
{
	const bool has = liveSource() != nullptr;
	for (QPushButton *button : {playButton, backButton, forwardButton, restartButton, muteButton, fitButton,
				    removeButton})
		button->setEnabled(has);
	volume->setEnabled(has);
	playButton->setText(state == 1 ? tr("⏸ Pausar") : tr("▶ Reproducir"));
	nowLabel->setText(has ? tr("En la escena: %1").arg(liveTitle.isEmpty() ? liveId : liveTitle)
			      : tr("Todavía no hay ningún video en la escena."));
}
