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

/* Loads the fonts shipped in data/worship/fuentes so they can be used
 * without installing them. Call once at startup. */
void RegisterBundledFonts(const QString &directory);

/* Recommended fonts for projection: shipped ones first, then installed ones. */
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

enum class LyricsLayout { FullScreen = 0, LowerThird = 1 };

struct LyricsSlide {
	QString text;
	QString footer;
	QString fontFamily;
	QString textColor;
	int maxFontSize = 84;
	bool uppercase = false;
	LyricsLayout layout = LyricsLayout::FullScreen;
	Background background;
};

/* Draws song lyrics centered and auto-fitted over the background (or in a
 * band when shown as lower third over a camera). */
QImage Lyrics(const LyricsSlide &slide, const QSize &canvas);
/* Same, for thumbnails and previews: a video background is shown with its
 * still (the real video only plays on screen, under the lyrics). */
QImage LyricsPreview(const LyricsSlide &slide, const QSize &canvas);

/* Full screen countdown: "El culto comienza en" + 04:59 over a background. */
QImage Countdown(const QString &title, const QString &time, const QString &footer, const Background &background,
		 const QString &fontFamily, const QSize &canvas);

/* Transparent overlays shown on top of whatever is on air. */
QImage NameLowerThird(const QString &name, const QString &role, const QString &fontFamily, const QSize &canvas);
QImage UrgentMessage(const QString &text, const QString &fontFamily, const QSize &canvas);

/* Confidence monitor for singers and preachers. */
struct StageInfo {
	QString clock;
	QString title;
	QString section;
	QString current;
	QString next;
	QString message;
	QString timer;
	bool hidden = false;
};
QImage StageDisplay(const StageInfo &info, const QSize &canvas);

/* Directory used for rendered images. */
QString CacheDir(const QString &subdir);

} // namespace WorshipRender
