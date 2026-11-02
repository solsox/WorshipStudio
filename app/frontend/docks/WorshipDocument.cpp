#include "WorshipDocument.hpp"
#include "WorshipRender.hpp"

#include <util/base.h>

#include <QCoreApplication>
#include <QCryptographicHash>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QImageReader>
#include <QProcess>
#include <QSettings>
#include <QStandardPaths>
#include <QTimer>
#include <QUrl>

#include <algorithm>

#include "moc_WorshipDocument.cpp"

namespace WorshipDocument {

namespace {
constexpr int ConversionTimeoutMs = 180000;

const QStringList &officeSuffixes()
{
	static const QStringList suffixes = {QStringLiteral("ppt"),  QStringLiteral("pptx"), QStringLiteral("pps"),
					     QStringLiteral("ppsx"), QStringLiteral("pptm"), QStringLiteral("key"),
					     QStringLiteral("odp")};
	return suffixes;
}

const QStringList &imageSuffixes()
{
	static const QStringList suffixes = {QStringLiteral("png"),  QStringLiteral("jpg"), QStringLiteral("jpeg"),
					     QStringLiteral("bmp"),  QStringLiteral("gif"), QStringLiteral("webp"),
					     QStringLiteral("tiff"), QStringLiteral("tif")};
	return suffixes;
}

QString suffixOf(const QString &path)
{
	return QFileInfo(path).suffix().toLower();
}

QString findLibreOffice()
{
	for (const char *name : {"soffice", "libreoffice"}) {
		const QString found = QStandardPaths::findExecutable(QString::fromLatin1(name));
		if (!found.isEmpty())
			return found;
	}

	QStringList candidates;
#if defined(__APPLE__)
	candidates << QStringLiteral("/Applications/LibreOffice.app/Contents/MacOS/soffice")
		   << QDir::homePath() + QStringLiteral("/Applications/LibreOffice.app/Contents/MacOS/soffice");
#elif defined(_WIN32)
	for (const char *env : {"ProgramFiles", "ProgramFiles(x86)", "ProgramW6432"}) {
		const QString base = qEnvironmentVariable(env);
		if (!base.isEmpty())
			candidates << QDir(base).absoluteFilePath(QStringLiteral("LibreOffice/program/soffice.exe"));
	}
#endif
	for (const QString &candidate : candidates) {
		if (QFileInfo(candidate).isExecutable())
			return candidate;
	}
	return {};
}

#if defined(__APPLE__)
bool appInstalled(const QString &bundleName)
{
	return QFileInfo::exists(QStringLiteral("/Applications/") + bundleName) ||
	       QFileInfo::exists(QStringLiteral("/System/Applications/") + bundleName) ||
	       QFileInfo::exists(QDir::homePath() + QStringLiteral("/Applications/") + bundleName);
}

/* Paths are passed as script arguments, so no quoting is needed. */
QStringList osascript(const QStringList &lines, const QStringList &arguments)
{
	QStringList args;
	for (const QString &line : lines)
		args << QStringLiteral("-e") << line;
	return args + arguments;
}
#endif

#if defined(_WIN32)
bool powerPointInstalled()
{
	QSettings key(QStringLiteral("HKEY_CLASSES_ROOT\\PowerPoint.Application"), QSettings::NativeFormat);
	return key.childGroups().contains(QStringLiteral("CLSID"), Qt::CaseInsensitive);
}

QString powerShellQuoted(QString text)
{
	return QStringLiteral("'") + text.replace(QStringLiteral("'"), QStringLiteral("''")) + QStringLiteral("'");
}
#endif
} // namespace

QString OpenFilter()
{
	QStringList all;
	all << QStringLiteral("*.pdf");
	for (const QString &suffix : officeSuffixes())
		all << QStringLiteral("*.") + suffix;
	for (const QString &suffix : imageSuffixes())
		all << QStringLiteral("*.") + suffix;

	QStringList office, images;
	for (const QString &suffix : officeSuffixes())
		office << QStringLiteral("*.") + suffix;
	for (const QString &suffix : imageSuffixes())
		images << QStringLiteral("*.") + suffix;

	return QStringLiteral("Presentaciones (%1);;PDF (*.pdf);;PowerPoint / Keynote / Impress (%2);;Imagenes (%3)")
		.arg(all.join(QLatin1Char(' ')), office.join(QLatin1Char(' ')), images.join(QLatin1Char(' ')));
}

bool IsPdf(const QString &path)
{
	return suffixOf(path) == QStringLiteral("pdf");
}

bool IsImage(const QString &path)
{
	return imageSuffixes().contains(suffixOf(path));
}

bool NeedsConversion(const QString &path)
{
	return officeSuffixes().contains(suffixOf(path));
}

/* ------------------------------------------------------------------------ */
/* PDF engine for Linux and FreeBSD: poppler-utils command line tools.      */
/* macOS and Windows use WorshipDocument_macOS.cpp / _Windows.cpp.           */

#if !defined(__APPLE__) && !defined(_WIN32)
namespace {
QString runTool(const QString &tool, const QStringList &arguments)
{
	QProcess process;
	process.start(tool, arguments);
	if (!process.waitForFinished(30000) || process.exitCode() != 0)
		return {};
	return QString::fromUtf8(process.readAllStandardOutput());
}
} // namespace

int PdfPageCount(const QString &pdfPath, QString *error)
{
	if (QStandardPaths::findExecutable(QStringLiteral("pdfinfo")).isEmpty()) {
		if (error)
			*error = QCoreApplication::translate("WorshipStudio", "Falta poppler-utils (pdfinfo/pdftoppm). Instalalo con el gestor de "
						"paquetes de tu sistema.");
		return 0;
	}

	const QString info = runTool(QStringLiteral("pdfinfo"), {pdfPath});
	for (const QString &line : info.split(QLatin1Char('\n'))) {
		if (line.startsWith(QStringLiteral("Pages:")))
			return line.mid(6).trimmed().toInt();
	}
	if (error)
		*error = QCoreApplication::translate("WorshipStudio", "No se pudo leer el PDF.");
	return 0;
}

QSizeF PdfPageSize(const QString &pdfPath, int page)
{
	const QString number = QString::number(page + 1);
	const QString info =
		runTool(QStringLiteral("pdfinfo"), {QStringLiteral("-f"), number, QStringLiteral("-l"), number, pdfPath});
	for (const QString &line : info.split(QLatin1Char('\n'))) {
		/* "Page    1 size: 612 x 792 pts (letter)" */
		const int at = line.indexOf(QStringLiteral(" size: "));
		if (!line.startsWith(QStringLiteral("Page")) || at < 0)
			continue;
		const QStringList parts = line.mid(at + 7).split(QLatin1Char(' '), Qt::SkipEmptyParts);
		if (parts.size() >= 3)
			return QSizeF(parts.at(0).toDouble(), parts.at(2).toDouble());
	}
	return QSizeF(612, 792);
}

QImage RenderPdfPage(const QString &pdfPath, int page, const QSize &box)
{
	const QSize size = PdfPageSize(pdfPath, page).toSize().scaled(box, Qt::KeepAspectRatio);
	const QString prefix = QDir(WorshipRender::CacheDir(QStringLiteral("presentations/tmp")))
				       .absoluteFilePath(QStringLiteral("page-%1").arg(QCoreApplication::applicationPid()));
	const QString number = QString::number(page + 1);

	QProcess process;
	process.start(QStringLiteral("pdftoppm"), {QStringLiteral("-f"), number, QStringLiteral("-l"), number,
						   QStringLiteral("-png"), QStringLiteral("-singlefile"),
						   QStringLiteral("-scale-to-x"), QString::number(size.width()),
						   QStringLiteral("-scale-to-y"), QString::number(size.height()), pdfPath,
						   prefix});
	if (!process.waitForFinished(60000) || process.exitCode() != 0)
		return {};

	const QString png = prefix + QStringLiteral(".png");
	QImage image(png);
	QFile::remove(png);
	return image;
}
#endif

/* ------------------------------------------------------------------------ */
/* Office to PDF conversion                                                  */

Converter::Converter(QObject *parent) : QObject(parent) {}

Converter::~Converter()
{
	cancel();
}

QStringList Converter::availableConverters()
{
	QStringList found;
#if defined(__APPLE__)
	if (appInstalled(QStringLiteral("Keynote.app")))
		found << QStringLiteral("Keynote");
	if (appInstalled(QStringLiteral("Microsoft PowerPoint.app")))
		found << QStringLiteral("PowerPoint");
#elif defined(_WIN32)
	if (powerPointInstalled())
		found << QStringLiteral("PowerPoint");
#endif
	if (!findLibreOffice().isEmpty())
		found << QStringLiteral("LibreOffice");
	return found;
}

void Converter::convert(const QString &path, Done done)
{
	cancel();

	const QFileInfo info(path);
	const QByteArray key = QStringLiteral("%1|%2|%3")
				       .arg(info.absoluteFilePath())
				       .arg(info.lastModified().toMSecsSinceEpoch())
				       .arg(info.size())
				       .toUtf8();
	const QString hash = QString::fromLatin1(QCryptographicHash::hash(key, QCryptographicHash::Sha1).toHex().left(20));
	const QDir cache(WorshipRender::CacheDir(QStringLiteral("presentations")));

	source = info.absoluteFilePath();
	target = cache.absoluteFilePath(hash + QStringLiteral(".pdf"));
	callback = std::move(done);
	failures.clear();
	attempts.clear();

	if (QFileInfo(target).size() > 0) {
		finish(target, QString());
		return;
	}

	const QString suffix = info.suffix().toLower();
	const QString work = cache.absoluteFilePath(QStringLiteral("work-") + hash);
	QDir(work).removeRecursively();
	QDir().mkpath(work);

	/* LibreOffice runs headless and with its own profile, so it works even
	 * while the user has LibreOffice open. */
	Attempt libreOffice;
	const QString soffice = findLibreOffice();
	if (!soffice.isEmpty()) {
		libreOffice.name = QStringLiteral("LibreOffice");
		libreOffice.program = soffice;
		libreOffice.arguments = {QStringLiteral("-env:UserInstallation=") +
						 QUrl::fromLocalFile(cache.absoluteFilePath(QStringLiteral("lo-profile")))
							 .toString(),
					 QStringLiteral("--headless"),
					 QStringLiteral("--norestore"),
					 QStringLiteral("--convert-to"),
					 QStringLiteral("pdf"),
					 QStringLiteral("--outdir"),
					 work,
					 source};
		libreOffice.output = QDir(work).absoluteFilePath(info.completeBaseName() + QStringLiteral(".pdf"));
	}

#if defined(__APPLE__)
	const QString output = QDir(work).absoluteFilePath(QStringLiteral("out.pdf"));

	Attempt keynote;
	if (appInstalled(QStringLiteral("Keynote.app")) && suffix != QStringLiteral("odp")) {
		keynote.name = QStringLiteral("Keynote");
		keynote.program = QStringLiteral("/usr/bin/osascript");
		keynote.arguments = osascript(
			{QStringLiteral("on run argv"),
			 QStringLiteral("set wasRunning to application \"Keynote\" is running"),
			 QStringLiteral("tell application \"Keynote\""),
			 QStringLiteral("set d to open (POSIX file (item 1 of argv))"),
			 QStringLiteral("export d to (POSIX file (item 2 of argv)) as PDF"),
			 QStringLiteral("close d saving no"),
			 QStringLiteral("if not wasRunning then quit"), QStringLiteral("end tell"),
			 QStringLiteral("end run")},
			{source, output});
		keynote.output = output;
	}

	/* PowerPoint for Mac is sandboxed: work inside its container so it does
	 * not ask for file access. */
	Attempt powerPoint;
	if (appInstalled(QStringLiteral("Microsoft PowerPoint.app")) && suffix != QStringLiteral("key")) {
		const QString container =
			QDir::homePath() + QStringLiteral("/Library/Containers/com.microsoft.Powerpoint/Data/tmp");
		QDir().mkpath(container);
		const QString inside = QDir(container).absoluteFilePath(QStringLiteral("ws-") + hash +
									QStringLiteral(".") + suffix);
		const QString insidePdf = QDir(container).absoluteFilePath(QStringLiteral("ws-") + hash +
									   QStringLiteral(".pdf"));
		QFile::remove(inside);
		QFile::copy(source, inside);

		powerPoint.name = QStringLiteral("PowerPoint");
		powerPoint.program = QStringLiteral("/usr/bin/osascript");
		powerPoint.arguments = osascript(
			{QStringLiteral("on run argv"),
			 QStringLiteral("set wasRunning to application \"Microsoft PowerPoint\" is running"),
			 QStringLiteral("set outPath to (POSIX file (item 2 of argv)) as text"),
			 QStringLiteral("tell application \"Microsoft PowerPoint\""),
			 QStringLiteral("open (POSIX file (item 1 of argv))"),
			 QStringLiteral("set p to active presentation"),
			 QStringLiteral("save p in outPath as save as PDF"),
			 QStringLiteral("close p saving no"),
			 QStringLiteral("if not wasRunning then quit"), QStringLiteral("end tell"),
			 QStringLiteral("end run")},
			{inside, insidePdf});
		powerPoint.output = insidePdf;
	}

	if (suffix == QStringLiteral("key")) {
		attempts << keynote;
	} else {
		/* Keynote is sandboxed and sometimes never answers for files it
		 * was not given by the user, so it goes last with a short timeout. */
		keynote.timeoutMs = 90000;
		attempts << libreOffice << powerPoint << keynote;
	}
#elif defined(_WIN32)
	Attempt powerPoint;
	if (powerPointInstalled() && suffix != QStringLiteral("key")) {
		const QString output = QDir::toNativeSeparators(QDir(work).absoluteFilePath(QStringLiteral("out.pdf")));
		const QString script =
			QStringLiteral("$ErrorActionPreference='Stop';"
				       "$app=New-Object -ComObject PowerPoint.Application;"
				       "$p=$app.Presentations.Open(%1,$true,$false,$false);"
				       "$p.SaveAs(%2,32);"
				       "$p.Close();"
				       "if($app.Presentations.Count -eq 0){$app.Quit()}")
				.arg(powerShellQuoted(QDir::toNativeSeparators(source)), powerShellQuoted(output));
		powerPoint.name = QStringLiteral("PowerPoint");
		powerPoint.program = QStringLiteral("powershell.exe");
		powerPoint.arguments = {QStringLiteral("-NoProfile"), QStringLiteral("-NonInteractive"),
					QStringLiteral("-ExecutionPolicy"), QStringLiteral("Bypass"),
					QStringLiteral("-Command"), script};
		powerPoint.output = output;
	}
	attempts << powerPoint << libreOffice;
#else
	attempts << libreOffice;
#endif

	attempts.erase(std::remove_if(attempts.begin(), attempts.end(),
				      [](const Attempt &attempt) { return attempt.program.isEmpty(); }),
		       attempts.end());

	if (attempts.isEmpty()) {
		QString error = tr("Para abrir %1 hace falta PowerPoint, Keynote o LibreOffice (gratis, "
				   "libreoffice.org). Tambien podes exportarlo a PDF y abrir el PDF.")
					.arg(info.fileName());
		if (suffix == QStringLiteral("key"))
			error = tr("Los archivos de Keynote solo se pueden abrir en macOS con Keynote instalado. "
				   "Exportalo a PDF desde Keynote y abri el PDF.");
		finish(QString(), error);
		return;
	}

	tryNext();
}

void Converter::tryNext()
{
	if (attempts.isEmpty()) {
		finish(QString(), tr("No se pudo convertir la presentacion (%1). Exportala a PDF y abri el PDF.")
					  .arg(failures.join(QStringLiteral("; "))));
		return;
	}

	const Attempt attempt = attempts.takeFirst();
	emit progress(tr("Convirtiendo con %1...").arg(attempt.name));

	process = new QProcess(this);
	auto *timeout = new QTimer(process);
	timeout->setSingleShot(true);
	connect(timeout, &QTimer::timeout, process, [this, attempt]() {
		failures << tr("%1 tardo demasiado").arg(attempt.name);
		if (process)
			process->kill();
	});

	connect(process, &QProcess::finished, this, [this, attempt](int exitCode, QProcess::ExitStatus status) {
		QProcess *finished = process;
		process = nullptr;
		finished->deleteLater();

		const bool produced = QFileInfo(attempt.output).size() > 0;
		if (status == QProcess::NormalExit && produced) {
			QFile::remove(target);
			if (QFile::rename(attempt.output, target) || QFile::copy(attempt.output, target)) {
				QFile::remove(attempt.output);
				QDir(QFileInfo(attempt.output).absolutePath()).removeRecursively();
				finish(target, QString());
				return;
			}
		}

		QString detail = QString::fromUtf8(finished->readAllStandardError()).trimmed().section('\n', -1);
		if (detail.isEmpty())
			detail = tr("codigo %1").arg(exitCode);
		failures << QStringLiteral("%1: %2").arg(attempt.name, detail);
		blog(LOG_WARNING, "[WorshipStudio] Conversion with %s failed: %s", attempt.name.toUtf8().constData(),
		     detail.toUtf8().constData());
		tryNext();
	});
	connect(process, &QProcess::errorOccurred, this, [this, attempt](QProcess::ProcessError error) {
		if (error != QProcess::FailedToStart || !process)
			return;
		QProcess *failed = process;
		process = nullptr;
		failed->deleteLater();
		failures << tr("%1 no se pudo iniciar").arg(attempt.name);
		tryNext();
	});

	timeout->start(attempt.timeoutMs > 0 ? attempt.timeoutMs : ConversionTimeoutMs);
	process->start(attempt.program, attempt.arguments);
}

void Converter::cancel()
{
	attempts.clear();
	if (process) {
		QProcess *running = process;
		process = nullptr;
		running->disconnect(this);
		running->kill();
		running->waitForFinished(2000);
		running->deleteLater();
	}
	callback = nullptr;
}

void Converter::finish(const QString &pdfPath, const QString &error)
{
	Done done = std::move(callback);
	callback = nullptr;
	if (done)
		done(pdfPath, error);
}

} // namespace WorshipDocument
