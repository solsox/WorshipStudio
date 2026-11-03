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

#include "PptxRender.hpp"

#include <QtCore/private/qzipreader_p.h>

#include <QAbstractTextDocumentLayout>
#include <QBuffer>
#include <QCoreApplication>
#include <QFile>
#include <QDomDocument>
#include <QFileInfo>
#include <QHash>
#include <QLinearGradient>
#include <QPainter>
#include <QPainterPath>
#include <QTextBlock>
#include <QThread>
#include <QTextCursor>
#include <QTextDocument>
#include <QtMath>

#include <atomic>
#include <memory>
#include <thread>
#include <vector>

namespace {
constexpr double EmuPerPoint = 12700.0;

/* ---------------------------------------------------------------- parts */

QString resolvePath(const QString &base, const QString &target)
{
	if (target.startsWith(QLatin1Char('/')))
		return target.mid(1);
	QStringList parts = base.section(QLatin1Char('/'), 0, -2).split(QLatin1Char('/'), Qt::SkipEmptyParts);
	for (const QString &piece : target.split(QLatin1Char('/'))) {
		if (piece == QStringLiteral(".."))
			parts.removeLast();
		else if (piece != QStringLiteral("."))
			parts << piece;
	}
	return parts.join(QLatin1Char('/'));
}

struct Part {
	QString path;
	QDomDocument xml;
	QHash<QString, QString> rels;     /* id -> part path */
	QHash<QString, QString> relTypes; /* id -> type (last word) */
};

class Package {
public:
	explicit Package(const QString &file) : zip(file) {}
	bool ok() const { return zip.status() == QZipReader::NoError && zip.exists(); }

	QByteArray data(const QString &path) { return zip.fileData(path); }

	std::shared_ptr<Part> part(const QString &path)
	{
		if (path.isEmpty())
			return nullptr;
		if (auto found = parts.value(path))
			return found;
		auto result = std::make_shared<Part>();
		result->path = path;
		const QByteArray bytes = zip.fileData(path);
		if (bytes.isEmpty() || !result->xml.setContent(bytes, QDomDocument::ParseOption::UseNamespaceProcessing))
			return nullptr;
		const QString relsPath = path.section(QLatin1Char('/'), 0, -2) + QStringLiteral("/_rels/") +
					 path.section(QLatin1Char('/'), -1) + QStringLiteral(".rels");
		QDomDocument rels;
		if (rels.setContent(zip.fileData(relsPath))) {
			for (QDomElement rel = rels.documentElement().firstChildElement(); !rel.isNull();
			     rel = rel.nextSiblingElement()) {
				const QString id = rel.attribute(QStringLiteral("Id"));
				if (rel.attribute(QStringLiteral("TargetMode")) == QStringLiteral("External"))
					continue;
				result->rels.insert(id, resolvePath(path, rel.attribute(QStringLiteral("Target"))));
				result->relTypes.insert(id, rel.attribute(QStringLiteral("Type")).section(QLatin1Char('/'), -1));
			}
		}
		parts.insert(path, result);
		return result;
	}

	/* The first related part of a type ("slideLayout", "slideMaster", "theme"). */
	std::shared_ptr<Part> related(const std::shared_ptr<Part> &from, const QString &type)
	{
		if (!from)
			return nullptr;
		for (auto it = from->relTypes.constBegin(); it != from->relTypes.constEnd(); ++it) {
			if (it.value() == type)
				return part(from->rels.value(it.key()));
		}
		return nullptr;
	}

