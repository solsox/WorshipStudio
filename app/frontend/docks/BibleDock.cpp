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

#include "BibleDock.hpp"
#include "BackgroundLibrary.hpp"
#include "BibleCatalog.hpp"
#include "WorshipLive.hpp"
#include "WorshipOutput.hpp"
#include "WorshipRender.hpp"

#include <qt-wrappers.hpp>
#include <Idian/Utils.hpp>

#include <QAbstractItemView>
#include <QComboBox>
#include <QDir>
#include <QColorDialog>
#include <QFileDialog>
#include <QFileInfo>
#include <QPainter>
#include <QStandardPaths>
#include <QFile>
#include <QHBoxLayout>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QListWidgetItem>
#include <QPushButton>
#include <QRegularExpression>
#include <QSpinBox>
#include <QVBoxLayout>

#include <algorithm>

#include "moc_BibleDock.cpp"

namespace {
constexpr const char *SceneName = "WS Biblia";
constexpr const char *ScreenSourceName = "WS Biblia - Pantalla";
constexpr const char *ConfigFontSize = "BibleFontSize";
constexpr const char *ConfigLayout = "BibleLayout";
constexpr const char *VideoSourceName = "WS Biblia - Video de fondo";
constexpr const char *ConfigVersion = "BibleVersion";
constexpr const char *ConfigFont = "BibleFont";
constexpr const char *ConfigBgKind = "BibleBgKind";
constexpr const char *ConfigBgPreset = "BibleBgPreset";
constexpr const char *ConfigBgPath = "BibleBgPath";
constexpr const char *ConfigBgColor = "BibleBgColor";
constexpr int MaxTextResults = 500;

constexpr int RefBookRole = Qt::UserRole;
constexpr int RefChapterRole = Qt::UserRole + 1;
constexpr int RefVerseRole = Qt::UserRole + 2;

/* Common Spanish abbreviations, in canonical book order. */
const char *const BookAbbreviations[66] = {
	"gn gen",    "ex exo",    "lv lev",  "nm num",   "dt deut",   "jos",     "jue jc",  "rt",    "1s 1sam", "2s 2sam",
	"1r 1re",    "2r 2re",    "1cr",     "2cr",      "esd",       "neh",     "est",     "jb",    "sal sl salmo", "pr prov",
	"ec ecl",    "cnt cantares", "is isa", "jer jr", "lm lam",    "ez eze",  "dn dan",  "os",    "jl",      "am",
	"abd",       "jon",       "mi miq",  "nah",      "hab",       "sof",     "hag",     "zac",   "mal",     "mt mateo",
	"mc mr",     "lc lucas",  "jn",      "hch hech", "ro rom",    "1co 1cor", "2co 2cor", "ga gal", "ef efe", "fil flp",
	"col",       "1ts 1tes",  "2ts 2tes", "1ti 1tim", "2ti 2tim", "tit",     "flm",     "heb",   "stg sant", "1p 1pe",
	"2p 2pe",    "1jn",       "2jn",     "3jn",      "jud",       "ap apoc",
};

QString normalizeText(const QString &text)
{
	QString normalized = text.normalized(QString::NormalizationForm_D);
	normalized.remove(QRegularExpression(QStringLiteral("[\\x{0300}-\\x{036f}]")));
	normalized = normalized.toLower();
	normalized.replace(QRegularExpression(QStringLiteral("[^a-z0-9:\\-]+")), QStringLiteral(" "));
	return normalized.simplified();
}

/* "1juan" -> "1 juan" so numbered books match with or without the space. */
QString normalizeBookQuery(const QString &text)
{
	QString normalized = normalizeText(text);
	normalized.replace(QRegularExpression(QStringLiteral("^([1-3])\\s*([a-z])")), QStringLiteral("\\1 \\2"));
	return normalized;
}
} // namespace

