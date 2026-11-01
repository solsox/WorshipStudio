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

#include "LiveToolsDock.hpp"
#include "SongEditor.hpp"
#include "WorshipLive.hpp"
#include "WorshipOutput.hpp"
#include "WorshipRemote.hpp"
#include "WorshipRender.hpp"

#include <qt-wrappers.hpp>
#include <Idian/Utils.hpp>

#include <QComboBox>
#include <QDir>
#include <QFormLayout>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMenu>
#include <QPixmap>
#include <QPushButton>
#include <QScreen>
#include <QScrollArea>
#include <QSpinBox>
#include <QTimeEdit>
#include <QTimer>
#include <QVBoxLayout>

#include "moc_LiveToolsDock.cpp"

namespace {
constexpr const char *CountdownSceneName = "WS Cuenta regresiva";
constexpr const char *CountdownSourceName = "WS Cuenta regresiva - Pantalla";
constexpr const char *StageSceneName = "WS Escenario";
constexpr const char *StageSourceName = "WS Escenario - Monitor";
constexpr const char *OverlaySceneName = "WS Superposiciones";
constexpr const char *NameSourceName = "WS Rotulo de nombre";
constexpr const char *MessageSourceName = "WS Mensaje urgente";
constexpr const char *ConfigRemote = "RemoteEnabled";

QLabel *heading(const QString &text, QWidget *parent)
{
	auto *label = new QLabel(text, parent);
	idian::Utils::addClass(label, "text-bold");
	label->setContentsMargins(0, 10, 0, 2);
	return label;
}

QString formatSeconds(qint64 seconds)
{
	seconds = qMax<qint64>(0, seconds);
	if (seconds >= 3600)
		return QStringLiteral("%1:%2:%3")
			.arg(seconds / 3600)
			.arg((seconds / 60) % 60, 2, 10, QLatin1Char('0'))
			.arg(seconds % 60, 2, 10, QLatin1Char('0'));
	return QStringLiteral("%1:%2").arg(seconds / 60, 2, 10, QLatin1Char('0')).arg(seconds % 60, 2, 10, QLatin1Char('0'));
}

/* Writes the image alternating two names so the image source reloads. */
QString saveFrame(const QImage &image, const QString &name, int *counter)
{
	*counter = (*counter + 1) % 2;
	const QString path = QDir(WorshipRender::CacheDir(QStringLiteral("live")))
				     .absoluteFilePath(QStringLiteral("%1-%2.png").arg(name).arg(*counter));
	image.save(path, "PNG");
	return path;
}

void setImage(obs_source_t *source, const QString &path)
{
	if (!source)
		return;
	OBSDataAutoRelease settings = obs_data_create();
	obs_data_set_string(settings, "file", QT_TO_UTF8(path));
	obs_source_update(source, settings);
}
} // namespace

