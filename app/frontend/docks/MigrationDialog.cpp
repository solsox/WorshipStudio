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

#include "MigrationDialog.hpp"
#include "BibleDock.hpp"
#include "PresentationCatalogDock.hpp"
#include "SongLibrary.hpp"
#include "SongsDock.hpp"

#include <OBSApp.hpp>
#include <Idian/Utils.hpp>
#include <util/config-file.h>

#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QPushButton>
#include <QScrollArea>
#include <QStandardPaths>
#include <QTimer>
#include <QVBoxLayout>

#include "moc_MigrationDialog.cpp"

extern bool restart;

namespace {
QString configBase()
{
	return QString::fromStdString(App()->userConfigLocation.u8string());
}

bool copyFolder(const QString &from, const QString &to)
{
	QDir().mkpath(to);
	QDirIterator it(from, QDir::Files | QDir::Hidden, QDirIterator::Subdirectories);
	while (it.hasNext()) {
		const QString path = it.next();
		const QString target = QDir(to).absoluteFilePath(QDir(from).relativeFilePath(path));
		QDir().mkpath(QFileInfo(target).absolutePath());
		QFile::remove(target);
		if (!QFile::copy(path, target))
			return false;
	}
	return true;
}
} // namespace

void MigrationDialog::openFor(QWidget *mainWindow)
{
	if (!mainWindow)
		return;
	auto *dialog = new MigrationDialog(mainWindow->findChild<SongsDock *>(QStringLiteral("songsDock")),
					   mainWindow->findChild<BibleDock *>(QStringLiteral("bibleDock")),
					   mainWindow->findChild<PresentationCatalogDock *>(QStringLiteral("hymnalDock")),
					   mainWindow);
	dialog->setAttribute(Qt::WA_DeleteOnClose);
	dialog->open();
}

QString MigrationDialog::obsFolder()
{
	const QString folder = QDir(configBase()).absoluteFilePath(QStringLiteral("obs-studio"));
	return QFileInfo(QDir(folder).absoluteFilePath(QStringLiteral("basic"))).isDir() ? folder : QString();
}

QStringList MigrationDialog::detectedPrograms()
{
	QStringList found;
	const QString home = QDir::homePath();
	const QString appData = QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation);
	struct Program {
		const char *name;
		QStringList paths;
	};
	const Program programs[] = {
		{"OBS Studio", {obsFolder()}},
		{"Holyrics", {QStringLiteral("/Applications/Holyrics.app"), home + QStringLiteral("/Holyrics"),
			      QStringLiteral("C:/Holyrics"), home + QStringLiteral("/AppData/Local/Holyrics")}},
		{"ProPresenter", {QStringLiteral("/Applications/ProPresenter.app"), QStringLiteral("/Applications/ProPresenter 6.app"),
				  QStringLiteral("C:/Program Files/Renewed Vision/ProPresenter")}},
		{"OpenLP", {appData + QStringLiteral("/openlp"), home + QStringLiteral("/.local/share/openlp"),
			    home + QStringLiteral("/AppData/Roaming/openlp")}},
		{"OpenSong", {home + QStringLiteral("/Documents/OpenSong")}},
		{"EasyWorship", {QStringLiteral("C:/Program Files (x86)/Softouch/EasyWorship 7"),
				 QStringLiteral("C:/Program Files/Softouch/EasyWorship 7")}},
		{"FreeShow", {appData + QStringLiteral("/freeshow"), home + QStringLiteral("/Documents/FreeShow")}},
	};
	for (const Program &program : programs)
		for (const QString &path : program.paths)
			if (!path.isEmpty() && QFileInfo::exists(path)) {
				found << QString::fromLatin1(program.name);
				break;
			}
	return found;
}

