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

#include "BibleCatalog.hpp"

#include <Idian/Utils.hpp>
#include <util/base.h>

#include <QComboBox>
#include <QDialogButtonBox>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QHash>
#include <QJsonDocument>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMap>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QProgressBar>
#include <QPushButton>
#include <QRegularExpression>
#include <QSaveFile>
#include <QStandardPaths>
#include <QVBoxLayout>

#include <algorithm>

#include "moc_BibleCatalog.cpp"

namespace {
constexpr const char *ApiBase = "https://bible.helloao.org";
constexpr const char *CatalogUrl = "https://bible.helloao.org/api/available_translations.json";
constexpr int IdRole = Qt::UserRole + 1;

const char *const BookIds[66] = {
	"GEN", "EXO", "LEV", "NUM", "DEU", "JOS", "JDG", "RUT", "1SA", "2SA", "1KI", "2KI", "1CH", "2CH",
	"EZR", "NEH", "EST", "JOB", "PSA", "PRO", "ECC", "SNG", "ISA", "JER", "LAM", "EZK", "DAN", "HOS",
	"JOL", "AMO", "OBA", "JON", "MIC", "NAM", "HAB", "ZEP", "HAG", "ZEC", "MAL", "MAT", "MRK", "LUK",
	"JHN", "ACT", "ROM", "1CO", "2CO", "GAL", "EPH", "PHP", "COL", "1TH", "2TH", "1TI", "2TI", "TIT",
	"PHM", "HEB", "JAS", "1PE", "2PE", "1JN", "2JN", "3JN", "JUD", "REV",
};

const char *const SpanishNames[66] = {
	"Génesis",   "Éxodo",       "Levítico",    "Números",         "Deuteronomio",    "Josué",
	"Jueces",    "Rut",         "1 Samuel",    "2 Samuel",        "1 Reyes",         "2 Reyes",
	"1 Crónicas", "2 Crónicas", "Esdras",      "Nehemías",        "Ester",           "Job",
	"Salmos",    "Proverbios",  "Eclesiastés", "Cantares",        "Isaías",          "Jeremías",
	"Lamentaciones", "Ezequiel", "Daniel",     "Oseas",           "Joel",            "Amós",
	"Abdías",    "Jonás",       "Miqueas",     "Nahúm",           "Habacuc",         "Sofonías",
	"Hageo",     "Zacarías",    "Malaquías",   "Mateo",           "Marcos",          "Lucas",
	"Juan",      "Hechos",      "Romanos",     "1 Corintios",     "2 Corintios",     "Gálatas",
	"Efesios",   "Filipenses",  "Colosenses",  "1 Tesalonicenses", "2 Tesalonicenses", "1 Timoteo",
	"2 Timoteo", "Tito",        "Filemón",     "Hebreos",         "Santiago",        "1 Pedro",
	"2 Pedro",   "1 Juan",      "2 Juan",      "3 Juan",          "Judas",           "Apocalipsis",
};

const char *const EnglishNames[66] = {
	"Genesis",     "Exodus",       "Leviticus",    "Numbers",         "Deuteronomy",     "Joshua",
	"Judges",      "Ruth",         "1 Samuel",     "2 Samuel",        "1 Kings",         "2 Kings",
	"1 Chronicles", "2 Chronicles", "Ezra",        "Nehemiah",        "Esther",          "Job",
	"Psalms",      "Proverbs",     "Ecclesiastes", "Song of Solomon", "Isaiah",          "Jeremiah",
	"Lamentations", "Ezekiel",     "Daniel",       "Hosea",           "Joel",            "Amos",
	"Obadiah",     "Jonah",        "Micah",        "Nahum",           "Habakkuk",        "Zephaniah",
	"Haggai",      "Zechariah",    "Malachi",      "Matthew",         "Mark",            "Luke",
	"John",        "Acts",         "Romans",       "1 Corinthians",   "2 Corinthians",   "Galatians",
	"Ephesians",   "Philippians",  "Colossians",   "1 Thessalonians", "2 Thessalonians", "1 Timothy",
	"2 Timothy",   "Titus",        "Philemon",     "Hebrews",         "James",           "1 Peter",
	"2 Peter",     "1 John",       "2 John",       "3 John",          "Jude",            "Revelation",
};

/* "SAN MATEO" -> "San Mateo"; names already in mixed case stay as they are. */
QString displayCase(const QString &name)
{
	if (name != name.toUpper())
		return name;
	QString result = name.toLower();
	bool start = true;
	for (QChar &ch : result) {
		if (start && ch.isLetter())
			ch = ch.toUpper();
		start = ch.isSpace();
	}
	return result;
}

QString bookName(const QJsonObject &book, int index)
{
	const QString id = book.value(QStringLiteral("id")).toString();
	const QString shortName = book.value(QStringLiteral("shortName")).toString().trimmed();
	if (shortName.size() > 3 && shortName != id && shortName != shortName.toUpper())
		return shortName;
	for (const char *key : {"commonName", "name", "title"}) {
		const QString value = book.value(QLatin1String(key)).toString().trimmed();
		if (!value.isEmpty() && value != id)
			return displayCase(value);
	}
	return QString::fromUtf8(SpanishNames[index]);
}

QString verseText(const QJsonValue &value)
{
	if (value.isString())
		return value.toString();
	/* Some translations split a verse into styled pieces. */
	QStringList parts;
	for (const QJsonValue &part : value.toArray()) {
		if (part.isString())
			parts << part.toString();
		else if (part.isObject())
			parts << part.toObject().value(QStringLiteral("text")).toString();
	}
	return parts.join(QLatin1Char(' '));
}

QString languageLabel(const QJsonObject &translation)
{
	const QString name = translation.value(QStringLiteral("languageName")).toString();
	if (name.isEmpty())
		return translation.value(QStringLiteral("language")).toString();
	return name.left(1).toUpper() + name.mid(1);
}
} // namespace

