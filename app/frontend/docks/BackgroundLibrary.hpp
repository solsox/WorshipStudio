#pragma once

#include <QIcon>
#include <QString>
#include <QStringList>
#include <QVector>

class QWidget;

/* "Mis fondos": backgrounds shipped with WorshipStudio plus every image or
 * video the user adds. Added files are copied into the user folder, so a
 * background keeps working after the original is moved or deleted. */
namespace BackgroundLibrary {
struct Item {
	QString path;
	QString name;
	bool video = false;
	bool bundled = false;
};

void SetBundledDir(const QString &directory);
QString UserDir();
QVector<Item> Items(bool includeVideos);
bool IsVideo(const QString &path);

/* Copies files into the library; returns the library paths (existing
 * copies are reused). */
QStringList Add(const QStringList &files);

/* Asks for images (and videos when allowed) and adds them. Returns the
 * library paths of the chosen files, empty when cancelled. */
QStringList AddWithDialog(QWidget *parent, bool allowVideos);

/* 48x27 thumbnail; videos get a play mark. */
QIcon Thumbnail(const QString &path);

QString ImageFilter();
QString VideoFilter();
} // namespace BackgroundLibrary
