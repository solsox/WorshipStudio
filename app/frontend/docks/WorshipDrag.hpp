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

/* Rows of `view` can be dragged; the drop names `panelObjectName`. With
 * `onlySelected`, only rows that were already selected start a drag (lists
 * where dragging the mouse selects several rows). */
void EnableDrag(QAbstractItemView *view, const QString &panelObjectName, bool onlySelected = false);

/* The Sources panel accepts those drops and calls `drop` with the panel. */
void InstallDropTarget(QAbstractItemView *sourcesView, std::function<void(const QString &panelObjectName)> drop);

} // namespace WorshipDrag