LiveToolsDock::LiveToolsDock(const QString &objectName, const QString &title, QWidget *parent)
	: OBSDock(title, parent)
{
	setObjectName(objectName);

	auto *scroll = new QScrollArea(this);
	scroll->setWidgetResizable(true);
	scroll->setFrameShape(QFrame::NoFrame);
	auto *root = new QWidget(scroll);
	auto *layout = new QVBoxLayout(root);
	layout->setContentsMargins(8, 4, 8, 8);
	layout->setSpacing(6);

	/* --- On air ------------------------------------------------------- */
	layout->addWidget(heading(tr("En pantalla"), root));
	nowTitle = new QLabel(tr("Nada en pantalla"), root);
	nowTitle->setWordWrap(true);
	nowText = new QLabel(root);
	nowText->setWordWrap(true);
	nextText = new QLabel(root);
	nextText->setWordWrap(true);
	idian::Utils::addClass(nextText, "text-muted");
	auto *previousButton = new QPushButton(tr("< Anterior"), root);
	auto *nextButton = new QPushButton(tr("Siguiente >"), root);
	idian::Utils::addClass(nextButton, "button-primary");
	hideButton = new QPushButton(tr("Ocultar"), root);
	auto *nowRow = new QHBoxLayout();
	nowRow->addWidget(previousButton);
	nowRow->addWidget(nextButton, 1);
	nowRow->addWidget(hideButton);
	layout->addWidget(nowTitle);
	layout->addWidget(nowText);
	layout->addWidget(nextText);
	layout->addLayout(nowRow);

	/* --- Countdown ---------------------------------------------------- */
	layout->addWidget(heading(tr("Cuenta regresiva"), root));
	countdownMode = new QComboBox(root);
	countdownMode->addItem(tr("Durante"));
	countdownMode->addItem(tr("Hasta la hora"));
	countdownMinutes = new QSpinBox(root);
	countdownMinutes->setRange(1, 240);
	countdownMinutes->setValue(5);
	countdownMinutes->setSuffix(tr(" min"));
	countdownTime = new QTimeEdit(QTime::currentTime().addSecs(15 * 60), root);
	countdownTime->setDisplayFormat(QStringLiteral("HH:mm"));
	countdownTime->setVisible(false);
	auto *modeRow = new QHBoxLayout();
	modeRow->addWidget(countdownMode, 1);
	modeRow->addWidget(countdownMinutes, 1);
	modeRow->addWidget(countdownTime, 1);
	countdownTitle = new QLineEdit(tr("El culto comienza en"), root);
	countdownEnd = new QLineEdit(tr("Bienvenidos"), root);
	countdownEnd->setPlaceholderText(tr("Texto al llegar a cero"));
	countdownBackground = new BackgroundPicker(false, false, root);
	WorshipRender::Background defaultBackground;
	defaultBackground.preset = 9;
	countdownBackground->setBackground(defaultBackground);
	countdownButton = new QPushButton(tr("Proyectar cuenta regresiva"), root);
	auto *countdownForm = new QFormLayout();
	countdownForm->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);
	countdownForm->addRow(tr("Tiempo"), modeRow);
	countdownForm->addRow(tr("Titulo"), countdownTitle);
	countdownForm->addRow(tr("Al final"), countdownEnd);
	countdownForm->addRow(tr("Fondo"), countdownBackground);
	layout->addLayout(countdownForm);
	layout->addWidget(countdownButton);

	/* --- Name lower third --------------------------------------------- */
	layout->addWidget(heading(tr("Rotulo de nombre"), root));
	nameEdit = new QLineEdit(root);
	nameEdit->setPlaceholderText(tr("Nombre (p. ej. Pr. Juan Perez)"));
	roleEdit = new QLineEdit(root);
	roleEdit->setPlaceholderText(tr("Cargo o tema (opcional)"));
	nameButton = new QPushButton(tr("Mostrar rotulo"), root);
	nameButton->setCheckable(true);
	layout->addWidget(nameEdit);
	layout->addWidget(roleEdit);
	layout->addWidget(nameButton);

	/* --- Urgent message ----------------------------------------------- */
	layout->addWidget(heading(tr("Mensaje urgente"), root));
	messageEdit = new QLineEdit(root);
	messageEdit->setPlaceholderText(tr("Se muestra arriba de todo lo que este al aire"));
	messageButton = new QPushButton(tr("Mostrar mensaje"), root);
	messageButton->setCheckable(true);
	layout->addWidget(messageEdit);
	layout->addWidget(messageButton);

	/* --- Stage display ------------------------------------------------ */
	layout->addWidget(heading(tr("Monitor de escenario"), root));
	auto *stageHelp = new QLabel(tr("Pantalla para cantantes y predicador: diapositiva actual, siguiente, reloj y "
					"mensajes."),
				     root);
	stageHelp->setWordWrap(true);
	idian::Utils::addClass(stageHelp, "text-muted");
	stageMessageEdit = new QLineEdit(root);
	stageMessageEdit->setPlaceholderText(tr("Mensaje al escenario (p. ej. \"5 minutos\")"));
	auto *sendButton = new QPushButton(tr("Enviar"), root);
	auto *clearMessage = new QPushButton(tr("Borrar"), root);
	auto *stageRow = new QHBoxLayout();
	stageRow->addWidget(stageMessageEdit, 1);
	stageRow->addWidget(sendButton);
	stageRow->addWidget(clearMessage);
	auto *openStage = new QPushButton(tr("Abrir en una pantalla..."), root);
	stopwatchButton = new QPushButton(tr("Iniciar cronometro"), root);
	stopwatchButton->setCheckable(true);
	auto *stageButtons = new QHBoxLayout();
	stageButtons->addWidget(openStage, 1);
	stageButtons->addWidget(stopwatchButton, 1);
	layout->addWidget(stageHelp);
	layout->addLayout(stageRow);
	layout->addLayout(stageButtons);

	/* --- Phone remote ------------------------------------------------- */
	layout->addWidget(heading(tr("Control desde el celular"), root));
	remoteButton = new QPushButton(tr("Activar control desde el celular"), root);
	remoteButton->setCheckable(true);
	remoteInfo = new QLabel(root);
	remoteInfo->setWordWrap(true);
	remoteInfo->setTextInteractionFlags(Qt::TextSelectableByMouse);
	remoteQr = new QLabel(root);
	remoteQr->setAlignment(Qt::AlignCenter);
	auto *newPin = new QPushButton(tr("Nuevo codigo"), root);
	layout->addWidget(remoteButton);
	layout->addWidget(remoteInfo);
	layout->addWidget(remoteQr);
	layout->addWidget(newPin);
	layout->addStretch(1);

	scroll->setWidget(root);
	setWidget(scroll);

	remoteServer = new WorshipRemote(this);
	clockTimer = new QTimer(this);
	clockTimer->setInterval(1000);
	clockTimer->start();

	WorshipLive &live = WorshipLive::instance();
	connect(&live, &WorshipLive::changed, this, &LiveToolsDock::onLiveChanged);
	connect(&live, &WorshipLive::stageMessageChanged, this, &LiveToolsDock::refreshStage);
	connect(previousButton, &QPushButton::clicked, &live, &WorshipLive::previous);
	connect(nextButton, &QPushButton::clicked, &live, &WorshipLive::next);
	connect(hideButton, &QPushButton::clicked, &live, &WorshipLive::toggleHidden);
	connect(clockTimer, &QTimer::timeout, this, &LiveToolsDock::onTick);

	connect(countdownMode, &QComboBox::currentIndexChanged, this, [this](int index) {
		countdownMinutes->setVisible(index == 0);
		countdownTime->setVisible(index == 1);
	});
	connect(countdownButton, &QPushButton::clicked, this, [this]() {
		if (countdownActive) {
			stopCountdown();
			return;
		}
		if (countdownMode->currentIndex() == 0) {
			startCountdown(countdownMinutes->value(), countdownTitle->text());
		} else {
			QDateTime target(QDate::currentDate(), countdownTime->time());
			if (target < QDateTime::currentDateTime())
				target = target.addDays(1);
			startCountdown(qMax<qint64>(1, (QDateTime::currentDateTime().secsTo(target) + 59) / 60),
				       countdownTitle->text());
			countdownTarget = target;
		}
	});
	connect(countdownBackground, &BackgroundPicker::backgroundChanged, this, [this]() {
		if (countdownActive)
			renderCountdown();
	});

	connect(nameButton, &QPushButton::clicked, this, [this](bool checked) {
		if (checked)
			showName(nameEdit->text(), roleEdit->text());
		else
			hideName();
	});
	connect(messageButton, &QPushButton::clicked, this, [this](bool checked) {
		if (checked)
			showUrgentMessage(messageEdit->text());
		else
			hideUrgentMessage();
	});

	connect(sendButton, &QPushButton::clicked, this,
		[this]() { WorshipLive::instance().setStageMessage(stageMessageEdit->text().trimmed()); });
	connect(stageMessageEdit, &QLineEdit::returnPressed, sendButton, &QPushButton::click);
	connect(clearMessage, &QPushButton::clicked, this, [this]() {
		stageMessageEdit->clear();
		WorshipLive::instance().setStageMessage(QString());
	});
	connect(openStage, &QPushButton::clicked, this, &LiveToolsDock::openStageProjector);
	connect(stopwatchButton, &QPushButton::clicked, this, [this](bool checked) {
		stopwatchStart = checked ? QDateTime::currentDateTime() : QDateTime();
		stopwatchButton->setText(checked ? tr("Detener cronometro") : tr("Iniciar cronometro"));
		refreshStage();
	});

	connect(remoteButton, &QPushButton::clicked, this, &LiveToolsDock::setRemoteEnabled);
	connect(newPin, &QPushButton::clicked, this, [this]() {
		remoteServer->regeneratePin();
		updateRemoteInfo();
	});

	obs_frontend_add_event_callback(&LiveToolsDock::onFrontendEvent, this);

	updateRemoteInfo();
	if (WorshipOutput::ConfigString(ConfigRemote) == QStringLiteral("1"))
		QTimer::singleShot(0, this, [this]() { setRemoteEnabled(true); });
}