BibleDock::BibleDock(const QString &objectName, const QString &title, const QString &bundledBibleDir, QWidget *parent)
	: OBSDock(title, parent),
	  bundledDir(bundledBibleDir)
{
	setObjectName(objectName);

	auto *root = new QWidget(this);
	auto *layout = new QVBoxLayout(root);
	layout->setContentsMargins(8, 8, 8, 8);
	layout->setSpacing(6);

	versionCombo = new QComboBox(root);
	versionCombo->setToolTip(tr("Version de la Biblia. Al cambiarla se mantiene el pasaje seleccionado."));
	auto *importButton = new QPushButton(tr("Importar..."), root);
	importButton->setToolTip(tr("Anadir una version en formato JSON de WorshipStudio"));
	auto *versionRow = new QHBoxLayout();
	versionRow->addWidget(new QLabel(tr("Version:"), root));
	versionRow->addWidget(versionCombo, 1);
	auto *catalogButton = new QPushButton(tr("Más Biblias..."), root);
	catalogButton->setToolTip(tr("Descargar gratis más de mil traducciones en cientos de idiomas"));
	versionRow->addWidget(catalogButton);
	versionRow->addWidget(importButton);

	searchEdit = new QLineEdit(root);
	searchEdit->setObjectName(QStringLiteral("bibleSearch"));
	searchEdit->setPlaceholderText(tr("Juan 3:16, Sal 23, 1 Cor 13:4-7 o una frase"));
	searchEdit->setClearButtonEnabled(true);

	resultsList = new QListWidget(root);
	resultsList->setSelectionMode(QAbstractItemView::ExtendedSelection);
	resultsList->setWordWrap(true);

	previewLabel = new QLabel(root);
	previewLabel->setMinimumHeight(120);
	previewLabel->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Ignored);
	previewLabel->setAlignment(Qt::AlignCenter);
	previewLabel->setToolTip(tr("Vista previa de como se vera en pantalla"));


	fontSizeSpin = new QSpinBox(root);
	fontSizeSpin->setRange(24, 200);
	fontSizeSpin->setSuffix(QStringLiteral(" pt"));
	fontSizeSpin->setToolTip(tr("Tamano maximo de letra (se reduce solo si el texto no cabe)"));
	const int savedSize = WorshipOutput::ConfigString(ConfigFontSize).toInt();
	fontSizeSpin->setValue(savedSize > 0 ? savedSize : 80);

	layoutCombo = new QComboBox(root);
	layoutCombo->addItem(tr("Pantalla completa"));
	layoutCombo->addItem(tr("Tercio inferior"));
	layoutCombo->setToolTip(tr("Tercio inferior deja ver la camara detras del versiculo"));
	layoutCombo->setCurrentIndex(qBound(0, WorshipOutput::ConfigString(ConfigLayout).toInt(), 1));

	fontCombo = new QComboBox(root);
	fontCombo->setToolTip(tr("Tipografia recomendada para proyeccion"));
	for (const QString &family : WorshipRender::RecommendedFonts()) {
		fontCombo->addItem(family);
		fontCombo->setItemData(fontCombo->count() - 1, QFont(family), Qt::FontRole);
	}
	const int savedFont = fontCombo->findText(WorshipOutput::ConfigString(ConfigFont));
	fontCombo->setCurrentIndex(savedFont >= 0 ? savedFont : 0);

	backgroundCombo = new QComboBox(root);
	backgroundCombo->setToolTip(tr("Fondo para pantalla completa"));
	backgroundCombo->setIconSize(QSize(48, 27));
	rebuildBackgroundCombo();

	auto *fontRow = new QHBoxLayout();
	fontRow->addWidget(new QLabel(tr("Letra:"), root));
	fontRow->addWidget(fontCombo, 1);
	fontRow->addWidget(fontSizeSpin);

	auto *styleRow = new QHBoxLayout();
	styleRow->addWidget(new QLabel(tr("Fondo:"), root));
	styleRow->addWidget(backgroundCombo, 1);
	styleRow->addWidget(layoutCombo);

	auto *hint = new QLabel(tr("Doble clic o Enter en un versiculo para mostrarlo. Selecciona varios con "
				   "Mayus para mostrarlos juntos."),
				root);
	hint->setWordWrap(true);
	idian::Utils::addClass(hint, "text-muted");

	statusLabel = new QLabel(root);
	idian::Utils::addClass(statusLabel, "text-muted");
	statusLabel->setWordWrap(true);

	layout->addLayout(versionRow);
	layout->addWidget(searchEdit);
	layout->addWidget(resultsList, 3);
	layout->addWidget(previewLabel, 2);
	layout->addLayout(fontRow);
	layout->addLayout(styleRow);
	layout->addWidget(hint);
	layout->addWidget(statusLabel);

	setWidget(root);

	connect(searchEdit, &QLineEdit::textChanged, this, &BibleDock::onSearchChanged);
	connect(searchEdit, &QLineEdit::returnPressed, this, &BibleDock::onProjectClicked);
	connect(resultsList, &QListWidget::itemSelectionChanged, this, &BibleDock::onSelectionChanged);
	connect(resultsList, &QListWidget::itemActivated, this, &BibleDock::onProjectClicked);
	WorshipLive::instance().registerTarget(QStringLiteral("Biblia"), this);
	connect(importButton, &QPushButton::clicked, this, &BibleDock::onImportVersionClicked);
	connect(catalogButton, &QPushButton::clicked, this, &BibleDock::openCatalog);
	connect(versionCombo, &QComboBox::currentIndexChanged, this, &BibleDock::onVersionChanged);
	connect(backgroundCombo, &QComboBox::activated, this, &BibleDock::onBackgroundActivated);
	connect(layoutCombo, &QComboBox::currentIndexChanged, this, [this](int index) {
		WorshipOutput::SetConfigString(ConfigLayout, QString::number(index));
		styleChanged();
	});
	connect(fontSizeSpin, &QSpinBox::valueChanged, this, [this](int value) {
		WorshipOutput::SetConfigString(ConfigFontSize, QString::number(value));
		styleChanged();
	});
	connect(fontCombo, &QComboBox::currentTextChanged, this, [this](const QString &family) {
		WorshipOutput::SetConfigString(ConfigFont, family);
		styleChanged();
	});

	rebuildVersionCombo(WorshipOutput::ConfigString(ConfigVersion));
}

