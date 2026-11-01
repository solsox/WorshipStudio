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

#include "PresentationDock.hpp"
#include "WorshipDocument.hpp"
#include "WorshipLive.hpp"
#include "WorshipOutput.hpp"
#include "WorshipRender.hpp"

#include <qt-wrappers.hpp>
#include <Idian/Utils.hpp>

#include <QAbstractItemView>
#include <QCheckBox>
#include <QCollator>
#include <QComboBox>
#include <QCryptographicHash>
#include <QDateTime>
#include <QDir>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QFileDialog>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QImageReader>
#include <QJsonArray>
#include <QJsonDocument>
#include <QKeyEvent>
#include <QLabel>
#include <QListWidget>
#include <QListWidgetItem>
#include <QMimeData>
#include <QPainter>
#include <QPixmap>
#include <QPushButton>
#include <QSplitter>
#include <QTimer>
#include <QVBoxLayout>

#include <algorithm>

#include "moc_PresentationDock.cpp"

namespace {
constexpr const char *SceneName = "WS Presentacion";
constexpr const char *SlideSourceName = "WS Presentacion - Diapositiva";
constexpr const char *ConfigRecent = "PresentationRecent";
constexpr const char *ConfigFit = "PresentationFit";
constexpr const char *ConfigBook = "PresentationBookMode";
constexpr const char *ConfigHotkeys = "PresentationHotkeys";
constexpr int MaxRecent = 15;
constexpr int ThumbnailsPerTick = 3;
const QSize ThumbnailSize(160, 90);
const QString ImagesPrefix = QStringLiteral("imagenes:");

/* Pages taller than this (height / width) are split in two in book mode. */
constexpr double TallPageRatio = 1.15;
/* Each half shows a bit of the other one so no line is cut in two. */
constexpr double PartOverlap = 0.12;

QStringList naturalSorted(QStringList paths)
{
	QCollator collator;
	collator.setNumericMode(true);
	collator.setCaseSensitivity(Qt::CaseInsensitive);
	std::sort(paths.begin(), paths.end(),
		  [&](const QString &a, const QString &b) { return collator.compare(a, b) < 0; });
	return paths;
}

QStringList imagesInFolder(const QString &folder)
{
	QStringList images;
	for (const QFileInfo &info : QDir(folder).entryInfoList(QDir::Files, QDir::NoSort)) {
		if (WorshipDocument::IsImage(info.filePath()))
			images << info.absoluteFilePath();
	}
	return naturalSorted(images);
}

/* Bounding box of everything that is not (almost) white, with a little
 * padding, so book pages are shown without their wide margins. */
QRect contentRect(const QImage &page)
{
	const QImage image = page.convertToFormat(QImage::Format_RGB32);
	const int step = qMax(1, image.width() / 600);
	int left = image.width(), right = -1, top = image.height(), bottom = -1;

	for (int y = 0; y < image.height(); y += step) {
		const QRgb *line = reinterpret_cast<const QRgb *>(image.constScanLine(y));
		for (int x = 0; x < image.width(); x += step) {
			const QRgb pixel = line[x];
			if (qRed(pixel) > 225 && qGreen(pixel) > 225 && qBlue(pixel) > 225)
				continue;
			left = qMin(left, x);
			right = qMax(right, x);
			top = qMin(top, y);
			bottom = qMax(bottom, y);
		}
	}

	if (right < left || bottom < top)
		return image.rect();

	const int pad = qMax(4, image.width() / 40);
	return QRect(QPoint(left, top), QPoint(right, bottom)).adjusted(-pad, -pad, pad, pad).intersected(image.rect());
}

void hotkeyNext(void *data, obs_hotkey_id, obs_hotkey_t *, bool pressed)
{
	if (pressed)
		QMetaObject::invokeMethod(static_cast<PresentationDock *>(data), "nextSlide", Qt::QueuedConnection);
}

void hotkeyPrevious(void *data, obs_hotkey_id, obs_hotkey_t *, bool pressed)
{
	if (pressed)
		QMetaObject::invokeMethod(static_cast<PresentationDock *>(data), "previousSlide", Qt::QueuedConnection);
}

void hotkeyHide(void *data, obs_hotkey_id, obs_hotkey_t *, bool pressed)
{
	if (pressed)
		QMetaObject::invokeMethod(static_cast<PresentationDock *>(data), "toggleHidden", Qt::QueuedConnection);
}
} // namespace

