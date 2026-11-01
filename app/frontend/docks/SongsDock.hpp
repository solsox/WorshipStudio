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
	~SongsDock() override;

	/* Used by the self test: saves a song, selects and projects it. */
	bool projectNewSong(const QString &title, const QString &text, int preset);
	void deleteSong(const QString &title);
	void goToSlide(int slide) { showSlide(slide); }
	bool jumpToSection(QChar key);
	int currentSlide() const { return liveSlide; }
	int slideCount() const;
	obs_scene_t *outputScene() const { return scene; }

public slots:
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
	QPushButton *projectButton = nullptr;
	QPushButton *previousButton = nullptr;
	QPushButton *nextButton = nullptr;
	QPushButton *clearButton = nullptr;
	QPushButton *hideButton = nullptr;
	QLabel *liveLabel = nullptr;
	QLabel *statusLabel = nullptr;

	int selectedSong = -1;
	Song liveSong;
	int liveSlide = -1;
	bool live = false;
	bool hidden = false;
	bool textCleared = false;

	OBSScene scene;
	OBSSource screenSource;

	obs_hotkey_id nextHotkey = OBS_INVALID_HOTKEY_ID;
	obs_hotkey_id previousHotkey = OBS_INVALID_HOTKEY_ID;
	obs_hotkey_id hideHotkey = OBS_INVALID_HOTKEY_ID;

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
	void registerHotkeys();
	void saveHotkeys();
	void createExampleSong();
};