QString BibleDock::userBibleDir()
{
	const QString base = QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation);
	const QString path = QDir(base).absoluteFilePath(QStringLiteral("WorshipStudio/biblias"));
	QDir().mkpath(path);
	return path;
}

void BibleDock::rebuildVersionCombo(const QString &selectFile)
{
	static const QRegularExpression nameRegex(QStringLiteral("\"name\"\\s*:\\s*\"([^\"]+)\""));
	static const QRegularExpression abbrevRegex(QStringLiteral("\"abbrev\"\\s*:\\s*\"([^\"]+)\""));

	QSignalBlocker blocker(versionCombo);
	versionCombo->clear();

	for (const QString &dirPath : {bundledDir, userBibleDir()}) {
		const QFileInfoList files =
			QDir(dirPath).entryInfoList({QStringLiteral("*.json")}, QDir::Files, QDir::Name);
		for (const QFileInfo &info : files) {
			QFile file(info.absoluteFilePath());
			if (!file.open(QIODevice::ReadOnly))
				continue;
			const QString head = QString::fromUtf8(file.read(400));
			const QString name = nameRegex.match(head).captured(1);
			const QString abbrev = abbrevRegex.match(head).captured(1);
			if (name.isEmpty())
				continue;
			versionCombo->addItem(abbrev.isEmpty() ? name : QStringLiteral("%1 - %2").arg(abbrev, name),
					      info.absoluteFilePath());
		}
	}

	/* Reina Valera 1909 first by default: it is the classic version. */
	int index = selectFile.isEmpty() ? -1 : versionCombo->findData(selectFile);
	if (index < 0) {
		for (int i = 0; i < versionCombo->count(); ++i) {
			if (versionCombo->itemData(i).toString().endsWith(QStringLiteral("rv1909.json")))
				index = i;
		}
	}
	versionCombo->setCurrentIndex(qMax(0, index));
	blocker.unblock();
	onVersionChanged(versionCombo->currentIndex());
}

void BibleDock::onVersionChanged(int index)
{
	const QString path = versionCombo->itemData(index).toString();
	const QVector<BibleVerseRef> keep = selectedVerses();

	QString error;
	if (path.isEmpty() || !loadBible(path, &error)) {
		blog(LOG_WARNING, "[WorshipStudio] Biblia not loaded: %s", QT_TO_UTF8(error));
		statusLabel->setText(path.isEmpty() ? tr("No hay versiones de la Biblia instaladas.") : error);
		searchEdit->setEnabled(false);
		return;
	}

	searchEdit->setEnabled(true);
	WorshipOutput::SetConfigString(ConfigVersion, path);
	blog(LOG_INFO, "[WorshipStudio] Biblia loaded: %s, %d verses", QT_TO_UTF8(versionName),
	     (int)allVerses.size());

	/* Keep the passage on screen, now in the new version. */
	onSearchChanged(searchEdit->text());
	if (!keep.isEmpty()) {
		QSignalBlocker blocker(resultsList);
		for (int row = 0; row < resultsList->count(); ++row) {
			QListWidgetItem *item = resultsList->item(row);
			const BibleVerseRef ref{item->data(RefBookRole).toInt(), item->data(RefChapterRole).toInt(),
						item->data(RefVerseRole).toInt()};
			item->setSelected(std::any_of(keep.begin(), keep.end(), [&](const BibleVerseRef &k) {
				return k.book == ref.book && k.chapter == ref.chapter && k.verse == ref.verse;
			}));
		}
	}
	statusLabel->setText(tr("%1 - %2 versiculos").arg(versionName).arg(allVerses.size()));
	styleChanged();
}

void BibleDock::openCatalog()
{
	auto *dialog = new BibleCatalogDialog(userBibleDir(), this);
	dialog->setAttribute(Qt::WA_DeleteOnClose);
	connect(dialog, &BibleCatalogDialog::installed, this, &BibleDock::installVersion);
	dialog->open();
}

void BibleDock::installVersion(const QString &path)
{
	rebuildVersionCombo(path);
}

void BibleDock::setBackgroundFile(const QString &path)
{
	const auto kind = BackgroundLibrary::IsVideo(path) ? WorshipRender::BackgroundKind::Video
							   : WorshipRender::BackgroundKind::Image;
	WorshipOutput::SetConfigString(ConfigBgKind, QString::number((int)kind));
	WorshipOutput::SetConfigString(ConfigBgPath, path);
	rebuildBackgroundCombo();
	styleChanged();
}

