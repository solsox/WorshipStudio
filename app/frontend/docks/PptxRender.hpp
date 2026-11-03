#pragma once

#include <QImage>
#include <QString>
#include <QVector>

#include <functional>

/* Draws PowerPoint (.pptx) slides inside WorshipStudio, without PowerPoint,
 * Keynote or LibreOffice: backgrounds (color, gradient, picture), pictures,
 * text boxes with their fonts, sizes, colors, alignment and bullets, simple
 * shapes and groups, with what each slide inherits from its layout, master
 * and theme. Charts, SmartArt and animations are left out (the slide shows
 * everything else). */
namespace PptxRender {

/* Whether the file is a PowerPoint Open XML presentation (.pptx, .ppsx). */
bool CanRender(const QString &path);

/* Every slide, `width` pixels wide; empty with `error` set on failure. */
QVector<QImage> RenderSlides(const QString &path, int width, QString *error = nullptr);

/* Writes the slides as a PDF (one page per slide) for the presentation
 * panel, which reads PDFs. */
bool ToPdf(const QString &path, const QString &pdfPath, QString *error = nullptr,
	   std::function<void(int done, int total)> progress = {});

} // namespace PptxRender
