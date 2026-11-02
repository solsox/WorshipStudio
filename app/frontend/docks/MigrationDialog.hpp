#pragma once

#include <QDialog>
#include <QPointer>

class BibleDock;
class PresentationCatalogDock;
class QLabel;
class SongsDock;

/* "Traer desde otro programa": scenes and profiles from OBS Studio, songs
 * exported from Holyrics, OpenLP, OpenSong, ProPresenter..., Bibles in open
 * formats and the hymnal folder. Nothing is removed from the other program. */
class MigrationDialog : public QDialog {
	Q_OBJECT

public:
	MigrationDialog(SongsDock *songs, BibleDock *bible, PresentationCatalogDock *hymnal, QWidget *parent = nullptr);

	/* Opens the dialog over the main window, finding the docks by name. */
	static void openFor(QWidget *mainWindow);

	/* OBS Studio's settings folder on this computer, empty when not found. */
	static QString obsFolder();
	/* Copies OBS scene collections and profiles; returns how many of each. */
	static bool importObs(int *collections, int *profiles, QString *firstCollection, QString *firstCollectionFile,
			      QString *firstProfile, QString *firstProfileDir, QString *error);
	/* Other presentation programs found on this computer, for the hints. */
	static QStringList detectedPrograms();

private:
	QPointer<SongsDock> songs;
	QPointer<BibleDock> bible;
	QPointer<PresentationCatalogDock> hymnal;
	QLabel *obsStatus = nullptr;
	QLabel *songsStatus = nullptr;
	QLabel *bibleStatus = nullptr;

	QWidget *section(const QString &title, const QString &text, QLabel **status);
	void importSongs(const QStringList &files, const QString &folder);
};