	QImage image(const std::shared_ptr<Part> &from, const QString &id)
	{
		if (!from)
			return {};
		const QString path = from->rels.value(id);
		if (path.isEmpty())
			return {};
		if (images.contains(path))
			return images.value(path);
		QImage result = QImage::fromData(zip.fileData(path));
		images.insert(path, result);
		return result;
	}

private:
	QZipReader zip;
	QHash<QString, std::shared_ptr<Part>> parts;
	QHash<QString, QImage> images;
};

/* --------------------------------------------------------------- helpers */

QDomElement child(const QDomElement &parent, const char *name)
{
	return parent.isNull() ? QDomElement() : parent.firstChildElement(QLatin1String(name));
}

QDomElement path(QDomElement element, std::initializer_list<const char *> names)
{
	for (const char *name : names)
		element = child(element, name);
	return element;
}

qint64 attr(const QDomElement &element, const char *name, qint64 fallback = 0)
{
	bool ok = false;
	const qint64 value = element.attribute(QLatin1String(name)).toLongLong(&ok);
	return ok ? value : fallback;
}

struct Theme {
	QHash<QString, QColor> colors;
	QString majorFont;
	QString minorFont;
};

Theme readTheme(const std::shared_ptr<Part> &theme)
{
	Theme result;
	result.majorFont = QStringLiteral("Calibri Light");
	result.minorFont = QStringLiteral("Calibri");
	if (!theme)
		return result;
	const QDomElement elements = path(theme->xml.documentElement(), {"themeElements"});
	for (QDomElement color = path(elements, {"clrScheme"}).firstChildElement(); !color.isNull();
	     color = color.nextSiblingElement()) {
		const QDomElement value = color.firstChildElement();
		const QString hex = value.localName() == QStringLiteral("sysClr") ? value.attribute(QStringLiteral("lastClr"))
										 : value.attribute(QStringLiteral("val"));
		result.colors.insert(color.localName(), QColor(QLatin1Char('#') + hex));
	}
	const QDomElement fonts = path(elements, {"fontScheme"});
	const QString major = path(fonts, {"majorFont", "latin"}).attribute(QStringLiteral("typeface"));
	const QString minor = path(fonts, {"minorFont", "latin"}).attribute(QStringLiteral("typeface"));
	if (!major.isEmpty())
		result.majorFont = major;
	if (!minor.isEmpty())
		result.minorFont = minor;
	return result;
}

/* A color element (srgbClr, schemeClr...) with its modifiers. */
QColor colorOf(const QDomElement &element, const Theme &theme)
{
	if (element.isNull())
		return {};
	QColor color;
	const QString kind = element.localName();
	const QString val = element.attribute(QStringLiteral("val"));
	if (kind == QStringLiteral("srgbClr")) {
		color = QColor(QLatin1Char('#') + val);
	} else if (kind == QStringLiteral("schemeClr")) {
		static const QHash<QString, QString> map{{QStringLiteral("bg1"), QStringLiteral("lt1")},
							 {QStringLiteral("tx1"), QStringLiteral("dk1")},
							 {QStringLiteral("bg2"), QStringLiteral("lt2")},
							 {QStringLiteral("tx2"), QStringLiteral("dk2")}};
		color = theme.colors.value(map.value(val, val), Qt::black);
	} else if (kind == QStringLiteral("sysClr")) {
		color = QColor(QLatin1Char('#') + element.attribute(QStringLiteral("lastClr"), QStringLiteral("000000")));
	} else if (kind == QStringLiteral("prstClr")) {
		color = QColor(val);
	} else if (kind == QStringLiteral("scrgbClr")) {
		color = QColor::fromRgbF(attr(element, "r") / 100000.0, attr(element, "g") / 100000.0,
					 attr(element, "b") / 100000.0);
	}
	if (!color.isValid())
		return {};
	for (QDomElement mod = element.firstChildElement(); !mod.isNull(); mod = mod.nextSiblingElement()) {
		const double amount = attr(mod, "val") / 100000.0;
		const QString name = mod.localName();
		float h, s, l, a;
		color.getHslF(&h, &s, &l, &a);
		if (name == QStringLiteral("lumMod"))
			l *= amount;
		else if (name == QStringLiteral("lumOff"))
			l += amount;
		else if (name == QStringLiteral("alpha"))
			a = amount;
		else if (name == QStringLiteral("shade"))
			l *= amount;
		else if (name == QStringLiteral("tint"))
			l = l + (1.0 - l) * (1.0 - amount);
		else
			continue;
		color = QColor::fromHslF(h < 0 ? 0 : h, s, qBound(0.0f, l, 1.0f), qBound(0.0f, a, 1.0f));
	}
	return color;
}

QColor colorIn(const QDomElement &parent, const Theme &theme)
{
	return parent.isNull() ? QColor() : colorOf(parent.firstChildElement(), theme);
}

/* ---------------------------------------------------------------- slides */

struct Box {
	double x = 0, y = 0, w = 0, h = 0; /* EMU */
	double rot = 0;                     /* degrees */
	bool valid = false;
};

/* Maps a group's child coordinates to the slide. */
struct GroupMap {
	double ox = 0, oy = 0, sx = 1, sy = 1;
	Box apply(const Box &box) const
	{
		Box out = box;
		out.x = ox + box.x * sx;
		out.y = oy + box.y * sy;
		out.w = box.w * sx;
		out.h = box.h * sy;
		return out;
	}
};

Box boxOf(const QDomElement &xfrm)
{
	Box box;
	if (xfrm.isNull())
		return box;
	const QDomElement off = child(xfrm, "off");
	const QDomElement ext = child(xfrm, "ext");
	if (off.isNull() || ext.isNull())
		return box;
	box.x = attr(off, "x");
	box.y = attr(off, "y");
	box.w = attr(ext, "cx");
	box.h = attr(ext, "cy");
	box.rot = attr(xfrm, "rot") / 60000.0;
	box.valid = true;
	return box;
}

class SlideRenderer {
public:
	SlideRenderer(Package &package, const std::shared_ptr<Part> &slide, double slideW, double slideH, int width)
		: pkg(package),
		  slidePart(slide),
		  slideW(slideW),
		  slideH(slideH)
	{
		layoutPart = pkg.related(slidePart, QStringLiteral("slideLayout"));
		masterPart = pkg.related(layoutPart, QStringLiteral("slideMaster"));
		theme = readTheme(pkg.related(masterPart, QStringLiteral("theme")));
		scale = width / slideW;
		image = QImage(width, qMax(1, qRound(slideH * scale)), QImage::Format_ARGB32_Premultiplied);
	}

