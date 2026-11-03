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
	DragStarter(QAbstractItemView *view_, const QString &panel_, bool onlySelected_)
		: QObject(view_),
		  view(view_),
		  panel(panel_),
		  onlySelected(onlySelected_)
	{
	}

protected:
	bool eventFilter(QObject *, QEvent *event) override
	{
		if (event->type() == QEvent::MouseButtonPress) {
			auto *mouse = static_cast<QMouseEvent *>(event);
			pressed = view->indexAt(mouse->position().toPoint());
			pressPos = mouse->position().toPoint();
			/* Checked before the view changes the selection. */
			wasSelected = pressed.isValid() && view->selectionModel() &&
				      view->selectionModel()->isSelected(pressed);
			armed = mouse->button() == Qt::LeftButton && pressed.isValid() &&
				(!onlySelected || wasSelected);
			return false;
		}
		if (event->type() == QEvent::MouseButtonRelease) {
			armed = false;
			return false;
		}
		if (event->type() != QEvent::MouseMove || !armed)
			return false;
		auto *mouse = static_cast<QMouseEvent *>(event);
		if ((mouse->position().toPoint() - pressPos).manhattanLength() < QApplication::startDragDistance() * 2)
			return onlySelected;
		armed = false;

		auto *mime = new QMimeData();
		mime->setData(QString::fromLatin1(WorshipDrag::MimeType), panel.toUtf8());
		auto *drag = new QDrag(view);
		drag->setMimeData(mime);
		const QPixmap label = dragLabel(pressed.data(Qt::DisplayRole).toString().section(QLatin1Char('\n'), 0, 0),
						view->font());
		drag->setPixmap(label);
		drag->setHotSpot(QPoint(12, (int)(label.height() / label.devicePixelRatio() / 2)));
		drag->exec(Qt::CopyAction);
		return true;
	}

private:
	QAbstractItemView *view;
	QString panel;
	bool onlySelected;
	bool armed = false;
	bool wasSelected = false;
	QPersistentModelIndex pressed;
	QPoint pressPos;
};

class DropTarget : public QObject {
public:
	DropTarget(QObject *parent, std::function<void(const QString &)> drop_) : QObject(parent), drop(std::move(drop_))
	{
	}

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
			/* After the drag has finished, so dialogs or focus changes are safe. */
			QMetaObject::invokeMethod(this, [this, panel]() { drop(panel); }, Qt::QueuedConnection);
			return true;
		}
		default:
			return false;
		}
	}

private:
	std::function<void(const QString &)> drop;
};
} // namespace

namespace WorshipDrag {

void EnableDrag(QAbstractItemView *view, const QString &panelObjectName, bool onlySelected)
{
	if (!view)
		return;
	view->viewport()->installEventFilter(new DragStarter(view, panelObjectName, onlySelected));
}

void InstallDropTarget(QAbstractItemView *sourcesView, std::function<void(const QString &)> drop)
{
	if (!sourcesView)
		return;
	sourcesView->setAcceptDrops(true);
	sourcesView->viewport()->setAcceptDrops(true);
	sourcesView->viewport()->installEventFilter(new DropTarget(sourcesView, std::move(drop)));
}

} // namespace WorshipDrag
