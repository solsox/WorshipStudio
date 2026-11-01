#include "PresentationCatalogData.hpp"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QHash>
#include <QRegularExpression>
#include <QSet>

#include <algorithm>

namespace {
QString appText(const QString &path)
{
	QFile file(path);
	if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
		return {};

	return QString::fromUtf8(file.readAll());
}

QString htmlDecode(QString text)
{
	text.replace(QStringLiteral("&amp;"), QStringLiteral("&"));
	text.replace(QStringLiteral("&quot;"), QStringLiteral("\""));
	text.replace(QStringLiteral("&#39;"), QStringLiteral("'"));
	text.replace(QStringLiteral("&lt;"), QStringLiteral("<"));
	text.replace(QStringLiteral("&gt;"), QStringLiteral(">"));
	return text;
}

int leadingNumber(const QString &name)
{
	static const QRegularExpression regex(QStringLiteral("^(\\d{1,3})"));
	const QRegularExpressionMatch match = regex.match(name);
	return match.hasMatch() ? match.captured(1).toInt() : -1;
}

/* Indexes the entries of a directory by their leading hymn number. */
QHash<int, QFileInfoList> indexByNumber(const QString &dirPath, QDir::Filters filters, const QStringList &nameFilters)
{
	QHash<int, QFileInfoList> index;
	const QFileInfoList entries =
		QDir(dirPath).entryInfoList(nameFilters, filters | QDir::NoDotAndDotDot, QDir::Name | QDir::IgnoreCase);
	for (const QFileInfo &info : entries) {
		const int number = leadingNumber(info.fileName());
		if (number >= 0)
			index[number].push_back(info);
	}
	return index;
}

/* Picks the candidate whose name contains the title; otherwise the first one,
 * which is the plain version when variants (alternate endings) exist. */
QString bestMatch(const QFileInfoList &candidates, const QString &normalizedTitle)
{
	if (candidates.isEmpty())
		return {};

	for (const QFileInfo &info : candidates) {
		const QString name = PresentationCatalogData::normalizeName(info.completeBaseName());
		if (name.endsWith(normalizedTitle))
			return info.absoluteFilePath();
	}
	for (const QFileInfo &info : candidates) {
		if (PresentationCatalogData::normalizeName(info.completeBaseName()).contains(normalizedTitle))
			return info.absoluteFilePath();
	}
	return candidates.first().absoluteFilePath();
}

QStringList listJpgSlides(const QString &folderPath)
{
	QDir folder(folderPath);
	const QStringList slides = folder.entryList(QStringList() << QStringLiteral("*.jpg") << QStringLiteral("*.jpeg")
								  << QStringLiteral("*.png"),
						    QDir::Files, QDir::Name | QDir::IgnoreCase);
	QStringList absoluteSlides;
	for (const QString &slide : slides)
		absoluteSlides << folder.absoluteFilePath(slide);
	return absoluteSlides;
}
} // namespace

QString PresentationCatalogData::normalizeName(const QString &text)
{
	QString normalized = text.normalized(QString::NormalizationForm_D);
	normalized.remove(QRegularExpression(QStringLiteral("[\\x{0300}-\\x{036f}]")));
	normalized = normalized.toLower();
	normalized.replace(QRegularExpression(QStringLiteral("[^a-z0-9]+")), QStringLiteral(" "));
	return normalized.simplified();
}

QString PresentationCatalogData::editionName(HymnalEdition edition)
{
	return edition == HymnalEdition::New2009 ? QStringLiteral("Himnario nuevo (2009)")
						 : QStringLiteral("Himnario antiguo (1962)");
}

/* A folder looks like this edition when its name says so. */
static bool matchesEdition(const QString &folderName, HymnalEdition edition)
{
	const QString name = PresentationCatalogData::normalizeName(folderName);
	if (!name.contains(QStringLiteral("himn")))
		return false;
	const bool old = name.contains(QStringLiteral("antig")) || name.contains(QStringLiteral("1962"));
	return edition == HymnalEdition::Old1962 ? old : !old;
}