void BibleDock::onImportVersionClicked()
{
	const QString file = QFileDialog::getOpenFileName(this, tr("Importar version de la Biblia"), QDir::homePath(),
							  tr("Biblia WorshipStudio (*.json)"));
	if (file.isEmpty())
		return;

	QString error;
	if (!loadBible(file, &error)) {
		statusLabel->setText(error);
		onVersionChanged(versionCombo->currentIndex());
		return;
	}

	const QString target = QDir(userBibleDir()).absoluteFilePath(QFileInfo(file).fileName());
	QFile::remove(target);
	if (!QFile::copy(file, target)) {
		statusLabel->setText(tr("No se pudo copiar la version importada."));
		return;
	}
	rebuildVersionCombo(target);
}

void BibleDock::rebuildBackgroundCombo()
{
	QSignalBlocker blocker(backgroundCombo);
	backgroundCombo->clear();
	const QStringList presets = WorshipRender::BackgroundPresetNames();
	for (int i = 0; i < presets.size(); ++i)
		backgroundCombo->addItem(presets.at(i), QStringLiteral("preset:%1").arg(i));
	backgroundCombo->insertSeparator(backgroundCombo->count());

	/* Mis fondos, shared with the songs. */
	const WorshipRender::Background bg = currentBackground();
	const bool usesFile =
		bg.kind == WorshipRender::BackgroundKind::Image || bg.kind == WorshipRender::BackgroundKind::Video;
	const QVector<BackgroundLibrary::Item> library = BackgroundLibrary::Items(true);
	bool fileListed = false;
	for (const BackgroundLibrary::Item &item : library) {
		backgroundCombo->addItem(BackgroundLibrary::Thumbnail(item.path),
					 item.video ? tr("%1 (video)").arg(item.name) : item.name,
					 QStringLiteral("file:") + item.path);
		fileListed = fileListed || (usesFile && item.path == bg.path);
	}
	if (usesFile && !fileListed)
		backgroundCombo->addItem(BackgroundLibrary::Thumbnail(bg.path), QFileInfo(bg.path).completeBaseName(),
					 QStringLiteral("file:") + bg.path);
	backgroundCombo->insertSeparator(backgroundCombo->count());

	backgroundCombo->addItem(bg.kind == WorshipRender::BackgroundKind::Color ? tr("Color: %1").arg(bg.color)
										 : tr("Color solido..."),
				 QStringLiteral("color"));
	backgroundCombo->addItem(tr("Agregar imágenes o videos..."), QStringLiteral("add"));

	switch (bg.kind) {
	case WorshipRender::BackgroundKind::Preset:
		backgroundCombo->setCurrentIndex(backgroundCombo->findData(QStringLiteral("preset:%1").arg(bg.preset)));
		break;
	case WorshipRender::BackgroundKind::Color:
		backgroundCombo->setCurrentIndex(backgroundCombo->findData(QStringLiteral("color")));
		break;
	case WorshipRender::BackgroundKind::Image:
	case WorshipRender::BackgroundKind::Video:
		backgroundCombo->setCurrentIndex(backgroundCombo->findData(QStringLiteral("file:") + bg.path));
		break;
	}
}

WorshipRender::Background BibleDock::currentBackground() const
{
	WorshipRender::Background bg;
	bg.kind = static_cast<WorshipRender::BackgroundKind>(qBound(0, WorshipOutput::ConfigString(ConfigBgKind).toInt(), 3));
	bg.preset = WorshipOutput::ConfigString(ConfigBgPreset).toInt();
	bg.path = WorshipOutput::ConfigString(ConfigBgPath);
	bg.color = WorshipOutput::ConfigString(ConfigBgColor);
	if ((bg.kind == WorshipRender::BackgroundKind::Image || bg.kind == WorshipRender::BackgroundKind::Video) &&
	    !QFileInfo::exists(bg.path))
		bg.kind = WorshipRender::BackgroundKind::Preset;
	return bg;
}

void BibleDock::onBackgroundActivated(int index)
{
	const QString data = backgroundCombo->itemData(index).toString();
	const WorshipRender::Background previous = currentBackground();

	if (data.startsWith(QStringLiteral("preset:"))) {
		WorshipOutput::SetConfigString(ConfigBgKind, QString::number((int)WorshipRender::BackgroundKind::Preset));
		WorshipOutput::SetConfigString(ConfigBgPreset, data.mid(7));
	} else if (data == QStringLiteral("color")) {
		const QColor color = QColorDialog::getColor(
			previous.color.isEmpty() ? QColor(0x1E, 0x3A, 0x5F) : QColor::fromString(previous.color), this,
			tr("Color de fondo"));
		if (color.isValid()) {
			WorshipOutput::SetConfigString(ConfigBgKind, QString::number((int)WorshipRender::BackgroundKind::Color));
			WorshipOutput::SetConfigString(ConfigBgColor, color.name());
		}
	} else {
		QString file;
		if (data.startsWith(QStringLiteral("file:")))
			file = data.mid(5);
		else
			file = BackgroundLibrary::AddWithDialog(this, true).value(0);
		if (!file.isEmpty()) {
			const auto kind = BackgroundLibrary::IsVideo(file) ? WorshipRender::BackgroundKind::Video
									   : WorshipRender::BackgroundKind::Image;
			WorshipOutput::SetConfigString(ConfigBgKind, QString::number((int)kind));
			WorshipOutput::SetConfigString(ConfigBgPath, file);
		}
	}

	rebuildBackgroundCombo();
	styleChanged();
}