LiveToolsDock::~LiveToolsDock()
{
	obs_frontend_remove_event_callback(&LiveToolsDock::onFrontendEvent, this);
}

/* ------------------------------------------------------------------------ */

void LiveToolsDock::onLiveChanged()
{
	const LiveState state = WorshipLive::instance().state();
	if (state.source.isEmpty()) {
		nowTitle->setText(tr("Nada en pantalla"));
		nowText->clear();
		nextText->clear();
	} else {
		QString heading = QStringLiteral("<b>%1</b> · %2").arg(state.source.toHtmlEscaped(), state.title.toHtmlEscaped());
		if (!state.section.isEmpty())
			heading += QStringLiteral(" · ") + state.section.toHtmlEscaped();
		if (state.hidden)
			heading += tr(" · <i>oculto</i>");
		nowTitle->setText(heading);
		nowText->setText(state.current);
		nextText->setText(state.next.isEmpty() ? QString() : tr("Siguiente: %1").arg(QString(state.next).replace(QLatin1Char('\n'), QStringLiteral(" / "))));
	}
	hideButton->setText(state.hidden ? tr("Mostrar") : tr("Ocultar"));
	refreshStage();
}

void LiveToolsDock::onTick()
{
	if (countdownActive)
		renderCountdown();
	if (stageSceneRef)
		refreshStage();
}

