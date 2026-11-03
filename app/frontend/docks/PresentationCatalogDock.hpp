#pragma once

#include "OBSDock.hpp"
#include "PresentationCatalogData.hpp"

#include <obs.hpp>
#include <obs-frontend-api.h>

#include <QVector>

#include <string>

class QComboBox;
class QLabel;
class QLineEdit;
class QListWidget;
class QListWidgetItem;
class QPushButton;
class QTimer;

class PresentationCatalogDock : public OBSDock {
	Q_OBJECT

public:
	PresentationCatalogDock(const QString &objectName, const QString &title, QWidget *parent = nullptr);
	~PresentationCatalogDock() override;

	/* Used by the self test. `mode`: 0 cantado, 1 instrumental, 2 solo letra. */
	bool projectHymn(int number, int mode);
	/* 0 cantado, 1 instrumental, 2 solo letra (as chosen in the dock). */
	int audioModeIndex() const;
	void goToSlide(int slide) { showSlide(slide, true); }
	int currentSlide() const { return liveSlide; }
	int slideCount() const { return liveEntry >= 0 ? (int)allEntries.at(liveEntry).slidePaths.size() : 0; }
	QVector<int> slideTimes() const { return liveEntry >= 0 ? allEntries.at(liveEntry).slideTimes : QVector<int>(); }
	obs_source_t *audio() const { return audioSource; }
	obs_scene_t *outputScene() const { return scene; }
	void stop() { onStopClicked(); }
	int hymnCount() const { return (int)allEntries.size(); }
	/* Switches edition and, if given, points it at a folder. Returns the previous folder. */
	QString useEdition(int editionIndex, const QString &root);

public slots:
	/* Shows what is selected inside the panel's "en vivo" scene, without
	 * changing the screen (dropped on the Sources panel). */
	bool useAsSource();
	void nextSlide() { onNextClicked(); }
	void previousSlide() { onPreviousClicked(); }
	void toggleHidden();

private slots:
	void onSearchChanged(const QString &text);
	void onCurrentHymnChanged(QListWidgetItem *current, QListWidgetItem *previous);
	void onSlideClicked(QListWidgetItem *item);
	void onSlideRowChanged(int row);
	void onProjectClicked();
	void onPreviousClicked();
	void onNextClicked();
	void onStopClicked();
	void onChooseFolderClicked();
	void onSyncTick();

private:
	enum class AudioMode { Sung = 0, Instrumental = 1, None = 2 };

	QVector<PresentationEntry> allEntries;
	QString hymnRoot;

	QLineEdit *searchEdit = nullptr;
	QListWidget *resultsList = nullptr;
	QComboBox *audioModeCombo = nullptr;
	QComboBox *fitCombo = nullptr;
	QComboBox *editionCombo = nullptr;
	QListWidget *slidesList = nullptr;
	QPushButton *stopButton = nullptr;
	QLabel *statusLabel = nullptr;
	QLabel *liveLabel = nullptr;
	QTimer *syncTimer = nullptr;

	int selectedEntry = -1;
	int liveEntry = -1;
	int liveSlide = -1;
	bool liveWithAudio = false;

	OBSScene scene;
	QString outputSlot;
	OBSSource slideSource;
	OBSSource audioSource;

	void loadCatalog();
	HymnalEdition edition() const;
	void rebuildResults(const QString &filter);
	void loadSlideThumbnails(int entryIndex);
	bool prepareOutput();
	void stopAudioOffAir();
	static void onFrontendEvent(enum obs_frontend_event event, void *data);
	std::string slideItemName() const;
	void showSlide(int slide, bool seekAudio);
	void selectSlideRow(int slide);
	QString renderedSlide(int entryIndex, int slide) const;
	void updateLiveLabel();
	void updateControls();
	AudioMode audioMode() const;
	QString audioPathFor(const PresentationEntry &entry) const;
};