PresentationDock::PresentationDock(const QString &objectName, const QString &title, QWidget *parent)
	: OBSDock(title, parent)
{
	setObjectName(objectName);
	setAcceptDrops(true);

	auto *root = new QWidget(this);
	auto *layout = new QVBoxLayout(root);
	layout->setContentsMargins(8, 8, 8, 8);
	layout->setSpacing(6);

	auto *openButton = new QPushButton(tr("Abrir..."), root);
	openButton->setToolTip(tr("PDF, PowerPoint, Keynote, Impress o imagenes. Tambien podes arrastrar archivos o "
				  "carpetas aqui."));
	auto *removeButton = new QPushButton(tr("Quitar"), root);
	removeButton->setToolTip(tr("Quitar la presentacion seleccionada de la lista (no borra el archivo)"));

	auto *openRow = new QHBoxLayout();
	openRow->addWidget(openButton, 1);
	openRow->addWidget(removeButton);

	decksList = new QListWidget(root);
	decksList->setSelectionMode(QAbstractItemView::SingleSelection);

	slidesList = new QListWidget(root);
	slidesList->setViewMode(QListView::IconMode);
	slidesList->setIconSize(ThumbnailSize);
	slidesList->setResizeMode(QListView::Adjust);
	slidesList->setMovement(QListView::Static);
	slidesList->setSpacing(4);
	slidesList->setUniformItemSizes(true);
	slidesList->setSelectionMode(QAbstractItemView::SingleSelection);
	slidesList->installEventFilter(this);

	auto *splitter = new QSplitter(Qt::Vertical, root);
	splitter->addWidget(decksList);
	splitter->addWidget(slidesList);
	splitter->setStretchFactor(0, 1);
	splitter->setStretchFactor(1, 4);

	fitCombo = new QComboBox(root);
	fitCombo->addItem(tr("Fondo difuminado"));
	fitCombo->addItem(tr("Estirar"));
	fitCombo->addItem(tr("Barras negras"));
	fitCombo->setToolTip(tr("Como llenar la pantalla cuando la pagina no es 16:9"));
	fitCombo->setCurrentIndex(qBound(0, WorshipOutput::ConfigString(ConfigFit).toInt(), 2));

	bookCheck = new QCheckBox(tr("Modo libro"), root);
	bookCheck->setToolTip(tr("Recorta los margenes blancos y divide las paginas verticales en dos mitades para "
				 "que el texto se lea grande"));
	bookCheck->setChecked(WorshipOutput::ConfigString(ConfigBook) == QStringLiteral("1"));

	projectButton = new QPushButton(tr("Proyectar"), root);
	idian::Utils::addClass(projectButton, "button-primary");
	projectButton->setDefault(true);

	auto *projectRow = new QHBoxLayout();
	projectRow->addWidget(fitCombo, 1);
	projectRow->addWidget(bookCheck);
	projectRow->addWidget(projectButton);

	previousButton = new QPushButton(tr("< Anterior"), root);
	nextButton = new QPushButton(tr("Siguiente >"), root);
	hideButton = new QPushButton(tr("Ocultar"), root);
	hideButton->setCheckable(true);
	hideButton->setToolTip(tr("Quita la diapositiva de la pantalla sin perder el lugar (tecla B)"));

	auto *liveRow = new QHBoxLayout();
	liveRow->addWidget(previousButton);
	liveRow->addWidget(nextButton);
	liveRow->addWidget(hideButton);

	liveLabel = new QLabel(root);
	idian::Utils::addClass(liveLabel, "text-bold");
	liveLabel->setWordWrap(true);
	statusLabel = new QLabel(root);
	idian::Utils::addClass(statusLabel, "text-muted");
	statusLabel->setWordWrap(true);

	layout->addLayout(openRow);
	layout->addWidget(splitter, 1);
	layout->addLayout(projectRow);
	layout->addLayout(liveRow);
	layout->addWidget(liveLabel);
	layout->addWidget(statusLabel);
	setWidget(root);

	thumbnailTimer = new QTimer(this);
	thumbnailTimer->setInterval(0);

	converter = new WorshipDocument::Converter(this);

	connect(openButton, &QPushButton::clicked, this, &PresentationDock::onOpenClicked);
	connect(removeButton, &QPushButton::clicked, this, &PresentationDock::onRemoveClicked);
	connect(decksList, &QListWidget::itemClicked, this, &PresentationDock::onDeckActivated);
	connect(decksList, &QListWidget::itemActivated, this, &PresentationDock::onDeckActivated);
	connect(slidesList, &QListWidget::itemClicked, this, &PresentationDock::onSlideClicked);
	connect(slidesList, &QListWidget::itemActivated, this, &PresentationDock::onSlideClicked);
	connect(projectButton, &QPushButton::clicked, this, &PresentationDock::project);
	connect(previousButton, &QPushButton::clicked, this, &PresentationDock::previousSlide);
	connect(nextButton, &QPushButton::clicked, this, &PresentationDock::nextSlide);
	connect(hideButton, &QPushButton::clicked, this, &PresentationDock::toggleHidden);
	connect(thumbnailTimer, &QTimer::timeout, this, &PresentationDock::onThumbnailTick);
	connect(converter, &WorshipDocument::Converter::progress, this, [this](const QString &text) { setStatus(text); });
	connect(fitCombo, &QComboBox::currentIndexChanged, this, [this](int index) {
		WorshipOutput::SetConfigString(ConfigFit, QString::number(index));
		if (live) {
			const int slide = liveSlide;
			liveSlide = -1;
			showSlide(slide);
		}
	});
	connect(bookCheck, &QCheckBox::toggled, this, [this](bool checked) {
		WorshipOutput::SetConfigString(ConfigBook, checked ? QStringLiteral("1") : QStringLiteral("0"));
		/* Keep showing the same page after the slides change. */
		const int page = live && liveSlide >= 0 ? slides.at(liveSlide).page : -1;
		rebuildSlides();
		if (page >= 0) {
			int slide = 0;
			while (slide < slides.size() && slides.at(slide).page < page)
				++slide;
			liveSlide = -1;
			showSlide(slide);
		}
	});

	WorshipLive::instance().registerTarget(QStringLiteral("Presentaciones"), this);
	registerHotkeys();
	rebuildDecksList();
	updateControls();
	updateLiveLabel();

	const QStringList recent = recentKeys();
	if (recent.isEmpty())
		setStatus(tr("Abri un PDF o un PowerPoint, o arrastralo aqui."));
	else
		setStatus(tr("Elegi una presentacion de la lista."));
}