QStringList PresentationCatalogData::candidateRoots(HymnalEdition edition, const QString &preferredRoot)
{
	QStringList candidates;
	if (!preferredRoot.isEmpty())
		candidates << QDir(preferredRoot).absolutePath();
	if (edition == HymnalEdition::New2009 && !qEnvironmentVariableIsEmpty("WORSHIPSTUDIO_MEDIA_ROOT"))
		candidates << qEnvironmentVariable("WORSHIPSTUDIO_MEDIA_ROOT");

	/* Look for hymnal folders next to the app bundle and in its parents. */
	QStringList searchDirs;
	QDir cursor(QCoreApplication::applicationDirPath());
	for (int level = 0; level < 8; ++level) {
		searchDirs << cursor.absolutePath();
		if (!cursor.cdUp())
			break;
	}
	searchDirs << QDir::currentPath() << QDir::homePath()
		   << QDir::home().absoluteFilePath(QStringLiteral("Music"))
		   << QDir::home().absoluteFilePath(QStringLiteral("Documents"))
		   << QDir::home().absoluteFilePath(QStringLiteral("Documents/WorshipStudio"));

	/* Content that cannot be published with the code lives in a
	 * "contenido-local" folder next to the repository checkout. */
	const QStringList baseDirs = searchDirs;
	for (const QString &dirPath : baseDirs)
		searchDirs << QDir(dirPath).absoluteFilePath(QStringLiteral("contenido-local"));

	for (const QString &dirPath : searchDirs) {
		const QFileInfoList children = QDir(dirPath).entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot);
		for (const QFileInfo &child : children) {
			if (matchesEdition(child.fileName(), edition))
				candidates << child.absoluteFilePath();
		}
	}

	candidates.removeDuplicates();
	return candidates;
}

QString PresentationCatalogData::lyricsDir(const QString &rootPath)
{
	const QString letra = QDir(rootPath).absoluteFilePath(QStringLiteral("Letra"));
	return QDir(letra).exists() ? letra : rootPath;
}

QString PresentationCatalogData::findCatalogFile(const QString &rootPath)
{
	const QStringList paths = {
		QDir(rootPath).absoluteFilePath(QStringLiteral("Resources/index.html")),
		QDir(rootPath).absoluteFilePath(
			QStringLiteral("HimnarioAdventista.app/Contents/Resources/Resources/index.html")),
		QDir(rootPath).absoluteFilePath(QStringLiteral("HimnarioAdventista.app/Contents/Resources/index.html")),
	};

	for (const QString &path : paths) {
		if (QFileInfo::exists(path))
			return path;
	}

	return {};
}

bool PresentationCatalogData::isHymnRoot(const QString &rootPath)
{
	if (rootPath.isEmpty() || !QDir(rootPath).exists())
		return false;
	if (!findCatalogFile(rootPath).isEmpty() && QDir(QDir(rootPath).absoluteFilePath(QStringLiteral("Letra"))).exists())
		return true;

	/* Plain layout: several numbered folders with images. */
	int hymnFolders = 0;
	const QFileInfoList folders = QDir(lyricsDir(rootPath)).entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot);
	for (const QFileInfo &folder : folders) {
		if (leadingNumber(folder.fileName()) >= 0 && !listJpgSlides(folder.absoluteFilePath()).isEmpty() &&
		    ++hymnFolders >= 3)
			return true;
	}
	return false;
}

QString PresentationCatalogData::discoverHymnRoot(HymnalEdition edition, const QString &preferredRoot)
{
	for (const QString &root : candidateRoots(edition, preferredRoot)) {
		if (isHymnRoot(root))
			return root;
	}

	return {};
}