	QImage render()
	{
		image.fill(Qt::white);
		QPainter painter(&image);
		painter.setRenderHints(QPainter::Antialiasing | QPainter::TextAntialiasing | QPainter::SmoothPixmapTransform);
		p = &painter;
		drawBackground();
		const auto showsMaster = [](const std::shared_ptr<Part> &part) {
			return !part || part->xml.documentElement().attribute(QStringLiteral("showMasterSp")) != QStringLiteral("0");
		};
		if (showsMaster(slidePart) && showsMaster(layoutPart))
			drawTree(masterPart, false);
		if (showsMaster(slidePart))
			drawTree(layoutPart, false);
		drawTree(slidePart, true);
		painter.end();
		return image;
	}

private:
	Package &pkg;
	std::shared_ptr<Part> slidePart, layoutPart, masterPart;
	Theme theme;
	double slideW, slideH, scale = 1;
	QImage image;
	QPainter *p = nullptr;

	QRectF toPixels(const Box &box) const { return QRectF(box.x * scale, box.y * scale, box.w * scale, box.h * scale); }

	QDomElement cSld(const std::shared_ptr<Part> &part) const
	{
		return part ? child(part->xml.documentElement(), "cSld") : QDomElement();
	}

	/* --- backgrounds and fills --- */

	bool paintFill(const QDomElement &fillParent, const QRectF &rect, const std::shared_ptr<Part> &part,
		       const QPainterPath *shape = nullptr)
	{
		for (QDomElement fill = fillParent.firstChildElement(); !fill.isNull(); fill = fill.nextSiblingElement()) {
			const QString kind = fill.localName();
			QBrush brush;
			if (kind == QStringLiteral("noFill")) {
				return true;
			} else if (kind == QStringLiteral("solidFill")) {
				brush = QBrush(colorIn(fill, theme));
			} else if (kind == QStringLiteral("gradFill")) {
				QLinearGradient gradient;
				const double angle = attr(child(fill, "lin"), "ang") / 60000.0;
				const QPointF c = rect.center();
				const double r = qSqrt(rect.width() * rect.width() + rect.height() * rect.height()) / 2.0;
				const double dx = qCos(qDegreesToRadians(angle)) * r, dy = qSin(qDegreesToRadians(angle)) * r;
				gradient = QLinearGradient(c - QPointF(dx, dy), c + QPointF(dx, dy));
				for (QDomElement stop = child(fill, "gsLst").firstChildElement(); !stop.isNull();
				     stop = stop.nextSiblingElement())
					gradient.setColorAt(qBound(0.0, attr(stop, "pos") / 100000.0, 1.0),
							    colorOf(stop.firstChildElement(), theme));
				brush = QBrush(gradient);
			} else if (kind == QStringLiteral("blipFill")) {
				const QImage picture = pkg.image(part, path(fill, {"blip"}).attribute(QStringLiteral("embed")));
				if (picture.isNull())
					continue;
				p->save();
				if (shape)
					p->setClipPath(*shape);
				p->drawImage(rect, picture);
				p->restore();
				return true;
			} else {
				continue;
			}
			if (shape)
				p->fillPath(*shape, brush);
			else
				p->fillRect(rect, brush);
			return true;
		}
		return false;
	}

	void drawBackground()
	{
		const QRectF all(0, 0, image.width(), image.height());
		for (const auto &part : {slidePart, layoutPart, masterPart}) {
			const QDomElement bg = child(cSld(part), "bg");
			if (bg.isNull())
				continue;
			const QDomElement bgPr = child(bg, "bgPr");
			if (!bgPr.isNull() && paintFill(bgPr, all, part))
				return;
			const QDomElement bgRef = child(bg, "bgRef");
			if (!bgRef.isNull()) {
				const QColor color = colorOf(bgRef.firstChildElement(), theme);
				if (color.isValid()) {
					p->fillRect(all, color);
					return;
				}
			}
		}
	}

	/* --- placeholders --- */

	struct Placeholder {
		QString type;
		QString idx;
	};

	Placeholder placeholderOf(const QDomElement &shape) const
	{
		QDomElement nv = shape.firstChildElement(); /* nvSpPr / nvPicPr */
		const QDomElement ph = path(nv, {"nvPr", "ph"});
		if (ph.isNull())
			return {};
		return {ph.attribute(QStringLiteral("type"), QStringLiteral("body")), ph.attribute(QStringLiteral("idx"))};
	}