/* ------------------------------------------------------------------------ */
/* Countdown                                                                 */

void LiveToolsDock::startCountdown(int minutes, const QString &title)
{
	countdownSceneRef = WorshipOutput::EnsureScene(CountdownSceneName);
	bool created = false;
	OBSDataAutoRelease settings = obs_data_create();
	countdownSource = WorshipOutput::EnsureSource(countdownSceneRef, CountdownSourceName, "image_source", settings,
						      &created);
	if (created && countdownSource)
		WorshipOutput::FitItem(countdownSceneRef, countdownSource, 0.0f, 0.0f, 1.0f, 1.0f);

	if (!title.isNull())
		countdownTitle->setText(title);
	countdownTarget = QDateTime::currentDateTime().addSecs(qint64(minutes) * 60);
	countdownActive = true;
	countdownButton->setText(tr("Detener cuenta regresiva"));
	renderCountdown();
	WorshipOutput::ShowScene(countdownSceneRef);
}

void LiveToolsDock::stopCountdown()
{
	countdownActive = false;
	countdownButton->setText(tr("Proyectar cuenta regresiva"));
	refreshStage();
}

void LiveToolsDock::renderCountdown()
{
	const qint64 remaining = QDateTime::currentDateTime().secsTo(countdownTarget);
	const QSize canvas(WorshipOutput::CanvasWidth(), WorshipOutput::CanvasHeight());
	QImage image;
	if (remaining <= 0) {
		/* At zero the end text replaces the clock and the countdown stops. */
		WorshipRender::LyricsSlide end;
		end.text = countdownEnd->text();
		end.background = countdownBackground->background();
		end.maxFontSize = 120;
		image = WorshipRender::Lyrics(end, canvas);
		countdownActive = false;
		countdownButton->setText(tr("Proyectar cuenta regresiva"));
	} else {
		image = WorshipRender::Countdown(countdownTitle->text(), formatSeconds(remaining), QString(),
						 countdownBackground->background(), WorshipRender::RecommendedFonts().value(0),
						 canvas);
	}
	setImage(countdownSource, saveFrame(image, QStringLiteral("cuenta"), &countdownCounter));
}

/* ------------------------------------------------------------------------ */
/* Overlays: one scene nested on top of whatever is on air                   */