PresentationDock::~PresentationDock()
{
	saveHotkeys();
	for (obs_hotkey_id id : {nextHotkey, previousHotkey, hideHotkey}) {
		if (id != OBS_INVALID_HOTKEY_ID)
			obs_hotkey_unregister(id);
	}
}

/* ------------------------------------------------------------------------ */
/* Hotkeys: work with presentation clickers and while OBS is not focused.   */

void PresentationDock::registerHotkeys()
{
	nextHotkey = obs_hotkey_register_frontend("WorshipStudio.Presentation.Next",
						  "Presentacion: diapositiva siguiente", hotkeyNext, this);
	previousHotkey = obs_hotkey_register_frontend("WorshipStudio.Presentation.Previous",
						      "Presentacion: diapositiva anterior", hotkeyPrevious, this);
	hideHotkey = obs_hotkey_register_frontend("WorshipStudio.Presentation.Hide",
						  "Presentacion: ocultar / mostrar", hotkeyHide, this);

	const QString json = WorshipOutput::ConfigString(ConfigHotkeys);
	if (json.isEmpty())
		return;
	OBSDataAutoRelease data = obs_data_create_from_json(QT_TO_UTF8(json));
	if (!data)
		return;
	const std::pair<const char *, obs_hotkey_id> entries[] = {
		{"next", nextHotkey}, {"previous", previousHotkey}, {"hide", hideHotkey}};
	for (const auto &[name, id] : entries) {
		OBSDataArrayAutoRelease array = obs_data_get_array(data, name);
		obs_hotkey_load(id, array);
	}
}

