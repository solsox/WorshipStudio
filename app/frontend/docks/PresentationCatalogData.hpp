#pragma once

#include <QString>
#include <QStringList>
#include <QVector>

struct PresentationEntry {
	int number = 0;
	QString displayLabel;
	QString title;
	int slideCount = 0;
	QVector<int> slideTimes;
	QString folderPath;
	QStringList slidePaths;
	QString cantadoPath;
	QString instrumentalPath;
	QString searchBlob;
};

enum class HymnalEdition { New2009 = 0, Old1962 = 1 };

/* Reads a hymnal folder. Two layouts are supported:
 *  - the "Himnario Adventista" PC program (Resources/index.html catalog with
 *    slide timings, Letra/NNN - Titulo/*.jpg, Musica/Cantado|Instrumental/*.mp3)
 *  - a plain folder of hymns: NNN - Titulo/ with images (and optionally mp3
 *    files, or Cantado/ and Instrumental/ folders next to them). Optional
 *    "tiempos.txt" in a hymn folder holds slide change times in ms. */
class PresentationCatalogData {
public:
	static QString editionName(HymnalEdition edition);

	static bool isHymnRoot(const QString &rootPath);
	static QString discoverHymnRoot(HymnalEdition edition, const QString &preferredRoot = QString());
	static QVector<PresentationEntry> loadHymnCatalog(HymnalEdition edition, const QString &preferredRoot,
							  QString *error = nullptr, QString *usedRoot = nullptr);

	static QString normalizeName(const QString &text);

private:
	static QStringList candidateRoots(HymnalEdition edition, const QString &preferredRoot);
	static QString findCatalogFile(const QString &rootPath);
	static QString lyricsDir(const QString &rootPath);
	static QVector<PresentationEntry> loadFromCatalog(const QString &root, QString *error);
	static QVector<PresentationEntry> loadFromFolders(const QString &root, QString *error);
};
