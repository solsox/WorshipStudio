#pragma once

#include "WorshipRender.hpp"

#include <QJsonObject>
#include <QString>
#include <QVector>

/* One screen of a song. `label` names the section (Estrofa 1, Coro...). */
struct SongSlide {
	QString label;
	QString text;
	bool hasBackground = false;
	WorshipRender::Background background;
};

/* A song or chorus written by the church. Saved as one JSON file per song
 * so songs can be copied between computers or shared. */
struct Song {
	QString id;
	QString title;
	QString author;
	QString copyright;
	QString fontFamily;
	int fontSize = 96;
	bool uppercase = false;
	bool showTitle = false;
	WorshipRender::LyricsLayout layout = WorshipRender::LyricsLayout::FullScreen;
	WorshipRender::Background background;
	QVector<SongSlide> slides;

	QString searchBlob() const;
};

namespace SongLibrary {

/* Folder with the song files (created if needed). */
QString directory();

QVector<Song> loadAll();
bool save(Song &song, QString *error = nullptr);
bool remove(const Song &song);

/* Plain text form used by the editor:
 *   [Estrofa 1]
 *   first line
 *   second line
 *   <blank line = new slide>
 *   [Coro]
 *   ...
 * A [label] names only the block under it; [Coro] alone repeats the chorus. */
QString toText(const Song &song);
QVector<SongSlide> parseText(const QString &text, const QVector<SongSlide> &previous = {});

/* Imports a .txt (one song, title from the first line or the file name) or
 * a WorshipStudio song .json. */
bool importFile(const QString &path, Song *song, QString *error);
/* Every song file under a folder (subfolders too); returns how many were imported. */
int importFolder(const QString &folder, QStringList *errors = nullptr);
QStringList importableSuffixes();

/* What the screen shows for a slide (song style + slide overrides). */
WorshipRender::LyricsSlide lyricsSlide(const Song &song, int index);

QJsonObject backgroundToJson(const WorshipRender::Background &background);
WorshipRender::Background backgroundFromJson(const QJsonObject &object);

} // namespace SongLibrary