void PresentationDock::saveHotkeys()
{
	OBSDataAutoRelease data = obs_data_create();
	const std::pair<const char *, obs_hotkey_id> entries[] = {
		{"next", nextHotkey}, {"previous", previousHotkey}, {"hide", hideHotkey}};
	for (const auto &[name, id] : entries) {
		OBSDataArrayAutoRelease array = obs_hotkey_save(id);
		obs_data_set_array(data, name, array);
	}
	WorshipOutput::SetConfigString(ConfigHotkeys, QT_UTF8(obs_data_get_json(data)));
}

/* ------------------------------------------------------------------------ */
/* Opening files                                                             */

void PresentationDock::onOpenClicked()
{
	const QString lastDir = WorshipOutput::ConfigString("PresentationLastDir");
	const QStringList files = QFileDialog::getOpenFileNames(this, tr("Abrir presentacion"),
								 lastDir.isEmpty() ? QDir::homePath() : lastDir,
								 WorshipDocument::OpenFilter());
	if (files.isEmpty())
		return;
	WorshipOutput::SetConfigString("PresentationLastDir", QFileInfo(files.first()).absolutePath());
	openFiles(files);
}

void PresentationDock::openFiles(const QStringList &paths)
{
	/* Images (and folders of images) become one presentation; every PDF or
	 * office file is its own presentation. */
	QStringList images, documents;
	for (const QString &path : paths) {
		const QFileInfo info(path);
		if (info.isDir())
			images << imagesInFolder(info.absoluteFilePath());
		else if (WorshipDocument::IsImage(path))
			images << info.absoluteFilePath();
		else if (WorshipDocument::IsPdf(path) || WorshipDocument::NeedsConversion(path))
			documents << info.absoluteFilePath();
	}

	if (images.isEmpty() && documents.isEmpty()) {
		setStatus(tr("No se reconoce el archivo. Formatos: PDF, PowerPoint (ppt, pptx, pps, ppsx), Keynote, "
			     "Impress (odp) e imagenes."),
			  true);
		return;
	}

	QString toLoad;
	for (const QString &document : documents) {
		rememberDeck(document);
		toLoad = document;
	}
	if (!images.isEmpty()) {
		toLoad = ImagesPrefix + images.join(QLatin1Char('|'));
		rememberDeck(toLoad);
	}
	loadDeck(toLoad);
}

void PresentationDock::onRemoveClicked()
{
	QListWidgetItem *item = decksList->currentItem();
	if (!item)
		return;
	const QString key = item->data(Qt::UserRole).toString();
	QStringList recent = recentKeys();
	recent.removeAll(key);
	WorshipOutput::SetConfigString(ConfigRecent,
				       QString::fromUtf8(QJsonDocument(QJsonArray::fromStringList(recent)).toJson(
					       QJsonDocument::Compact)));
	rebuildDecksList();
}

void PresentationDock::onDeckActivated(QListWidgetItem *item)
{
	if (!item)
		return;
	const QString key = item->data(Qt::UserRole).toString();
	if (key != deck.key || deck.pageCount == 0)
		loadDeck(key);
}