WorshipRender::BibleSlide BibleDock::slideFor(const QVector<BibleVerseRef> &verses) const
{
	WorshipRender::BibleSlide slide;
	for (const BibleVerseRef &ref : verses)
		slide.verses.push_back({ref.verse, verseText(ref)});
	slide.showVerseNumbers = verses.size() > 1;
	slide.reference = referenceLabel(verses);
	slide.version = versionName;
	slide.layout = static_cast<WorshipRender::BibleLayout>(layoutCombo->currentIndex());
	slide.maxFontSize = fontSizeSpin->value();
	slide.fontFamily = fontCombo->currentText();
	slide.background = currentBackground();
	return slide;
}

void BibleDock::updatePreview(const QVector<BibleVerseRef> &verses)
{
	if (verses.isEmpty()) {
		previewLabel->setPixmap(QPixmap());
		previewLabel->setText(tr("Selecciona uno o varios versiculos"));
		return;
	}

	/* Same renderer as the screen, at a small size, so the preview is exact. */
	const QSize canvas(WorshipOutput::CanvasWidth(), WorshipOutput::CanvasHeight());
	const int width = qMax(160, previewLabel->width() - 4);
	const QSize small = canvas.scaled(width, qMax(90, previewLabel->height() - 4), Qt::KeepAspectRatio);
	QImage image = WorshipRender::Bible(slideFor(verses), small);
	if (image.hasAlphaChannel()) {
		QImage backdrop(image.size(), QImage::Format_ARGB32_Premultiplied);
		backdrop.fill(QColor(0x40, 0x44, 0x4C));
		QPainter painter(&backdrop);
		painter.drawImage(0, 0, image);
		painter.end();
		image = backdrop;
	}
	previewLabel->setPixmap(QPixmap::fromImage(image));
}

void BibleDock::styleChanged()
{
	const QVector<BibleVerseRef> verses = selectedVerses();
	updatePreview(verses);
	if (live && !verses.isEmpty())
		project(verses);
}

void BibleDock::resizeEvent(QResizeEvent *event)
{
	OBSDock::resizeEvent(event);
	updatePreview(selectedVerses());
}

bool BibleDock::loadBible(const QString &path, QString *error)
{
	QFile file(path);
	if (path.isEmpty() || !file.open(QIODevice::ReadOnly)) {
		*error = tr("No se encontro el archivo de la Biblia (worship/rv1909.json).");
		return false;
	}

	QJsonParseError parseError;
	const QJsonDocument document = QJsonDocument::fromJson(file.readAll(), &parseError);
	if (parseError.error != QJsonParseError::NoError) {
		*error = tr("Archivo de Biblia invalido: %1").arg(parseError.errorString());
		return false;
	}

	const QJsonObject rootObject = document.object();
	if (rootObject.value(QStringLiteral("books")).toArray().isEmpty()) {
		*error = tr("El archivo no es una Biblia de WorshipStudio.");
		return false;
	}

	books.clear();
	allVerses.clear();
	normalizedVerses.clear();
	versionName = rootObject.value(QStringLiteral("name")).toString();

	const QJsonArray bookArray = rootObject.value(QStringLiteral("books")).toArray();
	for (int b = 0; b < bookArray.size(); ++b) {
		const QJsonObject bookObject = bookArray.at(b).toObject();
		BibleBook book;
		book.name = bookObject.value(QStringLiteral("name")).toString();
		book.key = normalizeBookQuery(book.name);
		if (book.key.startsWith(QStringLiteral("san ")))
			book.aliases << book.key.mid(4);
		else if (b >= 39 && b <= 42)
			book.aliases << QStringLiteral("san ") + book.key;
		if (b < 66) {
			book.aliases << QString::fromLatin1(BookAbbreviations[b]).split(QLatin1Char(' '));
			/* "Juan 3:16" and "John 3:16" work in a Bible of any language. */
			book.aliases << BibleCatalog::SpanishBookName(b) << BibleCatalog::EnglishBookName(b);
		}
		for (QString &alias : book.aliases)
			alias = normalizeBookQuery(alias);

		const QJsonArray chapterArray = bookObject.value(QStringLiteral("chapters")).toArray();
		for (int c = 0; c < chapterArray.size(); ++c) {
			QStringList verses;
			const QJsonArray verseArray = chapterArray.at(c).toArray();
			for (int v = 0; v < verseArray.size(); ++v) {
				const QString text = verseArray.at(v).toString();
				verses << text;
				allVerses.push_back({b, c + 1, v + 1});
				normalizedVerses << normalizeText(text);
			}
			book.chapters.push_back(verses);
		}
		books.push_back(book);
	}

	if (books.isEmpty()) {
		*error = tr("El archivo de la Biblia no contiene libros.");
		return false;
	}
	return true;
}

