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

#include "BackgroundLibrary.hpp"

#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QHash>
#include <QImageReader>
#include <QPainter>
#include <QPainterPath>
#include <QStandardPaths>

namespace {
QString bundledDir;
QHash<QString, QIcon> thumbnailCache;

const QStringList ImageSuffixes = {QStringLiteral("png"),  QStringLiteral("jpg"), QStringLiteral("jpeg"),
				   QStringLiteral("webp"), QStringLiteral("bmp"), QStringLiteral("gif"),
				   QStringLiteral("tif"),  QStringLiteral("tiff"), QStringLiteral("heic")};
const QStringList VideoSuffixes = {QStringLiteral("mp4"), QStringLiteral("mov"), QStringLiteral("m4v"),
				   QStringLiteral("mkv"), QStringLiteral("webm")};

QString displayName(const QFileInfo &info)
{
	QString name = info.completeBaseName();
	name.replace(QLatin1Char('-'), QLatin1Char(' ')).replace(QLatin1Char('_'), QLatin1Char(' '));
	return name.left(1).toUpper() + name.mid(1);
}

void collect(const QString &directory, bool bundled, bool includeVideos, QVector<BackgroundLibrary::Item> &items)
{
	if (directory.isEmpty())
		return;
	const QFileInfoList files = QDir(directory).entryInfoList(QDir::Files, QDir::Name | QDir::IgnoreCase);
	for (const QFileInfo &info : files) {
		const QString suffix = info.suffix().toLower();
		const bool video = VideoSuffixes.contains(suffix);
		if (!video && !ImageSuffixes.contains(suffix))
			continue;
		if (video && !includeVideos)
			continue;
		items.push_back({info.absoluteFilePath(), displayName(info), video, bundled});
	}
}

/* Same content -> same file, so adding twice does not duplicate. */
QString fileHash(const QString &path)
{
	QFile file(path);
	if (!file.open(QIODevice::ReadOnly))
		return {};
	QCryptographicHash hash(QCryptographicHash::Sha1);
	hash.addData(file.read(4 * 1024 * 1024));
	hash.addData(QByteArray::number(file.size()));
	return QString::fromLatin1(hash.result().toHex().left(8));
}
} // namespace

void BackgroundLibrary::SetBundledDir(const QString &directory)
{
	bundledDir = directory;
}

QString BackgroundLibrary::UserDir()
{
	const QString base = QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation);
	const QString path = QDir(base).absoluteFilePath(QStringLiteral("WorshipStudio/fondos"));
	QDir().mkpath(path);
	return path;
}

QVector<BackgroundLibrary::Item> BackgroundLibrary::Items(bool includeVideos)
{
	QVector<Item> items;
	collect(UserDir(), false, includeVideos, items);
	collect(bundledDir, true, includeVideos, items);
	return items;
}

bool BackgroundLibrary::IsVideo(const QString &path)
{
	return VideoSuffixes.contains(QFileInfo(path).suffix().toLower());
}

QStringList BackgroundLibrary::Add(const QStringList &files)
{
	QStringList added;
	const QDir dir(UserDir());
	for (const QString &file : files) {
		const QFileInfo info(file);
		if (!info.isFile())
			continue;
		/* Already in the library (or a bundled one): use it as is. */
		if (info.absolutePath() == dir.absolutePath() ||
		    (!bundledDir.isEmpty() && info.absolutePath() == QDir(bundledDir).absolutePath())) {
			added << info.absoluteFilePath();
			continue;
		}
		const QString target = dir.absoluteFilePath(
			QStringLiteral("%1-%2.%3").arg(info.completeBaseName(), fileHash(file), info.suffix().toLower()));
		if (QFileInfo::exists(target) || QFile::copy(file, target))
			added << target;
	}
	return added;
}

QString BackgroundLibrary::ImageFilter()
{
	QStringList patterns;
	for (const QString &suffix : ImageSuffixes)
		patterns << QStringLiteral("*.") + suffix;
	return QFileDialog::tr("Imágenes (%1)").arg(patterns.join(QLatin1Char(' ')));
}

QString BackgroundLibrary::VideoFilter()
{
	QStringList patterns;
	for (const QString &suffix : VideoSuffixes)
		patterns << QStringLiteral("*.") + suffix;
	return QFileDialog::tr("Videos (%1)").arg(patterns.join(QLatin1Char(' ')));
}

QStringList BackgroundLibrary::AddWithDialog(QWidget *parent, bool allowVideos)
{
	QString filter = ImageFilter();
	if (allowVideos) {
		QStringList all;
		for (const QString &suffix : ImageSuffixes + VideoSuffixes)
			all << QStringLiteral("*.") + suffix;
		filter = QFileDialog::tr("Imágenes y videos (%1)").arg(all.join(QLatin1Char(' '))) +
			 QStringLiteral(";;") + ImageFilter() + QStringLiteral(";;") + VideoFilter();
	}
	const QStringList files = QFileDialog::getOpenFileNames(
		parent, QFileDialog::tr("Agregar fondos (podés elegir varios)"), QDir::homePath(), filter);
	return Add(files);
}

QIcon BackgroundLibrary::Thumbnail(const QString &path)
{
	const auto cached = thumbnailCache.constFind(path);
	if (cached != thumbnailCache.constEnd())
		return cached.value();

	QPixmap pixmap(96, 54);
	pixmap.fill(QColor(0x22, 0x22, 0x2a));
	QPainter painter(&pixmap);
	painter.setRenderHint(QPainter::Antialiasing);
	/* Bundled videos have a still in "miniaturas"; others get a play mark. */
	const QString still = QFileInfo(path).dir().absoluteFilePath(QStringLiteral("miniaturas/%1.jpg").arg(QFileInfo(path).completeBaseName()));
	if (IsVideo(path) && QFileInfo::exists(still)) {
		const QImage image = QImage(still).scaled(96, 54, Qt::KeepAspectRatioByExpanding, Qt::SmoothTransformation);
		painter.drawImage(QPoint((96 - image.width()) / 2, (54 - image.height()) / 2), image);
		QPainterPath play;
		play.moveTo(80, 42);
		play.lineTo(90, 47);
		play.lineTo(80, 52);
		play.closeSubpath();
		painter.fillPath(play, QColor(255, 255, 255, 230));
	} else if (IsVideo(path)) {
		QPainterPath play;
		play.moveTo(40, 15);
		play.lineTo(62, 27);
		play.lineTo(40, 39);
		play.closeSubpath();
		painter.fillPath(play, QColor(255, 255, 255, 220));
	} else {
		QImageReader reader(path);
		reader.setAutoTransform(true);
		const QSize size = reader.size();
		if (size.isValid())
			reader.setScaledSize(size.scaled(96, 54, Qt::KeepAspectRatioByExpanding));
		const QImage image = reader.read();
		if (!image.isNull())
			painter.drawImage(QPoint((96 - image.width()) / 2, (54 - image.height()) / 2), image);
	}
	painter.end();

	const QIcon icon(pixmap);
	thumbnailCache.insert(path, icon);
	return icon;
}
