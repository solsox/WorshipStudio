#include "WorshipModes.hpp"
#include "WorshipOutput.hpp"

#include <Idian/Utils.hpp>

#include <QAction>
#include <QActionGroup>
#include <QDockWidget>
#include <QHBoxLayout>
#include <QMainWindow>
#include <QMenu>
#include <QPushButton>
#include <QStatusBar>

#include "moc_WorshipModes.cpp"

namespace {
constexpr const char *ConfigMode = "Mode";
constexpr const char *ConfigLayoutVersion = "LayoutVersion";
/* Bump when the default arrangement changes so it is applied once. */
constexpr const char *LayoutVersion = "3";

/* OBS docks a volunteer does not need during the service. */
const char *const TechnicalDocks[] = {"sourcesDock", "mixerDock", "transitionsDock", "statsDock"};
} // namespace

WorshipModes::WorshipModes(QMainWindow *window_, QMenu *viewMenu, QStatusBar *statusBar, QDockWidget *liveTools_,
			   const QList<QDockWidget *> &content_, std::function<void()> resetObsDocks_)
	: QObject(window_),
	  window(window_),
	  liveTools(liveTools_),
	  resetObsDocks(std::move(resetObsDocks_))
{
	for (QDockWidget *dock : content_)
		content << dock;

	auto *group = new QActionGroup(this);
	cultoAction = new QAction(tr("Modo culto (sencillo)"), this);
	tecnicoAction = new QAction(tr("Modo tecnico (todo OBS)"), this);
	for (QAction *action : {cultoAction, tecnicoAction}) {
		action->setCheckable(true);
		group->addAction(action);
	}
	cultoAction->setShortcut(QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_1));
	tecnicoAction->setShortcut(QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_2));
	auto *resetAction = new QAction(tr("Restablecer diseño de WorshipStudio"), this);
	if (viewMenu) {
		QAction *first = viewMenu->actions().value(0);
		viewMenu->insertAction(first, cultoAction);
		viewMenu->insertAction(first, tecnicoAction);
		viewMenu->insertAction(first, resetAction);
		viewMenu->insertSeparator(first);
	}

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
		cultoButton->setToolTip(tr("Oculta fuentes, mezclador y transiciones (Ctrl+Shift+1)"));
		tecnicoButton->setToolTip(tr("Muestra todas las herramientas de OBS (Ctrl+Shift+2)"));
		statusBar->insertWidget(0, box);
		connect(cultoButton, &QPushButton::clicked, this, [this]() { setMode(Mode::Culto); });
		connect(tecnicoButton, &QPushButton::clicked, this, [this]() { setMode(Mode::Tecnico); });
	}

	connect(cultoAction, &QAction::triggered, this, [this]() { setMode(Mode::Culto); });
	connect(tecnicoAction, &QAction::triggered, this, [this]() { setMode(Mode::Tecnico); });
	connect(resetAction, &QAction::triggered, this, &WorshipModes::applyLayout);
}

void WorshipModes::restore()
{
	current = WorshipOutput::ConfigString(ConfigMode) == QStringLiteral("tecnico") ? Mode::Tecnico : Mode::Culto;
	if (WorshipOutput::ConfigString(ConfigLayoutVersion) != QString::fromLatin1(LayoutVersion)) {
		applyLayout();
		WorshipOutput::SetConfigString(ConfigLayoutVersion, QString::fromLatin1(LayoutVersion));
	} else {
		applyVisibility();
	}
	syncControls();
}

void WorshipModes::setMode(Mode mode)
{
	current = mode;
	WorshipOutput::SetConfigString(ConfigMode, mode == Mode::Culto ? QStringLiteral("culto") : QStringLiteral("tecnico"));
	applyVisibility();
	syncControls();
}

void WorshipModes::applyLayout()
{
	/* OBS puts its own docks back along the bottom. */
	if (resetObsDocks)
		resetObsDocks();

	/* Right column: content tabs on top, live control below. The column is
	 * split before tabbing; splitting a tabbed dock adds another tab. */
	QList<QDockWidget *> docks;
	for (const QPointer<QDockWidget> &dock : content) {
		if (dock) {
			dock->setFloating(false);
			docks << dock;
		}
	}
	QDockWidget *first = docks.value(0);
	if (first) {
		window->addDockWidget(Qt::RightDockWidgetArea, first);
		first->show();
	}
	if (liveTools) {
		liveTools->setFloating(false);
		window->addDockWidget(Qt::RightDockWidgetArea, liveTools);
		if (first)
			window->splitDockWidget(first, liveTools, Qt::Vertical);
		liveTools->show();
	}
	for (int i = 1; i < docks.size(); ++i) {
		window->tabifyDockWidget(first, docks.at(i));
		docks.at(i)->show();
	}
	if (first) {
		first->raise();
		window->resizeDocks({first}, {qMax(380, window->width() * 27 / 100)}, Qt::Horizontal);
		if (liveTools)
			window->resizeDocks({first, liveTools}, {window->height() * 55 / 100, window->height() * 45 / 100},
					    Qt::Vertical);
	}
	applyVisibility();
}

void WorshipModes::applyVisibility()
{
	const bool culto = current == Mode::Culto;
	for (const char *name : TechnicalDocks) {
		if (QDockWidget *dock = window->findChild<QDockWidget *>(QString::fromLatin1(name))) {
			/* Stats stays closed unless the user opens it. */
			if (culto)
				dock->hide();
			else if (qstrcmp(name, "statsDock") != 0)
				dock->show();
		}
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