int BibleDock::findBook(const QString &query) const
{
	const QString key = normalizeBookQuery(query);
	if (key.isEmpty())
		return -1;

	for (int i = 0; i < books.size(); ++i) {
		if (books[i].key == key || books[i].aliases.contains(key))
			return i;
	}
	if (key.size() < 2)
		return -1;
	for (int i = 0; i < books.size(); ++i) {
		if (books[i].key.startsWith(key))
			return i;
		for (const QString &alias : books[i].aliases) {
			if (alias.size() > 2 && alias.startsWith(key))
				return i;
		}
	}
	return -1;
}

bool BibleDock::parseReference(const QString &query, int *book, int *chapter, int *verseFrom, int *verseTo) const
{
	static const QRegularExpression regex(
		QStringLiteral("^([1-3]?\\s*[a-z][a-z ]*?)\\s*(?:(\\d+)(?:\\s*[: ]\\s*(\\d+)(?:\\s*-\\s*(\\d+))?)?)?$"));

	const QRegularExpressionMatch match = regex.match(normalizeText(query));
	if (!match.hasMatch())
		return false;

	*book = findBook(match.captured(1));
	if (*book < 0)
		return false;

	const BibleBook &bibleBook = books.at(*book);
	*chapter = match.captured(2).isEmpty() ? 1 : match.captured(2).toInt();
	if (*chapter < 1 || *chapter > bibleBook.chapters.size())
		return false;

	const int verseCount = bibleBook.chapters.at(*chapter - 1).size();
	*verseFrom = match.captured(3).isEmpty() ? 0 : qBound(1, match.captured(3).toInt(), verseCount);
	*verseTo = match.captured(4).isEmpty() ? *verseFrom : qBound(1, match.captured(4).toInt(), verseCount);
	if (*verseTo < *verseFrom)
		std::swap(*verseFrom, *verseTo);
	return true;
}

void BibleDock::addVerseItem(const BibleVerseRef &ref)
{
	const QString text = verseText(ref);
	auto *item = new QListWidgetItem(QStringLiteral("%1 %2:%3  %4")
						 .arg(books.at(ref.book).name)
						 .arg(ref.chapter)
						 .arg(ref.verse)
						 .arg(text.isEmpty() ? tr("(sin texto en esta version)") : text));
	item->setData(RefBookRole, ref.book);
	item->setData(RefChapterRole, ref.chapter);
	item->setData(RefVerseRole, ref.verse);
	resultsList->addItem(item);
}

void BibleDock::onSearchChanged(const QString &text)
{
	QSignalBlocker blocker(resultsList);
	resultsList->clear();

	int book = -1, chapter = 0, verseFrom = 0, verseTo = 0;
	if (parseReference(text, &book, &chapter, &verseFrom, &verseTo)) {
		/* Show the whole chapter so the operator can keep moving through it. */
		const QStringList &verses = books.at(book).chapters.at(chapter - 1);
		for (int v = 1; v <= verses.size(); ++v)
			addVerseItem({book, chapter, v});

		const int first = qMax(verseFrom, 1);
		const int last = qMax(verseTo, 1);
		for (int v = first; v <= last; ++v)
			resultsList->item(v - 1)->setSelected(true);
		resultsList->setCurrentRow(first - 1, QItemSelectionModel::NoUpdate);
		resultsList->scrollToItem(resultsList->item(first - 1), QAbstractItemView::PositionAtTop);
		statusLabel->setText(tr("%1 %2 - %3 versiculos").arg(books.at(book).name).arg(chapter).arg(verses.size()));
	} else {
		const QString needle = normalizeText(text);
		if (needle.size() >= 3) {
			int count = 0;
			for (int i = 0; i < normalizedVerses.size() && count < MaxTextResults; ++i) {
				if (normalizedVerses.at(i).contains(needle)) {
					addVerseItem(allVerses.at(i));
					++count;
				}
			}
			statusLabel->setText(count >= MaxTextResults
						     ? tr("Mas de %1 resultados, refina la busqueda").arg(MaxTextResults)
						     : tr("%1 resultados").arg(count));
			if (count > 0)
				resultsList->setCurrentRow(0);
		} else if (!text.isEmpty()) {
			statusLabel->setText(tr("Escribe una referencia (Juan 3:16) o al menos 3 letras"));
		}
	}

	/* A new search only previews; the screen changes on Proyectar. */
	blocker.unblock();
	searching = true;
	onSelectionChanged();
	searching = false;
}

QVector<BibleVerseRef> BibleDock::selectedVerses() const
{
	QVector<BibleVerseRef> verses;
	for (int row = 0; row < resultsList->count(); ++row) {
		const QListWidgetItem *item = resultsList->item(row);
		if (!item->isSelected())
			continue;
		verses.push_back({item->data(RefBookRole).toInt(), item->data(RefChapterRole).toInt(),
				  item->data(RefVerseRole).toInt()});
	}
	return verses;
}

