#include "WorshipRender.hpp"

#include <QAbstractTextDocumentLayout>
#include <QCryptographicHash>
#include <QDateTime>
#include <QDir>
#include <QFileInfo>
#include <QFont>
#include <QFontDatabase>
#include <QFontMetricsF>
#include <QLinearGradient>
#include <QPainter>
#include <QPainterPath>
#include <QRadialGradient>
#include <QRandomGenerator>
#include <QtMath>
#include <QStandardPaths>
#include <QTextDocument>
#include <QTextOption>

namespace WorshipRender {

namespace {
const QColor AccentColor(0xE8, 0xC4, 0x6A);
QString textFontFamily = QStringLiteral("Montserrat");

/* Extra decoration painted over the base gradient. */
enum class Effect { None, Mesh, Bokeh, Rays, Stars, Waves };

struct Preset {
	const char *name;
	QColor top, bottom, glow;
	Effect effect = Effect::None;
	QColor accent = QColor();
};

/* The order matters: the saved setting is the index. New presets go at the end. */
const Preset Presets[] = {
	{"Aurora (azul y turquesa)", QColor(0x0B, 0x2A, 0x4A), QColor(0x0E, 0x4D, 0x5C), QColor(0x3F, 0xB8, 0xC9)},
	{"Amanecer (violeta y oro)", QColor(0x2B, 0x1B, 0x4E), QColor(0x8A, 0x3B, 0x4F), QColor(0xF2, 0xB2, 0x5C)},
	{"Atardecer calido", QColor(0x4A, 0x1C, 0x2A), QColor(0xB3, 0x5A, 0x2E), QColor(0xFF, 0xC9, 0x7A)},
	{"Bosque sereno", QColor(0x0F, 0x2E, 0x26), QColor(0x2C, 0x5E, 0x3F), QColor(0x9F, 0xD8, 0x8B)},
	{"Purpura real", QColor(0x24, 0x0F, 0x3D), QColor(0x55, 0x22, 0x77), QColor(0xC9, 0x8B, 0xF2)},
	{"Cielo de noche", QColor(0x0A, 0x12, 0x2E), QColor(0x1C, 0x2C, 0x5C), QColor(0x7A, 0x9B, 0xF0)},
	{"Gris elegante", QColor(0x22, 0x26, 0x2E), QColor(0x3A, 0x41, 0x4D), QColor(0xB8, 0xC2, 0xD1)},
	{"Aurora boreal", QColor(0x04, 0x10, 0x22), QColor(0x08, 0x2A, 0x3A), QColor(0x3D, 0xF5, 0xB0), Effect::Waves,
	 QColor(0x8E, 0x6B, 0xFF)},
	{"Rayos de luz", QColor(0x10, 0x16, 0x3A), QColor(0x2A, 0x1E, 0x4A), QColor(0xFF, 0xE3, 0xA8), Effect::Rays,
	 QColor(0xFF, 0xF4, 0xDC)},
	{"Noche estrellada", QColor(0x02, 0x05, 0x14), QColor(0x0E, 0x16, 0x38), QColor(0x5B, 0x7C, 0xE6), Effect::Stars,
	 QColor(0xFF, 0xFF, 0xFF)},
	{"Bokeh dorado", QColor(0x14, 0x0C, 0x06), QColor(0x2E, 0x1A, 0x0C), QColor(0xF5, 0xB9, 0x4A), Effect::Bokeh,
	 QColor(0xFF, 0xD9, 0x8A)},
	{"Rosa y durazno", QColor(0x3B, 0x14, 0x36), QColor(0x8C, 0x3A, 0x4E), QColor(0xFF, 0x9E, 0x7A), Effect::Mesh,
	 QColor(0xE8, 0x6A, 0xB8)},
	{"Oceano profundo", QColor(0x02, 0x16, 0x2E), QColor(0x05, 0x3A, 0x5E), QColor(0x2E, 0xC4, 0xE6), Effect::Mesh,
	 QColor(0x2B, 0x5B, 0xE8)},
	{"Lavanda", QColor(0x22, 0x1A, 0x48), QColor(0x4B, 0x3A, 0x7C), QColor(0xC3, 0xA6, 0xFF), Effect::Mesh,
	 QColor(0x7F, 0xB6, 0xFF)},
	{"Negro y oro", QColor(0x07, 0x07, 0x09), QColor(0x16, 0x14, 0x12), QColor(0xD4, 0xAF, 0x37), Effect::Rays,
	 QColor(0xD4, 0xAF, 0x37)},
	{"Esmeralda", QColor(0x02, 0x1F, 0x1A), QColor(0x06, 0x45, 0x36), QColor(0x4F, 0xE3, 0xA1), Effect::Mesh,
	 QColor(0x1F, 0x9E, 0xC9)},
	{"Brasas", QColor(0x16, 0x05, 0x04), QColor(0x3A, 0x0E, 0x08), QColor(0xFF, 0x6A, 0x2B), Effect::Bokeh,
	 QColor(0xFF, 0xB0, 0x5C)},
	{"Cielo de manana", QColor(0x12, 0x2A, 0x52), QColor(0x2F, 0x5E, 0x9A), QColor(0xFF, 0xD6, 0xA5), Effect::Rays,
	 QColor(0xFF, 0xFF, 0xFF)},
};

constexpr int PresetCount = (int)(sizeof(Presets) / sizeof(Presets[0]));

QColor withAlpha(QColor color, int alpha)
{
	color.setAlpha(alpha);
	return color;
}

void fillGlow(QPainter &painter, const QRectF &area, QPointF center, qreal radius, const QColor &color, int alpha)
{
	QRadialGradient light(center, radius);
	light.setColorAt(0.0, withAlpha(color, alpha));
	light.setColorAt(0.55, withAlpha(color, alpha / 3));
	light.setColorAt(1.0, withAlpha(color, 0));
	painter.fillRect(area, light);
}

/* Large soft color blobs, like the "mesh gradients" of modern apps. */
void drawMesh(QPainter &painter, const QRectF &area, const Preset &preset)
{
	const qreal W = area.width(), H = area.height();
	fillGlow(painter, area, QPointF(W * 0.15, H * 0.20), W * 0.55, preset.accent, 150);
	fillGlow(painter, area, QPointF(W * 0.85, H * 0.80), W * 0.60, preset.glow, 140);
	fillGlow(painter, area, QPointF(W * 0.70, H * 0.10), W * 0.40, preset.glow.lighter(120), 80);
	fillGlow(painter, area, QPointF(W * 0.25, H * 0.95), W * 0.45, preset.bottom.lighter(160), 90);
}

void drawBokeh(QPainter &painter, const QRectF &area, const Preset &preset, quint32 seed)
{
	const qreal W = area.width(), H = area.height();
	QRandomGenerator random(seed);
	painter.save();
	painter.setPen(Qt::NoPen);
	for (int i = 0; i < 46; ++i) {
		const qreal radius = H * (0.02 + random.bounded(0.09));
		const QPointF center(random.bounded(W), H * (0.15 + random.bounded(0.85)));
		const QColor color = random.bounded(2) ? preset.glow : preset.accent;
		const int alpha = 25 + random.bounded(70);
		QRadialGradient circle(center, radius);
		circle.setColorAt(0.0, withAlpha(color, alpha));
		circle.setColorAt(0.8, withAlpha(color, alpha * 2 / 3));
		circle.setColorAt(1.0, withAlpha(color, 0));
		painter.setBrush(circle);
		painter.drawEllipse(center, radius, radius);
	}
	painter.restore();
}

void drawRays(QPainter &painter, const QRectF &area, const Preset &preset)
{
	const qreal W = area.width(), H = area.height();
	const QPointF origin(W * 0.5, -H * 0.25);
	painter.save();
	painter.setPen(Qt::NoPen);
	const int rays = 11;
	for (int i = 0; i < rays; ++i) {
		const qreal angle = qDegreesToRadians(60.0 + 60.0 * i / (rays - 1));
		const qreal spread = qDegreesToRadians(i % 2 ? 2.2 : 3.6);
		const qreal length = H * 1.8;
		QPainterPath ray;
		ray.moveTo(origin);
		ray.lineTo(origin + QPointF(qCos(angle - spread), qSin(angle - spread)) * length);
		ray.lineTo(origin + QPointF(qCos(angle + spread), qSin(angle + spread)) * length);
		ray.closeSubpath();
		QLinearGradient fade(origin, QPointF(origin.x(), H));
		fade.setColorAt(0.0, withAlpha(preset.accent, i % 2 ? 34 : 52));
		fade.setColorAt(1.0, withAlpha(preset.accent, 0));
		painter.fillPath(ray, fade);
	}
	painter.restore();
	fillGlow(painter, area, QPointF(W * 0.5, 0), W * 0.45, preset.glow, 130);
}

void drawStars(QPainter &painter, const QRectF &area, const Preset &preset, quint32 seed)
{
	const qreal W = area.width(), H = area.height();
	QRandomGenerator random(seed);
	painter.save();
	painter.setPen(Qt::NoPen);
	for (int i = 0; i < 520; ++i) {
		const QPointF center(random.bounded(W), random.bounded(H));
		const qreal radius = H * (0.0006 + random.bounded(0.0018));
		painter.setBrush(withAlpha(preset.accent, 60 + random.bounded(170)));
		painter.drawEllipse(center, radius, radius);
	}
	/* A few bright stars with a halo. */
	for (int i = 0; i < 14; ++i) {
		const QPointF center(random.bounded(W), random.bounded(H * 0.8));
		fillGlow(painter, QRectF(center - QPointF(H * 0.03, H * 0.03), QSizeF(H * 0.06, H * 0.06)), center,
			 H * 0.03, preset.accent, 120);
	}
	painter.restore();
	fillGlow(painter, area, QPointF(W * 0.75, H * 0.9), W * 0.6, preset.glow, 70);
}

void drawWaves(QPainter &painter, const QRectF &area, const Preset &preset)
{
	const qreal W = area.width(), H = area.height();
	painter.save();
	painter.setPen(Qt::NoPen);
	for (int band = 0; band < 4; ++band) {
		QPainterPath path;
		const qreal base = H * (0.18 + band * 0.11);
		const qreal amplitude = H * (0.08 + band * 0.02);
		path.moveTo(0, base);
		for (int x = 0; x <= 48; ++x) {
			const qreal t = x / 48.0;
			path.lineTo(W * t, base + amplitude * qSin(t * M_PI * (1.4 + band * 0.3) + band));
		}
		for (int x = 48; x >= 0; --x) {
			const qreal t = x / 48.0;
			path.lineTo(W * t, base + H * 0.22 + amplitude * 0.6 * qSin(t * M_PI * 1.2 + band * 1.7));
		}
		path.closeSubpath();
		const QColor color = band % 2 ? preset.accent : preset.glow;
		QLinearGradient fade(0, base - amplitude, 0, base + H * 0.3);
		fade.setColorAt(0.0, withAlpha(color, 0));
		fade.setColorAt(0.35, withAlpha(color, 70 - band * 10));
		fade.setColorAt(1.0, withAlpha(color, 0));
		painter.fillPath(path, fade);
	}
	painter.restore();
}

void drawPreset(QPainter &painter, const QSize &canvas, int index)
{
	index = qBound(0, index, PresetCount - 1);
	const Preset &preset = Presets[index];
	const qreal W = canvas.width(), H = canvas.height();
	const QRectF area(0, 0, W, H);

	QLinearGradient base(QPointF(0, 0), QPointF(W * 0.35, H));
	base.setColorAt(0.0, preset.top);
	base.setColorAt(1.0, preset.bottom);
	painter.fillRect(area, base);

	switch (preset.effect) {
	case Effect::None:
		/* Two soft light glows give depth without distracting from the text. */
		fillGlow(painter, area, QPointF(W * 0.82, H * 0.12), W * 0.55, preset.glow, 90);
		fillGlow(painter, area, QPointF(W * 0.10, H * 0.95), W * 0.45, preset.glow, 60);
		break;
	case Effect::Mesh:
		drawMesh(painter, area, preset);
		break;
	case Effect::Bokeh:
		fillGlow(painter, area, QPointF(W * 0.5, H * 0.6), W * 0.6, preset.glow, 50);
		drawBokeh(painter, area, preset, 1000 + index);
		break;
	case Effect::Rays:
		drawRays(painter, area, preset);
		break;
	case Effect::Stars:
		drawStars(painter, area, preset, 2000 + index);
		break;
	case Effect::Waves:
		drawWaves(painter, area, preset);
		break;
	}

	/* Gentle vignette keeps the edges calm and the text readable. */
	QRadialGradient vignette(QPointF(W / 2, H / 2), W * 0.75);
	vignette.setColorAt(0.55, QColor(0, 0, 0, 0));
	vignette.setColorAt(1.0, QColor(0, 0, 0, 120));
	painter.fillRect(area, vignette);
}

/* Cheap, good-looking blur: shrink a lot, then scale back up smoothly. */
QImage blurred(const QImage &image, int factor)
{
	const QSize small(qMax(1, image.width() / factor), qMax(1, image.height() / factor));
	QImage tiny = image.scaled(small, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
	QImage mid = tiny.scaled(small * 4, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
	return mid.scaled(image.size(), Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
}

QImage coverScaled(const QImage &image, const QSize &canvas)
{
	QImage scaled = image.scaled(canvas, Qt::KeepAspectRatioByExpanding, Qt::SmoothTransformation);
	const QPoint offset((scaled.width() - canvas.width()) / 2, (scaled.height() - canvas.height()) / 2);
	return scaled.copy(QRect(offset, canvas));
}

void drawBackground(QPainter &painter, const QSize &canvas, const Background &bg)
{
	const QRect area(QPoint(0, 0), canvas);
	const QImage image = bg.kind == BackgroundKind::Image ? QImage(bg.path) : QImage();
	if (bg.kind == BackgroundKind::Image && !image.isNull()) {
		painter.drawImage(0, 0, coverScaled(image, canvas));
		painter.fillRect(area, QColor(0, 0, 0, 120));
	} else if (bg.kind == BackgroundKind::Video) {
		/* The video plays underneath; only darken it for legibility. */
		painter.fillRect(area, QColor(0, 0, 0, 105));
	} else if (bg.kind == BackgroundKind::Color && QColor::isValidColorName(bg.color)) {
		painter.fillRect(area, QColor::fromString(bg.color));
	} else {
		drawPreset(painter, canvas, bg.preset);
	}
}

void drawSoftShadow(QPainter &painter, const QRect &rect, int radius)
{
	painter.save();
	painter.setPen(Qt::NoPen);
	for (int i = radius; i > 0; i -= 2) {
		painter.setBrush(QColor(0, 0, 0, 10));
		painter.drawRoundedRect(rect.adjusted(-i, -i + radius / 3, i, i + radius / 3), i, i);
	}
	painter.restore();
}

QString versesHtml(const BibleSlide &slide, bool shadow)
{
	const QString textColor = shadow ? QStringLiteral("#000000") : QStringLiteral("#FFFFFF");
	const QString numberColor = shadow ? QStringLiteral("#000000") : AccentColor.name();

	QStringList parts;
	for (const BibleVerseLine &verse : slide.verses) {
		QString part;
		if (slide.showVerseNumbers)
			part += QStringLiteral("<span style=\"color:%1; font-size:60%; vertical-align:super; font-weight:600;\">%2</span>&#8201;")
					.arg(numberColor)
					.arg(verse.number);
		part += verse.text.toHtmlEscaped();
		parts << part;
	}

	return QStringLiteral("<p align=\"center\" style=\"color:%1; line-height:122%;\">%2</p>")
		.arg(textColor, parts.join(QStringLiteral(" ")));
}

void setupDocument(QTextDocument &doc, const QString &html, int pixelSize, qreal width)
{
	QFont font(textFontFamily);
	font.setPixelSize(pixelSize);
	font.setWeight(QFont::Medium);
	font.setStyleStrategy(QFont::PreferAntialias);
	doc.setDefaultFont(font);
	doc.setDocumentMargin(0);
	QTextOption option;
	option.setAlignment(Qt::AlignHCenter);
	option.setWrapMode(QTextOption::WordWrap);
	doc.setDefaultTextOption(option);
	doc.setHtml(html);
	doc.setTextWidth(width);
}

/* Draws the verses centered in `box`, picking the largest size that fits. */
bool drawFittedText(QPainter &painter, const BibleSlide &slide, const QRectF &box, int maxPixelSize, int minPixelSize,
		    qreal shadowOffset)
{
	const QString html = versesHtml(slide, false);

	int low = minPixelSize, high = qMax(minPixelSize, maxPixelSize), best = minPixelSize;
	bool fits = false;
	while (low <= high) {
		const int mid = (low + high) / 2;
		QTextDocument probe;
		setupDocument(probe, html, mid, box.width());
		if (probe.size().height() <= box.height()) {
			best = mid;
			fits = true;
			low = mid + 1;
		} else {
			high = mid - 1;
		}
	}

	QTextDocument doc;
	setupDocument(doc, html, best, box.width());
	QTextDocument shadowDoc;
	setupDocument(shadowDoc, versesHtml(slide, true), best, box.width());

	const qreal top = box.top() + qMax<qreal>(0.0, (box.height() - doc.size().height()) / 2.0);

	painter.save();
	painter.setClipRect(box.adjusted(-shadowOffset * 4, -shadowOffset * 4, shadowOffset * 4, shadowOffset * 4));
	painter.translate(box.left(), top);
	painter.save();
	painter.setOpacity(0.55);
	painter.translate(0, shadowOffset);
	shadowDoc.drawContents(&painter);
	painter.restore();
	doc.drawContents(&painter);
	painter.restore();

	return fits;
}

/* The reference is the protagonist (big, accent color); the version goes
 * underneath, small and spaced, like a caption. */
void drawReference(QPainter &painter, const BibleSlide &slide, const QRectF &refBox, const QRectF &versionBox,
		   int pixelSize)
{
	QFont refFont(textFontFamily);
	refFont.setPixelSize(pixelSize);
	refFont.setWeight(QFont::DemiBold);
	refFont.setLetterSpacing(QFont::PercentageSpacing, 102);

	painter.save();
	painter.setFont(refFont);
	painter.setPen(QColor(0, 0, 0, 150));
	painter.drawText(refBox.translated(0, pixelSize * 0.05), Qt::AlignCenter, slide.reference);
	painter.setPen(AccentColor);
	painter.drawText(refBox, Qt::AlignCenter, slide.reference);

	if (!slide.version.isEmpty()) {
		QFont versionFont(textFontFamily);
		versionFont.setPixelSize(qMax(10, qRound(pixelSize * 0.40)));
		versionFont.setWeight(QFont::Medium);
		versionFont.setCapitalization(QFont::AllUppercase);
		versionFont.setLetterSpacing(QFont::PercentageSpacing, 135);
		painter.setFont(versionFont);
		painter.setPen(QColor(255, 255, 255, 175));
		painter.drawText(versionBox, Qt::AlignCenter, slide.version);
	}
	painter.restore();
}
} // namespace

QStringList BackgroundPresetNames()
{
	QStringList names;
	for (const Preset &preset : Presets)
		names << QString::fromUtf8(preset.name);
	return names;
}

namespace {
QStringList bundledFamilies;
}

void RegisterBundledFonts(const QString &directory)
{
	const QFileInfoList files =
		QDir(directory).entryInfoList({QStringLiteral("*.ttf"), QStringLiteral("*.otf")}, QDir::Files);
	for (const QFileInfo &file : files) {
		const int id = QFontDatabase::addApplicationFont(file.absoluteFilePath());
		for (const QString &family : QFontDatabase::applicationFontFamilies(id)) {
			if (!bundledFamilies.contains(family))
				bundledFamilies << family;
		}
	}
}

QStringList RecommendedFonts()
{
	/* Fonts shipped with WorshipStudio first (same look on every computer),
	 * then clean system faces and classic serifs for scripture. */
	static const char *const preferred[] = {"Montserrat", "Inter", "Poppins", "Raleway", "Nunito", "Work Sans",
						"Lato", "Josefin Sans", "Oswald", "Bebas Neue", "Playfair Display",
						"Lora", "Merriweather", "Source Serif 4", "EB Garamond",
						"Cormorant Garamond", "DM Serif Display"};
	static const char *const system[] = {"Helvetica Neue", "Avenir Next", "Open Sans", "Segoe UI", "Gill Sans",
					     "Futura", "Optima", "Seravek", "Georgia", "Palatino", "Baskerville",
					     "Hoefler Text", "Charter", "Didot", "Cambria", "Calibri"};
	QStringList fonts;
	const QStringList installed = QFontDatabase::families();
	for (const char *candidate : preferred) {
		const QString family = QString::fromLatin1(candidate);
		if (installed.contains(family))
			fonts << family;
	}
	for (const QString &family : bundledFamilies) {
		if (!fonts.contains(family))
			fonts << family;
	}
	for (const char *candidate : system) {
		const QString family = QString::fromLatin1(candidate);
		if (installed.contains(family) && !fonts.contains(family))
			fonts << family;
	}
	if (fonts.isEmpty())
		fonts << QFontDatabase::systemFont(QFontDatabase::GeneralFont).family();
	return fonts;
}

QString CacheDir(const QString &subdir)
{
	const QString base = QStandardPaths::writableLocation(QStandardPaths::GenericCacheLocation);
	const QString path = QDir(base).absoluteFilePath(QStringLiteral("WorshipStudio/") + subdir);
	QDir().mkpath(path);
	return path;
}

QString HymnSlide(const QString &slidePath, SlideFit fit, const QSize &canvas)
{
	const QFileInfo info(slidePath);
	const QByteArray key = QStringLiteral("%1|%2|%3|%4x%5")
				       .arg(info.absoluteFilePath())
				       .arg(info.lastModified().toMSecsSinceEpoch())
				       .arg((int)fit)
				       .arg(canvas.width())
				       .arg(canvas.height())
				       .toUtf8();
	const QString hash = QString::fromLatin1(QCryptographicHash::hash(key, QCryptographicHash::Sha1).toHex().left(20));
	const QString target = QDir(CacheDir(QStringLiteral("hymns"))).absoluteFilePath(hash + QStringLiteral(".jpg"));
	if (QFileInfo::exists(target))
		return target;

	const QImage source(slidePath);
	if (source.isNull())
		return slidePath;

	QImage output(canvas, QImage::Format_RGB32);
	output.fill(Qt::black);
	QPainter painter(&output);
	painter.setRenderHint(QPainter::SmoothPixmapTransform);
	painter.setRenderHint(QPainter::Antialiasing);

	if (fit == SlideFit::Stretch) {
		painter.drawImage(QRect(QPoint(0, 0), canvas), source);
	} else {
		if (fit == SlideFit::BlurredFill) {
			painter.drawImage(0, 0, blurred(coverScaled(source, canvas), 40));
			painter.fillRect(output.rect(), QColor(0, 0, 0, 90));
		}
		const QSize scaled = source.size().scaled(canvas, Qt::KeepAspectRatio);
		const QRect slideRect(QPoint((canvas.width() - scaled.width()) / 2, (canvas.height() - scaled.height()) / 2),
				      scaled);
		if (fit == SlideFit::BlurredFill && slideRect.width() < canvas.width())
			drawSoftShadow(painter, slideRect, qMax(8, canvas.height() / 60));
		painter.drawImage(slideRect, source);
	}
	painter.end();

	return output.save(target, "JPG", 93) ? target : slidePath;
}

QImage Bible(const BibleSlide &slide, const QSize &canvas, bool *fits)
{
	const qreal W = canvas.width();
	const qreal H = canvas.height();
	const qreal scale = H / 1080.0;
	textFontFamily = slide.fontFamily.isEmpty() ? QStringLiteral("Montserrat") : slide.fontFamily;

	QImage output(canvas, QImage::Format_ARGB32_Premultiplied);
	output.fill(Qt::transparent);
	QPainter painter(&output);
	painter.setRenderHint(QPainter::Antialiasing);
	painter.setRenderHint(QPainter::TextAntialiasing);
	painter.setRenderHint(QPainter::SmoothPixmapTransform);

	bool textFits = true;

	if (slide.layout == BibleLayout::FullScreen) {
		drawBackground(painter, canvas, slide.background);

		const QRectF textBox(W * 0.08, H * 0.07, W * 0.84, H * 0.63);
		textFits = drawFittedText(painter, slide, textBox, qRound(slide.maxFontSize * scale), qRound(34 * scale),
					  3.0 * scale);

		/* Divider, big reference and the version underneath. */
		const qreal dividerY = H * 0.745;
		painter.setPen(Qt::NoPen);
		painter.setBrush(AccentColor);
		painter.drawRoundedRect(QRectF(W * 0.5 - W * 0.03, dividerY, W * 0.06, qMax(2.0, 3.0 * scale)), 2, 2);
		drawReference(painter, slide, QRectF(0, H * 0.77, W, H * 0.11), QRectF(0, H * 0.875, W, H * 0.05),
			      qRound(64 * scale));
	} else {
		const QRectF band(W * 0.05, H * 0.655, W * 0.90, H * 0.29);
		const qreal radius = 18 * scale;

		QLinearGradient bandGradient(band.topLeft(), band.bottomLeft());
		bandGradient.setColorAt(0.0, QColor(0x0C, 0x12, 0x24, 225));
		bandGradient.setColorAt(1.0, QColor(0x06, 0x09, 0x14, 240));
		QPainterPath path;
		path.addRoundedRect(band, radius, radius);
		painter.fillPath(path, bandGradient);

		/* Accent bar on the left edge of the band. */
		painter.save();
		painter.setClipPath(path);
		painter.fillRect(QRectF(band.left(), band.top(), 8 * scale, band.height()), AccentColor);
		painter.restore();

		const QRectF textBox(band.left() + band.width() * 0.04, band.top() + band.height() * 0.08,
				     band.width() * 0.92, band.height() * 0.56);
		textFits = drawFittedText(painter, slide, textBox, qRound(slide.maxFontSize * 0.75 * scale),
					  qRound(26 * scale), 2.0 * scale);
		drawReference(painter, slide,
			      QRectF(band.left(), band.top() + band.height() * 0.66, band.width(), band.height() * 0.18),
			      QRectF(band.left(), band.top() + band.height() * 0.83, band.width(), band.height() * 0.12),
			      qRound(38 * scale));
	}

	painter.end();
	if (fits)
		*fits = textFits;
	return output;
}

QImage Lyrics(const LyricsSlide &slide, const QSize &canvas)
{
	const qreal W = canvas.width();
	const qreal H = canvas.height();
	const qreal scale = H / 1080.0;
	const QString family = slide.fontFamily.isEmpty() ? QStringLiteral("Montserrat") : slide.fontFamily;

	QImage output(canvas, QImage::Format_ARGB32_Premultiplied);
	output.fill(Qt::transparent);
	QPainter painter(&output);
	painter.setRenderHint(QPainter::Antialiasing);
	painter.setRenderHint(QPainter::TextAntialiasing);
	painter.setRenderHint(QPainter::SmoothPixmapTransform);

	const bool lowerThird = slide.layout == LyricsLayout::LowerThird;
	QRectF box;
	if (lowerThird) {
		const QRectF band(W * 0.06, H * 0.70, W * 0.88, H * 0.25);
		QLinearGradient bandGradient(band.topLeft(), band.bottomLeft());
		bandGradient.setColorAt(0.0, QColor(0x0C, 0x12, 0x24, 215));
		bandGradient.setColorAt(1.0, QColor(0x06, 0x09, 0x14, 235));
		QPainterPath path;
		path.addRoundedRect(band, 18 * scale, 18 * scale);
		painter.fillPath(path, bandGradient);
		box = band.adjusted(band.width() * 0.04, band.height() * 0.12, -band.width() * 0.04, -band.height() * 0.12);
	} else {
		drawBackground(painter, canvas, slide.background);
		box = QRectF(W * 0.07, H * 0.10, W * 0.86, slide.footer.isEmpty() ? H * 0.80 : H * 0.72);
	}

	if (!slide.text.trimmed().isEmpty()) {
		QString text = slide.text.trimmed();
		if (slide.uppercase)
			text = text.toUpper();

		QStringList lines;
		for (const QString &line : text.split(QLatin1Char('\n')))
			lines << line.trimmed().toHtmlEscaped();
		const auto html = [&](const QString &color) {
			return QStringLiteral("<p align=\"center\" style=\"color:%1; line-height:118%;\">%2</p>")
				.arg(color, lines.join(QStringLiteral("<br>")));
		};

		const auto setup = [&](QTextDocument &doc, const QString &content, int pixelSize) {
			QFont font(family);
			font.setPixelSize(pixelSize);
			font.setWeight(QFont::DemiBold);
			font.setStyleStrategy(QFont::PreferAntialias);
			doc.setDefaultFont(font);
			doc.setDocumentMargin(0);
			QTextOption option;
			option.setAlignment(Qt::AlignHCenter);
			option.setWrapMode(QTextOption::WordWrap);
			doc.setDefaultTextOption(option);
			doc.setHtml(content);
			doc.setTextWidth(box.width());
		};

		const int maxSize = qRound((lowerThird ? slide.maxFontSize * 0.6 : slide.maxFontSize) * scale);
		int low = qRound(18 * scale), high = qMax(low, maxSize), best = low;
		while (low <= high) {
			const int mid = (low + high) / 2;
			QTextDocument probe;
			setup(probe, html(QStringLiteral("#FFFFFF")), mid);
			if (probe.size().height() <= box.height() && probe.idealWidth() <= box.width() + 1) {
				best = mid;
				low = mid + 1;
			} else {
				high = mid - 1;
			}
		}

		/* Lines are written on purpose: if a slightly smaller size keeps
		 * every line whole, prefer it over breaking a line in two. */
		QFont probeFont(family);
		probeFont.setWeight(QFont::DemiBold);
		const QStringList rawLines = text.split(QLatin1Char('\n'));
		for (int size = best; size >= qRound(best * 0.72); --size) {
			probeFont.setPixelSize(size);
			const QFontMetricsF metrics(probeFont);
			qreal widest = 0;
			for (const QString &line : rawLines)
				widest = qMax(widest, metrics.horizontalAdvance(line.trimmed()));
			if (widest <= box.width() * 0.98) {
				best = size;
				break;
			}
		}

		QTextDocument doc, shadow;
		setup(doc, html(slide.textColor.isEmpty() ? QStringLiteral("#FFFFFF") : slide.textColor), best);
		setup(shadow, html(QStringLiteral("#000000")), best);
		const qreal top = box.top() + qMax<qreal>(0.0, (box.height() - doc.size().height()) / 2.0);
		painter.save();
		painter.translate(box.left(), top);
		painter.save();
		painter.setOpacity(0.5);
		painter.translate(0, 3.0 * scale);
		shadow.drawContents(&painter);
		painter.restore();
		doc.drawContents(&painter);
		painter.restore();
	}

	if (!lowerThird && !slide.footer.isEmpty()) {
		QFont footerFont(family);
		footerFont.setPixelSize(qRound(24 * scale));
		footerFont.setWeight(QFont::Medium);
		footerFont.setLetterSpacing(QFont::PercentageSpacing, 115);
		painter.setFont(footerFont);
		painter.setPen(QColor(255, 255, 255, 160));
		painter.drawText(QRectF(0, H * 0.88, W, H * 0.07), Qt::AlignCenter, slide.footer);
	}

	painter.end();
	return output;
}

namespace {
/* Largest font size (up to maxSize) so that `text` fits in `box`. */
QFont fittedFont(const QString &family, int weight, const QString &text, const QRectF &box, int maxSize, int minSize,
		 int flags)
{
	QFont font(family);
	font.setWeight(static_cast<QFont::Weight>(weight));
	for (int size = maxSize; size >= minSize; size -= 2) {
		font.setPixelSize(size);
		const QRectF needed = QFontMetricsF(font).boundingRect(box, flags, text);
		if (needed.width() <= box.width() && needed.height() <= box.height())
			return font;
	}
	font.setPixelSize(minSize);
	return font;
}

void shadowedText(QPainter &painter, const QRectF &box, int flags, const QString &text, const QColor &color,
		  qreal offset)
{
	painter.setPen(QColor(0, 0, 0, 140));
	painter.drawText(box.translated(0, offset), flags, text);
	painter.setPen(color);
	painter.drawText(box, flags, text);
}
} // namespace

QImage Countdown(const QString &title, const QString &time, const QString &footer, const Background &background,
		 const QString &fontFamily, const QSize &canvas)
{
	const qreal W = canvas.width(), H = canvas.height(), scale = H / 1080.0;
	const QString family = fontFamily.isEmpty() ? QStringLiteral("Montserrat") : fontFamily;
	QImage output(canvas, QImage::Format_ARGB32_Premultiplied);
	output.fill(Qt::transparent);
	QPainter painter(&output);
	painter.setRenderHint(QPainter::Antialiasing);
	painter.setRenderHint(QPainter::TextAntialiasing);
	drawBackground(painter, canvas, background);

	if (!title.isEmpty()) {
		QFont titleFont(family);
		titleFont.setPixelSize(qRound(46 * scale));
		titleFont.setWeight(QFont::Medium);
		titleFont.setLetterSpacing(QFont::PercentageSpacing, 112);
		titleFont.setCapitalization(QFont::AllUppercase);
		painter.setFont(titleFont);
		shadowedText(painter, QRectF(0, H * 0.22, W, H * 0.10), Qt::AlignCenter, title, QColor(255, 255, 255, 220),
			     3 * scale);
	}

	QFont timeFont(family);
	timeFont.setPixelSize(qRound(300 * scale));
	timeFont.setWeight(QFont::Bold);
#if QT_VERSION >= QT_VERSION_CHECK(6, 6, 0)
	timeFont.setFeature("tnum", 1); /* digits of equal width */
#endif
	painter.setFont(timeFont);
	shadowedText(painter, QRectF(0, H * 0.30, W, H * 0.40), Qt::AlignCenter, time, Qt::white, 6 * scale);

	if (!footer.isEmpty()) {
		QFont footerFont(family);
		footerFont.setPixelSize(qRound(34 * scale));
		footerFont.setWeight(QFont::Medium);
		painter.setFont(footerFont);
		shadowedText(painter, QRectF(0, H * 0.74, W, H * 0.08), Qt::AlignCenter, footer, AccentColor, 2 * scale);
	}
	painter.end();
	return output;
}

QImage NameLowerThird(const QString &name, const QString &role, const QString &fontFamily, const QSize &canvas)
{
	const qreal W = canvas.width(), H = canvas.height(), scale = H / 1080.0;
	const QString family = fontFamily.isEmpty() ? QStringLiteral("Montserrat") : fontFamily;
	QImage output(canvas, QImage::Format_ARGB32_Premultiplied);
	output.fill(Qt::transparent);
	if (name.trimmed().isEmpty())
		return output;

	QPainter painter(&output);
	painter.setRenderHint(QPainter::Antialiasing);
	painter.setRenderHint(QPainter::TextAntialiasing);

	QFont nameFont(family);
	nameFont.setPixelSize(qRound(54 * scale));
	nameFont.setWeight(QFont::Bold);
	QFont roleFont(family);
	roleFont.setPixelSize(qRound(30 * scale));
	roleFont.setWeight(QFont::Medium);
	roleFont.setLetterSpacing(QFont::PercentageSpacing, 108);

	const qreal padX = 36 * scale, padY = 22 * scale;
	const qreal nameW = QFontMetricsF(nameFont).horizontalAdvance(name);
	const qreal roleW = role.isEmpty() ? 0 : QFontMetricsF(roleFont).horizontalAdvance(role);
	const qreal width = qMin(W * 0.7, qMax(nameW, roleW) + padX * 2 + 10 * scale);
	const qreal height = padY * 2 + 62 * scale + (role.isEmpty() ? 0 : 40 * scale);
	const QRectF band(W * 0.06, H * 0.80 - height, width, height);

	QPainterPath path;
	path.addRoundedRect(band, 14 * scale, 14 * scale);
	QLinearGradient gradient(band.topLeft(), band.bottomRight());
	gradient.setColorAt(0, QColor(0x10, 0x14, 0x26, 235));
	gradient.setColorAt(1, QColor(0x1C, 0x22, 0x40, 235));
	painter.fillPath(path, gradient);
	painter.save();
	painter.setClipPath(path);
	painter.fillRect(QRectF(band.left(), band.top(), 8 * scale, band.height()), AccentColor);
	painter.restore();

	painter.setFont(nameFont);
	painter.setPen(Qt::white);
	painter.drawText(QRectF(band.left() + padX, band.top() + padY, band.width() - padX * 1.5, 62 * scale),
			 Qt::AlignLeft | Qt::AlignVCenter, name);
	if (!role.isEmpty()) {
		painter.setFont(roleFont);
		painter.setPen(AccentColor);
		painter.drawText(QRectF(band.left() + padX, band.top() + padY + 62 * scale, band.width() - padX * 1.5,
					40 * scale),
				 Qt::AlignLeft | Qt::AlignVCenter, role);
	}
	painter.end();
	return output;
}

QImage UrgentMessage(const QString &text, const QString &fontFamily, const QSize &canvas)
{
	const qreal W = canvas.width(), H = canvas.height(), scale = H / 1080.0;
	const QString family = fontFamily.isEmpty() ? QStringLiteral("Montserrat") : fontFamily;
	QImage output(canvas, QImage::Format_ARGB32_Premultiplied);
	output.fill(Qt::transparent);
	if (text.trimmed().isEmpty())
		return output;

	QPainter painter(&output);
	painter.setRenderHint(QPainter::Antialiasing);
	painter.setRenderHint(QPainter::TextAntialiasing);
	const QRectF band(W * 0.04, H * 0.04, W * 0.92, H * 0.11);
	QPainterPath path;
	path.addRoundedRect(band, 14 * scale, 14 * scale);
	painter.fillPath(path, QColor(0xC0, 0x1C, 0x37, 240));
	const QRectF textBox = band.adjusted(30 * scale, 8 * scale, -30 * scale, -8 * scale);
	painter.setFont(fittedFont(family, QFont::DemiBold, text, textBox, qRound(48 * scale), qRound(20 * scale),
				   Qt::AlignCenter | Qt::TextWordWrap));
	painter.setPen(Qt::white);
	painter.drawText(textBox, Qt::AlignCenter | Qt::TextWordWrap, text);
	painter.end();
	return output;
}

QImage StageDisplay(const StageInfo &info, const QSize &canvas)
{
	const qreal W = canvas.width(), H = canvas.height(), scale = H / 1080.0;
	const QString family = QStringLiteral("Inter");
	QImage output(canvas, QImage::Format_RGB32);
	output.fill(QColor(0x0B, 0x0C, 0x10));
	QPainter painter(&output);
	painter.setRenderHint(QPainter::Antialiasing);
	painter.setRenderHint(QPainter::TextAntialiasing);

	/* Top bar: what is on screen and the clock. */
	QFont small(family);
	small.setPixelSize(qRound(34 * scale));
	small.setWeight(QFont::Medium);
	painter.setFont(small);
	painter.setPen(QColor(0x9A, 0xA0, 0xB4));
	const QString heading = info.section.isEmpty() ? info.title : info.title + QStringLiteral("  ·  ") + info.section;
	painter.drawText(QRectF(W * 0.03, H * 0.02, W * 0.62, H * 0.07), Qt::AlignLeft | Qt::AlignVCenter,
			 QFontMetrics(small).elidedText(heading, Qt::ElideRight, qRound(W * 0.62)));

	QFont clock(family);
	clock.setPixelSize(qRound(64 * scale));
	clock.setWeight(QFont::Bold);
#if QT_VERSION >= QT_VERSION_CHECK(6, 6, 0)
	clock.setFeature("tnum", 1); /* digits of equal width */
#endif
	painter.setFont(clock);
	painter.setPen(Qt::white);
	painter.drawText(QRectF(W * 0.60, H * 0.01, W * 0.37, H * 0.09), Qt::AlignRight | Qt::AlignVCenter, info.clock);
	if (!info.timer.isEmpty()) {
		painter.setFont(small);
		painter.setPen(AccentColor);
		painter.drawText(QRectF(W * 0.60, H * 0.09, W * 0.37, H * 0.05), Qt::AlignRight | Qt::AlignVCenter,
				 info.timer);
	}

	painter.fillRect(QRectF(W * 0.03, H * 0.145, W * 0.94, qMax(1.0, 2 * scale)), QColor(0x26, 0x29, 0x35));

	/* Current slide, as big as possible. */
	const QRectF currentBox(W * 0.04, H * 0.17, W * 0.92, H * 0.50);
	QString current = info.current.trimmed();
	if (info.hidden)
		current = QStringLiteral("(pantalla oculta)");
	else if (current.isEmpty())
		current = info.title.isEmpty() ? QStringLiteral("Sin proyeccion") : QStringLiteral("(imagen en pantalla)");
	painter.setFont(fittedFont(family, QFont::Bold, current, currentBox, qRound(110 * scale), qRound(30 * scale),
				   Qt::AlignCenter | Qt::TextWordWrap));
	painter.setPen(info.hidden ? QColor(0x9A, 0xA0, 0xB4) : Qt::white);
	painter.drawText(currentBox, Qt::AlignCenter | Qt::TextWordWrap, current);

	/* Next slide. */
	painter.fillRect(QRectF(W * 0.03, H * 0.69, W * 0.94, qMax(1.0, 2 * scale)), QColor(0x26, 0x29, 0x35));
	painter.setFont(small);
	painter.setPen(QColor(0xE2, 0x58, 0x2B));
	painter.drawText(QRectF(W * 0.04, H * 0.70, W * 0.3, H * 0.05), Qt::AlignLeft | Qt::AlignVCenter,
			 QStringLiteral("SIGUIENTE"));
	const QRectF nextBox(W * 0.04, H * 0.75, W * 0.92, info.message.isEmpty() ? H * 0.22 : H * 0.12);
	if (!info.next.trimmed().isEmpty()) {
		painter.setFont(fittedFont(family, QFont::Medium, info.next.trimmed(), nextBox, qRound(58 * scale),
					   qRound(22 * scale), Qt::AlignLeft | Qt::AlignTop | Qt::TextWordWrap));
		painter.setPen(QColor(0xB8, 0xBD, 0xCC));
		painter.drawText(nextBox, Qt::AlignLeft | Qt::AlignTop | Qt::TextWordWrap, info.next.trimmed());
	}

	/* Message from the booth. */
	if (!info.message.isEmpty()) {
		const QRectF band(W * 0.03, H * 0.88, W * 0.94, H * 0.10);
		QPainterPath path;
		path.addRoundedRect(band, 12 * scale, 12 * scale);
		painter.fillPath(path, AccentColor);
		const QRectF box = band.adjusted(24 * scale, 4 * scale, -24 * scale, -4 * scale);
		painter.setFont(fittedFont(family, QFont::Bold, info.message, box, qRound(56 * scale), qRound(20 * scale),
					   Qt::AlignCenter | Qt::TextWordWrap));
		painter.setPen(QColor(0x1A, 0x14, 0x07));
		painter.drawText(box, Qt::AlignCenter | Qt::TextWordWrap, info.message);
	}
	painter.end();
	return output;
}

} // namespace WorshipRender
