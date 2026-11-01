#include "SongLibrary.hpp"
#include "PresentationCatalogData.hpp"

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QRegularExpression>
#include <QSaveFile>
#include <QStandardPaths>
#include <QStringDecoder>
#include <QUuid>
#include <QXmlStreamReader>

#include <algorithm>

QString Song::searchBlob() const
{
	QString blob = title + QLatin1Char(' ') + author;
	for (const SongSlide &slide : slides)
		blob += QLatin1Char(' ') + slide.text;
	return PresentationCatalogData::normalizeName(blob);
}

namespace SongLibrary {

namespace {
const QRegularExpression LabelLine(QStringLiteral("^\\s*\\[(.+)\\]\\s*$"));

QJsonObject toJson(const Song &song)
{
	QJsonArray slides;
	for (const SongSlide &slide : song.slides) {
		QJsonObject object{{QStringLiteral("label"), slide.label}, {QStringLiteral("text"), slide.text}};
		if (slide.hasBackground)
			object.insert(QStringLiteral("background"), backgroundToJson(slide.background));
		slides.append(object);
	}

	return QJsonObject{
		{QStringLiteral("format"), QStringLiteral("worshipstudio-song")},
		{QStringLiteral("version"), 1},
		{QStringLiteral("id"), song.id},
		{QStringLiteral("title"), song.title},
		{QStringLiteral("author"), song.author},
		{QStringLiteral("copyright"), song.copyright},
		{QStringLiteral("style"),
		 QJsonObject{{QStringLiteral("font"), song.fontFamily},
			     {QStringLiteral("size"), song.fontSize},
			     {QStringLiteral("uppercase"), song.uppercase},
			     {QStringLiteral("showTitle"), song.showTitle},
			     {QStringLiteral("layout"), (int)song.layout},
			     {QStringLiteral("background"), backgroundToJson(song.background)}}},
		{QStringLiteral("slides"), slides},
	};
}

Song fromJson(const QJsonObject &object)
{
	Song song;
	song.id = object.value(QStringLiteral("id")).toString();
	song.title = object.value(QStringLiteral("title")).toString();
	song.author = object.value(QStringLiteral("author")).toString();
	song.copyright = object.value(QStringLiteral("copyright")).toString();

	const QJsonObject style = object.value(QStringLiteral("style")).toObject();
	song.fontFamily = style.value(QStringLiteral("font")).toString();
	song.fontSize = qBound(30, style.value(QStringLiteral("size")).toInt(96), 160);
	song.uppercase = style.value(QStringLiteral("uppercase")).toBool();
	song.showTitle = style.value(QStringLiteral("showTitle")).toBool();
	song.layout = static_cast<WorshipRender::LyricsLayout>(qBound(0, style.value(QStringLiteral("layout")).toInt(), 1));
	song.background = backgroundFromJson(style.value(QStringLiteral("background")).toObject());

	for (const QJsonValue value : object.value(QStringLiteral("slides")).toArray()) {
		const QJsonObject item = value.toObject();
		SongSlide slide;
		slide.label = item.value(QStringLiteral("label")).toString();
		slide.text = item.value(QStringLiteral("text")).toString();
		if (item.contains(QStringLiteral("background"))) {
			slide.hasBackground = true;
			slide.background = backgroundFromJson(item.value(QStringLiteral("background")).toObject());
		}
		song.slides.push_back(slide);
	}
	return song;
}

QString fileFor(const Song &song)
{
	return QDir(directory()).absoluteFilePath(song.id + QStringLiteral(".json"));
}
} // namespace

WorshipRender::LyricsSlide lyricsSlide(const Song &song, int index)
{
	WorshipRender::LyricsSlide slide;
	slide.fontFamily = song.fontFamily;
	slide.maxFontSize = song.fontSize;
	slide.uppercase = song.uppercase;
	slide.layout = song.layout;
	slide.background = song.background;
	if (index >= 0 && index < song.slides.size()) {
		const SongSlide &source = song.slides.at(index);
		slide.text = source.text;
		if (source.hasBackground)
			slide.background = source.background;
		/* The title is shown on the first slide, like printed hymnals. */
		if (song.showTitle && index == 0)
			slide.footer = song.author.isEmpty() ? song.title : song.title + QStringLiteral("  ·  ") + song.author;
	}
	return slide;
}

QJsonObject backgroundToJson(const WorshipRender::Background &background)
{
	return QJsonObject{{QStringLiteral("kind"), (int)background.kind},
			   {QStringLiteral("preset"), background.preset},
			   {QStringLiteral("path"), background.path},
			   {QStringLiteral("color"), background.color}};
}

WorshipRender::Background backgroundFromJson(const QJsonObject &object)
{
	WorshipRender::Background background;
	background.kind =
		static_cast<WorshipRender::BackgroundKind>(qBound(0, object.value(QStringLiteral("kind")).toInt(), 3));
	background.preset = object.value(QStringLiteral("preset")).toInt();
	background.path = object.value(QStringLiteral("path")).toString();
	background.color = object.value(QStringLiteral("color")).toString();
	return background;
}

QString directory()
{
	const QString base = QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation);
	const QString path = QDir(base).absoluteFilePath(QStringLiteral("WorshipStudio/cantos"));
	QDir().mkpath(path);
	return path;
}