void LiveToolsDock::ensureOverlays()
{
	overlaySceneRef = WorshipOutput::EnsureScene(OverlaySceneName);
	OBSDataAutoRelease settings = obs_data_create();
	bool created = false;
	nameSource = WorshipOutput::EnsureSource(overlaySceneRef, NameSourceName, "image_source", settings, &created);
	if (created && nameSource)
		WorshipOutput::FitItem(overlaySceneRef, nameSource, 0.0f, 0.0f, 1.0f, 1.0f);
	messageSource = WorshipOutput::EnsureSource(overlaySceneRef, MessageSourceName, "image_source", settings, &created);
	if (created && messageSource)
		WorshipOutput::FitItem(overlaySceneRef, messageSource, 0.0f, 0.0f, 1.0f, 1.0f);
}

void LiveToolsDock::attachOverlays()
{
	if (!overlaySceneRef || (!nameVisible && !messageVisible))
		return;
	obs_source_t *overlay = obs_scene_get_source(overlaySceneRef);

	/* The program scene, and the preview one in Studio Mode. */
	OBSSourceAutoRelease program = obs_frontend_get_current_scene();
	OBSSourceAutoRelease preview = obs_frontend_preview_program_mode_active()
					       ? obs_frontend_get_current_preview_scene()
					       : nullptr;
	for (obs_source_t *target : {program.Get(), preview.Get()}) {
		obs_scene_t *scene = target ? obs_scene_from_source(target) : nullptr;
		if (!scene || target == overlay)
			continue;
		obs_sceneitem_t *item = obs_scene_find_source(scene, OverlaySceneName);
		if (!item) {
			item = obs_scene_add(scene, overlay);
			WorshipOutput::FitItem(scene, overlay, 0.0f, 0.0f, 1.0f, 1.0f);
		}
		obs_sceneitem_set_visible(item, true);
		obs_sceneitem_set_order(item, OBS_ORDER_MOVE_TOP);
	}
}

void LiveToolsDock::setOverlayVisible(const char *sourceName, bool visible)
{
	ensureOverlays();
	if (obs_sceneitem_t *item = obs_scene_find_source(overlaySceneRef, sourceName))
		obs_sceneitem_set_visible(item, visible);
	attachOverlays();
}

void LiveToolsDock::showName(const QString &name, const QString &role)
{
	if (name.trimmed().isEmpty()) {
		nameButton->setChecked(false);
		return;
	}
	ensureOverlays();
	nameEdit->setText(name);
	roleEdit->setText(role);
	const QSize canvas(WorshipOutput::CanvasWidth(), WorshipOutput::CanvasHeight());
	static int counter = 0;
	setImage(nameSource, saveFrame(WorshipRender::NameLowerThird(name, role, WorshipRender::RecommendedFonts().value(0),
								    canvas),
				       QStringLiteral("rotulo"), &counter));
	nameVisible = true;
	nameButton->setChecked(true);
	nameButton->setText(tr("Ocultar rotulo"));
	setOverlayVisible(NameSourceName, true);
}

void LiveToolsDock::hideName()
{
	nameVisible = false;
	nameButton->setChecked(false);
	nameButton->setText(tr("Mostrar rotulo"));
	setOverlayVisible(NameSourceName, false);
}

void LiveToolsDock::showUrgentMessage(const QString &text)
{
	if (text.trimmed().isEmpty()) {
		messageButton->setChecked(false);
		return;
	}
	ensureOverlays();
	messageEdit->setText(text);
	const QSize canvas(WorshipOutput::CanvasWidth(), WorshipOutput::CanvasHeight());
	static int counter = 0;
	setImage(messageSource,
		 saveFrame(WorshipRender::UrgentMessage(text, WorshipRender::RecommendedFonts().value(0), canvas),
			   QStringLiteral("mensaje"), &counter));
	messageVisible = true;
	messageButton->setChecked(true);
	messageButton->setText(tr("Ocultar mensaje"));
	setOverlayVisible(MessageSourceName, true);
}

void LiveToolsDock::hideUrgentMessage()
{
	messageVisible = false;
	messageButton->setChecked(false);
	messageButton->setText(tr("Mostrar mensaje"));
	setOverlayVisible(MessageSourceName, false);
}