QString BibleCatalog::BookId(int index)
{
	return index >= 0 && index < 66 ? QString::fromLatin1(BookIds[index]) : QString();
}

QString BibleCatalog::SpanishBookName(int index)
{
	return index >= 0 && index < 66 ? QString::fromUtf8(SpanishNames[index]) : QString();
}

QString BibleCatalog::EnglishBookName(int index)
{
	return index >= 0 && index < 66 ? QString::fromUtf8(EnglishNames[index]) : QString();
}

QString BibleCatalog::LicenseText(const QJsonObject &translation)
{
	const QJsonObject license = translation.value(QStringLiteral("license")).toObject();
	if (license.value(QStringLiteral("publicDomain")).toBool())
		return QDialog::tr("Dominio público");

	QStringList parts;
	const QString commercial = license.value(QStringLiteral("commercialUse")).toString();
	parts << (commercial == QStringLiteral("allowed") ? QDialog::tr("Uso libre")
							  : QDialog::tr("Uso libre no comercial"));
	if (license.value(QStringLiteral("attribution")).toString() == QStringLiteral("required"))
		parts << QDialog::tr("citando la fuente");
	if (license.value(QStringLiteral("derivatives")).toString() != QStringLiteral("allowed"))
		parts << QDialog::tr("sin modificar el texto");
	return parts.join(QStringLiteral(", "));
}

QString BibleCatalog::FileName(const QString &translationId)
{
	QString safe = translationId;
	safe.replace(QRegularExpression(QStringLiteral("[^A-Za-z0-9_-]")), QStringLiteral("_"));
	return QStringLiteral("libre-%1.json").arg(safe.toLower());
}

