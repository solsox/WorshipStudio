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

#include "BackgroundSearch.hpp"
#include "BackgroundLibrary.hpp"
#include "WorshipOutput.hpp"

#include <Idian/Utils.hpp>

#include <QButtonGroup>
#include <QDesktopServices>
#include <QDialogButtonBox>
#include <QDir>
#include <QFile>
#include <QHBoxLayout>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QPainter>
#include <QPointer>
#include <QPushButton>
#include <QStandardPaths>
#include <QUrlQuery>
#include <QVBoxLayout>

#include "moc_BackgroundSearch.cpp"

namespace {
constexpr int FileRole = Qt::UserRole;     /* URL of the file to download */
constexpr int VideoRole = Qt::UserRole + 1;
constexpr int IdRole = Qt::UserRole + 2;
const QSize Thumb(240, 135);

QNetworkRequest request(const QUrl &url)
{
	QNetworkRequest req(url);
	req.setRawHeader("Authorization", BackgroundSearch::ApiKey().toUtf8());
	req.setTransferTimeout(30000);
	return req;
}

QIcon placeholder()
{
	QPixmap pixmap(Thumb);
	pixmap.fill(QColor(0x18, 0x17, 0x16));
	return QIcon(pixmap);
}
} // namespace

QString BackgroundSearch::ApiKey()
{
	const QByteArray environment = qgetenv("WORSHIPSTUDIO_PEXELS_KEY");
	if (!environment.isEmpty())
		return QString::fromUtf8(environment);
	const QString configured = WorshipOutput::ConfigString("PexelsKey");
	if (!configured.isEmpty())
		return configured;
#ifdef WORSHIPSTUDIO_PEXELS_KEY
	return QStringLiteral(WORSHIPSTUDIO_PEXELS_KEY);
#else
	return {};
#endif
}

QStringList BackgroundSearch::Run(QWidget *parent, bool allowVideo)
{
	if (ApiKey().isEmpty()) {
		/* No key in this build: the Biblioteca page lists free sites. */
		QDesktopServices::openUrl(QUrl(QStringLiteral("https://solsox.github.io/WorshipStudio/biblioteca.html#mas")));
		return {};
	}
	BackgroundSearch dialog(parent, allowVideo);
	dialog.exec();
	return dialog.added;
}

BackgroundSearch::BackgroundSearch(QWidget *parent, bool allowVideo_)
	: QDialog(parent),
	  allowVideo(allowVideo_),
	  network(new QNetworkAccessManager(this))
{
	setWindowTitle(tr("Buscar fondos en internet"));
	resize(860, 640);
	auto *layout = new QVBoxLayout(this);
	layout->setSpacing(10);

	searchEdit = new QLineEdit(this);
	searchEdit->setPlaceholderText(tr("Paisaje, cielo, luces, cruz, montañas..."));
	searchEdit->setClearButtonEnabled(true);
	auto *searchButton = new QPushButton(tr("Buscar"), this);
	auto *top = new QHBoxLayout();
	top->addWidget(searchEdit, 1);
	top->addWidget(searchButton);
	layout->addLayout(top);

	/* Ideas to start, and photos or moving backgrounds. */
	auto *chips = new QHBoxLayout();
	chips->setSpacing(6);
	const QStringList ideas{tr("Paisajes"), tr("Cielo"), tr("Montañas"), tr("Luces"), tr("Cruz"), tr("Naturaleza"),
				tr("Abstracto")};
	for (const QString &idea : ideas) {
		auto *chip = new QPushButton(idea, this);
		connect(chip, &QPushButton::clicked, this, [this, idea]() {
			searchEdit->setText(idea);
			search(idea);
		});
		chips->addWidget(chip);
	}
	chips->addStretch(1);
	kindGroup = new QButtonGroup(this);
	auto *photos = new QPushButton(tr("Fotos"), this);
	photos->setCheckable(true);
	photos->setChecked(true);
	kindGroup->addButton(photos, 0);
	chips->addWidget(photos);
	if (allowVideo) {
		auto *videos = new QPushButton(tr("En movimiento"), this);
		videos->setCheckable(true);
		kindGroup->addButton(videos, 1);
		chips->addWidget(videos);
	}
	layout->addLayout(chips);

	results = new QListWidget(this);
	results->setViewMode(QListView::IconMode);
	results->setIconSize(Thumb);
	results->setGridSize(Thumb + QSize(16, 16));
	results->setResizeMode(QListView::Adjust);
	results->setMovement(QListView::Static);
	results->setSelectionMode(QAbstractItemView::ExtendedSelection);
	layout->addWidget(results, 1);

	status = new QLabel(this);
	status->setTextFormat(Qt::RichText);
	status->setOpenExternalLinks(true);
	idian::Utils::addClass(status, "text-muted");
	layout->addWidget(status);

	auto *buttons = new QDialogButtonBox(QDialogButtonBox::Close, this);
	addButton = buttons->addButton(tr("Agregar a Mis fondos"), QDialogButtonBox::ActionRole);
	idian::Utils::addClass(addButton, "button-primary");
	addButton->setEnabled(false);
	layout->addWidget(buttons);

	connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
	connect(addButton, &QPushButton::clicked, this, &BackgroundSearch::addSelected);
	connect(results, &QListWidget::itemDoubleClicked, this, [this]() { addSelected(); });
	connect(results, &QListWidget::itemSelectionChanged, this,
		[this]() { addButton->setEnabled(!results->selectedItems().isEmpty()); });
	connect(searchButton, &QPushButton::clicked, this, [this]() { search(searchEdit->text()); });
	connect(searchEdit, &QLineEdit::returnPressed, this, [this]() { search(searchEdit->text()); });
	connect(kindGroup, &QButtonGroup::idClicked, this, [this]() { search(searchEdit->text()); });

	searchEdit->setText(tr("Paisajes"));
	search(searchEdit->text());
}

