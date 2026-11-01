#pragma once

#include "OBSDock.hpp"
#include "WorshipRender.hpp"

#include <obs.hpp>

#include <QString>
#include <QStringList>
#include <QVector>

#include <string>

class QComboBox;
class QLabel;
class QLineEdit;
class QListWidget;
class QPushButton;
class QSpinBox;
class QResizeEvent;

struct BibleBook {
	QString name;
	QString key;
	QStringList aliases;
	QVector<QStringList> chapters;
};

struct BibleVerseRef {
	int book = -1;
	int chapter = 0; /* 1-based */
	int verse = 0;   /* 1-based */
};

class BibleDock : public OBSDock {
	Q_OBJECT

public:
	BibleDock(const QString &objectName, const QString &title, const QString &bundledBibleDir,
		  QWidget *parent = nullptr);

	/* Searches and projects in one step (used by the self test). */
	bool projectQuery(const QString &query, int layout);
	/* 0 full screen, 1 lower third (as chosen in the dock). */
	int layoutIndex() const;
	bool selectVersion(const QString &abbrev);
	QStringList versionAbbrevs() const;
	void setStyle(const QString &fontFamily, int preset);
	obs_scene_t *outputScene() const { return scene; }
	QString lastRenderedImage() const { return lastRenderPath; }

public slots:
	/* Remote control (phone, stage, hotkeys through WorshipLive). */
	void nextSlide() { moveSelection(1); }
	void previousSlide() { moveSelection(-1); }
	void toggleHidden();

protected:
	void resizeEvent(QResizeEvent *event) override;

private slots:
	void onSearchChanged(const QString &text);
	void onSelectionChanged();
	void onProjectClicked();
	void onClearClicked();
	void onVersionChanged(int index);
	void onImportVersionClicked();
	void onBackgroundActivated(int index);

private:
	QString bundledDir;
	QString versionName;
	QVector<BibleBook> books;
	QVector<BibleVerseRef> allVerses;
	QStringList normalizedVerses;

	QLineEdit *searchEdit = nullptr;
	QListWidget *resultsList = nullptr;
	QLabel *previewLabel = nullptr;
	QLabel *statusLabel = nullptr;
	QSpinBox *fontSizeSpin = nullptr;
	QComboBox *layoutCombo = nullptr;
	QComboBox *versionCombo = nullptr;
	QComboBox *fontCombo = nullptr;
	QComboBox *backgroundCombo = nullptr;

	OBSScene scene;
	QString outputSlot;
	OBSSource screenSource;
	int renderCounter = 0;
	QString lastRenderPath;
	bool live = false;
	bool searching = false;

	static QString userBibleDir();
	void rebuildVersionCombo(const QString &selectFile);
	void rebuildBackgroundCombo();
	WorshipRender::Background currentBackground() const;
	WorshipRender::BibleSlide slideFor(const QVector<BibleVerseRef> &verses) const;
	void updatePreview(const QVector<BibleVerseRef> &verses);
	void styleChanged();
	bool loadBible(const QString &path, QString *error);
	int findBook(const QString &query) const;
	bool parseReference(const QString &query, int *book, int *chapter, int *verseFrom, int *verseTo) const;
	void addVerseItem(const BibleVerseRef &ref);
	QVector<BibleVerseRef> selectedVerses() const;
	QString verseText(const BibleVerseRef &ref) const;
	QString referenceLabel(const QVector<BibleVerseRef> &verses) const;
	bool prepareOutput(bool fresh);
	/* `fresh`: a new passage (goes to the preview in Studio Mode). */
	bool project(const QVector<BibleVerseRef> &verses, bool fresh = false);
	void moveSelection(int delta);
};