QByteArray BibleCatalog::Convert(const QJsonObject &complete, const QJsonObject &translation, QString *error,
				 int *verseCount)
{
	QHash<QString, QJsonObject> byId;
	for (const QJsonValue &value : complete.value(QStringLiteral("books")).toArray()) {
		const QJsonObject book = value.toObject();
		byId.insert(book.value(QStringLiteral("id")).toString().toUpper(), book);
	}

	int verses = 0;
	QJsonArray books;
	for (int b = 0; b < 66; ++b) {
		const QJsonObject source = byId.value(QString::fromLatin1(BookIds[b]));
		QJsonArray chapters;
		for (const QJsonValue &chapterValue : source.value(QStringLiteral("chapters")).toArray()) {
			const QJsonObject chapter = chapterValue.toObject().value(QStringLiteral("chapter")).toObject();
			QStringList texts;
			for (const QJsonValue &item : chapter.value(QStringLiteral("content")).toArray()) {
				const QJsonObject object = item.toObject();
				if (object.value(QStringLiteral("type")).toString() != QStringLiteral("verse"))
					continue;
				const int number = object.value(QStringLiteral("number")).toInt();
				const QString text = verseText(object.value(QStringLiteral("text"))).simplified();
				if (number < 1 || number > 200)
					continue;
				while (texts.size() < number)
					texts << QString();
				/* Poetry can repeat a verse number for each line. */
				QString &slot = texts[number - 1];
				slot = slot.isEmpty() ? text : slot + QLatin1Char(' ') + text;
			}
			QJsonArray verseArray;
			for (const QString &text : texts) {
				verseArray.append(text);
				if (!text.isEmpty())
					++verses;
			}
			chapters.append(verseArray);
		}

		QJsonObject book;
		book.insert(QStringLiteral("name"), source.isEmpty() ? QString::fromUtf8(SpanishNames[b])
								     : bookName(source, b));
		book.insert(QStringLiteral("chapters"), chapters);
		books.append(book);
	}

	if (verses == 0) {
		if (error)
			*error = QDialog::tr("La traducción descargada no tiene versículos de los 66 libros.");
		return {};
	}
	if (verseCount)
		*verseCount = verses;

	const QString id = translation.value(QStringLiteral("id")).toString();
	QString abbrev = translation.value(QStringLiteral("shortName")).toString();
	if (abbrev.isEmpty())
		abbrev = id;
	QString source = translation.value(QStringLiteral("website")).toString();
	if (source.isEmpty())
		source = translation.value(QStringLiteral("licenseUrl")).toString();

	QJsonObject root;
	root.insert(QStringLiteral("language"), translation.value(QStringLiteral("language")).toString());
	root.insert(QStringLiteral("license"), QStringLiteral("%1. Fuente: %2").arg(LicenseText(translation), source));
	root.insert(QStringLiteral("books"), books);

	/* QJsonObject sorts its keys, but the version list reads only the file
	 * head: write name and abbrev first by hand. */
	const QByteArray header =
		QJsonDocument(QJsonObject{{QStringLiteral("name"), translation.value(QStringLiteral("name")).toString()}})
			.toJson(QJsonDocument::Compact);
	const QByteArray abbrevJson =
		QJsonDocument(QJsonObject{{QStringLiteral("abbrev"), abbrev}}).toJson(QJsonDocument::Compact);
	const QByteArray body = QJsonDocument(root).toJson(QJsonDocument::Compact);
	return header.chopped(1) + "," + abbrevJson.mid(1).chopped(1) + "," + body.mid(1);
}

