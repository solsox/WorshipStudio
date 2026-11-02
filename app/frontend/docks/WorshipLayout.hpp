#pragma once

#include <QList>
#include <QObject>
#include <QPointer>

#include <obs-frontend-api.h>

#include <functional>

class QDockWidget;
class QMainWindow;
class QMenu;

/* The WorshipStudio window: preview in the center, scenes, sources, mixer,
 * transitions and controls where they always are, and one column on the
 * right with the content tabs and the live control. The scene list hides
 * WorshipStudio's own scenes, which the content tabs drive. */
class WorshipLayout : public QObject {
	Q_OBJECT

public:
	WorshipLayout(QMainWindow *window, QMenu *viewMenu, QDockWidget *liveTools, const QList<QDockWidget *> &content,
		      std::function<void()> resetDefaultDocks);
	~WorshipLayout() override;

	/* Arranges the docks the first time (or after a layout change). */
	void restore();
	void applyLayout();

private:
	QMainWindow *window;
	QPointer<QDockWidget> liveTools;
	QList<QPointer<QDockWidget>> content;
	std::function<void()> resetDefaultDocks;

	void filterSceneList();
	void filterSources();
	static void onFrontendEvent(enum obs_frontend_event event, void *data);
};
