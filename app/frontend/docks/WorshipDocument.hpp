#pragma once

#include <QImage>
#include <QObject>
#include <QSize>
#include <QSizeF>
#include <QString>
#include <QStringList>

#include <functional>

class QProcess;

/* Opens presentations (PDF, PowerPoint, Keynote, OpenDocument, images) and
 * renders their pages so they can be projected one slide at a time.
 *
 * PDF pages are drawn with the PDF engine of each system, so no extra
 * dependencies are needed:
 *  - macOS: CoreGraphics
 *  - Windows: Windows.Data.Pdf (Windows 10 or later)
 *  - Linux/FreeBSD: pdftoppm/pdfinfo from poppler-utils
 * PowerPoint (.pptx) is drawn inside WorshipStudio (PptxRender.hpp), so no
 * other program opens. Older .ppt, Keynote and OpenDocument files, or a
 * .pptx that cannot be read, are converted with the office suite installed
 * (PowerPoint, Keynote or LibreOffice). */
namespace WorshipDocument {

/* File dialog filter with every supported format. */
QString OpenFilter();

bool IsPdf(const QString &path);
bool IsImage(const QString &path);
/* PowerPoint, Keynote and OpenDocument presentations. */
bool NeedsConversion(const QString &path);

/* Page count of a PDF, 0 on error. */
int PdfPageCount(const QString &pdfPath, QString *error = nullptr);
/* Size of a PDF page in points (only the aspect ratio matters). */
QSizeF PdfPageSize(const QString &pdfPath, int page);
/* Renders a PDF page as big as possible inside `box`, on white. */
QImage RenderPdfPage(const QString &pdfPath, int page, const QSize &box);

/* Converts office presentations to PDF in the background. The result is
 * cached, so opening the same unchanged file again is immediate. */
class Converter : public QObject {
	Q_OBJECT

public:
	using Done = std::function<void(const QString &pdfPath, const QString &error)>;

	explicit Converter(QObject *parent = nullptr);
	~Converter() override;

	void convert(const QString &path, Done done);
	bool busy() const { return process != nullptr || nativeRunning; }
	void cancel();

	/* Human readable list of the converters found on this computer. */
	static QStringList availableConverters();

signals:
	void progress(const QString &message);

private:
	struct Attempt {
		QString name;
		QString program;
		QStringList arguments;
		QString output;
		int timeoutMs = 0;
	};

	QList<Attempt> attempts;
	QStringList failures;
	QString source;
	QString target;
	Done callback;
	QProcess *process = nullptr;
	/* The built-in PowerPoint reader was tried for this file. */
	bool nativeTried = false;
	bool nativeRunning = false;

	void tryNext();
	void finish(const QString &pdfPath, const QString &error);
};

} // namespace WorshipDocument