BibleCatalogDialog::BibleCatalogDialog(const QString &bibleDir_, QWidget *parent)
	: QDialog(parent),
	  bibleDir(bibleDir_),
	  network(new QNetworkAccessManager(this))
{
	setWindowTitle(tr("Descargar Biblias"));
	resize(620, 560);

	auto *layout = new QVBoxLayout(this);
	auto *title = new QLabel(tr("Biblias gratuitas"), this);
	idian::Utils::addClass(title, "text-large");
	idian::Utils::addClass(title, "text-bold");
	auto *intro = new QLabel(tr("Más de mil traducciones en cientos de idiomas, todas de uso libre. Elegí una y "
				    "tocá Descargar: queda guardada en esta computadora y funciona sin internet."),
				 this);
	intro->setWordWrap(true);
	idian::Utils::addClass(intro, "text-muted");

	auto *filterRow = new QHBoxLayout();
	searchEdit = new QLineEdit(this);
	searchEdit->setPlaceholderText(tr("Buscar por nombre o sigla"));
	searchEdit->setClearButtonEnabled(true);
	languageCombo = new QComboBox(this);
	languageCombo->setMinimumWidth(180);
	filterRow->addWidget(searchEdit, 1);
	filterRow->addWidget(languageCombo);

	list = new QListWidget(this);
	list->setAlternatingRowColors(true);

	infoLabel = new QLabel(this);
	infoLabel->setWordWrap(true);
	infoLabel->setTextFormat(Qt::RichText);
	infoLabel->setOpenExternalLinks(true);
	infoLabel->setMinimumHeight(48);

	progress = new QProgressBar(this);
	progress->setVisible(false);

	auto *buttons = new QDialogButtonBox(this);
	downloadButton = buttons->addButton(tr("Descargar"), QDialogButtonBox::AcceptRole);
	idian::Utils::addClass(downloadButton, "button-primary");
	downloadButton->setEnabled(false);
	buttons->addButton(QDialogButtonBox::Close)->setText(tr("Cerrar"));

	layout->addWidget(title);
	layout->addWidget(intro);
	layout->addLayout(filterRow);
	layout->addWidget(list, 1);
	layout->addWidget(infoLabel);
	layout->addWidget(progress);
	layout->addWidget(buttons);

	connect(searchEdit, &QLineEdit::textChanged, this, &BibleCatalogDialog::refill);
	connect(languageCombo, &QComboBox::currentIndexChanged, this, &BibleCatalogDialog::refill);
	connect(list, &QListWidget::currentItemChanged, this, &BibleCatalogDialog::onCurrentChanged);
	connect(list, &QListWidget::itemActivated, this, &BibleCatalogDialog::onDownloadClicked);
	connect(downloadButton, &QPushButton::clicked, this, &BibleCatalogDialog::onDownloadClicked);
	connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
	connect(this, &QDialog::finished, this, [this]() {
		if (reply)
			reply->abort();
	});

	loadCatalog();
}

QString BibleCatalogDialog::cachePath()
{
	const QString dir = QStandardPaths::writableLocation(QStandardPaths::CacheLocation);
	QDir().mkpath(dir);
	return QDir(dir).absoluteFilePath(QStringLiteral("catalogo-biblias.json"));
}

void BibleCatalogDialog::loadCatalog()
{
	QFile cached(cachePath());
	const bool haveCache = cached.open(QIODevice::ReadOnly) && setCatalog(cached.readAll());

	setBusy(true, haveCache ? QString() : tr("Cargando el catálogo..."));
	QNetworkRequest request{QUrl(QString::fromLatin1(CatalogUrl))};
	request.setTransferTimeout(30000);
	reply = network->get(request);
	connect(reply, &QNetworkReply::finished, this, [this, haveCache, r = reply.data()]() {
		r->deleteLater();
		reply = nullptr;
		setBusy(false, QString());
		if (r->error() == QNetworkReply::NoError) {
			const QByteArray data = r->readAll();
			if (setCatalog(data)) {
				QSaveFile file(cachePath());
				if (file.open(QIODevice::WriteOnly)) {
					file.write(data);
					file.commit();
				}
				return;
			}
		}
		blog(LOG_WARNING, "[WorshipStudio] Bible catalog not loaded: %s", r->errorString().toUtf8().constData());
		if (!haveCache && r->error() != QNetworkReply::OperationCanceledError)
			infoLabel->setText(tr("No se pudo cargar el catálogo. Revisá la conexión a internet y volvé a "
					      "abrir esta ventana."));
	});
}

bool BibleCatalogDialog::setCatalog(const QByteArray &data)
{
	const QJsonArray parsed =
		QJsonDocument::fromJson(data).object().value(QStringLiteral("translations")).toArray();
	if (parsed.isEmpty())
		return false;
	translations = parsed;
	fillLanguages();
	refill();
	return true;
}

