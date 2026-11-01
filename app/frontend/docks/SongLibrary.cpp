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

	/* Plain text: UTF-8, or Latin-1 for files written by older programs. */
	auto decoder = QStringDecoder(QStringDecoder::Utf8);
	QString text = decoder(data);
	if (decoder.hasError())
		text = QString::fromLatin1(data);

	Song imported;
	imported.title = QFileInfo(path).completeBaseName();
	imported.slides = parseText(text);
	if (imported.slides.isEmpty()) {
		if (error)
			*error = QStringLiteral("El archivo esta vacio.");
		return false;
	}
	*song = imported;
	return true;
}

} // namespace SongLibrary