void PresentationDock::loadDeck(const QString &key)
{
	lastError.clear();
	converter->cancel();

	Deck loaded;
	loaded.key = key;
	loaded.title = titleFor(key);

	if (key.startsWith(ImagesPrefix)) {
		for (const QString &path : key.mid(ImagesPrefix.size()).split(QLatin1Char('|'), Qt::SkipEmptyParts)) {
			if (QFileInfo::exists(path))
				loaded.images << path;
		}
		loaded.pageCount = (int)loaded.images.size();
		if (loaded.pageCount == 0) {
			setStatus(tr("Las imagenes de esta presentacion ya no existen."), true);
			return;
		}
		setDeck(loaded);
		return;
	}

	if (!QFileInfo::exists(key)) {
		setStatus(tr("No se encuentra el archivo: %1").arg(key), true);
		return;
	}

	if (WorshipDocument::NeedsConversion(key)) {
		setStatus(tr("Preparando %1...").arg(loaded.title));
		converter->convert(key, [this, loaded](const QString &pdfPath, const QString &error) mutable {
			if (pdfPath.isEmpty()) {
				setStatus(error, true);
				return;
			}
			loaded.pdfPath = pdfPath;
			QString pdfError;
			loaded.pageCount = WorshipDocument::PdfPageCount(pdfPath, &pdfError);
			if (loaded.pageCount == 0) {
				setStatus(pdfError, true);
				return;
			}
			setDeck(loaded);
		});
		return;
	}

	QString error;
	loaded.pdfPath = key;
	loaded.pageCount = WorshipDocument::PdfPageCount(key, &error);
	if (loaded.pageCount == 0) {
		setStatus(error, true);
		return;
	}
	setDeck(loaded);
}

bool PresentationDock::isConverting() const
{
	return converter->busy();
}

void PresentationDock::setDeck(const Deck &newDeck)
{
	if (live)
		stopLive();

	deck = newDeck;
	rebuildSlides();
	rebuildDecksList();
	setStatus(tr("%1: %2 paginas, %3 diapositivas").arg(deck.title).arg(deck.pageCount).arg(slides.size()));
	updateControls();
}

void PresentationDock::setBookMode(bool enabled)
{
	bookCheck->setChecked(enabled);
}

/* ------------------------------------------------------------------------ */
/* Slides and rendering                                                      */

void PresentationDock::rebuildSlides()
{
	slides.clear();
	const bool book = bookCheck->isChecked();

	for (int page = 0; page < deck.pageCount; ++page) {
		int parts = 1;
		if (book) {
			QSizeF size;
			if (!deck.pdfPath.isEmpty())
				size = WorshipDocument::PdfPageSize(deck.pdfPath, page);
			else
				size = QImageReader(deck.images.at(page)).size();
			if (size.width() > 0 && size.height() / size.width() > TallPageRatio)
				parts = 2;
		}
		for (int part = 0; part < parts; ++part)
			slides.push_back({page, part, parts});
	}

	rebuildThumbnails();
}

void PresentationDock::rebuildThumbnails()
{
	thumbnailTimer->stop();
	QSignalBlocker blocker(slidesList);
	slidesList->clear();

	QPixmap placeholder(ThumbnailSize);
	placeholder.fill(QColor(40, 40, 40));
	const QIcon placeholderIcon(placeholder);

	for (int i = 0; i < slides.size(); ++i) {
		const Slide &slide = slides.at(i);
		QString label = QString::number(slide.page + 1);
		if (slide.parts > 1)
			label += slide.part == 0 ? QStringLiteral(" (arriba)") : QStringLiteral(" (abajo)");
		auto *item = new QListWidgetItem(placeholderIcon, label);
		item->setData(Qt::UserRole, i);
		slidesList->addItem(item);
	}

	if (live && liveSlide >= 0)
		slidesList->setCurrentRow(liveSlide);

	nextThumbnail = 0;
	if (!slides.isEmpty())
		thumbnailTimer->start();
}

void PresentationDock::onThumbnailTick()
{
	/* A few per tick keeps the dock responsive with long books. */
	for (int n = 0; n < ThumbnailsPerTick && nextThumbnail < slides.size(); ++n, ++nextThumbnail) {
		const QImage image = renderSlide(nextThumbnail, ThumbnailSize * 2);
		if (QListWidgetItem *item = slidesList->item(nextThumbnail); item && !image.isNull())
			item->setIcon(QIcon(QPixmap::fromImage(
				image.scaled(ThumbnailSize, Qt::KeepAspectRatio, Qt::SmoothTransformation))));
	}
	if (nextThumbnail >= slides.size())
		thumbnailTimer->stop();
}