	/* The matching placeholder shape in the layout or the master. */
	QDomElement inherited(const std::shared_ptr<Part> &part, const Placeholder &ph) const
	{
		const QDomElement tree = child(cSld(part), "spTree");
		QDomElement byType;
		for (QDomElement shape = tree.firstChildElement(); !shape.isNull(); shape = shape.nextSiblingElement()) {
			const Placeholder other = placeholderOf(shape);
			if (other.type.isEmpty() && other.idx.isEmpty())
				continue;
			if (!ph.idx.isEmpty() && other.idx == ph.idx)
				return shape;
			const auto family = [](const QString &type) {
				return type == QStringLiteral("ctrTitle") ? QStringLiteral("title")
				       : type == QStringLiteral("subTitle") || type == QStringLiteral("obj")
					       ? QStringLiteral("body")
					       : type;
			};
			if (byType.isNull() && family(other.type) == family(ph.type))
				byType = shape;
		}
		return byType;
	}

	Box placeholderBox(const Placeholder &ph) const
	{
		for (const auto &part : {layoutPart, masterPart}) {
			const QDomElement shape = inherited(part, ph);
			const Box box = boxOf(path(shape, {"spPr", "xfrm"}));
			if (box.valid)
				return box;
		}
		return {};
	}

	/* --- shapes --- */

	void drawTree(const std::shared_ptr<Part> &part, bool placeholders, const GroupMap &map = {},
		      QDomElement tree = QDomElement())
	{
		if (!part)
			return;
		if (tree.isNull())
			tree = child(cSld(part), "spTree");
		for (QDomElement shape = tree.firstChildElement(); !shape.isNull(); shape = shape.nextSiblingElement()) {
			const QString kind = shape.localName();
			const Placeholder ph = placeholderOf(shape);
			const bool isPlaceholder = !ph.type.isEmpty() || !ph.idx.isEmpty();
			/* Layout and master placeholders are only prompts ("Click to add title"). */
			if (isPlaceholder && !placeholders)
				continue;
			if (kind == QStringLiteral("grpSp")) {
				const QDomElement xfrm = path(shape, {"grpSpPr", "xfrm"});
				const Box outer = map.apply(boxOf(xfrm));
				const QDomElement chOff = child(xfrm, "chOff"), chExt = child(xfrm, "chExt");
				GroupMap inner;
				const double chW = attr(chExt, "cx", 1), chH = attr(chExt, "cy", 1);
				inner.sx = chW ? outer.w / chW : 1;
				inner.sy = chH ? outer.h / chH : 1;
				inner.ox = outer.x - attr(chOff, "x") * inner.sx;
				inner.oy = outer.y - attr(chOff, "y") * inner.sy;
				drawTree(part, placeholders, inner, shape);
			} else if (kind == QStringLiteral("sp") || kind == QStringLiteral("pic") ||
				   kind == QStringLiteral("cxnSp")) {
				Box box = boxOf(path(shape, {"spPr", "xfrm"}));
				if (!box.valid && isPlaceholder)
					box = placeholderBox(ph);
				if (!box.valid)
					continue;
				drawShape(part, shape, kind, map.apply(box), ph);
			}
		}
	}

