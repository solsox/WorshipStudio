#pragma once

#include "OBSDock.hpp"

#include <QDateTime>
#include <QJsonObject>
#include <QPointer>
#include <QVector>

#include <functional>

class BibleDock;
class LiveToolsDock;
class PresentationCatalogDock;
class PresentationDock;
class QLabel;
class QListWidget;
class QPushButton;
class QTimer;
class SongsDock;

/* One step of the service. `value` holds the reference, song id, file,
 * scene name or text depending on the kind. Kinds are saved as numbers:
 * new ones go at the end so older files keep opening. */
struct ServiceItem {
	enum Kind { Hymn, Song, Bible, Presentation, Countdown, Name, Announcement, Scene, Video, Sermon, Prayer, Custom };

	Kind kind = Hymn;
	QString title;
	QString value;
	QString extra;
	int number = 0;
	/* Planning: expected length, who is in charge and notes for the team. */
	int minutes = 0;
	QString person;
	QString notes;

	QJsonObject toJson() const;
	static ServiceItem fromJson(const QJsonObject &object);
	QString kindName() const;
	static int defaultMinutes(Kind kind);
};

/* The order of the service, prepared in advance and run with one button. */
class ServiceDock : public OBSDock {
	Q_OBJECT

public:
	ServiceDock(const QString &objectName, const QString &title, PresentationCatalogDock *hymnal, SongsDock *songs,
		    BibleDock *bible, PresentationDock *presentations, LiveToolsDock *liveTools,
		    QWidget *parent = nullptr);

	/* Used by the self test. */
	void setItems(const QVector<ServiceItem> &items);
	int itemCount() const { return (int)items.size(); }
	int currentItem() const { return current; }

public slots:
	bool activate(int index);
	bool activateNext();

private:
	QPointer<PresentationCatalogDock> hymnal;
	QPointer<SongsDock> songs;
	QPointer<BibleDock> bible;
	QPointer<PresentationDock> presentations;
	QPointer<LiveToolsDock> liveTools;

	QVector<ServiceItem> items;
	int current = -1;
	QString filePath;

	QListWidget *list = nullptr;
	QLabel *nameLabel = nullptr;
	QLabel *timeLabel = nullptr;
	QPushButton *goButton = nullptr;
	QPushButton *nextButton = nullptr;
	QTimer *clock = nullptr;
	/* When the current item went on screen, for the time left. */
	QDateTime currentStarted;

	void rebuild();
	void changed();
	void updateTimes();
	void addItem(ServiceItem::Kind kind);
	void insertItem(const ServiceItem &item);
	/* A song, verse, hymn or presentation dropped from its panel. */
	void addFromPanel(const QString &panel, const std::function<void()> &selectDragged);
	bool editDetails(int row);
	void duplicateItem(int row);
	void showItemMenu(const QPoint &pos);
	void syncFromList();
	void save(const QString &path);
	bool load(const QString &path);
	bool saveAsTemplate();
	void startFromTemplate(const QString &path);
	static QString autosavePath();
	static QString templatesDir();
	static QVector<ServiceItem> sundayTemplate();
};