void BackgroundSearch::search(const QString &text)
{
	const QString query = text.trimmed().isEmpty() ? tr("Paisajes") : text.trimmed();
	const bool videos = kindGroup->checkedId() == 1;
	const int token = ++searchToken;
	results->clear();
	status->setText(tr("Buscando..."));

	QUrl url(videos ? QStringLiteral("https://api.pexels.com/videos/search")
			: QStringLiteral("https://api.pexels.com/v1/search"));
	QUrlQuery params;
	params.addQueryItem(QStringLiteral("query"), query);
	params.addQueryItem(QStringLiteral("orientation"), QStringLiteral("landscape"));
	params.addQueryItem(QStringLiteral("per_page"), QStringLiteral("30"));
	params.addQueryItem(QStringLiteral("locale"), QStringLiteral("es-ES"));
	url.setQuery(params);

	QNetworkReply *reply = network->get(request(url));
	connect(reply, &QNetworkReply::finished, this, [this, reply, videos, token]() {
		reply->deleteLater();
		if (token != searchToken)
			return;
		if (reply->error() != QNetworkReply::NoError) {
			status->setText(tr("No se pudo buscar. Revisá la conexión a internet."));
			return;
		}
		const QJsonObject root = QJsonDocument::fromJson(reply->readAll()).object();
		const QJsonArray list = root.value(videos ? QStringLiteral("videos") : QStringLiteral("photos")).toArray();
		for (const QJsonValue &value : list) {
			const QJsonObject entry = value.toObject();
			QString thumb, file, author;
			if (videos) {
				thumb = entry.value(QStringLiteral("image")).toString();
				author = entry.value(QStringLiteral("user")).toObject().value(QStringLiteral("name")).toString();
				/* The largest file up to Full HD: sharp and not too heavy. */
				int best = 0;
				for (const QJsonValue &f : entry.value(QStringLiteral("video_files")).toArray()) {
					const QJsonObject vf = f.toObject();
					const int width = vf.value(QStringLiteral("width")).toInt();
					if (vf.value(QStringLiteral("file_type")).toString() == QStringLiteral("video/mp4") &&
					    width <= 1920 && width > best) {
						best = width;
						file = vf.value(QStringLiteral("link")).toString();
					}
				}
			} else {
				const QJsonObject src = entry.value(QStringLiteral("src")).toObject();
				thumb = src.value(QStringLiteral("medium")).toString();
				file = src.value(QStringLiteral("large2x")).toString();
				author = entry.value(QStringLiteral("photographer")).toString();
			}
			if (file.isEmpty())
				continue;
			auto *item = new QListWidgetItem(placeholder(), QString());
			item->setToolTip(videos ? tr("Video de %1 en Pexels").arg(author) : tr("Foto de %1 en Pexels").arg(author));
			item->setData(FileRole, file);
			item->setData(VideoRole, videos);
			item->setData(IdRole, entry.value(QStringLiteral("id")).toVariant().toString());
			results->addItem(item);
			/* Thumbnails arrive one by one. */
			QPointer<BackgroundSearch> self(this);
			QNetworkReply *picture = network->get(QNetworkRequest(QUrl(thumb)));
			connect(picture, &QNetworkReply::finished, this, [self, picture, item, token]() {
				picture->deleteLater();
				if (!self || token != self->searchToken)
					return;
				QImage image = QImage::fromData(picture->readAll());
				if (image.isNull())
					return;
				image = image.scaled(Thumb, Qt::KeepAspectRatioByExpanding, Qt::SmoothTransformation);
				image = image.copy((image.width() - Thumb.width()) / 2, (image.height() - Thumb.height()) / 2,
						   Thumb.width(), Thumb.height());
				if (item->data(VideoRole).toBool()) {
					QPainter painter(&image);
					painter.setRenderHint(QPainter::Antialiasing);
					painter.setBrush(QColor(0, 0, 0, 140));
					painter.setPen(Qt::NoPen);
					painter.drawEllipse(QPointF(Thumb.width() / 2.0, Thumb.height() / 2.0), 20, 20);
					QPolygonF play{QPointF(Thumb.width() / 2.0 - 6, Thumb.height() / 2.0 - 10),
						       QPointF(Thumb.width() / 2.0 - 6, Thumb.height() / 2.0 + 10),
						       QPointF(Thumb.width() / 2.0 + 11, Thumb.height() / 2.0)};
					painter.setBrush(Qt::white);
					painter.drawPolygon(play);
				}
				item->setIcon(QIcon(QPixmap::fromImage(image)));
			});
		}
		status->setText(list.isEmpty() ? tr("Sin resultados. Probá con otra palabra.")
					       : tr("Elegí uno o varios y tocá Agregar. Fotos y videos gratis de "
						    "<a href=\"https://www.pexels.com\">Pexels</a>."));
	});
}

