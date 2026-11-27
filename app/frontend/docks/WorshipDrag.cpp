/******************************************************************************
    Copyright (C) 2026 by WorshipStudio Contributors

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
******************************************************************************/

#include "WorshipDrag.hpp"

#include <QAbstractItemView>
#include <QApplication>
#include <QDrag>
#include <QDragEnterEvent>
#include <QFontMetrics>
#include <QItemSelectionModel>
#include <QMimeData>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QPointer>

#include <memory>

namespace {
/* A rounded label that follows the pointer while dragging. */
QPixmap dragLabel(const QString &text, const QFont &font)
{
	const QFontMetrics metrics(font);
	const QString shown = metrics.elidedText(text, Qt::ElideRight, 280);
	const QSize size(metrics.horizontalAdvance(shown) + 28, metrics.height() + 14);
	QPixmap pixmap(size * 2);
	pixmap.setDevicePixelRatio(2.0);
	pixmap.fill(Qt::transparent);
	QPainter painter(&pixmap);
	painter.setRenderHint(QPainter::Antialiasing);
	QPainterPath path;
	path.addRoundedRect(QRectF(0.5, 0.5, size.width() - 1, size.height() - 1), 8, 8);
	painter.fillPath(path, QColor(0xDC, 0xC5, 0x9C, 240));
	painter.setPen(QColor(0x1B, 0x19, 0x17));
	painter.setFont(font);
	painter.drawText(QRect(QPoint(0, 0), size), Qt::AlignCenter, shown);
	painter.end();
	return pixmap;
}

class DragStarter : public QObject {
public:
	DragStarter(QAbstractItemView *view_, const QString &panel_) : QObject(view_), view(view_), panel(panel_) {}

protected:
	bool eventFilter(QObject *, QEvent *event) override
	{
		switch (event->type()) {
		case QEvent::MouseButtonPress: {
			if (replaying)
				return false;
			auto *mouse = static_cast<QMouseEvent *>(event);
			pressed = view->indexAt(mouse->position().toPoint());
			const Qt::KeyboardModifiers several = Qt::ShiftModifier | Qt::ControlModifier | Qt::MetaModifier;
			armed = mouse->button() == Qt::LeftButton && pressed.isValid() && !(mouse->modifiers() & several);
			if (!armed)
				return false;
			/* The click waits for the release: a drag must not select the
			 * row first (a selected verse goes straight to the screen). */
			pressPos = mouse->position().toPoint();
			deferredPress.reset(mouse->clone());
			return true;
		}
		case QEvent::MouseButtonDblClick:
		case QEvent::MouseButtonRelease:
			if (armed) {
				armed = false;
				replayPress();
			}
			return false;
		case QEvent::MouseMove: {
			if (!armed)
				return false;
			auto *mouse = static_cast<QMouseEvent *>(event);
			if ((mouse->position().toPoint() - pressPos).manhattanLength() >= QApplication::startDragDistance()) {
				armed = false;
				deferredPress.reset();
				startDrag();
			}
			return true;
		}
		default:
			return false;
		}
	}

private:
	void replayPress()
	{
		if (!deferredPress)
			return;
		std::unique_ptr<QMouseEvent> press = std::move(deferredPress);
		replaying = true;
		QCoreApplication::sendEvent(view->viewport(), press.get());
		replaying = false;
	}

	void startDrag()
	{
		/* A row of the selection carries the whole selection (several
		 * verses); any other row is selected when it is dropped. */
		const bool selected = view->selectionModel() && view->selectionModel()->isSelected(pressed);
		auto *mime = new QMimeData();
		mime->setData(QString::fromLatin1(WorshipDrag::MimeType), panel.toUtf8());
		mime->setData(QString::fromLatin1(WorshipDrag::RowMimeType),
			      QByteArray::number(selected ? -1 : pressed.row()));
		auto *drag = new QDrag(view);
		drag->setMimeData(mime);
		const QPixmap label = dragLabel(pressed.data(Qt::DisplayRole).toString().section(QLatin1Char('\n'), 0, 0),
						view->font());
		drag->setPixmap(label);
		drag->setHotSpot(QPoint(12, (int)(label.height() / label.devicePixelRatio() / 2)));
		drag->exec(Qt::CopyAction);
	}

	QAbstractItemView *view;
	QString panel;
	bool armed = false;
	bool replaying = false;
	std::unique_ptr<QMouseEvent> deferredPress;
	QPersistentModelIndex pressed;
	QPoint pressPos;
};

class DropTarget : public QObject {
public:
	DropTarget(QObject *parent, WorshipDrag::DropHandler drop_) : QObject(parent), drop(std::move(drop_)) {}

protected:
	bool eventFilter(QObject *, QEvent *event) override
	{
		const QString type = QString::fromLatin1(WorshipDrag::MimeType);
		switch (event->type()) {
		case QEvent::DragEnter:
		case QEvent::DragMove: {
			auto *dragEvent = static_cast<QDragMoveEvent *>(event);
			if (!dragEvent->mimeData()->hasFormat(type))
				return false;
			dragEvent->setDropAction(Qt::CopyAction);
			dragEvent->accept();
			return true;
		}
		case QEvent::Drop: {
			auto *dropEvent = static_cast<QDropEvent *>(event);
			if (!dropEvent->mimeData()->hasFormat(type))
				return false;
			dropEvent->setDropAction(Qt::CopyAction);
			dropEvent->accept();
			const QString panel = QString::fromUtf8(dropEvent->mimeData()->data(type));
			bool ok = false;
			const int row = dropEvent->mimeData()
						->data(QString::fromLatin1(WorshipDrag::RowMimeType))
						.toInt(&ok);
			QPointer<QAbstractItemView> view = qobject_cast<QAbstractItemView *>(dropEvent->source());
			const std::function<void()> selectDragged = [view, row, ok]() {
				if (!ok || row < 0 || !view || !view->model())
					return;
				const QModelIndex index = view->model()->index(row, 0, view->rootIndex());
				if (!index.isValid())
					return;
				view->setCurrentIndex(index);
				view->scrollTo(index);
			};
			/* After the drag has finished, so dialogs or focus changes are safe. */
			QMetaObject::invokeMethod(
				this, [this, panel, selectDragged]() { drop(panel, selectDragged); }, Qt::QueuedConnection);
			return true;
		}
		default:
			return false;
		}
	}

private:
	WorshipDrag::DropHandler drop;
};
} // namespace

namespace WorshipDrag {

void EnableDrag(QAbstractItemView *view, const QString &panelObjectName)
{
	if (!view)
		return;
	view->viewport()->installEventFilter(new DragStarter(view, panelObjectName));
}

void InstallDropTarget(QAbstractItemView *sourcesView, DropHandler drop)
{
	if (!sourcesView)
		return;
	sourcesView->setAcceptDrops(true);
	sourcesView->viewport()->setAcceptDrops(true);
	sourcesView->viewport()->installEventFilter(new DropTarget(sourcesView, std::move(drop)));
}

} // namespace WorshipDrag
