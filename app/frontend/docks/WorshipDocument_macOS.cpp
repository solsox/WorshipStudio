#include "WorshipDocument.hpp"
#include <QCoreApplication>

#include <CoreGraphics/CoreGraphics.h>

#include <QDateTime>
#include <QFileInfo>
#include <QPainter>

namespace WorshipDocument {

namespace {
/* Pages of the same document are rendered one after another, so the last
 * opened document is kept around. Only used from the UI thread. */
struct OpenDocument {
	QString path;
	qint64 modified = 0;
	CGPDFDocumentRef document = nullptr;

	~OpenDocument() { CGPDFDocumentRelease(document); }
};

CGPDFDocumentRef documentFor(const QString &pdfPath)
{
	static OpenDocument cached;

	const QFileInfo info(pdfPath);
	const qint64 modified = info.lastModified().toMSecsSinceEpoch();
	if (cached.document && cached.path == info.absoluteFilePath() && cached.modified == modified)
		return cached.document;

	CGPDFDocumentRelease(cached.document);
	cached.document = nullptr;

	const QByteArray utf8 = info.absoluteFilePath().toUtf8();
	CFURLRef url = CFURLCreateFromFileSystemRepresentation(nullptr, (const UInt8 *)utf8.constData(),
							       utf8.size(), false);
	if (!url)
		return nullptr;
	cached.document = CGPDFDocumentCreateWithURL(url);
	CFRelease(url);

	if (cached.document && CGPDFDocumentIsEncrypted(cached.document) &&
	    !CGPDFDocumentIsUnlocked(cached.document) && !CGPDFDocumentUnlockWithPassword(cached.document, "")) {
		CGPDFDocumentRelease(cached.document);
		cached.document = nullptr;
	}

	cached.path = info.absoluteFilePath();
	cached.modified = modified;
	return cached.document;
}

/* Visible page size, with the page rotation applied. */
CGRect pageBox(CGPDFPageRef page)
{
	CGRect box = CGPDFPageGetBoxRect(page, kCGPDFCropBox);
	const int rotation = ((CGPDFPageGetRotationAngle(page) % 360) + 360) % 360;
	if (rotation == 90 || rotation == 270)
		box.size = CGSizeMake(box.size.height, box.size.width);
	return box;
}
} // namespace

int PdfPageCount(const QString &pdfPath, QString *error)
{
	CGPDFDocumentRef document = documentFor(pdfPath);
	if (!document) {
		if (error)
			*error = QCoreApplication::translate("WorshipStudio", "No se pudo abrir el PDF (puede estar dañado o protegido con clave).");
		return 0;
	}
	return (int)CGPDFDocumentGetNumberOfPages(document);
}

QSizeF PdfPageSize(const QString &pdfPath, int page)
{
	CGPDFDocumentRef document = documentFor(pdfPath);
	CGPDFPageRef pdfPage = document ? CGPDFDocumentGetPage(document, (size_t)page + 1) : nullptr;
	if (!pdfPage)
		return QSizeF(612, 792);
	const CGRect box = pageBox(pdfPage);
	return QSizeF(box.size.width, box.size.height);
}

QImage RenderPdfPage(const QString &pdfPath, int page, const QSize &box)
{
	CGPDFDocumentRef document = documentFor(pdfPath);
	CGPDFPageRef pdfPage = document ? CGPDFDocumentGetPage(document, (size_t)page + 1) : nullptr;
	if (!pdfPage)
		return {};

	const CGRect rect = pageBox(pdfPage);
	const QSize size = QSizeF(rect.size.width, rect.size.height).toSize().scaled(box, Qt::KeepAspectRatio);
	if (size.isEmpty())
		return {};

	QImage image(size, QImage::Format_ARGB32_Premultiplied);
	image.fill(Qt::white);

	CGColorSpaceRef colorSpace = CGColorSpaceCreateWithName(kCGColorSpaceSRGB);
	CGContextRef context = CGBitmapContextCreate(image.bits(), size.width(), size.height(), 8,
						     image.bytesPerLine(), colorSpace,
						     kCGImageAlphaPremultipliedFirst | kCGBitmapByteOrder32Little);
	CGColorSpaceRelease(colorSpace);
	if (!context)
		return {};

	CGContextSetInterpolationQuality(context, kCGInterpolationHigh);
	CGContextSetShouldAntialias(context, true);
	CGContextSetShouldSmoothFonts(context, true);

	/* The drawing transform applies the page rotation at 1:1 (it never
	 * scales up), the context scale then fills the bitmap. */
	const CGRect pageRect = CGRectMake(0, 0, rect.size.width, rect.size.height);
	const CGAffineTransform transform = CGPDFPageGetDrawingTransform(pdfPage, kCGPDFCropBox, pageRect, 0, true);
	CGContextScaleCTM(context, size.width() / rect.size.width, size.height() / rect.size.height);
	CGContextConcatCTM(context, transform);
	CGContextClipToRect(context, CGPDFPageGetBoxRect(pdfPage, kCGPDFCropBox));
	CGContextDrawPDFPage(context, pdfPage);
	CGContextRelease(context);

	return image;
}

} // namespace WorshipDocument
