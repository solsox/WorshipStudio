#pragma once

#include <QList>
#include <QObject>
#include <QPointer>

#include <functional>

class QAction;
class QDockWidget;
class QMainWindow;
class QMenu;
class QPushButton;
class QStatusBar;

/* The WorshipStudio layout keeps OBS where OBS users expect it (preview in
 * the center, scenes, sources, mixer, transitions and controls along the
 * bottom) and adds one column on the right with the content tabs and the
 * live control. Modo culto only hides the technical docks; nothing moves. */
class WorshipModes : public QObject {
	Q_OBJECT

public:
	enum class Mode { Culto, Tecnico };

	WorshipModes(QMainWindow *window, QMenu *viewMenu, QStatusBar *statusBar, QDockWidget *liveTools,
		     const QList<QDockWidget *> &content, std::function<void()> resetObsDocks);

	Mode mode() const { return current; }
	void setMode(Mode mode);
	/* Applies the saved mode; the first time also arranges the docks. */
	void restore();
	/* OBS default docks plus the WorshipStudio column. */
	void applyLayout();

private:
	QMainWindow *window;
	QPointer<QDockWidget> liveTools;
	QList<QPointer<QDockWidget>> content;
	std::function<void()> resetObsDocks;
	Mode current = Mode::Culto;

	QAction *cultoAction = nullptr;
	QAction *tecnicoAction = nullptr;
	QPushButton *cultoButton = nullptr;
	QPushButton *tecnicoButton = nullptr;

	void applyVisibility();
	void syncControls();
};