QVector<Song> loadAll()
{
	QVector<Song> songs;
	const QFileInfoList files = QDir(directory()).entryInfoList({QStringLiteral("*.json")}, QDir::Files);
	for (const QFileInfo &info : files) {
		QFile file(info.absoluteFilePath());
		if (!file.open(QIODevice::ReadOnly))
			continue;
		const QJsonObject object = QJsonDocument::fromJson(file.readAll()).object();
		if (object.value(QStringLiteral("format")).toString() != QStringLiteral("worshipstudio-song"))
			continue;
		Song song = fromJson(object);
		if (song.id.isEmpty())
			song.id = info.completeBaseName();
		songs.push_back(song);
	}

	std::sort(songs.begin(), songs.end(), [](const Song &a, const Song &b) {
		return PresentationCatalogData::normalizeName(a.title) < PresentationCatalogData::normalizeName(b.title);
	});
	return songs;
}

bool save(Song &song, QString *error)
{
	if (song.id.isEmpty())
		song.id = QUuid::createUuid().toString(QUuid::WithoutBraces);

	QSaveFile file(fileFor(song));
	if (!file.open(QIODevice::WriteOnly)) {
		if (error)
			*error = file.errorString();
		return false;
	}
	file.write(QJsonDocument(toJson(song)).toJson(QJsonDocument::Indented));
	if (!file.commit()) {
		if (error)
			*error = file.errorString();
		return false;
	}
	return true;
}

bool remove(const Song &song)
{
	return !song.id.isEmpty() && QFile::remove(fileFor(song));
}

QString toText(const Song &song)
{
	QStringList blocks;
	QString lastLabel;
	for (const SongSlide &slide : song.slides) {
		QString block;
		if (!slide.label.isEmpty() && slide.label != lastLabel)
			block = QStringLiteral("[%1]\n").arg(slide.label);
		block += slide.text.trimmed();
		blocks << block;
		lastLabel = slide.label;
	}
	return blocks.join(QStringLiteral("\n\n"));
}

QVector<SongSlide> parseText(const QString &text, const QVector<SongSlide> &previous)
{
	QVector<SongSlide> slides;
	QString label;
	QStringList current;

	const auto flush = [&]() {
		if (current.isEmpty())
			return;
		SongSlide slide;
		slide.label = label;
		slide.text = current.join(QLatin1Char('\n'));
		/* Keep per-slide backgrounds while the text is being edited. */
		const int index = (int)slides.size();
		if (index < previous.size() && previous.at(index).hasBackground) {
			slide.hasBackground = true;
			slide.background = previous.at(index).background;
		}
		slides.push_back(slide);
		current.clear();
	};

	QString normalized = text;
	normalized.replace(QStringLiteral("\r\n"), QStringLiteral("\n"));
	for (const QString &raw : normalized.split(QLatin1Char('\n'))) {
		const QString line = raw.trimmed();
		const QRegularExpressionMatch match = LabelLine.match(line);
		if (match.hasMatch()) {
			flush();
			label = match.captured(1).trimmed();
		} else if (line.isEmpty()) {
			flush();
		} else {
			current << line;
		}
	}
	flush();
	return slides;
}


