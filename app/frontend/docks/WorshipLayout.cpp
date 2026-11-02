#include "WorshipLayout.hpp"
#include "WorshipOutput.hpp"

#include <QAction>
#include <QDockWidget>
#include <QAbstractItemModel>
#include <QListView>
#include <QListWidget>
#include <QMainWindow>
#include <QMenu>

#include "moc_WorshipLayout.cpp"

namespace {
constexpr const char *ConfigLayoutVersion = "LayoutVersion";
/* Bump when the default arrangement changes so it is applied once. */
constexpr const char *LayoutVersion = "4";
} // namespace

WorshipLayout::WorshipLayout(QMainWindow *window_, QMenu *viewMenu, QDockWidget *liveTools_,
			     const QList<QDockWidget *> &content_, std::function<void()> resetDefaultDocks_)
	: QObject(window_),
	  window(window_),
	  liveTools(liveTools_),
	  resetDefaultDocks(std::move(resetDefaultDocks_))
{
	for (QDockWidget *dock : content_)
		content << dock;

	if (viewMenu) {
		auto *resetAction = new QAction(tr("Restablecer diseño de WorshipStudio"), this);
		viewMenu->insertAction(viewMenu->actions().value(0), resetAction);
		connect(resetAction, &QAction::triggered, this, &WorshipLayout::applyLayout);
	}

	obs_frontend_add_event_callback(&WorshipLayout::onFrontendEvent, this);

	/* WorshipStudio's helper sources (overlays nested in the user's scenes)
	 * stay out of the sources list too. */
	if (auto *sources = window->findChild<QListView *>(QStringLiteral("sources")); sources && sources->model()) {
		QAbstractItemModel *model = sources->model();
		auto refilter = [this]() {
			QMetaObject::invokeMethod(this, &WorshipLayout::filterSources, Qt::QueuedConnection);
		};
		connect(model, &QAbstractItemModel::rowsInserted, this, refilter);
		connect(model, &QAbstractItemModel::modelReset, this, refilter);
		connect(model, &QAbstractItemModel::layoutChanged, this, refilter);
		connect(model, &QAbstractItemModel::rowsMoved, this, refilter);
	}
}

WorshipLayout::~WorshipLayout()
{
	obs_frontend_remove_event_callback(&WorshipLayout::onFrontendEvent, this);
}

void WorshipLayout::onFrontendEvent(enum obs_frontend_event event, void *data)
{
	if (event != OBS_FRONTEND_EVENT_SCENE_LIST_CHANGED && event != OBS_FRONTEND_EVENT_FINISHED_LOADING)
		return;
	auto *layout = static_cast<WorshipLayout *>(data);
	QMetaObject::invokeMethod(layout, [layout]() { layout->filterSceneList(); }, Qt::QueuedConnection);
}

/* WorshipStudio's own scenes ("WS Cantos", "WS Cantos B"...) are driven by
 * the content tabs; the scene list shows only the user's scenes. */
void WorshipLayout::filterSceneList()
{
	auto *list = window->findChild<QListWidget *>(QStringLiteral("scenes"));
	if (!list)
		return;
	for (int row = 0; row < list->count(); ++row) {
		QListWidgetItem *item = list->item(row);
		item->setHidden(item->text().startsWith(QStringLiteral("WS ")));
	}
}

void WorshipLayout::filterSources()
{
	auto *sources = window->findChild<QListView *>(QStringLiteral("sources"));
	if (!sources || !sources->model())
		return;
	QAbstractItemModel *model = sources->model();
	for (int row = 0; row < model->rowCount(); ++row) {
		const QString name = model->data(model->index(row, 0), Qt::AccessibleTextRole).toString();
		sources->setRowHidden(row, name.startsWith(QStringLiteral("WS ")));
	}
}

void WorshipLayout::restore()
{
	if (WorshipOutput::ConfigString(ConfigLayoutVersion) != QString::fromLatin1(LayoutVersion)) {
		applyLayout();
		WorshipOutput::SetConfigString(ConfigLayoutVersion, QString::fromLatin1(LayoutVersion));
	}
	filterSceneList();
	filterSources();
}

void WorshipLayout::applyLayout()
{
	/* Scenes, sources, mixer, transitions and controls back in their place. */
	if (resetDefaultDocks)
		resetDefaultDocks();

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
	filterSceneList();
}
