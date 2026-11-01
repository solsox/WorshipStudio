#include "WorshipModes.hpp"
#include "WorshipOutput.hpp"

#include <Idian/Utils.hpp>

#include <QAction>
#include <QActionGroup>
#include <QDockWidget>
#include <QHBoxLayout>
#include <QLabel>
#include <QMainWindow>
#include <QMenu>
#include <QPushButton>
#include <QStatusBar>
#include <QTimer>

#include "moc_WorshipModes.cpp"

namespace {
constexpr const char *ConfigMode = "Mode";
constexpr const char *ConfigTecnicoState = "TecnicoDockState";
} // namespace

WorshipModes::WorshipModes(QMainWindow *window_, QMenu *viewMenu, QStatusBar *statusBar, QDockWidget *service_,
			   QDockWidget *liveTools_, const QList<QDockWidget *> &content_, QDockWidget *controls_)
	: QObject(window_),
	  window(window_),
	  service(service_),
	  liveTools(liveTools_),
	  controls(controls_)
{
	for (QDockWidget *dock : content_)
		content << dock;

	/* View menu entries. */
	auto *group = new QActionGroup(this);
	cultoAction = new QAction(tr("Modo culto (sencillo)"), this);
	tecnicoAction = new QAction(tr("Modo tecnico (completo)"), this);
	for (QAction *action : {cultoAction, tecnicoAction}) {
		action->setCheckable(true);
		group->addAction(action);
	}
	cultoAction->setShortcut(QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_1));
	tecnicoAction->setShortcut(QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_2));
	if (viewMenu) {
		QAction *first = viewMenu->actions().value(0);
		viewMenu->insertAction(first, cultoAction);
		viewMenu->insertAction(first, tecnicoAction);
		viewMenu->insertSeparator(first);
	}

	/* Always visible switch in the status bar. */
	if (statusBar) {
		auto *box = new QWidget(statusBar);
		auto *layout = new QHBoxLayout(box);
		layout->setContentsMargins(6, 0, 6, 0);
		layout->setSpacing(4);
		cultoButton = new QPushButton(tr("Modo culto"), box);
		tecnicoButton = new QPushButton(tr("Modo tecnico"), box);
		for (QPushButton *button : {cultoButton, tecnicoButton}) {
			button->setCheckable(true);
			button->setAutoExclusive(true);
			button->setFocusPolicy(Qt::NoFocus);
			layout->addWidget(button);
		}
		cultoButton->setToolTip(tr("Solo lo necesario para el culto (Ctrl+Shift+1)"));
		tecnicoButton->setToolTip(tr("Todas las herramientas de OBS: escenas, fuentes, audio (Ctrl+Shift+2)"));
		statusBar->insertWidget(0, box);
		connect(cultoButton, &QPushButton::clicked, this, [this]() { setMode(Mode::Culto); });
		connect(tecnicoButton, &QPushButton::clicked, this, [this]() { setMode(Mode::Tecnico); });
	}

	connect(cultoAction, &QAction::triggered, this, [this]() { setMode(Mode::Culto); });
	connect(tecnicoAction, &QAction::triggered, this, [this]() { setMode(Mode::Tecnico); });
}

bool WorshipModes::isWorshipDock(QDockWidget *dock) const
{
	if (dock == service || dock == liveTools)
		return true;
	for (const QPointer<QDockWidget> &item : content) {
		if (item == dock)
			return true;
	}
	return false;
}

void WorshipModes::restore()
{
	const QString saved = WorshipOutput::ConfigString(ConfigMode);
	current = saved == QStringLiteral("tecnico") ? Mode::Tecnico : Mode::Culto;
	if (current == Mode::Culto) {
		/* Keep the technical layout to come back to it later. */
		if (WorshipOutput::ConfigString(ConfigTecnicoState).isEmpty())
			WorshipOutput::SetConfigString(ConfigTecnicoState, QString::fromLatin1(window->saveState().toBase64()));
		applyCulto();
	}
	syncControls();
}

void WorshipModes::setMode(Mode mode)
{
	if (mode == current) {
		syncControls();
		return;
	}
	if (mode == Mode::Culto) {
		WorshipOutput::SetConfigString(ConfigTecnicoState, QString::fromLatin1(window->saveState().toBase64()));
		applyCulto();
	} else {
		applyTecnico();
	}
	current = mode;
	WorshipOutput::SetConfigString(ConfigMode, mode == Mode::Culto ? QStringLiteral("culto") : QStringLiteral("tecnico"));
	syncControls();
}

void WorshipModes::applyCulto()
{
	/* Hide every OBS dock; the preview and program stay in the center. */
	for (QDockWidget *dock : window->findChildren<QDockWidget *>()) {
		if (!isWorshipDock(dock) && dock != controls)
			dock->hide();
	}

	/* Left: service order with the live tools under it.
	 * Right: the content docks as tabs, controls under them. */
	if (service) {
		service->setFloating(false);
		window->addDockWidget(Qt::LeftDockWidgetArea, service);
		service->show();
	}
	if (liveTools) {
		liveTools->setFloating(false);
		window->addDockWidget(Qt::LeftDockWidgetArea, liveTools);
		window->splitDockWidget(service, liveTools, Qt::Vertical);
		liveTools->show();
	}

	QDockWidget *first = nullptr;
	for (const QPointer<QDockWidget> &dock : content) {
		if (!dock)
			continue;
		dock->setFloating(false);
		window->addDockWidget(Qt::RightDockWidgetArea, dock);
		dock->show();
		if (first)
			window->tabifyDockWidget(first, dock);
		else
			first = dock;
	}
	if (controls) {
		controls->setFloating(false);
		window->addDockWidget(Qt::RightDockWidgetArea, controls);
		if (first)
			window->splitDockWidget(first, controls, Qt::Vertical);
		controls->show();
	}
	if (first)
		first->raise();

	const int side = qMax(300, window->width() * 24 / 100);
	QList<QDockWidget *> sized;
	QList<int> widths;
	if (service) {
		sized << service;
		widths << side;
	}
	if (first) {
		sized << first;
		widths << qMax(340, window->width() * 30 / 100);
	}
	window->resizeDocks(sized, widths, Qt::Horizontal);
	if (first && controls)
		window->resizeDocks({first, controls}, {window->height() * 3, window->height()}, Qt::Vertical);
	if (service && liveTools)
		window->resizeDocks({service, liveTools}, {window->height(), window->height()}, Qt::Vertical);
}

void WorshipModes::applyTecnico()
{
	const QByteArray state = QByteArray::fromBase64(WorshipOutput::ConfigString(ConfigTecnicoState).toLatin1());
	if (state.isEmpty() || !window->restoreState(state)) {
		/* No saved layout: show everything. */
		for (QDockWidget *dock : window->findChildren<QDockWidget *>())
			dock->show();
	}
	/* Docks that did not exist when the layout was saved stay reachable. */
	for (QDockWidget *dock : {service.data(), liveTools.data()}) {
		if (dock && dock->isHidden())
			dock->show();
	}
}

void WorshipModes::syncControls()
{
	const bool culto = current == Mode::Culto;
	cultoAction->setChecked(culto);
	tecnicoAction->setChecked(!culto);
	if (cultoButton) {
		cultoButton->setChecked(culto);
		tecnicoButton->setChecked(!culto);
		idian::Utils::toggleClass(cultoButton, "button-primary", culto);
		idian::Utils::toggleClass(tecnicoButton, "button-primary", !culto);
	}
}
