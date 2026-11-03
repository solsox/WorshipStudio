#pragma once

#include "OBSDock.hpp"
#include "SongLibrary.hpp"

#include <obs.hpp>

#include <QVector>

class QLabel;
class QLineEdit;
class QListWidget;
class QListWidgetItem;
class QPushButton;

/* Songs and choruses written by the church (like Holyrics): a library of
 * songs, an editor and live projection slide by slide. */
class SongsDock : public OBSDock {
	Q_OBJECT

public:
	SongsDock(const QString &objectName, const QString &title, QWidget *parent = nullptr);

	/* Used by the self test: saves a song, selects and projects it. */
	bool projectNewSong(const QString &title, const QString &text, int preset);
	void deleteSong(const QString &title);
	void goToSlide(int slide) { showSlide(slide); }
	bool jumpToSection(QChar key);
	/* Used by the service order. */
	bool projectSongById(const QString &id);
	/* Projects a song that is not saved in the library (announcements). */
	bool projectTransient(const Song &song);
	int currentSlide() const { return liveSlide; }
	int slideCount() const;
	obs_scene_t *outputScene() const { return scene; }

public slots:
	/* Shows what is selected inside the panel's "en vivo" scene, without
	 * changing the screen (dropped on the Sources panel). */
	bool useAsSource();
	/* Songs were added from outside the dock (migration, folder import). */
	void reloadLibrary() { reload(); }
	void nextSlide();
	void previousSlide();
	void toggleHidden();
	void toggleClearText();

protected:
	bool eventFilter(QObject *watched, QEvent *event) override;

private slots:
	void onSearchChanged(const QString &text);
	void onSongChanged();
	void onNewClicked();
	void onEditClicked();
	void onDeleteClicked();
	void onImportClicked();
	void onSlideClicked(QListWidgetItem *item);

private:
	QVector<Song> songs;
	QLineEdit *searchEdit = nullptr;
	QListWidget *songsList = nullptr;
	QListWidget *slidesList = nullptr;
	QPushButton *clearButton = nullptr;
	QLabel *liveLabel = nullptr;
	QLabel *statusLabel = nullptr;

	int selectedSong = -1;
	Song liveSong;
	int liveSlide = -1;
	bool live = false;
	bool hidden = false;
	bool textCleared = false;

	OBSScene scene;
	QString outputSlot;
	bool freshOutput = false;
	OBSSource screenSource;

	void reload(const QString &selectId = QString());
	void rebuildList();
	void rebuildSlides();
	bool editSong(Song song);
	bool project();
	bool prepareOutput(const WorshipRender::Background &background);
	void showSlide(int slide, bool force = false);
	QString renderedSlide(int slide) const;
	void setItemVisible(bool visible);
	void updateControls();
	void updateLiveLabel();
	void publishLive();
};