void BackgroundSearch::addSelected()
{
	const QList<QListWidgetItem *> items = results->selectedItems();
	if (items.isEmpty())
		return;
	addButton->setEnabled(false);
	const QString temp = QDir(QStandardPaths::writableLocation(QStandardPaths::TempLocation)).absoluteFilePath(
		QStringLiteral("worshipstudio-fondos"));
	QDir().mkpath(temp);
	auto pending = std::make_shared<int>((int)items.size());
	status->setText(tr("Descargando %n fondo(s)...", nullptr, (int)items.size()));
	for (QListWidgetItem *item : items) {
		const bool video = item->data(VideoRole).toBool();
		const QString name = QStringLiteral("pexels-%1.%2").arg(item->data(IdRole).toString(), video ? "mp4" : "jpg");
		QNetworkReply *reply = network->get(QNetworkRequest(QUrl(item->data(FileRole).toString())));
		QPointer<BackgroundSearch> self(this);
		connect(reply, &QNetworkReply::finished, this, [self, reply, temp, name, pending]() {
			reply->deleteLater();
			if (!self)
				return;
			if (reply->error() == QNetworkReply::NoError) {
				const QString path = QDir(temp).absoluteFilePath(name);
				QFile file(path);
				if (file.open(QIODevice::WriteOnly)) {
					file.write(reply->readAll());
					file.close();
					self->added << BackgroundLibrary::Add({path});
					QFile::remove(path);
				}
			}
			if (--*pending == 0) {
				self->status->setText(tr("Listo: ya están en Mis fondos."));
				self->addButton->setEnabled(true);
			}
		});
	}
}