QImage PresentationDock::renderSlide(int index, const QSize &canvas) const
{
	if (index < 0 || index >= slides.size())
		return {};

	const Slide &slide = slides.at(index);
	const bool book = bookCheck->isChecked();

	/* Book pages are cropped and split, so they are rendered bigger. */
	const QSize box = book ? QSize(canvas.width() * 3 / 2, canvas.height() * slide.parts * 3 / 2) : canvas;

	QImage page;
	if (!deck.pdfPath.isEmpty()) {
		page = WorshipDocument::RenderPdfPage(deck.pdfPath, slide.page, box);
	} else {
		QImageReader reader(deck.images.at(slide.page));
		reader.setAutoTransform(true);
		const QSize size = reader.size();
		if (size.isValid() && (size.width() > box.width() * 2 || size.height() > box.height() * 2))
			reader.setScaledSize(size.scaled(box * 2, Qt::KeepAspectRatio));
		page = reader.read();
	}
	if (page.isNull())
		return {};

	if (book)
		page = page.copy(contentRect(page));

	if (slide.parts > 1) {
		const int height = qMin(page.height(), (int)(page.height() / slide.parts * (1.0 + PartOverlap)));
		const int top = (page.height() - height) * slide.part / (slide.parts - 1);
		page = page.copy(0, top, page.width(), height);
	}
	return page;
}

QString PresentationDock::renderedSlide(int index) const
{
	const Slide &slide = slides.at(index);
	const QSize canvas(WorshipOutput::CanvasWidth(), WorshipOutput::CanvasHeight());
	const QString file = deck.pdfPath.isEmpty() ? deck.images.at(slide.page) : deck.pdfPath;

	const QByteArray key = QStringLiteral("%1|%2|%3|%4/%5|%6|%7x%8")
				       .arg(QFileInfo(file).absoluteFilePath())
				       .arg(QFileInfo(file).lastModified().toMSecsSinceEpoch())
				       .arg(slide.page)
				       .arg(slide.part)
				       .arg(slide.parts)
				       .arg(bookCheck->isChecked())
				       .arg(canvas.width())
				       .arg(canvas.height())
				       .toUtf8();
	const QString hash = QString::fromLatin1(QCryptographicHash::hash(key, QCryptographicHash::Sha1).toHex().left(20));
	const QString page =
		QDir(WorshipRender::CacheDir(QStringLiteral("presentations/pages"))).absoluteFilePath(hash + ".png");

	if (!QFileInfo::exists(page)) {
		const QImage image = renderSlide(index, canvas);
		if (image.isNull() || !image.save(page, "PNG"))
			return {};
	}

	/* Same composition as the hymnal: blurred fill, stretch or bars. */
	return WorshipRender::HymnSlide(page, static_cast<WorshipRender::SlideFit>(fitCombo->currentIndex()), canvas);
}

/* ------------------------------------------------------------------------ */
/* Live output                                                               */

bool PresentationDock::prepareOutput()
{
	scene = WorshipOutput::EnsureScene(SceneName);
	if (!scene)
		return false;

	bool created = false;
	OBSDataAutoRelease settings = obs_data_create();
	slideSource = WorshipOutput::EnsureSource(scene, SlideSourceName, "image_source", settings, &created);
	if (created && slideSource)
		WorshipOutput::FitItem(scene, slideSource, 0.0f, 0.0f, 1.0f, 1.0f);
	return slideSource;
}

bool PresentationDock::project()
{
	if (slides.isEmpty())
		return false;
	if (!prepareOutput()) {
		setStatus(tr("No se pudieron crear las fuentes de proyeccion."), true);
		return false;
	}

	const int start = qMax(0, slidesList->currentRow());
	live = true;
	liveSlide = -1;
	hidden = false;
	if (obs_sceneitem_t *item = obs_scene_find_source(scene, SlideSourceName))
		obs_sceneitem_set_visible(item, true);

	showSlide(start);
	WorshipOutput::ShowScene(scene);
	return liveSlide >= 0;
}