void BibleCatalogDialog::fillLanguages()
{
	const QString previous = languageCombo->currentData().toString();
	QMap<QString, int> counts;
	QHash<QString, QString> labels;
	for (const QJsonValue &value : translations) {
		const QJsonObject t = value.toObject();
		const QString code = t.value(QStringLiteral("language")).toString();
		counts[code]++;
		labels.insert(code, languageLabel(t));
	}

	QSignalBlocker blocker(languageCombo);
	languageCombo->clear();
	languageCombo->addItem(tr("Todos los idiomas (%1)").arg(translations.size()), QString());
	/* The languages of most congregations first, then the rest by name. */
	const QStringList first = {QStringLiteral("spa"), QStringLiteral("eng"), QStringLiteral("por"),
				   QStringLiteral("fra"), QStringLiteral("deu"), QStringLiteral("ita")};
	for (const QString &code : first) {
		if (counts.contains(code))
			languageCombo->addItem(QStringLiteral("%1 (%2)").arg(labels.value(code)).arg(counts.value(code)),
					       code);
	}
	QList<QPair<QString, QString>> rest;
	for (auto it = counts.cbegin(); it != counts.cend(); ++it) {
		if (!first.contains(it.key()))
			rest.append({QStringLiteral("%1 (%2)").arg(labels.value(it.key())).arg(it.value()), it.key()});
	}
	std::sort(rest.begin(), rest.end(), [](const auto &a, const auto &b) {
		return QString::localeAwareCompare(a.first, b.first) < 0;
	});
	languageCombo->insertSeparator(languageCombo->count());
	for (const auto &entry : rest)
		languageCombo->addItem(entry.first, entry.second);

	const int index = languageCombo->findData(previous.isEmpty() ? QStringLiteral("spa") : previous);
	languageCombo->setCurrentIndex(qMax(0, index));
}

bool BibleCatalogDialog::isInstalled(const QString &id) const
{
	return QFileInfo::exists(QDir(bibleDir).absoluteFilePath(BibleCatalog::FileName(id)));
}

void BibleCatalogDialog::refill()
{
	const QString language = languageCombo->currentData().toString();
	const QString needle = searchEdit->text().trimmed();
	const QString selected = list->currentItem() ? list->currentItem()->data(IdRole).toString() : QString();

	QSignalBlocker blocker(list);
	list->clear();
	for (const QJsonValue &value : translations) {
		const QJsonObject t = value.toObject();
		if (!language.isEmpty() && t.value(QStringLiteral("language")).toString() != language)
			continue;
		const QString id = t.value(QStringLiteral("id")).toString();
		const QString name = t.value(QStringLiteral("name")).toString();
		const QString english = t.value(QStringLiteral("englishName")).toString();
		const QString shortName = t.value(QStringLiteral("shortName")).toString();
		if (!needle.isEmpty() && !name.contains(needle, Qt::CaseInsensitive) &&
		    !english.contains(needle, Qt::CaseInsensitive) && !shortName.contains(needle, Qt::CaseInsensitive))
			continue;

		const int bookCount = t.value(QStringLiteral("numberOfBooks")).toInt();
		QString label = QStringLiteral("%1  ·  %2").arg(name, shortName.isEmpty() ? id : shortName);
		if (language.isEmpty())
			label += QStringLiteral("  ·  ") + languageLabel(t);
		if (bookCount > 0 && bookCount < 66)
			label += tr("  ·  %1 libros").arg(bookCount);
		if (isInstalled(id))
			label = QStringLiteral("✓ ") + label;
		auto *item = new QListWidgetItem(label, list);
		item->setData(IdRole, id);
		if (id == selected)
			list->setCurrentItem(item);
	}
	blocker.unblock();
	if (!list->currentItem() && list->count() > 0)
		list->setCurrentRow(0);
	onCurrentChanged();
}

QJsonObject BibleCatalogDialog::currentTranslation() const
{
	const QString id = list->currentItem() ? list->currentItem()->data(IdRole).toString() : QString();
	for (const QJsonValue &value : translations) {
		if (value.toObject().value(QStringLiteral("id")).toString() == id)
			return value.toObject();
	}
	return {};
}

