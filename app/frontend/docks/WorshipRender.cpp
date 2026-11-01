#include "WorshipRender.hpp"

#include <QAbstractTextDocumentLayout>
#include <QCryptographicHash>
#include <QDateTime>
#include <QDir>
#include <QFileInfo>
#include <QFont>
#include <QFontDatabase>
#include <QLinearGradient>
#include <QPainter>
#include <QPainterPath>
#include <QRadialGradient>
#include <QStandardPaths>
#include <QTextDocument>
#include <QTextOption>

namespace WorshipRender {

namespace {
const QColor AccentColor(0xE8, 0xC4, 0x6A);
QString textFontFamily = QStringLiteral("Helvetica Neue");

struct Preset {
	const char *name;
	QColor top, bottom, glow;
};

const Preset Presets[] = {
	{"Aurora (azul y turquesa)", QColor(0x0B, 0x2A, 0x4A), QColor(0x0E, 0x4D, 0x5C), QColor(0x3F, 0xB8, 0xC9)},
	{"Amanecer (violeta y oro)", QColor(0x2B, 0x1B, 0x4E), QColor(0x8A, 0x3B, 0x4F), QColor(0xF2, 0xB2, 0x5C)},
	{"Atardecer calido", QColor(0x4A, 0x1C, 0x2A), QColor(0xB3, 0x5A, 0x2E), QColor(0xFF, 0xC9, 0x7A)},
	{"Bosque sereno", QColor(0x0F, 0x2E, 0x26), QColor(0x2C, 0x5E, 0x3F), QColor(0x9F, 0xD8, 0x8B)},
	{"Purpura real", QColor(0x24, 0x0F, 0x3D), QColor(0x55, 0x22, 0x77), QColor(0xC9, 0x8B, 0xF2)},
	{"Cielo de noche", QColor(0x0A, 0x12, 0x2E), QColor(0x1C, 0x2C, 0x5C), QColor(0x7A, 0x9B, 0xF0)},
	{"Gris elegante", QColor(0x22, 0x26, 0x2E), QColor(0x3A, 0x41, 0x4D), QColor(0xB8, 0xC2, 0xD1)},
};

void drawPreset(QPainter &painter, const QSize &canvas, int index)
{
	const Preset &preset = Presets[qBound(0, index, (int)(sizeof(Presets) / sizeof(Presets[0])) - 1)];
	const qreal W = canvas.width(), H = canvas.height();

	QLinearGradient base(QPointF(0, 0), QPointF(W * 0.35, H));
	base.setColorAt(0.0, preset.top);
	base.setColorAt(1.0, preset.bottom);
	painter.fillRect(QRectF(0, 0, W, H), base);

	/* Two soft light glows give depth without distracting from the text. */
	auto glow = [&](QPointF center, qreal radius, int alpha) {
		QRadialGradient light(center, radius);
		QColor color = preset.glow;
		color.setAlpha(alpha);
		light.setColorAt(0.0, color);
		color.setAlpha(0);
		light.setColorAt(1.0, color);
		painter.fillRect(QRectF(0, 0, W, H), light);
	};
	glow(QPointF(W * 0.82, H * 0.12), W * 0.55, 90);
	glow(QPointF(W * 0.10, H * 0.95), W * 0.45, 60);

	/* Gentle vignette keeps the edges calm. */
	QRadialGradient vignette(QPointF(W / 2, H / 2), W * 0.75);
	vignette.setColorAt(0.6, QColor(0, 0, 0, 0));
	vignette.setColorAt(1.0, QColor(0, 0, 0, 110));
	painter.fillRect(QRectF(0, 0, W, H), vignette);
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

void drawReference(QPainter &painter, const BibleSlide &slide, const QRectF &box, int pixelSize, Qt::Alignment align)
{
	QFont refFont(textFontFamily);
	refFont.setPixelSize(pixelSize);
	refFont.setWeight(QFont::DemiBold);
	refFont.setLetterSpacing(QFont::PercentageSpacing, 104);

	QFont versionFont = refFont;
	versionFont.setWeight(QFont::Normal);
	versionFont.setPixelSize(qRound(pixelSize * 0.72));

	const QString separator = QStringLiteral("   ");
	const QFontMetricsF refMetrics(refFont);
	const QFontMetricsF versionMetrics(versionFont);
	const qreal refWidth = refMetrics.horizontalAdvance(slide.reference);
	const qreal versionWidth =
		slide.version.isEmpty() ? 0.0 : versionMetrics.horizontalAdvance(separator + slide.version);
	const qreal total = refWidth + versionWidth;

	qreal x = box.left();
	if (align & Qt::AlignHCenter)
		x = box.center().x() - total / 2.0;
	else if (align & Qt::AlignRight)
		x = box.right() - total;
	const qreal baseline = box.center().y() + refMetrics.capHeight() / 2.0;

	painter.save();
	painter.setFont(refFont);
	painter.setPen(QColor(0, 0, 0, 140));
	painter.drawText(QPointF(x, baseline + pixelSize * 0.06), slide.reference);
	painter.setPen(AccentColor);
	painter.drawText(QPointF(x, baseline), slide.reference);
	if (!slide.version.isEmpty()) {
		painter.setFont(versionFont);
		painter.setPen(QColor(255, 255, 255, 170));
		painter.drawText(QPointF(x + refWidth, baseline), separator + slide.version);
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

QStringList RecommendedFonts()
{
	/* Clean, highly legible faces for screens, plus classic serifs for scripture. */
	static const char *const candidates[] = {"Helvetica Neue", "Avenir Next", "Montserrat", "Open Sans",
						 "Source Sans Pro", "Gill Sans", "Futura", "Optima",
						 "Seravek", "Georgia", "Palatino", "Baskerville",
						 "Hoefler Text", "Charter", "Didot"};
	QStringList fonts;
	const QStringList installed = QFontDatabase::families();
	for (const char *candidate : candidates) {
		const QString family = QString::fromLatin1(candidate);
		if (installed.contains(family))
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
	textFontFamily = slide.fontFamily.isEmpty() ? QStringLiteral("Helvetica Neue") : slide.fontFamily;

	QImage output(canvas, QImage::Format_ARGB32_Premultiplied);
	output.fill(Qt::transparent);
	QPainter painter(&output);
	painter.setRenderHint(QPainter::Antialiasing);
	painter.setRenderHint(QPainter::TextAntialiasing);
	painter.setRenderHint(QPainter::SmoothPixmapTransform);

	bool textFits = true;

	if (slide.layout == BibleLayout::FullScreen) {
		const Background &bg = slide.background;
		const QImage image = bg.kind == BackgroundKind::Image ? QImage(bg.path) : QImage();
		if (bg.kind == BackgroundKind::Image && !image.isNull()) {
			painter.drawImage(0, 0, coverScaled(image, canvas));
			painter.fillRect(output.rect(), QColor(0, 0, 0, 120));
		} else if (bg.kind == BackgroundKind::Video) {
			/* The video plays underneath; only darken it for legibility. */
			painter.fillRect(output.rect(), QColor(0, 0, 0, 105));
		} else if (bg.kind == BackgroundKind::Color && QColor::isValidColorName(bg.color)) {
			painter.fillRect(output.rect(), QColor::fromString(bg.color));
		} else {
			drawPreset(painter, canvas, bg.preset);
		}

		const QRectF textBox(W * 0.08, H * 0.09, W * 0.84, H * 0.66);
		textFits = drawFittedText(painter, slide, textBox, qRound(slide.maxFontSize * scale), qRound(34 * scale),
					  3.0 * scale);

		/* Divider and reference. */
		const qreal dividerY = H * 0.815;
		painter.setPen(Qt::NoPen);
		painter.setBrush(AccentColor);
		painter.drawRoundedRect(QRectF(W * 0.5 - W * 0.035, dividerY, W * 0.07, qMax(2.0, 3.0 * scale)), 2, 2);
		drawReference(painter, slide, QRectF(0, H * 0.84, W, H * 0.08), qRound(40 * scale), Qt::AlignHCenter);
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

		const QRectF textBox(band.left() + band.width() * 0.04, band.top() + band.height() * 0.10,
				     band.width() * 0.92, band.height() * 0.62);
		textFits = drawFittedText(painter, slide, textBox, qRound(slide.maxFontSize * 0.75 * scale),
					  qRound(26 * scale), 2.0 * scale);
		drawReference(painter, slide,
			      QRectF(band.left(), band.top() + band.height() * 0.76, band.width(), band.height() * 0.18),
			      qRound(30 * scale), Qt::AlignHCenter);
	}

	painter.end();
	if (fits)
		*fits = textFits;
	return output;
}

} // namespace WorshipRender