bool MigrationDialog::importObs(int *collections, int *profiles, QString *firstCollection, QString *firstCollectionFile,
				QString *firstProfile, QString *firstProfileDir, QString *error)
{
	const QString obs = obsFolder();
	if (obs.isEmpty()) {
		*error = tr("No se encontró OBS Studio en esta computadora.");
		return false;
	}
	*collections = *profiles = 0;
	const QDir ours(QDir(configBase()).absoluteFilePath(QStringLiteral(OBS_APP_CONFIG_DIR)));

	/* Scene collections: same format, renamed "(OBS)" so nothing is overwritten. */
	const QDir scenes(QDir(obs).absoluteFilePath(QStringLiteral("basic/scenes")));
	ours.mkpath(QStringLiteral("basic/scenes"));
	for (const QFileInfo &file : scenes.entryInfoList({QStringLiteral("*.json")}, QDir::Files)) {
		QFile input(file.absoluteFilePath());
		if (!input.open(QIODevice::ReadOnly))
			continue;
		QJsonObject collection = QJsonDocument::fromJson(input.readAll()).object();
		if (collection.isEmpty())
			continue;
		const QString name = collection.value(QStringLiteral("name")).toString(file.completeBaseName()) +
				     QStringLiteral(" (OBS)");
		collection.insert(QStringLiteral("name"), name);
		const QString stem = file.completeBaseName() + QStringLiteral("_OBS");
		QFile output(ours.absoluteFilePath(QStringLiteral("basic/scenes/%1.json").arg(stem)));
		if (!output.open(QIODevice::WriteOnly))
			continue;
		output.write(QJsonDocument(collection).toJson());
		if (++*collections == 1) {
			*firstCollection = name;
			*firstCollectionFile = stem;
		}
	}

	/* Profiles: stream keys, video and output settings. */
	const QDir profileRoot(QDir(obs).absoluteFilePath(QStringLiteral("basic/profiles")));
	for (const QString &dir : profileRoot.entryList(QDir::Dirs | QDir::NoDotAndDotDot)) {
		const QString target = ours.absoluteFilePath(QStringLiteral("basic/profiles/%1_OBS").arg(dir));
		if (!copyFolder(profileRoot.absoluteFilePath(dir), target))
			continue;
		QString name = dir;
		QFile ini(QDir(target).absoluteFilePath(QStringLiteral("basic.ini")));
		if (ini.open(QIODevice::ReadOnly)) {
			QStringList lines = QString::fromUtf8(ini.readAll()).split(QLatin1Char('\n'));
			ini.close();
			for (QString &line : lines)
				if (line.startsWith(QStringLiteral("Name="))) {
					name = line.mid(5).trimmed() + QStringLiteral(" (OBS)");
					line = QStringLiteral("Name=") + name;
				}
			if (ini.open(QIODevice::WriteOnly | QIODevice::Truncate))
				ini.write(lines.join(QLatin1Char('\n')).toUtf8());
		}
		if (++*profiles == 1) {
			*firstProfile = name;
			*firstProfileDir = dir + QStringLiteral("_OBS");
		}
	}
	if (*collections == 0 && *profiles == 0) {
		*error = tr("OBS Studio no tiene escenas ni perfiles para traer.");
		return false;
	}
	return true;
}

