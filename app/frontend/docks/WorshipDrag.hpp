#pragma once

#include <QString>

#include <functional>

class QAbstractItemView;
class QWidget;

/* Songs, verses, hymns and presentations dragged from their panels onto the
 * Sources panel become a source of the scene ("Cantos en vivo"...), so they
 * can be placed, resized and filtered like a camera or an image. */
namespace WorshipDrag {

constexpr const char *MimeType = "application/x-worshipstudio-panel";
constexpr const char *RowMimeType = "application/x-worshipstudio-row";

/* Rows of `view` can be dragged; the drop names `panelObjectName`. A plain
 * click selects its row when the button is released, so starting a drag
 * never selects (or shows on screen) anything; Shift and Ctrl clicks select
 * several rows as usual, and dragging one of them carries them all. */
void EnableDrag(QAbstractItemView *view, const QString &panelObjectName);

/* The Sources panel accepts those drops and calls `drop` with the panel and
 * a function that selects the dragged row in its panel (call it before
 * reading the panel's selection). */
using DropHandler = std::function<void(const QString &panelObjectName, const std::function<void()> &selectDragged)>;
void InstallDropTarget(QAbstractItemView *sourcesView, DropHandler drop);

} // namespace WorshipDrag