void PresentationDock::showSlide(int slide)
{
	if (slides.isEmpty())
		return;
	if (!live) {
		/* Clicking a thumbnail before projecting starts from there. */
		selectSlideRow(qBound(0, slide, (int)slides.size() - 1));
		project();
		return;
	}

	slide = qBound(0, slide, (int)slides.size() - 1);
	if (slide != liveSlide) {
		const QString path = renderedSlide(slide);
		if (path.isEmpty()) {
			setStatus(tr("No se pudo dibujar la pagina %1.").arg(slides.at(slide).page + 1), true);
			return;
		}
		OBSDataAutoRelease settings = obs_data_create();
		obs_data_set_string(settings, "file", QT_TO_UTF8(path));
		obs_source_update(slideSource, settings);
		liveSlide = slide;

		/* Prepare the next slide ahead of time so it changes instantly. */
		if (slide + 1 < slides.size()) {
			const QString key = deck.key;
			QTimer::singleShot(0, this, [this, key, slide]() {
				if (live && key == deck.key && slide + 1 < slides.size())
					renderedSlide(slide + 1);
			});
		}
	}

	selectSlideRow(slide);
	updateControls();
	updateLiveLabel();
	publishLive();
}

void PresentationDock::publishLive()
{
	LiveState state;
	state.source = QStringLiteral("Presentaciones");
	state.title = deck.title;
	const Slide &current = slides.at(liveSlide);
	state.section = tr("Pagina %1").arg(current.page + 1);
	state.index = liveSlide;
	state.count = (int)slides.size();
	state.hidden = hidden;
	WorshipLive::instance().publish(state);
}

void PresentationDock::nextSlide()
{
	if (live)
		showSlide(liveSlide + 1);
}

void PresentationDock::previousSlide()
{
	if (live)
		showSlide(liveSlide - 1);
}

void PresentationDock::toggleHidden()
{
	if (!live || !scene) {
		hideButton->setChecked(false);
		return;
	}
	hidden = !hidden;
	if (obs_sceneitem_t *item = obs_scene_find_source(scene, SlideSourceName))
		obs_sceneitem_set_visible(item, !hidden);
	hideButton->setChecked(hidden);
	updateLiveLabel();
	publishLive();
}

void PresentationDock::stopLive()
{
	live = false;
	liveSlide = -1;
	if (hidden && scene) {
		if (obs_sceneitem_t *item = obs_scene_find_source(scene, SlideSourceName))
			obs_sceneitem_set_visible(item, true);
	}
	hidden = false;
	hideButton->setChecked(false);
	updateControls();
	updateLiveLabel();
	WorshipLive::instance().clear(QStringLiteral("Presentaciones"));
}

void PresentationDock::onSlideClicked(QListWidgetItem *item)
{
	if (item)
		showSlide(item->data(Qt::UserRole).toInt());
}

void PresentationDock::selectSlideRow(int slide)
{
	QSignalBlocker blocker(slidesList);
	slidesList->setCurrentRow(slide);
	if (QListWidgetItem *item = slidesList->item(slide))
		slidesList->scrollToItem(item);
}

bool PresentationDock::eventFilter(QObject *watched, QEvent *event)
{
	if (watched == slidesList && event->type() == QEvent::KeyPress) {
		/* Arrow keys, Page Up/Down (presentation clickers) and B for black. */
		switch (static_cast<QKeyEvent *>(event)->key()) {
		case Qt::Key_Right:
		case Qt::Key_Down:
		case Qt::Key_PageDown:
		case Qt::Key_Space:
			if (!live)
				project();
			else
				nextSlide();
			return true;
		case Qt::Key_Left:
		case Qt::Key_Up:
		case Qt::Key_PageUp:
		case Qt::Key_Backspace:
			previousSlide();
			return true;
		case Qt::Key_Home:
			showSlide(0);
			return true;
		case Qt::Key_End:
			showSlide((int)slides.size() - 1);
			return true;
		case Qt::Key_Return:
		case Qt::Key_Enter:
			showSlide(slidesList->currentRow());
			return true;
		case Qt::Key_B:
		case Qt::Key_Period:
			toggleHidden();
			return true;
		default:
			break;
		}
	}
	return OBSDock::eventFilter(watched, event);
}