QVector<PresentationEntry> PresentationCatalogData::loadHymnCatalog(HymnalEdition edition,
								     const QString &preferredRoot, QString *error,
								     QString *usedRoot)
{
	if (error)
		error->clear();

	const QString root = discoverHymnRoot(edition, preferredRoot);
	if (usedRoot)
		*usedRoot = root;
	if (root.isEmpty()) {
		if (error)
			*error = QStringLiteral("No se encontro la carpeta del %1.").arg(editionName(edition).toLower());
		return {};
	}

	QVector<PresentationEntry> entries = findCatalogFile(root).isEmpty() ? loadFromFolders(root, error)
									       : loadFromCatalog(root, error);
	std::sort(entries.begin(), entries.end(),
		  [](const PresentationEntry &a, const PresentationEntry &b) { return a.number < b.number; });
	return entries;
}

QVector<PresentationEntry> PresentationCatalogData::loadFromFolders(const QString &root, QString *error)
{
	static const QRegularExpression titleRegex(QStringLiteral("^\\d{1,3}\\s*[-_.]*\\s*(.*)$"));
	const QDir rootDir(root);
	const QString lyrics = lyricsDir(root);
	const QStringList mp3Filter = {QStringLiteral("*.mp3"), QStringLiteral("*.m4a"), QStringLiteral("*.wav")};

	auto firstExisting = [&](const QStringList &relative) {
		for (const QString &path : relative) {
			if (QDir(rootDir.absoluteFilePath(path)).exists())
				return rootDir.absoluteFilePath(path);
		}
		return QString();
	};
	const QHash<int, QFileInfoList> cantado =
		indexByNumber(firstExisting({QStringLiteral("Musica/Cantado"), QStringLiteral("Cantado"),
					     QStringLiteral("Musica")}),
			      QDir::Files, mp3Filter);
	const QHash<int, QFileInfoList> instrumental = indexByNumber(
		firstExisting({QStringLiteral("Musica/Instrumental"), QStringLiteral("Instrumental")}), QDir::Files,
		mp3Filter);

	QVector<PresentationEntry> entries;
	QSet<int> seen;
	const QFileInfoList folders = QDir(lyrics).entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name);
	for (const QFileInfo &folder : folders) {
		const int number = leadingNumber(folder.fileName());
		if (number < 0 || seen.contains(number))
			continue;

		PresentationEntry entry;
		entry.number = number;
		entry.title = titleRegex.match(folder.fileName()).captured(1).trimmed();
		if (entry.title.isEmpty())
			entry.title = QStringLiteral("Himno %1").arg(number);
		entry.folderPath = folder.absoluteFilePath();
		entry.slidePaths = listJpgSlides(entry.folderPath);
		if (entry.slidePaths.isEmpty())
			continue;
		seen.insert(number);
		entry.slideCount = entry.slidePaths.size();
		entry.displayLabel = QStringLiteral("%1 - %2").arg(number, 3, 10, QLatin1Char('0')).arg(entry.title);

		const QString normalizedTitle = normalizeName(entry.title);
		entry.cantadoPath = bestMatch(cantado.value(number), normalizedTitle);
		entry.instrumentalPath = bestMatch(instrumental.value(number), normalizedTitle);

		/* Audio stored inside the hymn folder itself. */
		const QFileInfoList localAudio = QDir(entry.folderPath).entryInfoList(mp3Filter, QDir::Files, QDir::Name);
		for (const QFileInfo &audio : localAudio) {
			const bool isInstrumental = normalizeName(audio.fileName()).contains(QStringLiteral("instrum"));
			QString &target = isInstrumental ? entry.instrumentalPath : entry.cantadoPath;
			if (target.isEmpty())
				target = audio.absoluteFilePath();
		}

		QFile timesFile(QDir(entry.folderPath).absoluteFilePath(QStringLiteral("tiempos.txt")));
		if (timesFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
			const QString text = QString::fromUtf8(timesFile.readAll());
			for (const QString &part : text.split(QRegularExpression(QStringLiteral("[,\\s]+")), Qt::SkipEmptyParts))
				entry.slideTimes.push_back(part.toInt());
		}

		entry.searchBlob = QStringLiteral("%1 %2 %3").arg(number).arg(number, 3, 10, QLatin1Char('0')).arg(normalizedTitle);
		entries.push_back(entry);
	}

	if (entries.isEmpty() && error)
		*error = QStringLiteral("La carpeta no contiene himnos (carpetas \"NNN - Titulo\" con imagenes).");
	return entries;
}