void BibleCatalogDialog::onCurrentChanged()
{
	const QJsonObject t = currentTranslation();
	if (t.isEmpty()) {
		downloadButton->setEnabled(false);
		if (!translations.isEmpty())
			infoLabel->setText(tr("No hay traducciones con ese nombre."));
		return;
	}

	const int books = t.value(QStringLiteral("numberOfBooks")).toInt();
	QString coverage;
	if (books >= 66)
		coverage = tr("Biblia completa");
	else if (books == 27)
		coverage = tr("Solo Nuevo Testamento");
	else
		coverage = tr("%1 libros").arg(books);
	const QString url = t.value(QStringLiteral("licenseUrl")).toString();
	const bool installed = isInstalled(t.value(QStringLiteral("id")).toString());

	infoLabel->setText(QStringLiteral("<b>%1</b><br>%2 · %3 · %4 versículos · <a href=\"%5\">%6</a>%7")
				   .arg(t.value(QStringLiteral("name")).toString().toHtmlEscaped(), languageLabel(t).toHtmlEscaped(),
					coverage)
				   .arg(t.value(QStringLiteral("totalNumberOfVerses")).toInt())
				   .arg(url.toHtmlEscaped(), BibleCatalog::LicenseText(t).toHtmlEscaped(),
					installed ? tr("<br>Ya está instalada. Descargala de nuevo para actualizarla.")
						  : QString()));
	downloadButton->setEnabled(!reply);
	downloadButton->setText(installed ? tr("Descargar de nuevo") : tr("Descargar"));
}

void BibleCatalogDialog::setBusy(bool busy, const QString &message)
{
	progress->setVisible(busy);
	progress->setRange(0, 0);
	downloadButton->setEnabled(!busy && list->currentItem());
	if (!message.isEmpty())
		infoLabel->setText(message);
}

void BibleCatalogDialog::onDownloadClicked()
{
	const QJsonObject t = currentTranslation();
	if (t.isEmpty() || reply)
		return;

	const QString name = t.value(QStringLiteral("name")).toString();
	const QString link = t.value(QStringLiteral("simpleCompleteTranslationApiLink")).toString();
	if (link.isEmpty()) {
		infoLabel->setText(tr("Esta traducción no se puede descargar."));
		return;
	}

	setBusy(true, tr("Descargando %1...").arg(name.toHtmlEscaped()));
	QNetworkRequest request{QUrl(QString::fromLatin1(ApiBase) + link)};
	request.setTransferTimeout(120000);
	reply = network->get(request);
	connect(reply, &QNetworkReply::downloadProgress, this, [this](qint64 received, qint64 total) {
		if (total > 0) {
			progress->setRange(0, 100);
			progress->setValue(int(received * 100 / total));
		}
	});
	connect(reply, &QNetworkReply::finished, this, [this, t, name, r = reply.data()]() {
		r->deleteLater();
		reply = nullptr;
		const QByteArray data = r->readAll();
		const QNetworkReply::NetworkError networkError = r->error();
		setBusy(false, QString());
		if (networkError == QNetworkReply::OperationCanceledError)
			return;
		if (networkError != QNetworkReply::NoError) {
			infoLabel->setText(tr("No se pudo descargar: %1").arg(r->errorString().toHtmlEscaped()));
			return;
		}

		QString error;
		int verses = 0;
		const QByteArray bible = BibleCatalog::Convert(QJsonDocument::fromJson(data).object(), t, &error, &verses);
		if (bible.isEmpty()) {
			infoLabel->setText(error.toHtmlEscaped());
			return;
		}

		const QString path =
			QDir(bibleDir).absoluteFilePath(BibleCatalog::FileName(t.value(QStringLiteral("id")).toString()));
		QSaveFile file(path);
		if (!file.open(QIODevice::WriteOnly) || file.write(bible) != bible.size() || !file.commit()) {
			infoLabel->setText(tr("No se pudo guardar la Biblia en %1.").arg(path.toHtmlEscaped()));
			return;
		}

		refill();
		infoLabel->setText(tr("<b>%1</b> quedó instalada (%2 versículos). Ya la podés elegir en Versión.")
					   .arg(name.toHtmlEscaped())
					   .arg(verses));
		emit installed(path);
	});
}