void PresentationDock::dragEnterEvent(QDragEnterEvent *event)
{
	if (event->mimeData()->hasUrls())
		event->acceptProposedAction();
}

void PresentationDock::dropEvent(QDropEvent *event)
{
	QStringList paths;
	for (const QUrl &url : event->mimeData()->urls()) {
		if (url.isLocalFile())
			paths << url.toLocalFile();
	}
	if (!paths.isEmpty()) {
		openFiles(paths);
		event->acceptProposedAction();
	}
}

void PresentationDock::updateControls()
{
	projectButton->setEnabled(!slides.isEmpty());
	previousButton->setEnabled(live && liveSlide > 0);
	nextButton->setEnabled(live && liveSlide < slides.size() - 1);
	hideButton->setEnabled(live);
}

void PresentationDock::updateLiveLabel()
{
	if (!live || liveSlide < 0) {
		liveLabel->setText(tr("Sin proyeccion activa"));
		return;
	}
	const Slide &slide = slides.at(liveSlide);
	QString text = tr("En vivo: %1  |  Diapositiva %2/%3").arg(deck.title).arg(liveSlide + 1).arg(slides.size());
	if (slide.parts > 1 || slides.size() != deck.pageCount)
		text += tr("  (pagina %1)").arg(slide.page + 1);
	if (hidden)
		text += tr("  |  OCULTA");
	liveLabel->setText(text);
}

void PresentationDock::setStatus(const QString &text, bool error)
{
	if (error) {
		lastError = text;
		blog(LOG_WARNING, "[WorshipStudio] Presentaciones: %s", QT_TO_UTF8(text));
	}
	statusLabel->setText(text);
}

/* ------------------------------------------------------------------------ */
/* Recent presentations                                                      */

QStringList PresentationDock::recentKeys() const
{
	QStringList keys;
	const QJsonArray array = QJsonDocument::fromJson(WorshipOutput::ConfigString(ConfigRecent).toUtf8()).array();
	for (const QJsonValue value : array)
		keys << value.toString();
	return keys;
}

void PresentationDock::rememberDeck(const QString &key)
{
	QStringList recent = recentKeys();
	recent.removeAll(key);
	recent.prepend(key);
	while (recent.size() > MaxRecent)
		recent.removeLast();
	WorshipOutput::SetConfigString(ConfigRecent,
				       QString::fromUtf8(QJsonDocument(QJsonArray::fromStringList(recent)).toJson(
					       QJsonDocument::Compact)));
	rebuildDecksList();
}

void PresentationDock::rebuildDecksList()
{
	QSignalBlocker blocker(decksList);
	decksList->clear();
	for (const QString &key : recentKeys()) {
		auto *item = new QListWidgetItem(titleFor(key));
		item->setData(Qt::UserRole, key);
		item->setToolTip(key.startsWith(ImagesPrefix) ? key.mid(ImagesPrefix.size()).replace('|', '\n') : key);
		const bool missing = key.startsWith(ImagesPrefix) ? false : !QFileInfo::exists(key);
		if (missing)
			item->setForeground(palette().brush(QPalette::Disabled, QPalette::Text));
		decksList->addItem(item);
		if (key == deck.key)
			decksList->setCurrentItem(item);
	}
}

QString PresentationDock::titleFor(const QString &key)
{
	if (key.startsWith(ImagesPrefix)) {
		const QStringList images = key.mid(ImagesPrefix.size()).split(QLatin1Char('|'), Qt::SkipEmptyParts);
		if (images.isEmpty())
			return QStringLiteral("Imagenes");
		const QString folder = QFileInfo(images.first()).absoluteDir().dirName();
		return images.size() == 1 ? QFileInfo(images.first()).fileName()
					  : QStringLiteral("%1 (%2 imagenes)").arg(folder).arg(images.size());
	}
	return QFileInfo(key).fileName();
}
