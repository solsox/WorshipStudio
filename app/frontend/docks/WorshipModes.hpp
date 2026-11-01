#pragma once

#include <QList>
#include <QObject>
#include <QPointer>

class QAction;
class QDockWidget;
class QMainWindow;
class QMenu;
class QPushButton;
class QStatusBar;

/* Modo Culto shows only what a volunteer needs during the service: the
 * service order, the content docks and the live tools next to the
 * preview. Modo Tecnico restores the full OBS layout as the user left it. */
class WorshipModes : public QObject {
	Q_OBJECT

public:
	enum class Mode { Culto, Tecnico };

	WorshipModes(QMainWindow *window, QMenu *viewMenu, QStatusBar *statusBar, QDockWidget *service,
		     QDockWidget *liveTools, const QList<QDockWidget *> &content, QDockWidget *controls);

	Mode mode() const { return current; }
	void setMode(Mode mode);
	/* Applies the saved mode at startup (Culto on the first run). */
	void restore();

private:
	QMainWindow *window;
	QPointer<QDockWidget> service;
	QPointer<QDockWidget> liveTools;
	QList<QPointer<QDockWidget>> content;
	QPointer<QDockWidget> controls;
	Mode current = Mode::Tecnico;

	QAction *cultoAction = nullptr;
	QAction *tecnicoAction = nullptr;
	QPushButton *cultoButton = nullptr;
	QPushButton *tecnicoButton = nullptr;

	bool isWorshipDock(QDockWidget *dock) const;
	void applyCulto();
	void applyTecnico();
	void syncControls();
};