MigrationDialog::MigrationDialog(SongsDock *songs_, BibleDock *bible_, PresentationCatalogDock *hymnal_, QWidget *parent)
	: QDialog(parent),
	  songs(songs_),
	  bible(bible_),
	  hymnal(hymnal_)
{
	setWindowTitle(tr("Traer desde otro programa"));
	resize(620, 680);

	auto *outer = new QVBoxLayout(this);
	auto *title = new QLabel(tr("Traé lo que ya tenés armado"), this);
	idian::Utils::addClass(title, "text-large");
	idian::Utils::addClass(title, "text-bold");
	QString introText = tr("Escenas, cantos, Biblias e himnario de otros programas. Nada se borra del otro programa.");
	const QStringList found = detectedPrograms();
	if (!found.isEmpty())
		introText += QStringLiteral("<br><b>") + tr("Encontrado en esta computadora:") + QStringLiteral("</b> ") +
			     found.join(QStringLiteral(" · "));
	auto *intro = new QLabel(introText, this);
	intro->setWordWrap(true);
	intro->setTextFormat(Qt::RichText);
	idian::Utils::addClass(intro, "text-muted");

	auto *content = new QWidget(this);
	auto *layout = new QVBoxLayout(content);
	layout->setContentsMargins(0, 0, 0, 0);
	layout->setSpacing(12);

	/* OBS Studio */
	QWidget *obsBox = section(tr("Escenas de OBS Studio"),
				  tr("Cámaras, fuentes, escenas y perfiles (transmisión, video y grabación) tal como los tenías."),
				  &obsStatus);
	auto *obsButton = new QPushButton(tr("Traer escenas y perfiles de OBS"), obsBox);
	idian::Utils::addClass(obsButton, "button-primary");
	obsBox->layout()->addWidget(obsButton);
	obsButton->setEnabled(!obsFolder().isEmpty());
	obsStatus->setText(obsFolder().isEmpty() ? tr("No se encontró OBS Studio en esta computadora.")
						 : tr("OBS Studio encontrado."));
	connect(obsButton, &QPushButton::clicked, this, [this, obsButton]() {
		int collections = 0, profiles = 0;
		QString collection, collectionFile, profile, profileDir, error;
		if (!importObs(&collections, &profiles, &collection, &collectionFile, &profile, &profileDir, &error)) {
			obsStatus->setText(error);
			return;
		}
		obsButton->setEnabled(false);
		obsStatus->setText(tr("Listo: %1 colecciones de escenas y %2 perfiles. WorshipStudio se reinicia para usarlos.")
					   .arg(collections)
					   .arg(profiles));
		/* Open with what was imported. */
		config_t *config = App()->GetUserConfig();
		if (!collection.isEmpty()) {
			config_set_string(config, "Basic", "SceneCollection", collection.toUtf8().constData());
			config_set_string(config, "Basic", "SceneCollectionFile", collectionFile.toUtf8().constData());
		}
		if (!profile.isEmpty()) {
			config_set_string(config, "Basic", "Profile", profile.toUtf8().constData());
			config_set_string(config, "Basic", "ProfileDir", profileDir.toUtf8().constData());
		}
		config_save_safe(config, "tmp", nullptr);
		auto *restartButton = new QPushButton(tr("Reiniciar ahora"), obsStatus->parentWidget());
		obsStatus->parentWidget()->layout()->addWidget(restartButton);
		connect(restartButton, &QPushButton::clicked, this, [this]() {
			restart = true;
			QWidget *main = parentWidget() ? parentWidget()->window() : nullptr;
			accept();
			if (main)
				QTimer::singleShot(0, main, &QWidget::close);
		});
	});
	layout->addWidget(obsBox);

	/* Songs */
	QWidget *songBox = section(tr("Cantos"),
				   tr("Desde Holyrics, OpenLP, OpenSong, ProPresenter 6, EasyWorship y otros: exportá los cantos "
				      "como texto, OpenLyrics o ProPresenter y elegí la carpeta (se leen también las subcarpetas)."),
				   &songsStatus);
	auto *songRow = new QHBoxLayout();
	auto *songFolder = new QPushButton(tr("Elegir carpeta..."), songBox);
	auto *songFiles = new QPushButton(tr("Elegir archivos..."), songBox);
	songRow->addWidget(songFolder);
	songRow->addWidget(songFiles);
	qobject_cast<QVBoxLayout *>(songBox->layout())->addLayout(songRow);
	connect(songFolder, &QPushButton::clicked, this, [this]() {
		const QString folder = QFileDialog::getExistingDirectory(this, tr("Carpeta con cantos"), QDir::homePath());
		if (!folder.isEmpty())
			importSongs({}, folder);
	});
	connect(songFiles, &QPushButton::clicked, this, [this]() {
		QStringList patterns;
		for (const QString &suffix : SongLibrary::importableSuffixes())
			patterns << QStringLiteral("*.") + suffix;
		const QStringList files = QFileDialog::getOpenFileNames(
			this, tr("Importar cantos"), QDir::homePath(),
			tr("Cantos (%1);;Todos los archivos (*)").arg(patterns.join(QLatin1Char(' '))));
		if (!files.isEmpty())
			importSongs(files, {});
	});
	layout->addWidget(songBox);

	/* Bibles */
	QWidget *bibleBox = section(tr("Biblias"),
				    tr("Si tu iglesia tiene una versión con licencia (por ejemplo la RV1960) en formato Zefania u "
				       "OSIS, importala. También podés descargar gratis más de 1.500 traducciones libres."),
				    &bibleStatus);
	auto *bibleRow = new QHBoxLayout();
	auto *bibleImport = new QPushButton(tr("Importar Biblia..."), bibleBox);
	auto *bibleMore = new QPushButton(tr("Más Biblias..."), bibleBox);
	bibleRow->addWidget(bibleImport);
	bibleRow->addWidget(bibleMore);
	qobject_cast<QVBoxLayout *>(bibleBox->layout())->addLayout(bibleRow);
	connect(bibleImport, &QPushButton::clicked, this, [this]() {
		const QString file = QFileDialog::getOpenFileName(
			this, tr("Importar version de la Biblia"), QDir::homePath(),
			tr("Biblias (*.json *.xml *.osis);;Zefania u OSIS (*.xml *.osis);;Biblia WorshipStudio (*.json)"));
		if (file.isEmpty() || !bible)
			return;
		bibleStatus->setText(bible->importBibleFile(file) ? tr("Biblia importada. Ya la podés elegir en Versión.")
								   : tr("No se reconoce la Biblia: se aceptan Zefania XML, OSIS XML y Biblias de WorshipStudio."));
	});
	connect(bibleMore, &QPushButton::clicked, this, [this]() {
		if (bible)
			QMetaObject::invokeMethod(bible, "openCatalog");
	});
	layout->addWidget(bibleBox);

	/* Hymnal */
	QLabel *hymnStatus = nullptr;
	QWidget *hymnBox = section(tr("Himnario"),
				   tr("Elegí la carpeta del himnario que ya tenés (una carpeta por himno con sus imágenes, y la "
				      "música cantada e instrumental)."),
				   &hymnStatus);
	auto *hymnButton = new QPushButton(tr("Elegir carpeta del himnario..."), hymnBox);
	hymnBox->layout()->addWidget(hymnButton);
	connect(hymnButton, &QPushButton::clicked, this, [this]() {
		if (hymnal)
			QMetaObject::invokeMethod(hymnal, "onChooseFolderClicked");
	});
	layout->addWidget(hymnBox);
	layout->addStretch(1);

	auto *scroll = new QScrollArea(this);
	scroll->setWidgetResizable(true);
	scroll->setFrameShape(QFrame::NoFrame);
	scroll->setWidget(content);

	auto *close = new QPushButton(tr("Listo"), this);
	connect(close, &QPushButton::clicked, this, &QDialog::accept);

	outer->addWidget(title);
	outer->addWidget(intro);
	outer->addWidget(scroll, 1);
	outer->addWidget(close, 0, Qt::AlignRight);
}