void LiveToolsDock::onFrontendEvent(enum obs_frontend_event event, void *data)
{
	auto *dock = static_cast<LiveToolsDock *>(data);
	/* Overlays follow the operator to whatever scene goes on air. */
	if (event == OBS_FRONTEND_EVENT_SCENE_CHANGED || event == OBS_FRONTEND_EVENT_PREVIEW_SCENE_CHANGED)
		QMetaObject::invokeMethod(dock, [dock]() { dock->attachOverlays(); }, Qt::QueuedConnection);
}

/* ------------------------------------------------------------------------ */
/* Stage display                                                             */

void LiveToolsDock::refreshStage()
{
	if (!stageSceneRef)
		return;
	const LiveState state = WorshipLive::instance().state();
	WorshipRender::StageInfo info;
	info.clock = QTime::currentTime().toString(QStringLiteral("HH:mm"));
	info.title = state.title;
	info.section = state.section;
	info.current = state.current;
	info.next = state.next;
	info.hidden = state.hidden;
	info.message = WorshipLive::instance().stageMessage();
	if (countdownActive)
		info.timer = tr("Cuenta regresiva %1").arg(formatSeconds(QDateTime::currentDateTime().secsTo(countdownTarget)));
	else if (stopwatchStart.isValid())
		info.timer = tr("Cronometro %1").arg(formatSeconds(stopwatchStart.secsTo(QDateTime::currentDateTime())));

	const QSize canvas(WorshipOutput::CanvasWidth(), WorshipOutput::CanvasHeight());
	setImage(stageSource, saveFrame(WorshipRender::StageDisplay(info, canvas), QStringLiteral("escenario"),
					&stageCounter));
}

void LiveToolsDock::ensureStage()
{
	stageSceneRef = WorshipOutput::EnsureScene(StageSceneName);
	OBSDataAutoRelease settings = obs_data_create();
	bool created = false;
	stageSource = WorshipOutput::EnsureSource(stageSceneRef, StageSourceName, "image_source", settings, &created);
	if (created && stageSource)
		WorshipOutput::FitItem(stageSceneRef, stageSource, 0.0f, 0.0f, 1.0f, 1.0f);
	refreshStage();
}

void LiveToolsDock::openStageProjector()
{
	ensureStage();

	/* Full screen on the chosen display, or a window. */
	QMenu menu(this);
	const QList<QScreen *> screens = QGuiApplication::screens();
	for (int i = 0; i < screens.size(); ++i) {
		const QSize size = screens.at(i)->geometry().size();
		menu.addAction(tr("Pantalla %1: %2 (%3x%4)").arg(i + 1).arg(screens.at(i)->name()).arg(size.width()).arg(size.height()),
			       this, [i]() { obs_frontend_open_projector("Scene", i, "", StageSceneName); });
	}
	menu.addSeparator();
	menu.addAction(tr("En una ventana"), this, []() { obs_frontend_open_projector("Scene", -1, "", StageSceneName); });
	menu.exec(QCursor::pos());
}

/* ------------------------------------------------------------------------ */
/* Phone remote                                                              */

void LiveToolsDock::setRemoteEnabled(bool enabled)
{
	if (enabled && !remoteServer->start())
		enabled = false;
	if (!enabled)
		remoteServer->stop();
	WorshipOutput::SetConfigString(ConfigRemote, enabled ? QStringLiteral("1") : QStringLiteral("0"));
	remoteButton->setChecked(enabled);
	updateRemoteInfo();
}

void LiveToolsDock::updateRemoteInfo()
{
	if (!remoteServer->isRunning()) {
		remoteButton->setText(tr("Activar control desde el celular"));
		remoteInfo->setText(tr("El celular debe estar en la misma red Wi-Fi que esta computadora. No hace falta "
				       "instalar nada."));
		remoteQr->clear();
		remoteQr->setVisible(false);
		return;
	}
	remoteButton->setText(tr("Desactivar control desde el celular"));
	remoteInfo->setText(tr("Escanea el codigo o abre <b>%1</b> en el navegador del celular.<br>Codigo de acceso: "
			       "<b>%2</b>")
				    .arg(remoteServer->url(), remoteServer->pin()));
	remoteQr->setPixmap(QPixmap::fromImage(remoteServer->qrCode(180)));
	remoteQr->setVisible(true);
}