namespace {
/* "v1" -> "Estrofa 1", "c" -> "Coro"... (OpenLyrics and OpenSong tags). */
QString sectionLabel(QString tag)
{
	tag = tag.trimmed().toLower();
	const QString number = tag.mid(1).remove(QRegularExpression(QStringLiteral("[^0-9]")));
	const auto withNumber = [&](const QString &name) { return number.isEmpty() ? name : name + QLatin1Char(' ') + number; };
	if (tag.startsWith(QLatin1Char('v')))
		return withNumber(QStringLiteral("Estrofa"));
	if (tag.startsWith(QLatin1Char('c')))
		return withNumber(QStringLiteral("Coro"));
	if (tag.startsWith(QLatin1Char('b')))
		return withNumber(QStringLiteral("Puente"));
	if (tag.startsWith(QLatin1Char('p')))
		return withNumber(QStringLiteral("Pre-coro"));
	if (tag.startsWith(QLatin1Char('e')))
		return QStringLiteral("Final");
	if (tag.startsWith(QLatin1Char('i')))
		return QStringLiteral("Intro");
	if (tag.startsWith(QLatin1Char('t')))
		return withNumber(QStringLiteral("Etiqueta"));
	return tag.isEmpty() ? QString() : tag.left(1).toUpper() + tag.mid(1);
}

/* OpenLyrics (OpenLP, Worship Extreme, Quelea...). */
bool parseOpenLyrics(const QByteArray &data, Song *song)
{
	QXmlStreamReader xml(data);
	QStringList blocks;
	QString verseName, lines;
	bool inLines = false, inTitle = false, inAuthor = false;
	while (!xml.atEnd()) {
		xml.readNext();
		const QString name = xml.name().toString();
		if (xml.isStartElement()) {
			if (name == QStringLiteral("title") && song->title.isEmpty())
				inTitle = true;
			else if (name == QStringLiteral("author") && song->author.isEmpty())
				inAuthor = true;
			else if (name == QStringLiteral("verse"))
				verseName = xml.attributes().value(QStringLiteral("name")).toString();
			else if (name == QStringLiteral("lines")) {
				inLines = true;
				lines.clear();
			} else if (name == QStringLiteral("br") && inLines)
				lines += QLatin1Char('\n');
			else if (name == QStringLiteral("copyright"))
				song->copyright = xml.readElementText(QXmlStreamReader::IncludeChildElements).trimmed();
		} else if (xml.isCharacters() && !xml.isWhitespace()) {
			if (inTitle)
				song->title += xml.text().toString().trimmed();
			else if (inAuthor)
				song->author += xml.text().toString().trimmed();
			else if (inLines)
				lines += xml.text().toString();
		} else if (xml.isEndElement()) {
			if (name == QStringLiteral("title"))
				inTitle = false;
			else if (name == QStringLiteral("author"))
				inAuthor = false;
			else if (name == QStringLiteral("lines")) {
				inLines = false;
				const QString label = sectionLabel(verseName);
				blocks << (label.isEmpty() ? QString() : QStringLiteral("[%1]\n").arg(label)) + lines.trimmed();
			}
		}
	}
	if (xml.hasError() || blocks.isEmpty())
		return false;
	song->slides = parseText(blocks.join(QStringLiteral("\n\n")));
	return !song->slides.isEmpty();
}

/* OpenSong: [V1] markers, "." chord lines, ";" comments, "|" slide breaks. */
bool parseOpenSong(const QByteArray &data, Song *song)
{
	QXmlStreamReader xml(data);
	QString lyrics;
	while (!xml.atEnd()) {
		xml.readNext();
		if (!xml.isStartElement())
			continue;
		const QString name = xml.name().toString();
		if (name == QStringLiteral("title"))
			song->title = xml.readElementText(QXmlStreamReader::IncludeChildElements).trimmed();
		else if (name == QStringLiteral("author"))
			song->author = xml.readElementText(QXmlStreamReader::IncludeChildElements).trimmed();
		else if (name == QStringLiteral("copyright"))
			song->copyright = xml.readElementText(QXmlStreamReader::IncludeChildElements).trimmed();
		else if (name == QStringLiteral("lyrics"))
			lyrics = xml.readElementText(QXmlStreamReader::IncludeChildElements);
	}
	if (lyrics.trimmed().isEmpty())
		return false;

	QStringList out;
	for (QString line : lyrics.split(QLatin1Char('\n'))) {
		if (line.startsWith(QLatin1Char('.')) || line.startsWith(QLatin1Char(';')))
			continue;
		const QString trimmed = line.trimmed();
		if (trimmed.startsWith(QLatin1Char('[')) && trimmed.endsWith(QLatin1Char(']'))) {
			out << QString() << QStringLiteral("[%1]").arg(sectionLabel(trimmed.mid(1, trimmed.size() - 2)));
			continue;
		}
		if (trimmed == QStringLiteral("||") || trimmed == QStringLiteral("|")) {
			out << QString();
			continue;
		}
		line.replace(QStringLiteral("||"), QStringLiteral("\n\n")).replace(QLatin1Char('|'), QLatin1Char('\n'));
		out << line.trimmed().remove(QRegularExpression(QStringLiteral("^[0-9]+\\s?")));
	}
	song->slides = parseText(out.join(QLatin1Char('\n')));
	return !song->slides.isEmpty();
}

/* ChordPro: {title: ...}, {soc}...{eoc}, inline [G] chords. */
bool parseChordPro(const QString &text, Song *song)
{
	const QRegularExpression directive(QStringLiteral("^\\{\\s*([a-z_]+)\\s*(?::\\s*(.*))?\\}$"),
					   QRegularExpression::CaseInsensitiveOption);
	const QRegularExpression chord(QStringLiteral("\\[[^\\]]*\\]"));
	QStringList out;
	int verse = 0;
	for (const QString &raw : text.split(QLatin1Char('\n'))) {
		const QString line = raw.trimmed();
		const QRegularExpressionMatch match = directive.match(line);
		if (match.hasMatch()) {
			const QString key = match.captured(1).toLower();
			const QString value = match.captured(2).trimmed();
			if (key == QStringLiteral("title") || key == QStringLiteral("t"))
				song->title = value;
			else if (key == QStringLiteral("artist") || key == QStringLiteral("subtitle") || key == QStringLiteral("st"))
				song->author = song->author.isEmpty() ? value : song->author;
			else if (key == QStringLiteral("copyright"))
				song->copyright = value;
			else if (key == QStringLiteral("start_of_chorus") || key == QStringLiteral("soc"))
				out << QString() << QStringLiteral("[Coro]");
			else if (key == QStringLiteral("start_of_bridge") || key == QStringLiteral("sob"))
				out << QString() << QStringLiteral("[Puente]");
			else if (key == QStringLiteral("start_of_verse") || key == QStringLiteral("sov"))
				out << QString() << QStringLiteral("[Estrofa %1]").arg(++verse);
			else if (key == QStringLiteral("comment") || key == QStringLiteral("c"))
				out << QString() << QStringLiteral("[%1]").arg(value);
			else if (key.startsWith(QStringLiteral("end_of")) || key.startsWith(QStringLiteral("eo")))
				out << QString();
			continue;
		}
		if (line.startsWith(QLatin1Char('#')))
			continue;
		out << QString(line).remove(chord).simplified();
	}
	song->slides = parseText(out.join(QLatin1Char('\n')));
	return !song->slides.isEmpty();
}
} // namespace

