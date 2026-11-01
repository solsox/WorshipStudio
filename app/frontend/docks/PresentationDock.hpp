#pragma once

#include "OBSDock.hpp"

#include <obs.hpp>

#include <QHash>
#include <QIcon>
#include <QStringList>
#include <QVector>

class QCheckBox;
class QComboBox;
class QLabel;
class QListWidget;
class QListWidgetItem;
class QPushButton;
class QTimer;

namespace WorshipDocument {
class Converter;
}

/* Projects PDF files, PowerPoint/Keynote/Impress presentations and sets of
 * images one slide at a time through a regular OBS scene. */
class PresentationDock : public OBSDock {
	Q_OBJECT

public:
	PresentationDock(const QString &objectName, const QString &title, QWidget *parent = nullptr);
	~PresentationDock() override;

	/* Used by the self test. */
	void openFiles(const QStringList &paths);
	bool isConverting() const;
	bool project();
	void goToSlide(int slide) { showSlide(slide); }
	void setBookMode(bool enabled);
	int currentSlide() const { return liveSlide; }
	int slideCount() const { return (int)slides.size(); }
	QString deckError() const { return lastError; }
	obs_scene_t *outputScene() const { return scene; }

public slots:
	void nextSlide();
	void previousSlide();
	void toggleHidden();

protected:
	bool eventFilter(QObject *watched, QEvent *event) override;
	void dragEnterEvent(QDragEnterEvent *event) override;
	void dropEvent(QDropEvent *event) override;

private slots:
	void onOpenClicked();
	void onRemoveClicked();
	void onDeckActivated(QListWidgetItem *item);
	void onSlideClicked(QListWidgetItem *item);
	void onThumbnailTick();

private:
	/* A presentation: one PDF (maybe converted from an office file) or a
	 * list of images. `key` identifies it in the recent list. */
	struct Deck {
		QString key;
		QString title;
		QString pdfPath;
		QStringList images;
		int pageCount = 0;
	};

	/* A projected slide: a page, or part of a tall page in book mode. */
	struct Slide {
		int page = 0;
		int part = 0;
		int parts = 1;
	};

	QListWidget *decksList = nullptr;
	QListWidget *slidesList = nullptr;
	QComboBox *fitCombo = nullptr;
	QCheckBox *bookCheck = nullptr;
	QPushButton *projectButton = nullptr;
	QPushButton *previousButton = nullptr;
	QPushButton *nextButton = nullptr;
	QPushButton *hideButton = nullptr;
	QLabel *liveLabel = nullptr;
	QLabel *statusLabel = nullptr;
	QTimer *thumbnailTimer = nullptr;
	WorshipDocument::Converter *converter = nullptr;

	Deck deck;
	QVector<Slide> slides;
	int nextThumbnail = 0;
	int liveSlide = -1;
	bool live = false;
	bool hidden = false;
	QString lastError;

	OBSScene scene;
	OBSSource slideSource;

	obs_hotkey_id nextHotkey = OBS_INVALID_HOTKEY_ID;
	obs_hotkey_id previousHotkey = OBS_INVALID_HOTKEY_ID;
	obs_hotkey_id hideHotkey = OBS_INVALID_HOTKEY_ID;

	void registerHotkeys();
	void saveHotkeys();

	void loadDeck(const QString &key);
	void setDeck(const Deck &newDeck);
	void rebuildSlides();
	void rebuildThumbnails();
	QImage renderSlide(int index, const QSize &canvas) const;
	QString renderedSlide(int index) const;
	bool prepareOutput();
	void showSlide(int slide);
	void stopLive();
	void selectSlideRow(int slide);
	void updateControls();
	void updateLiveLabel();
	void publishLive();
	void setStatus(const QString &text, bool error = false);

	QStringList recentKeys() const;
	void rememberDeck(const QString &key);
	void rebuildDecksList();
	static QString titleFor(const QString &key);
};
