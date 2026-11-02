/* WinRT headers go first: Qt defines macros (signals, slots, emit) that
 * must not leak into them. */
#include <winrt/Windows.Foundation.h>
#include <QCoreApplication>
#include <winrt/Windows.Data.Pdf.h>
#include <winrt/Windows.Storage.h>
#include <winrt/Windows.Storage.Streams.h>
#include <winrt/Windows.UI.h>

#include <combaseapi.h>

#include "WorshipDocument.hpp"

#include <QDateTime>
#include <QDir>
#include <QFileInfo>

#include <mutex>
#include <thread>
#include <vector>

namespace WorshipDocument {

namespace {
using winrt::Windows::Data::Pdf::PdfDocument;
using winrt::Windows::Data::Pdf::PdfPage;
using winrt::Windows::Data::Pdf::PdfPageRenderOptions;
using winrt::Windows::Storage::StorageFile;
using winrt::Windows::Storage::Streams::DataReader;
using winrt::Windows::Storage::Streams::InMemoryRandomAccessStream;

/* The UI thread is single-threaded (STA) and must not block on WinRT async
 * calls, so the work runs on a short-lived thread of the multi-threaded
 * apartment. The MTA is kept alive so the cached document stays valid. */
template<typename T, typename F> T inMta(F work, T fallback)
{
	static std::once_flag keepMta;
	std::call_once(keepMta, []() {
		CO_MTA_USAGE_COOKIE cookie;
		CoIncrementMTAUsage(&cookie);
	});

	T result = fallback;
	std::thread worker([&]() {
		try {
			winrt::init_apartment(winrt::apartment_type::multi_threaded);
			result = work();
		} catch (...) {
			result = fallback;
		}
		winrt::uninit_apartment();
	});
	worker.join();
	return result;
}

struct OpenDocument {
	QString path;
	qint64 modified = 0;
	PdfDocument document{nullptr};
};

/* Must run inside inMta(). */
PdfDocument documentFor(const QString &pdfPath)
{
	static OpenDocument cached;
	static std::mutex lock;
	std::lock_guard<std::mutex> guard(lock);

	const QFileInfo info(pdfPath);
	const qint64 modified = info.lastModified().toMSecsSinceEpoch();
	if (cached.document && cached.path == info.absoluteFilePath() && cached.modified == modified)
		return cached.document;

	cached.document = nullptr;
	const std::wstring native = QDir::toNativeSeparators(info.absoluteFilePath()).toStdWString();
	StorageFile file = StorageFile::GetFileFromPathAsync(winrt::hstring(native)).get();
	cached.document = PdfDocument::LoadFromFileAsync(file).get();
	cached.path = info.absoluteFilePath();
	cached.modified = modified;
	return cached.document;
}
} // namespace

int PdfPageCount(const QString &pdfPath, QString *error)
{
	const int count = inMta<int>([&]() { return (int)documentFor(pdfPath).PageCount(); }, 0);
	if (count <= 0 && error)
		*error = QCoreApplication::translate("WorshipStudio", "No se pudo abrir el PDF (puede estar dañado o protegido con clave).");
	return count;
}

QSizeF PdfPageSize(const QString &pdfPath, int page)
{
	return inMta<QSizeF>(
		[&]() {
			/* Size already includes the page rotation. */
			const auto size = documentFor(pdfPath).GetPage((uint32_t)page).Size();
			return QSizeF(size.Width, size.Height);
		},
		QSizeF(612, 792));
}

QImage RenderPdfPage(const QString &pdfPath, int page, const QSize &box)
{
	const QByteArray png = inMta<QByteArray>(
		[&]() {
			PdfPage pdfPage = documentFor(pdfPath).GetPage((uint32_t)page);
			const auto pageSize = pdfPage.Size();
			const QSize size = QSizeF(pageSize.Width, pageSize.Height).toSize().scaled(box, Qt::KeepAspectRatio);

			PdfPageRenderOptions options;
			options.DestinationWidth((uint32_t)qMax(1, size.width()));
			options.DestinationHeight((uint32_t)qMax(1, size.height()));
			options.BackgroundColor(winrt::Windows::UI::Color{255, 255, 255, 255});

			InMemoryRandomAccessStream stream;
			pdfPage.RenderToStreamAsync(stream, options).get();

			const uint32_t length = (uint32_t)stream.Size();
			stream.Seek(0);
			DataReader reader(stream);
			reader.LoadAsync(length).get();
			std::vector<uint8_t> bytes(length);
			reader.ReadBytes(bytes);
			return QByteArray((const char *)bytes.data(), (qsizetype)bytes.size());
		},
		QByteArray());

	return png.isEmpty() ? QImage() : QImage::fromData(png, "PNG");
}

} // namespace WorshipDocument