QString BibleDock::verseText(const BibleVerseRef &ref) const
{
	if (ref.book < 0 || ref.book >= books.size())
		return {};
	const BibleBook &book = books.at(ref.book);
	if (ref.chapter < 1 || ref.chapter > book.chapters.size())
		return {};
	return book.chapters.at(ref.chapter - 1).value(ref.verse - 1);
}

QString BibleDock::referenceLabel(const QVector<BibleVerseRef> &verses) const
{
	if (verses.isEmpty())
		return {};

	const BibleVerseRef &first = verses.first();
	const BibleVerseRef &last = verses.last();
	QString label = QStringLiteral("%1 %2:%3").arg(books.at(first.book).name).arg(first.chapter).arg(first.verse);

	const bool sameChapter = std::all_of(verses.begin(), verses.end(), [&](const BibleVerseRef &ref) {
		return ref.book == first.book && ref.chapter == first.chapter;
	});
	if (verses.size() == 1)
		return label;
	if (sameChapter && last.verse - first.verse == verses.size() - 1)
		return label + QStringLiteral("-%1").arg(last.verse);

	QStringList parts;
	for (const BibleVerseRef &ref : verses)
		parts << QStringLiteral("%1 %2:%3").arg(books.at(ref.book).name).arg(ref.chapter).arg(ref.verse);
	return parts.join(QStringLiteral("; "));
}

void BibleDock::onSelectionChanged()
{
	const QVector<BibleVerseRef> verses = selectedVerses();

	updatePreview(verses);
	if (verses.isEmpty())
		return;

	/* While live, moving through the verses updates the screen immediately. */
	if (live && !searching)
		project(verses);
}

bool BibleDock::prepareOutput(bool fresh)
{
	/* A new passage goes to the copy of the scene that is not on air;
	 * moving through the verses keeps updating the same copy. */
	if (fresh || !scene) {
		outputSlot = WorshipOutput::OutputSlot(SceneName);
		scene = WorshipOutput::EnsureScene(WorshipOutput::Named(SceneName, outputSlot).c_str());
	}
	if (!scene)
		return false;

	/* Earlier builds drew the verse with separate text sources. */
	for (const char *legacyName : {"WS Biblia - Fondo", "WS Biblia - Texto", "WS Biblia - Referencia"}) {
		if (obs_sceneitem_t *legacy = obs_scene_find_source(scene, legacyName))
			obs_sceneitem_remove(legacy);
	}

	bool created = false;
	OBSDataAutoRelease settings = obs_data_create();
	screenSource = WorshipOutput::EnsureSource(scene, WorshipOutput::Named(ScreenSourceName, outputSlot).c_str(), "image_source", settings, &created);
	if (created && screenSource)
		WorshipOutput::FitItem(scene, screenSource, 0.0f, 0.0f, 1.0f, 1.0f);

	/* Optional looping video under the verse (full screen layout only). */
	const WorshipRender::Background bg = currentBackground();
	const bool useVideo = bg.kind == WorshipRender::BackgroundKind::Video && layoutCombo->currentIndex() == 0;
	const std::string videoName = WorshipOutput::Named(VideoSourceName, outputSlot);
	obs_sceneitem_t *videoItem = obs_scene_find_source(scene, videoName.c_str());
	if (useVideo) {
		OBSDataAutoRelease videoSettings = obs_data_create();
		obs_data_set_bool(videoSettings, "is_local_file", true);
		obs_data_set_bool(videoSettings, "looping", true);
		obs_data_set_bool(videoSettings, "restart_on_activate", false);
		obs_data_set_string(videoSettings, "local_file", QT_TO_UTF8(bg.path));
		OBSSource video = WorshipOutput::EnsureSource(scene, videoName.c_str(), "ffmpeg_source", videoSettings, &created);
		if (video) {
			if (created) {
				obs_source_set_muted(video, true);
				WorshipOutput::FitItem(scene, video, 0.0f, 0.0f, 1.0f, 1.0f, OBS_ALIGN_CENTER,
						       OBS_BOUNDS_SCALE_OUTER);
			} else {
				obs_source_update(video, videoSettings);
			}
			videoItem = obs_scene_find_source(scene, videoName.c_str());
			obs_sceneitem_set_order(videoItem, OBS_ORDER_MOVE_BOTTOM);
		}
	}
	if (videoItem)
		obs_sceneitem_set_visible(videoItem, useVideo);

	return screenSource;
}