	void drawShape(const std::shared_ptr<Part> &part, const QDomElement &shape, const QString &kind, const Box &box,
		       const Placeholder &ph)
	{
		const QRectF rect = toPixels(box);
		p->save();
		if (box.rot != 0) {
			p->translate(rect.center());
			p->rotate(box.rot);
			p->translate(-rect.center());
		}
		const QDomElement spPr = child(shape, "spPr");
		const QString geometry = path(spPr, {"prstGeom"}).attribute(QStringLiteral("prst"), QStringLiteral("rect"));
		QPainterPath outline;
		if (geometry == QStringLiteral("ellipse"))
			outline.addEllipse(rect);
		else if (geometry == QStringLiteral("roundRect"))
			outline.addRoundedRect(rect, qMin(rect.width(), rect.height()) * 0.166,
					       qMin(rect.width(), rect.height()) * 0.166);
		else
			outline.addRect(rect);

		if (kind == QStringLiteral("pic")) {
			const QDomElement blipFill = child(shape, "blipFill");
			QImage picture = pkg.image(part, path(blipFill, {"blip"}).attribute(QStringLiteral("embed")));
			if (!picture.isNull()) {
				const QDomElement crop = child(blipFill, "srcRect");
				QRectF source(0, 0, picture.width(), picture.height());
				if (!crop.isNull()) {
					const double l = attr(crop, "l") / 100000.0, t = attr(crop, "t") / 100000.0,
						     r = attr(crop, "r") / 100000.0, b = attr(crop, "b") / 100000.0;
					source = QRectF(picture.width() * l, picture.height() * t,
							picture.width() * (1 - l - r), picture.height() * (1 - t - b));
				}
				p->setClipPath(outline);
				p->drawImage(rect, picture, source);
			}
		} else if (kind == QStringLiteral("cxnSp")) {
			const QColor color = colorIn(path(spPr, {"ln", "solidFill"}), theme);
			if (color.isValid()) {
				p->setPen(QPen(color, qMax(1.0, attr(child(spPr, "ln"), "w", 12700) * scale)));
				p->drawLine(rect.topLeft(), rect.bottomRight());
			}
		} else {
			bool filled = paintFill(spPr, rect, part, &outline);
			if (!filled) {
				/* Theme style reference (fillRef) of the shape. */
				const QDomElement fillRef = path(shape, {"style", "fillRef"});
				if (!fillRef.isNull() && attr(fillRef, "idx") > 0) {
					const QColor color = colorOf(fillRef.firstChildElement(), theme);
					if (color.isValid())
						p->fillPath(outline, color);
				}
			}
			const QDomElement ln = child(spPr, "ln");
			const QColor line = colorIn(child(ln, "solidFill"), theme);
			if (line.isValid() && child(ln, "noFill").isNull()) {
				p->setPen(QPen(line, qMax(1.0, attr(ln, "w", 12700) * scale)));
				p->setBrush(Qt::NoBrush);
				p->drawPath(outline);
			}
			drawText(shape, rect, ph);
		}
		p->restore();
	}

	/* --- text --- */

	/* Default run properties for a paragraph level, from the master styles
	 * and the layout/master placeholder. */
	QDomElement levelStyle(const QDomElement &lstStyle, int level) const
	{
		return child(lstStyle, QStringLiteral("lvl%1pPr").arg(level).toLatin1().constData());
	}

	QList<QDomElement> styleChain(const QDomElement &shape, const Placeholder &ph, int level) const
	{
		QList<QDomElement> chain; /* most specific first */
		chain << levelStyle(path(shape, {"txBody", "lstStyle"}), level);
		if (!ph.type.isEmpty() || !ph.idx.isEmpty()) {
			for (const auto &part : {layoutPart, masterPart})
				chain << levelStyle(path(inherited(part, ph), {"txBody", "lstStyle"}), level);
			const QDomElement styles = child(masterPart ? masterPart->xml.documentElement() : QDomElement(), "txStyles");
			const bool title = ph.type == QStringLiteral("title") || ph.type == QStringLiteral("ctrTitle");
			const bool body = !title && ph.type != QStringLiteral("dt") && ph.type != QStringLiteral("ftr") &&
					  ph.type != QStringLiteral("sldNum");
			chain << levelStyle(child(styles, title ? "titleStyle" : body ? "bodyStyle" : "otherStyle"), level);
		} else {
			const QDomElement styles = child(masterPart ? masterPart->xml.documentElement() : QDomElement(), "txStyles");
			chain << levelStyle(child(styles, "otherStyle"), level);
		}
		return chain;
	}

	template<typename F> QString firstAttr(const QList<QDomElement> &chain, F get) const
	{
		for (const QDomElement &element : chain) {
			const QString value = get(element);
			if (!value.isEmpty())
				return value;
		}
		return {};
	}

