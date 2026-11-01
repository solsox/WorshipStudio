#pragma once

#include <QColor>
#include <QImage>
#include <QSize>
#include <QString>
#include <QStringList>
#include <QVector>

/* Renders WorshipStudio content into canvas-sized images that are shown
 * through regular OBS image sources. */
namespace WorshipRender {

enum class SlideFit { BlurredFill = 0, Stretch = 1, Letterbox = 2 };

/* Composes a hymn slide onto a canvas-sized image and returns the path of
 * the cached file (rendered once per slide, fit mode and canvas size). */
QString HymnSlide(const QString &slidePath, SlideFit fit, const QSize &canvas);

enum class BibleLayout { FullScreen = 0, LowerThird = 1 };

struct BibleVerseLine {
	int number = 0;
	QString text;
};

enum class BackgroundKind { Preset = 0, Color = 1, Image = 2, Video = 3 };

struct Background {
	BackgroundKind kind = BackgroundKind::Preset;
	int preset = 0;
	QString path;
	QString color;
};

/* Built-in gradient backgrounds, all with enough contrast for white text. */
QStringList BackgroundPresetNames();

/* Recommended fonts for projection that are installed on this system. */
QStringList RecommendedFonts();

struct BibleSlide {
	QVector<BibleVerseLine> verses;
	bool showVerseNumbers = false;
	QString reference;
	QString version;
	BibleLayout layout = BibleLayout::FullScreen;
	int maxFontSize = 72;
	QString fontFamily;
	Background background;
};

/* Draws the verses centered and auto-fitted. `fits` is false when the text
 * needed to be shrunk below the readable minimum. */
QImage Bible(const BibleSlide &slide, const QSize &canvas, bool *fits = nullptr);

/* Directory used for rendered images. */
QString CacheDir(const QString &subdir);

} // namespace WorshipRender