bool BibleDock::project(const QVector<BibleVerseRef> &verses, bool fresh)
{
	if (verses.isEmpty())
		return false;

	const bool newOutput = fresh || !live;
	if (!prepareOutput(newOutput)) {
		statusLabel->setText(tr("No se pudieron crear las fuentes de proyeccion."));
		return false;
	}

	const WorshipRender::BibleSlide slide = slideFor(verses);

	const QSize canvas(WorshipOutput::CanvasWidth(), WorshipOutput::CanvasHeight());
	bool fits = true;
	const QImage image = WorshipRender::Bible(slide, canvas, &fits);

	/* Alternate file names so the image source always reloads. */
	renderCounter = (renderCounter + 1) % 2;
	lastRenderPath = QDir(WorshipRender::CacheDir(QStringLiteral("bible")))
				 .absoluteFilePath(QStringLiteral("pantalla-%1.png").arg(renderCounter));
	if (!image.save(lastRenderPath, "PNG")) {
		statusLabel->setText(tr("No se pudo generar la imagen del versiculo."));
		return false;
	}

	OBSDataAutoRelease settings = obs_data_create();
	obs_data_set_string(settings, "file", QT_TO_UTF8(lastRenderPath));
	obs_source_update(screenSource, settings);
	obs_source_set_enabled(screenSource, true);

	statusLabel->setText(fits ? tr("En pantalla: %1").arg(slide.reference)
				  : tr("En pantalla: %1 (texto muy largo, selecciona menos versiculos)")
					    .arg(slide.reference));

	live = true;
	if (newOutput)
		WorshipOutput::ShowScene(scene);

	LiveState state;
	state.source = QStringLiteral("Biblia");
	state.title = slide.reference + (slide.version.isEmpty() ? QString() : QStringLiteral(" · ") + slide.version);
	QStringList text;
	for (const BibleVerseRef &ref : verses)
		text << verseText(ref);
	state.current = text.join(QLatin1Char(' '));
	/* The next block has the same size as the selection on screen. */
	int lastRow = -1;
	for (int row = 0; row < resultsList->count(); ++row) {
		if (resultsList->item(row)->isSelected())
			lastRow = row;
	}
	QStringList next;
	for (int row = lastRow + 1; row >= 1 && row < qMin(lastRow + 1 + (int)verses.size(), resultsList->count()); ++row)
		next << resultsList->item(row)->text();
	state.next = next.join(QLatin1Char(' '));
	state.index = qMax(0, lastRow);
	state.count = resultsList->count();
	WorshipLive::instance().publish(state);
	return true;
}

int BibleDock::layoutIndex() const
{
	return layoutCombo->currentIndex();
}

void BibleDock::toggleHidden()
{
	if (live)
		onClearClicked();
	else
		project(selectedVerses(), true);
}

void BibleDock::onProjectClicked()
{
	project(selectedVerses(), true);
}

void BibleDock::moveSelection(int delta)
{
	const int count = resultsList->count();
	if (count == 0)
		return;

	/* Move the whole selected block, so a 3-verse selection advances by 3. */
	QList<int> rows;
	for (int row = 0; row < count; ++row) {
		if (resultsList->item(row)->isSelected())
			rows << row;
	}
	const int blockSize = rows.isEmpty() ? 1 : rows.size();
	int start = rows.isEmpty() ? 0 : rows.first() + delta * blockSize;
	start = qBound(0, start, count - 1);

	QSignalBlocker blocker(resultsList);
	resultsList->clearSelection();
	for (int row = start; row < qMin(start + blockSize, count); ++row)
		resultsList->item(row)->setSelected(true);
	resultsList->setCurrentRow(start, QItemSelectionModel::NoUpdate);
	resultsList->scrollToItem(resultsList->item(start));
	blocker.unblock();
	onSelectionChanged();
}

void BibleDock::onClearClicked()
{
	live = false;
	if (screenSource)
		obs_source_set_enabled(screenSource, false);
	statusLabel->setText(tr("Pantalla limpia"));
	LiveState state = WorshipLive::instance().state();
	if (state.source == QStringLiteral("Biblia")) {
		state.hidden = true;
		WorshipLive::instance().publish(state);
	}
}

bool BibleDock::selectVersion(const QString &abbrev)
{
	for (int i = 0; i < versionCombo->count(); ++i) {
		if (versionCombo->itemText(i).startsWith(abbrev + QStringLiteral(" "))) {
			versionCombo->setCurrentIndex(i);
			return true;
		}
	}
	return false;
}

QStringList BibleDock::versionAbbrevs() const
{
	QStringList list;
	for (int i = 0; i < versionCombo->count(); ++i)
		list << versionCombo->itemText(i).section(QStringLiteral(" - "), 0, 0);
	return list;
}

void BibleDock::setStyle(const QString &fontFamily, int preset)
{
	WorshipOutput::SetConfigString(ConfigBgKind, QString::number((int)WorshipRender::BackgroundKind::Preset));
	WorshipOutput::SetConfigString(ConfigBgPreset, QString::number(preset));
	rebuildBackgroundCombo();
	const int fontIndex = fontCombo->findText(fontFamily);
	if (fontIndex >= 0 && fontIndex != fontCombo->currentIndex())
		fontCombo->setCurrentIndex(fontIndex);
	else
		styleChanged();
}

bool BibleDock::projectQuery(const QString &query, int layout)
{
	layoutCombo->setCurrentIndex(layout);
	searchEdit->setText(query);
	return project(selectedVerses(), true);
}
