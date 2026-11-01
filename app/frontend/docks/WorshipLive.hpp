#pragma once

#include <QHash>
#include <QObject>
#include <QPointer>
#include <QString>

/* What is on screen right now, shared by every WorshipStudio dock so the
 * stage display, the phone remote and the service order all agree. */
struct LiveState {
	QString source;  /* "Cantos", "Biblia", "Himnario", "Presentaciones"... */
	QString title;   /* song title, reference, file name */
	QString current; /* text of the slide on screen (may be empty for images) */
	QString next;    /* text of the next slide */
	QString section; /* "Coro", "Estrofa 2"... */
	int index = -1;
	int count = 0;
	bool hidden = false;
};

class WorshipLive : public QObject {
	Q_OBJECT

public:
	static WorshipLive &instance();

	/* A dock that can be driven remotely. It must have the slots
	 * nextSlide(), previousSlide() and toggleHidden(). */
	void registerTarget(const QString &source, QObject *dock);

	/* Called by a dock whenever its live slide changes. The dock that
	 * publishes last becomes the one that next/previous control. */
	void publish(const LiveState &state);
	void clear(const QString &source);

	LiveState state() const { return current; }

	/* Operator message for the stage display ("2 minutos", "Alargar"). */
	void setStageMessage(const QString &message);
	QString stageMessage() const { return message; }

	/* Service order shown on the phone; the Orden del culto dock owns it. */
	void setService(const QStringList &items, int currentItem);
	QStringList serviceItems() const { return service; }
	int serviceCurrent() const { return serviceIndex; }
	void requestServiceItem(int index) { emit serviceItemRequested(index); }

	/* Content asked for from the phone; the docks show it. */
	void requestHymn(int number) { emit hymnRequested(number); }
	void requestBible(const QString &reference) { emit bibleRequested(reference); }
	void requestSong(const QString &id) { emit songRequested(id); }

public slots:
	void next();
	void previous();
	void toggleHidden();

signals:
	void changed();
	void stageMessageChanged();
	void serviceChanged();
	void serviceItemRequested(int index);
	void hymnRequested(int number);
	void bibleRequested(const QString &reference);
	void songRequested(const QString &id);

private:
	WorshipLive() = default;
	void invoke(const char *method);

	LiveState current;
	QString message;
	QStringList service;
	int serviceIndex = -1;
	QHash<QString, QPointer<QObject>> targets;
};