	void drawText(const QDomElement &shape, const QRectF &rect, const Placeholder &ph)
	{
		const QDomElement txBody = child(shape, "txBody");
		if (txBody.isNull())
			return;
		const QDomElement bodyPr = child(txBody, "bodyPr");
		const double left = attr(bodyPr, "lIns", 91440) * scale, right = attr(bodyPr, "rIns", 91440) * scale;
		const double top = attr(bodyPr, "tIns", 45720) * scale, bottom = attr(bodyPr, "bIns", 45720) * scale;
		const QRectF area = rect.adjusted(left, top, -right, -bottom);
		if (area.width() <= 2)
			return;
		const QDomElement autofit = child(bodyPr, "normAutofit");
		double fontScale = autofit.isNull() ? 1.0 : attr(autofit, "fontScale", 100000) / 100000.0;
		const bool title = ph.type == QStringLiteral("title") || ph.type == QStringLiteral("ctrTitle");

		const auto build = [&](double extraScale) {
			auto doc = std::make_unique<QTextDocument>();
			doc->documentLayout()->setPaintDevice(&image);
			doc->setDocumentMargin(0);
			doc->setTextWidth(area.width());
			QTextCursor cursor(doc.get());
			bool first = true;
			bool any = false;
			for (QDomElement para = child(txBody, "p"); !para.isNull(); para = para.nextSiblingElement(QStringLiteral("p"))) {
				const QDomElement pPr = child(para, "pPr");
				const int level = (int)attr(pPr, "lvl", 0) + 1;
				QList<QDomElement> chain = styleChain(shape, ph, level);
				chain.prepend(pPr);
				QTextBlockFormat block;
				const QString algn = firstAttr(chain, [](const QDomElement &e) { return e.attribute(QStringLiteral("algn")); });
				block.setAlignment(algn == QStringLiteral("ctr") ? Qt::AlignHCenter
						   : algn == QStringLiteral("r") ? Qt::AlignRight
						   : algn == QStringLiteral("just") ? Qt::AlignJustify
										    : Qt::AlignLeft);
				const QString spcBef = firstAttr(chain, [](const QDomElement &e) {
					return path(e, {"spcBef", "spcPts"}).attribute(QStringLiteral("val"));
				});
				if (!spcBef.isEmpty())
					block.setTopMargin(spcBef.toDouble() / 100.0 * EmuPerPoint * scale * fontScale * extraScale);
				const QString lnSpc = firstAttr(chain, [](const QDomElement &e) {
					return path(e, {"lnSpc", "spcPct"}).attribute(QStringLiteral("val"));
				});
				if (!lnSpc.isEmpty())
					block.setLineHeight(qBound(60.0, lnSpc.toDouble() / 1000.0, 300.0),
							    QTextBlockFormat::ProportionalHeight);
				const qint64 marL = firstAttr(chain, [](const QDomElement &e) { return e.attribute(QStringLiteral("marL")); }).toLongLong();
				const qint64 indent = firstAttr(chain, [](const QDomElement &e) { return e.attribute(QStringLiteral("indent")); }).toLongLong();
				block.setLeftMargin(marL * scale);
				block.setTextIndent(indent * scale);
				if (first)
					cursor.setBlockFormat(block);
				else
					cursor.insertBlock(block);
				first = false;

				/* Defaults of the level, then each run. */
				const auto runFormat = [&](const QDomElement &rPr) {
					QList<QDomElement> runChain{rPr};
					for (const QDomElement &level : chain)
						runChain << child(level, "defRPr");
					QTextCharFormat format;
					const double size = firstAttr(runChain, [](const QDomElement &e) { return e.attribute(QStringLiteral("sz")); })
								    .toDouble();
					const double points = (size > 0 ? size / 100.0 : 18.0) * fontScale * extraScale;
					format.setFont(QFont());
					QString face = firstAttr(runChain, [](const QDomElement &e) {
						return child(e, "latin").attribute(QStringLiteral("typeface"));
					});
					if (face.isEmpty() || face == QStringLiteral("+mn-lt"))
						face = title ? theme.majorFont : theme.minorFont;
					else if (face == QStringLiteral("+mj-lt"))
						face = theme.majorFont;
					format.setFontFamilies({face});
					format.setFontPointSize(qMax(1.0, points * EmuPerPoint * scale * 0.75));
					format.setFontWeight(firstAttr(runChain, [](const QDomElement &e) { return e.attribute(QStringLiteral("b")); }) ==
									     QStringLiteral("1")
								     ? QFont::Bold
								     : QFont::Normal);
					format.setFontItalic(firstAttr(runChain, [](const QDomElement &e) { return e.attribute(QStringLiteral("i")); }) ==
							     QStringLiteral("1"));
					const QString underline = firstAttr(runChain, [](const QDomElement &e) { return e.attribute(QStringLiteral("u")); });
					format.setFontUnderline(!underline.isEmpty() && underline != QStringLiteral("none"));
					QColor color;
					for (const QDomElement &e : runChain) {
						color = colorIn(child(e, "solidFill"), theme);
						if (color.isValid())
							break;
					}
					if (!color.isValid()) {
						const QDomElement fontRef = path(shape, {"style", "fontRef"});
						color = colorOf(fontRef.firstChildElement(), theme);
					}
					format.setForeground(color.isValid() ? color : theme.colors.value(QStringLiteral("dk1"), Qt::black));
					return format;
				};

				/* Bullets of the level. */
				const QString bullet = firstAttr(chain, [](const QDomElement &e) {
					return child(e, "buNone").isNull() ? child(e, "buChar").attribute(QStringLiteral("char"))
									   : QStringLiteral("\x01");
				});
				bool hasText = false;
				for (QDomElement run = para.firstChildElement(); !run.isNull(); run = run.nextSiblingElement())
					if (run.localName() == QStringLiteral("r") || run.localName() == QStringLiteral("fld"))
						hasText = true;
				if (hasText && !bullet.isEmpty() && bullet != QStringLiteral("\x01"))
					cursor.insertText(bullet + QStringLiteral(" "), runFormat(child(child(para, "r"), "rPr")));

				for (QDomElement run = para.firstChildElement(); !run.isNull(); run = run.nextSiblingElement()) {
					const QString kind = run.localName();
					if (kind == QStringLiteral("r") || kind == QStringLiteral("fld")) {
						cursor.insertText(child(run, "t").text(), runFormat(child(run, "rPr")));
						any = true;
					} else if (kind == QStringLiteral("br")) {
						cursor.insertText(QStringLiteral(" "), runFormat(child(run, "rPr")));
					}
				}
				if (!hasText) {
					/* Empty paragraph: keep its height. */
					QTextCharFormat format = runFormat(child(para, "endParaRPr"));
					cursor.setCharFormat(format);
					cursor.setBlockCharFormat(format);
				}
			}
			return any ? std::move(doc) : std::unique_ptr<QTextDocument>();
		};

		std::unique_ptr<QTextDocument> doc = build(1.0);
		if (!doc)
			return;
		/* Text that was shrunk to fit in PowerPoint is shrunk here too. */
		double extra = 1.0;
		while (!autofit.isNull() && doc->size().height() > area.height() && extra > 0.4) {
			extra -= 0.08;
			doc = build(extra);
		}
		const QString anchor = bodyPr.attribute(QStringLiteral("anchor"), title ? QStringLiteral("ctr") : QStringLiteral("t"));
		const double height = doc->size().height();
		double y = area.top();
		if (anchor == QStringLiteral("ctr"))
			y = area.top() + (area.height() - height) / 2.0;
		else if (anchor == QStringLiteral("b"))
			y = area.bottom() - height;
		p->save();
		p->translate(area.left(), y);
		doc->drawContents(p);
		p->restore();
	}
};

struct Presentation {
	double width = 9144000, height = 5143500; /* EMU, 16:9 by default */
	QStringList slides;
};

bool readPresentation(Package &pkg, Presentation *presentation, QString *error)
{
	auto main = pkg.part(QStringLiteral("ppt/presentation.xml"));
	if (!main) {
		if (error)
			*error = QCoreApplication::translate("WorshipStudio", "El archivo no es una presentación de PowerPoint.");
		return false;
	}
	const QDomElement root = main->xml.documentElement();
	const QDomElement size = child(root, "sldSz");
	if (!size.isNull()) {
		presentation->width = attr(size, "cx", 9144000);
		presentation->height = attr(size, "cy", 5143500);
	}
	for (QDomElement id = path(root, {"sldIdLst"}).firstChildElement(); !id.isNull(); id = id.nextSiblingElement()) {
		QString rid;
		const QDomNamedNodeMap attributes = id.attributes();
		for (int i = 0; i < attributes.count(); ++i) {
			const QDomAttr a = attributes.item(i).toAttr();
			if (a.localName() == QStringLiteral("id") && a.namespaceURI().contains(QStringLiteral("relationships")))
				rid = a.value();
		}
		const QString slide = main->rels.value(rid);
		if (!slide.isEmpty() &&
		    id.attribute(QStringLiteral("show")) != QStringLiteral("0"))
			presentation->slides << slide;
	}
	if (presentation->slides.isEmpty()) {
		if (error)
			*error = QCoreApplication::translate("WorshipStudio", "La presentación no tiene diapositivas.");
		return false;
	}
	return true;
}
} // namespace