QWidget *MigrationDialog::section(const QString &title, const QString &text, QLabel **status)
{
	auto *box = new QFrame(this);
	box->setObjectName(QStringLiteral("migrationBox"));
	box->setStyleSheet(QStringLiteral("#migrationBox{border:1px solid palette(mid);border-radius:12px;}"));
	auto *layout = new QVBoxLayout(box);
	layout->setContentsMargins(16, 14, 16, 14);
	auto *heading = new QLabel(title, box);
	idian::Utils::addClass(heading, "text-bold");
	auto *body = new QLabel(text, box);
	body->setWordWrap(true);
	*status = new QLabel(box);
	(*status)->setWordWrap(true);
	idian::Utils::addClass(*status, "text-muted");
	layout->addWidget(heading);
	layout->addWidget(body);
	layout->addWidget(*status);
	return box;
}

void MigrationDialog::importSongs(const QStringList &files, const QString &folder)
{
	int imported = 0;
	QStringList errors;
	if (!folder.isEmpty())
		imported = SongLibrary::importFolder(folder, &errors);
	for (const QString &file : files) {
		Song song;
		QString error;
		if (SongLibrary::importFile(file, &song, &error) && SongLibrary::save(song, &error))
			++imported;
		else
			errors << QStringLiteral("%1: %2").arg(QFileInfo(file).fileName(), error);
	}
	if (songs)
		songs->reloadLibrary();
	QString text = tr("%1 cantos importados.").arg(imported);
	if (!errors.isEmpty())
		text += QStringLiteral(" ") + tr("No se pudieron leer %1 archivos.").arg(errors.size());
	songsStatus->setText(text);
}
