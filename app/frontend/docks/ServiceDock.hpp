#pragma once

#include "OBSDock.hpp"

#include <QJsonObject>
#include <QPointer>
#include <QVector>

class BibleDock;
class LiveToolsDock;
class PresentationCatalogDock;
class PresentationDock;
class QLabel;
class QListWidget;
class QPushButton;
class SongsDock;

/* One step of the service. `value` holds the reference, song id, file,
 * scene name or text depending on the kind. */
struct ServiceItem {
	enum Kind { Hymn, Song, Bible, Presentation, Countdown, Name, Announcement, Scene };

	Kind kind = Hymn;
	QString title;
	QString value;
	QString extra;
	int number = 0;

	QJsonObject toJson() const;
	static ServiceItem fromJson(const QJsonObject &object);
	QString kindName() const;
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
	QPushButton *goButton = nullptr;
	QPushButton *nextButton = nullptr;

	void rebuild();
	void changed();
	void addItem(ServiceItem::Kind kind);
	void syncFromList();
	void save(const QString &path);
	bool load(const QString &path);
	static QString autosavePath();
};