QVector<PresentationEntry> PresentationCatalogData::loadFromCatalog(const QString &root, QString *error)
{
	const QString html = appText(findCatalogFile(root));
	if (html.isEmpty()) {
		if (error)
			*error = QStringLiteral("No se pudo leer Resources/index.html.");
		return {};
	}

	const QDir rootDir(root);
	const QHash<int, QFileInfoList> folders =
		indexByNumber(rootDir.absoluteFilePath(QStringLiteral("Letra")), QDir::Dirs, {});
	const QStringList mp3Filter = {QStringLiteral("*.mp3")};
	const QHash<int, QFileInfoList> cantado =
		indexByNumber(rootDir.absoluteFilePath(QStringLiteral("Musica/Cantado")), QDir::Files, mp3Filter);
	const QHash<int, QFileInfoList> instrumental =
		indexByNumber(rootDir.absoluteFilePath(QStringLiteral("Musica/Instrumental")), QDir::Files, mp3Filter);

	static const QRegularExpression spanRegex(QStringLiteral(R"(<span\s+class="himn"([^>]*)>)"),
						  QRegularExpression::CaseInsensitiveOption);
	static const QRegularExpression attrRegex(QStringLiteral(R"re(data-(\w+)="([^"]*)")re"));

	QVector<PresentationEntry> entries;
	QSet<int> seen;

	auto iterator = spanRegex.globalMatch(html);
	while (iterator.hasNext()) {
		const QRegularExpressionMatch span = iterator.next();
		QHash<QString, QString> attrs;
		auto attrIterator = attrRegex.globalMatch(span.captured(1));
		while (attrIterator.hasNext()) {
			const QRegularExpressionMatch attr = attrIterator.next();
			attrs.insert(attr.captured(1).toLower(), htmlDecode(attr.captured(2)));
		}

		PresentationEntry entry;
		entry.number = attrs.value(QStringLiteral("number")).toInt();
		entry.title = attrs.value(QStringLiteral("title")).trimmed();
		if (entry.title.isEmpty() || seen.contains(entry.number))
			continue;
		seen.insert(entry.number);

		entry.displayLabel = QStringLiteral("%1 - %2").arg(entry.number, 3, 10, QLatin1Char('0')).arg(entry.title);
		entry.slideCount = attrs.value(QStringLiteral("slides")).toInt();

		const QString timesText = attrs.value(QStringLiteral("times"));
		for (const QString &part : timesText.split(QLatin1Char(','), Qt::SkipEmptyParts))
			entry.slideTimes.push_back(part.trimmed().toInt());

		const QString normalizedTitle = normalizeName(entry.title);
		entry.folderPath = bestMatch(folders.value(entry.number), normalizedTitle);
		if (!entry.folderPath.isEmpty())
			entry.slidePaths = listJpgSlides(entry.folderPath);
		entry.cantadoPath = bestMatch(cantado.value(entry.number), normalizedTitle);
		entry.instrumentalPath = bestMatch(instrumental.value(entry.number), normalizedTitle);
		entry.searchBlob =
			QStringLiteral("%1 %2 %3").arg(entry.number).arg(entry.number, 3, 10, QLatin1Char('0')).arg(normalizedTitle);

		entries.push_back(entry);
	}

	if (entries.isEmpty() && error)
		*error = QStringLiteral("No se pudieron leer himnos desde el catalogo.");

	return entries;
}