namespace PptxRender {

bool CanRender(const QString &file)
{
	const QString suffix = QFileInfo(file).suffix().toLower();
	return suffix == QStringLiteral("pptx") || suffix == QStringLiteral("ppsx") || suffix == QStringLiteral("potx") ||
	       suffix == QStringLiteral("pptm");
}

QVector<QImage> RenderSlides(const QString &file, int width, QString *error)
{
	QVector<QImage> images;
	Package pkg(file);
	if (!pkg.ok()) {
		if (error)
			*error = QCoreApplication::translate("WorshipStudio", "No se pudo abrir el archivo.");
		return images;
	}
	Presentation presentation;
	if (!readPresentation(pkg, &presentation, error))
		return images;
	for (const QString &slidePath : presentation.slides) {
		auto slide = pkg.part(slidePath);
		if (!slide)
			continue;
		SlideRenderer renderer(pkg, slide, presentation.width, presentation.height, width);
		images.push_back(renderer.render());
	}
	return images;
}

bool ToPdf(const QString &file, const QString &pdfPath, QString *error, std::function<void(int, int)> progress)
{
	/* Slides are drawn in parallel, one package reader per thread (the zip
	 * reader is not shared between threads), and kept as JPEG. */
	int count = 0;
	double slideW = 0, slideH = 0;
	{
		Package pkg(file);
		Presentation presentation;
		if (!pkg.ok() || !readPresentation(pkg, &presentation, error)) {
			if (error && error->isEmpty())
				*error = QCoreApplication::translate("WorshipStudio", "No se pudo abrir el archivo.");
			return false;
		}
		count = (int)presentation.slides.size();
		slideW = presentation.width;
		slideH = presentation.height;
	}
	const int width = 1920;
	const int height = qMax(1, qRound(slideH * width / slideW));
	QVector<QByteArray> jpegs(count);
	std::atomic<int> nextSlide{0}, done{0};
	const int threads = qBound(1, QThread::idealThreadCount(), 8);
	std::vector<std::thread> workers;
	for (int t = 0; t < threads; ++t) {
		workers.emplace_back([&]() {
			Package pkg(file);
			Presentation presentation;
			if (!pkg.ok() || !readPresentation(pkg, &presentation, nullptr))
				return;
			for (int i = nextSlide++; i < count; i = nextSlide++) {
				auto slide = pkg.part(presentation.slides.at(i));
				QImage image(width, height, QImage::Format_RGB32);
				image.fill(Qt::white);
				if (slide)
					image = SlideRenderer(pkg, slide, presentation.width, presentation.height, width).render();
				QBuffer buffer(&jpegs[i]);
				buffer.open(QIODevice::WriteOnly);
				image.convertToFormat(QImage::Format_RGB32).save(&buffer, "JPG", 88);
				const int finished = ++done;
				if (progress)
					progress(finished, count);
			}
		});
	}
	for (std::thread &worker : workers)
		worker.join();

	/* A small PDF by hand: one JPEG picture per page (PDF writing is not in
	 * every Qt build). Pages are in points: half the pixels (144 dpi). */
	QByteArray pdf("%PDF-1.4\n%\xE2\xE3\xCF\xD3\n");
	QVector<qint64> offsets;
	const auto object = [&](const QByteArray &body) {
		offsets << pdf.size();
		pdf += QByteArray::number(offsets.size()) + " 0 obj\n" + body + "\nendobj\n";
		return offsets.size();
	};
	/* 1: catalog, 2: pages (filled in at the end). */
	object("<< /Type /Catalog /Pages 2 0 R >>");
	offsets << 0;
	pdf += "";
	const int pagesIndex = (int)offsets.size() - 1;
	QByteArray kids;
	for (const QByteArray &jpeg : jpegs) {
		if (jpeg.isEmpty())
			continue;
		const QSize slide(width, height);
		const int imageId = object("<< /Type /XObject /Subtype /Image /Width " + QByteArray::number(slide.width()) +
					   " /Height " + QByteArray::number(slide.height()) +
					   " /ColorSpace /DeviceRGB /BitsPerComponent 8 /Filter /DCTDecode /Length " +
					   QByteArray::number(jpeg.size()) + " >>\nstream\n" + jpeg + "\nendstream");
		const QByteArray w = QByteArray::number(slide.width() / 2.0, 'f', 2);
		const QByteArray h = QByteArray::number(slide.height() / 2.0, 'f', 2);
		const QByteArray draw = "q " + w + " 0 0 " + h + " 0 0 cm /Im Do Q";
		const int contentId =
			object("<< /Length " + QByteArray::number(draw.size()) + " >>\nstream\n" + draw + "\nendstream");
		const int pageId = object("<< /Type /Page /Parent 2 0 R /MediaBox [0 0 " + w + " " + h +
					  "] /Resources << /XObject << /Im " + QByteArray::number(imageId) +
					  " 0 R >> >> /Contents " + QByteArray::number(contentId) + " 0 R >>");
		kids += QByteArray::number(pageId) + " 0 R ";
	}
	offsets[pagesIndex] = pdf.size();
	pdf += "2 0 obj\n<< /Type /Pages /Kids [" + kids + "] /Count " + QByteArray::number(kids.count(" 0 R")) +
	       " >>\nendobj\n";
	const qint64 xref = pdf.size();
	pdf += "xref\n0 " + QByteArray::number(offsets.size() + 1) + "\n0000000000 65535 f \n";
	for (qint64 offset : offsets)
		pdf += QByteArray::number(offset).rightJustified(10, '0') + " 00000 n \n";
	pdf += "trailer\n<< /Size " + QByteArray::number(offsets.size() + 1) + " /Root 1 0 R >>\nstartxref\n" +
	       QByteArray::number(xref) + "\n%%EOF\n";

	/* Written aside and renamed: a half written file is never read. */
	const QString part = pdfPath + QStringLiteral(".part");
	QFile out(part);
	if (!out.open(QIODevice::WriteOnly) || out.write(pdf) != pdf.size()) {
		if (error)
			*error = QCoreApplication::translate("WorshipStudio", "No se pudo guardar la presentación convertida.");
		return false;
	}
	out.close();
	QFile::remove(pdfPath);
	return QFile::rename(part, pdfPath);
}

} // namespace PptxRender