bool importFile(const QString &path, Song *song, QString *error)
{
	QFile file(path);
	if (!file.open(QIODevice::ReadOnly)) {
		if (error)
			*error = file.errorString();
		return false;
	}
	const QByteArray data = file.readAll();

	if (QFileInfo(path).suffix().compare(QStringLiteral("json"), Qt::CaseInsensitive) == 0) {
		const QJsonObject object = QJsonDocument::fromJson(data).object();
		if (object.value(QStringLiteral("format")).toString() != QStringLiteral("worshipstudio-song")) {
			if (error)
				*error = QStringLiteral("No es un canto de WorshipStudio.");
			return false;
		}
		*song = fromJson(object);
		song->id.clear();
		return true;
	}

	const QString suffix = QFileInfo(path).suffix().toLower();
	const QByteArray head = data.left(512).trimmed();
	if (head.startsWith("<?xml") || head.startsWith("<song")) {
		Song imported;
		const bool ok = data.contains("openlyrics.info") ? parseOpenLyrics(data, &imported)
								 : parseOpenSong(data, &imported);
		if (!ok) {
			if (error)
				*error = QStringLiteral("Formato XML no reconocido (se aceptan OpenLyrics y OpenSong).");
			return false;
		}
		if (imported.title.isEmpty())
			imported.title = QFileInfo(path).completeBaseName();
		*song = imported;
		return true;
	}

	/* Plain text: UTF-8, or Latin-1 for files written by older programs. */
	auto decoder = QStringDecoder(QStringDecoder::Utf8);
	QString text = decoder(data);
	if (decoder.hasError())
		text = QString::fromLatin1(data);

	Song imported;
	imported.title = QFileInfo(path).completeBaseName();
	const bool chordPro = suffix == QStringLiteral("cho") || suffix == QStringLiteral("chopro") ||
			      suffix == QStringLiteral("chordpro") || suffix == QStringLiteral("crd") ||
			      text.contains(QStringLiteral("{title")) || text.contains(QStringLiteral("{t:"));
	if (chordPro) {
		parseChordPro(text, &imported);
		if (imported.title.isEmpty())
			imported.title = QFileInfo(path).completeBaseName();
	} else {
		imported.slides = parseText(text);
	}
	if (imported.slides.isEmpty()) {
		if (error)
			*error = QStringLiteral("El archivo esta vacio.");
		return false;
	}
	*song = imported;
	return true;
}

} // namespace SongLibrary
