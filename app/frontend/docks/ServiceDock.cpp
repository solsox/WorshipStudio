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

#include "ServiceDock.hpp"
#include <QCoreApplication>
#include "BibleDock.hpp"
#include "LiveToolsDock.hpp"
#include "PresentationCatalogDock.hpp"
#include "PresentationDock.hpp"
#include "SongLibrary.hpp"
#include "SongsDock.hpp"
#include "WorshipDocument.hpp"
#include "WorshipDrag.hpp"
#include "WorshipLive.hpp"
#include "WorshipOutput.hpp"

#include <obs-frontend-api.h>
#include <qt-wrappers.hpp>
#include <Idian/Utils.hpp>

#include <QDialog>
#include <QDialogButtonBox>
#include <QDir>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QInputDialog>
#include <QJsonArray>
#include <QJsonDocument>
#include <QLabel>
#include <QListWidget>
#include <QMenu>
#include <QLocale>
#include <QPlainTextEdit>
#include <QRegularExpression>
#include <QPushButton>
#include <QSaveFile>
#include <QSpinBox>
#include <QStandardPaths>
#include <QTimer>
#include <QVBoxLayout>

#include "moc_ServiceDock.cpp"

namespace {
QString fileFilter()
{
	return QCoreApplication::translate("WorshipStudio", "Orden del culto (*.wsservice)");
}

/* "1 h 25 min", "40 min". */
QString formatMinutes(int minutes)
{
	if (minutes < 60)
		return QCoreApplication::translate("WorshipStudio", "%1 min").arg(minutes);
	return QCoreApplication::translate("WorshipStudio", "%1 h %2 min").arg(minutes / 60).arg(minutes % 60, 2, 10, QLatin1Char('0'));
}

/* Kinds that show a text of their own (announcements, prayer, custom). */
bool showsText(ServiceItem::Kind kind)
{
	return kind == ServiceItem::Announcement || kind == ServiceItem::Prayer || kind == ServiceItem::Custom;
}
} // namespace

QJsonObject ServiceItem::toJson() const
{
	return QJsonObject{{QStringLiteral("kind"), (int)kind},
			   {QStringLiteral("title"), title},
			   {QStringLiteral("value"), value},
			   {QStringLiteral("extra"), extra},
			   {QStringLiteral("number"), number},
			   {QStringLiteral("minutes"), minutes},
			   {QStringLiteral("person"), person},
			   {QStringLiteral("notes"), notes}};
}

ServiceItem ServiceItem::fromJson(const QJsonObject &object)
{
	ServiceItem item;
	item.kind = static_cast<Kind>(qBound(0, object.value(QStringLiteral("kind")).toInt(), (int)Custom));
	item.title = object.value(QStringLiteral("title")).toString();
	item.value = object.value(QStringLiteral("value")).toString();
	item.extra = object.value(QStringLiteral("extra")).toString();
	item.number = object.value(QStringLiteral("number")).toInt();
	/* Files of earlier versions have no planning fields. */
	item.minutes = object.value(QStringLiteral("minutes")).toInt(defaultMinutes(item.kind));
	item.person = object.value(QStringLiteral("person")).toString();
	item.notes = object.value(QStringLiteral("notes")).toString();
	return item;
}

int ServiceItem::defaultMinutes(Kind kind)
{
	switch (kind) {
	case Hymn:
		return 4;
	case Song:
		return 5;
	case Bible:
		return 3;
	case Presentation:
		return 10;
	case Countdown:
		return 5;
	case Name:
		return 1;
	case Announcement:
		return 3;
	case Scene:
		return 5;
	case Video:
		return 5;
	case Sermon:
		return 35;
	case Prayer:
		return 5;
	case Custom:
		return 5;
	}
	return 5;
}

QString ServiceItem::kindName() const
{
	switch (kind) {
	case Hymn:
		return QStringLiteral("Himno");
	case Song:
		return QStringLiteral("Canto");
	case Bible:
		return QStringLiteral("Biblia");
	case Presentation:
		return QStringLiteral("Presentacion");
	case Countdown:
		return QCoreApplication::translate("WorshipStudio", "Cuenta regresiva");
	case Name:
		return QStringLiteral("Rotulo");
	case Announcement:
		return QStringLiteral("Anuncio");
	case Scene:
		return QStringLiteral("Escena");
	case Video:
		return QStringLiteral("Video");
	case Sermon:
		return QCoreApplication::translate("WorshipStudio", "Prédica");
	case Prayer:
		return QCoreApplication::translate("WorshipStudio", "Oración");
	case Custom:
		return QCoreApplication::translate("WorshipStudio", "Momento");
	}
	return {};
}

ServiceDock::ServiceDock(const QString &objectName, const QString &title, PresentationCatalogDock *hymnal_,
			 SongsDock *songs_, BibleDock *bible_, PresentationDock *presentations_,
			 LiveToolsDock *liveTools_, QWidget *parent)
	: OBSDock(title, parent),
	  hymnal(hymnal_),
	  songs(songs_),
	  bible(bible_),
	  presentations(presentations_),
	  liveTools(liveTools_)
{
	setObjectName(objectName);

	auto *root = new QWidget(this);
	auto *layout = new QVBoxLayout(root);
	layout->setContentsMargins(8, 8, 8, 8);
	layout->setSpacing(6);

	nameLabel = new QLabel(root);
	idian::Utils::addClass(nameLabel, "text-muted");
	auto *newButton = new QPushButton(tr("Nuevo"), root);
	auto *openButton = new QPushButton(tr("Abrir..."), root);
	auto *saveButton = new QPushButton(tr("Guardar..."), root);
	auto *moreButton = new QPushButton(tr("Más"), root);
	auto *moreMenu = new QMenu(moreButton);
	moreMenu->addAction(tr("Duplicar reunión..."), this, [this]() {
		const QString base = filePath.isEmpty() ? tr("Culto") : QFileInfo(filePath).completeBaseName();
		const QString dir = filePath.isEmpty() ? QDir::homePath() : QFileInfo(filePath).absolutePath();
		QString path = QFileDialog::getSaveFileName(this, tr("Duplicar reunión"),
							    QDir(dir).absoluteFilePath(tr("%1 (copia).wsservice").arg(base)),
							    fileFilter());
		if (path.isEmpty())
			return;
		if (!path.endsWith(QStringLiteral(".wsservice")))
			path += QStringLiteral(".wsservice");
		filePath = path;
		current = -1;
		changed();
	});
	moreMenu->addAction(tr("Guardar como plantilla..."), this, &ServiceDock::saveAsTemplate);
	QMenu *templatesMenu = moreMenu->addMenu(tr("Nueva desde plantilla"));
	connect(templatesMenu, &QMenu::aboutToShow, this, [this, templatesMenu]() {
		templatesMenu->clear();
		templatesMenu->addAction(tr("Culto de domingo"), this, [this]() { startFromTemplate(QString()); });
		const QFileInfoList files = QDir(templatesDir()).entryInfoList({QStringLiteral("*.wsservice")}, QDir::Files,
										QDir::Name);
		if (!files.isEmpty())
			templatesMenu->addSeparator();
		for (const QFileInfo &file : files) {
			const QString path = file.absoluteFilePath();
			templatesMenu->addAction(file.completeBaseName(), this, [this, path]() { startFromTemplate(path); });
		}
	});
	moreButton->setMenu(moreMenu);
	auto *fileRow = new QHBoxLayout();
	fileRow->addWidget(newButton);
	fileRow->addWidget(openButton);
	fileRow->addWidget(saveButton);
	fileRow->addWidget(moreButton);

	list = new QListWidget(root);
	list->setDragDropMode(QAbstractItemView::InternalMove);
	list->setDefaultDropAction(Qt::MoveAction);
	list->setSelectionMode(QAbstractItemView::SingleSelection);
	list->setToolTip(tr("Arrastra para cambiar el orden o trae cantos, versículos, himnos y presentaciones desde "
			    "sus paneles. Doble clic para pasar a ese elemento; clic derecho para los detalles."));
	list->setWordWrap(true);
	list->setContextMenuPolicy(Qt::CustomContextMenu);
	WorshipDrag::InstallDropTarget(list, [this](const QString &panel, const std::function<void()> &selectDragged) {
		addFromPanel(panel, selectDragged);
	});

	timeLabel = new QLabel(root);
	timeLabel->setWordWrap(true);
	idian::Utils::addClass(timeLabel, "text-bold");

	auto *addButton = new QPushButton(tr("+ Agregar"), root);
	auto *addMenu = new QMenu(addButton);
	const std::pair<ServiceItem::Kind, QString> kinds[] = {
		{ServiceItem::Hymn, tr("Himno")},
		{ServiceItem::Song, tr("Canto")},
		{ServiceItem::Bible, tr("Pasaje biblico")},
		{ServiceItem::Presentation, tr("Presentacion (PDF, PowerPoint...)")},
		{ServiceItem::Countdown, tr("Cuenta regresiva")},
		{ServiceItem::Name, tr("Rotulo de nombre")},
		{ServiceItem::Announcement, tr("Anuncio (texto)")},
		{ServiceItem::Scene, tr("Escena (camara, video...)")},
		{ServiceItem::Video, tr("Video")},
		{ServiceItem::Sermon, tr("Prédica")},
		{ServiceItem::Prayer, tr("Momento de oración")},
		{ServiceItem::Custom, tr("Otro momento (personalizado)")},
	};
	for (const auto &[kind, label] : kinds) {
		const ServiceItem::Kind itemKind = kind;
		addMenu->addAction(label, this, [this, itemKind]() { addItem(itemKind); });
	}
	addButton->setMenu(addMenu);
	auto *detailsButton = new QPushButton(tr("Detalles..."), root);
	detailsButton->setToolTip(tr("Duración, responsable y notas del elemento elegido"));
	auto *removeButton = new QPushButton(tr("Quitar"), root);
	auto *editRow = new QHBoxLayout();
	editRow->addWidget(addButton, 1);
	editRow->addWidget(detailsButton);
	editRow->addWidget(removeButton);

	goButton = new QPushButton(tr("Ir a este elemento"), root);
	nextButton = new QPushButton(tr("Siguiente elemento"), root);
	idian::Utils::addClass(nextButton, "button-primary");
	auto *liveRow = new QHBoxLayout();
	liveRow->addWidget(goButton);
	liveRow->addWidget(nextButton, 1);

	layout->addLayout(fileRow);
	layout->addWidget(nameLabel);
	layout->addWidget(list, 1);
	layout->addWidget(timeLabel);
	layout->addLayout(editRow);
	layout->addLayout(liveRow);
	setWidget(root);

	connect(newButton, &QPushButton::clicked, this, [this]() {
		items.clear();
		current = -1;
		filePath.clear();
		changed();
	});
	connect(openButton, &QPushButton::clicked, this, [this]() {
		const QString path = QFileDialog::getOpenFileName(this, tr("Abrir orden del culto"), QDir::homePath(), fileFilter());
		if (!path.isEmpty() && load(path)) {
			filePath = path;
			changed();
		}
	});
	connect(saveButton, &QPushButton::clicked, this, [this]() {
		QString path = QFileDialog::getSaveFileName(this, tr("Guardar orden del culto"),
							    filePath.isEmpty() ? QDir::home().absoluteFilePath(tr("Culto.wsservice"))
									       : filePath,
							    fileFilter());
		if (path.isEmpty())
			return;
		if (!path.endsWith(QStringLiteral(".wsservice")))
			path += QStringLiteral(".wsservice");
		save(path);
		filePath = path;
		changed();
	});
	connect(removeButton, &QPushButton::clicked, this, [this]() {
		const int row = list->currentRow();
		if (row < 0)
			return;
		items.removeAt(row);
		if (current >= row)
			--current;
		changed();
	});
	connect(detailsButton, &QPushButton::clicked, this, [this]() { editDetails(list->currentRow()); });
	connect(list, &QWidget::customContextMenuRequested, this, &ServiceDock::showItemMenu);
	connect(goButton, &QPushButton::clicked, this, [this]() { activate(list->currentRow()); });
	connect(nextButton, &QPushButton::clicked, this, &ServiceDock::activateNext);
	connect(list, &QListWidget::itemActivated, this, [this](QListWidgetItem *item) { activate(list->row(item)); });
	connect(list->model(), &QAbstractItemModel::rowsMoved, this, [this]() { QTimer::singleShot(0, this, &ServiceDock::syncFromList); });
	connect(&WorshipLive::instance(), &WorshipLive::serviceItemRequested, this, &ServiceDock::activate);

	/* The time left follows the clock. */
	clock = new QTimer(this);
	clock->setInterval(15000);
	connect(clock, &QTimer::timeout, this, &ServiceDock::updateTimes);
	clock->start();

	load(autosavePath());
	rebuild();
	WorshipLive::instance().setService({}, -1);
	changed();
}

QString ServiceDock::autosavePath()
{
	/* The self test plays with the order: never over the user's. */
	const QString selfTest = qEnvironmentVariable("WORSHIPSTUDIO_SELFTEST");
	const QString base = selfTest.isEmpty() ? QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation)
						: selfTest;
	const QString dir = QDir(base).absoluteFilePath(QStringLiteral("WorshipStudio/servicios"));
	QDir().mkpath(dir);
	return QDir(dir).absoluteFilePath(QStringLiteral("actual.wsservice"));
}

QString ServiceDock::templatesDir()
{
	const QString dir = QDir(QFileInfo(autosavePath()).absolutePath()).absoluteFilePath(QStringLiteral("plantillas"));
	QDir().mkpath(dir);
	return dir;
}

/* A usual Sunday service, to start from and adjust. */
QVector<ServiceItem> ServiceDock::sundayTemplate()
{
	const auto make = [](ServiceItem::Kind kind, const QString &title, const QString &value = QString(),
			     int number = 0) {
		ServiceItem item;
		item.kind = kind;
		item.title = title;
		item.value = value;
		item.number = number;
		item.minutes = kind == ServiceItem::Countdown ? number : ServiceItem::defaultMinutes(kind);
		return item;
	};
	const auto text = [](const char *source) { return QCoreApplication::translate("WorshipStudio", source); };
	QVector<ServiceItem> items;
	items << make(ServiceItem::Countdown, text("Cuenta regresiva de %1 minutos").arg(5), text("El culto comienza en"), 5);
	items << make(ServiceItem::Announcement, text("Bienvenida"), text("Bienvenidos"));
	items << make(ServiceItem::Prayer, text("Oración de apertura"), text("Oración"));
	items << make(ServiceItem::Custom, text("Alabanza (elige los cantos)"));
	items << make(ServiceItem::Announcement, text("Anuncios"), text("Anuncios"));
	items << make(ServiceItem::Custom, text("Lectura bíblica (elige el pasaje)"));
	items << make(ServiceItem::Sermon, text("Prédica"));
	items << make(ServiceItem::Prayer, text("Oración final"), text("Oración"));
	items << make(ServiceItem::Custom, text("Canto final y despedida"));
	return items;
}

bool ServiceDock::saveAsTemplate()
{
	bool ok = false;
	const QString name = QInputDialog::getText(this, tr("Guardar como plantilla"),
						   tr("Nombre de la plantilla (p. ej. Culto de jóvenes)"),
						   QLineEdit::Normal, QString(), &ok)
				     .trimmed()
				     .replace(QRegularExpression(QStringLiteral("[\\\\/:*?\"<>|]")), QStringLiteral(" "));
	if (!ok || name.isEmpty())
		return false;
	save(QDir(templatesDir()).absoluteFilePath(name + QStringLiteral(".wsservice")));
	return true;
}

/* A new, unsaved meeting with the items of a template (built in when empty). */
void ServiceDock::startFromTemplate(const QString &path)
{
	if (path.isEmpty())
		items = sundayTemplate();
	else if (!load(path))
		return;
	current = -1;
	filePath.clear();
	changed();
}

void ServiceDock::setItems(const QVector<ServiceItem> &newItems)
{
	items = newItems;
	current = -1;
	changed();
}

void ServiceDock::rebuild()
{
	QSignalBlocker blocker(list);
	const int row = list->currentRow();
	list->clear();
	for (int i = 0; i < items.size(); ++i) {
		const ServiceItem &item = items.at(i);
		QString details = item.minutes > 0 ? formatMinutes(item.minutes) : QString();
		if (!item.person.isEmpty())
			details += (details.isEmpty() ? QString() : QStringLiteral("  ·  ")) + item.person;
		if (!item.notes.isEmpty())
			details += (details.isEmpty() ? QString() : QStringLiteral("  ·  ")) + tr("nota");
		auto *entry = new QListWidgetItem(QStringLiteral("%1.  %2  —  %3")
							  .arg(i + 1)
							  .arg(item.kindName(), item.title.isEmpty() ? item.value : item.title) +
						  (details.isEmpty() ? QString() : QStringLiteral("\n       ") + details));
		entry->setData(Qt::UserRole, i);
		entry->setToolTip(item.notes.isEmpty() ? QString() : item.notes);
		if (i == current) {
			QFont font = entry->font();
			font.setBold(true);
			entry->setFont(font);
			entry->setText(QStringLiteral("▶  ") + entry->text());
		}
		list->addItem(entry);
	}
	list->setCurrentRow(qBound(-1, row < 0 ? current : row, (int)items.size() - 1));
	nameLabel->setText(filePath.isEmpty() ? tr("Culto sin guardar (se guarda solo en este equipo)")
					      : QFileInfo(filePath).completeBaseName());
	nextButton->setEnabled(!items.isEmpty());
	goButton->setEnabled(!items.isEmpty());
	updateTimes();
}

/* Total length, time left and expected end, from the minutes of each item. */
void ServiceDock::updateTimes()
{
	int total = 0;
	for (const ServiceItem &item : items)
		total += item.minutes;
	if (items.isEmpty() || total == 0) {
		timeLabel->setText(items.isEmpty() ? QString()
						   : tr("Pon la duración de cada elemento en Detalles para ver el tiempo total."));
		return;
	}
	int left = 0;
	if (current >= 0 && current < items.size()) {
		const int elapsed = currentStarted.isValid() ? int(currentStarted.secsTo(QDateTime::currentDateTime()) / 60) : 0;
		left = qMax(0, items.at(current).minutes - elapsed);
		for (int i = current + 1; i < items.size(); ++i)
			left += items.at(i).minutes;
	} else {
		left = total;
	}
	const QString end = QLocale().toString(QTime::currentTime().addSecs(qint64(left) * 60), QLocale::ShortFormat);
	if (current < 0)
		timeLabel->setText(tr("Total: %1  ·  si empieza ahora termina ≈ %2").arg(formatMinutes(total), end));
	else
		timeLabel->setText(tr("Total: %1  ·  quedan %2  ·  termina ≈ %3").arg(formatMinutes(total), formatMinutes(left), end));
}

void ServiceDock::changed()
{
	rebuild();
	save(autosavePath());
	if (!filePath.isEmpty())
		save(filePath);

	QStringList titles;
	for (const ServiceItem &item : items)
		titles << QStringLiteral("%1 · %2").arg(item.kindName(), item.title.isEmpty() ? item.value : item.title);
	WorshipLive::instance().setService(titles, current);
}

void ServiceDock::syncFromList()
{
	/* Drag and drop reordered the rows: rebuild `items` in the new order. */
	QVector<ServiceItem> reordered;
	int newCurrent = -1;
	for (int row = 0; row < list->count(); ++row) {
		const int index = list->item(row)->data(Qt::UserRole).toInt();
		if (index >= 0 && index < items.size()) {
			if (index == current)
				newCurrent = row;
			reordered << items.at(index);
		}
	}
	items = reordered;
	current = newCurrent;
	changed();
}

void ServiceDock::save(const QString &path)
{
	QJsonArray array;
	for (const ServiceItem &item : items)
		array.append(item.toJson());
	QSaveFile file(path);
	if (!file.open(QIODevice::WriteOnly))
		return;
	file.write(QJsonDocument(QJsonObject{{QStringLiteral("format"), QStringLiteral("worshipstudio-service")},
					     {QStringLiteral("version"), 1},
					     {QStringLiteral("items"), array}})
			   .toJson(QJsonDocument::Indented));
	file.commit();
}

bool ServiceDock::load(const QString &path)
{
	QFile file(path);
	if (!file.open(QIODevice::ReadOnly))
		return false;
	const QJsonObject object = QJsonDocument::fromJson(file.readAll()).object();
	if (object.value(QStringLiteral("format")).toString() != QStringLiteral("worshipstudio-service"))
		return false;
	items.clear();
	for (const QJsonValue value : object.value(QStringLiteral("items")).toArray())
		items << ServiceItem::fromJson(value.toObject());
	current = -1;
	return true;
}

void ServiceDock::addItem(ServiceItem::Kind kind)
{
	ServiceItem item;
	item.kind = kind;
	bool ok = false;

	switch (kind) {
	case ServiceItem::Hymn: {
		item.number = QInputDialog::getInt(this, tr("Himno"), tr("Numero del himno"), 1, 1, 999, 1, &ok);
		if (!ok)
			return;
		const QStringList modes = {tr("Cantado"), tr("Instrumental"), tr("Solo letra")};
		const QString mode = QInputDialog::getItem(this, tr("Himno"), tr("Modo"), modes, 0, false, &ok);
		if (!ok)
			return;
		item.extra = QString::number(modes.indexOf(mode));
		item.title = tr("Himno %1 (%2)").arg(item.number).arg(mode.toLower());
		break;
	}
	case ServiceItem::Song: {
		const QVector<Song> library = SongLibrary::loadAll();
		if (library.isEmpty())
			return;
		QStringList titles;
		for (const Song &song : library)
			titles << song.title;
		const QString chosen = QInputDialog::getItem(this, tr("Canto"), tr("Elegi el canto"), titles, 0, false, &ok);
		if (!ok)
			return;
		const Song &song = library.at(titles.indexOf(chosen));
		item.value = song.id;
		item.title = song.title;
		break;
	}
	case ServiceItem::Bible:
		item.value = QInputDialog::getText(this, tr("Pasaje biblico"), tr("Referencia (p. ej. Juan 3:16-18)"),
						   QLineEdit::Normal, QString(), &ok)
				     .trimmed();
		if (!ok || item.value.isEmpty())
			return;
		item.title = item.value;
		break;
	case ServiceItem::Presentation:
		item.value = QFileDialog::getOpenFileName(this, tr("Presentacion"), QDir::homePath(),
							  WorshipDocument::OpenFilter());
		if (item.value.isEmpty())
			return;
		item.title = QFileInfo(item.value).fileName();
		break;
	case ServiceItem::Countdown:
		item.number = QInputDialog::getInt(this, tr("Cuenta regresiva"), tr("Minutos"), 5, 1, 240, 1, &ok);
		if (!ok)
			return;
		item.value = QInputDialog::getText(this, tr("Cuenta regresiva"), tr("Titulo"), QLineEdit::Normal,
						   tr("El culto comienza en"), &ok);
		if (!ok)
			return;
		item.title = tr("Cuenta regresiva de %1 minutos").arg(item.number);
		break;
	case ServiceItem::Name:
		item.value = QInputDialog::getText(this, tr("Rotulo de nombre"), tr("Nombre"), QLineEdit::Normal,
						   QString(), &ok)
				     .trimmed();
		if (!ok || item.value.isEmpty())
			return;
		item.extra = QInputDialog::getText(this, tr("Rotulo de nombre"), tr("Cargo o tema (opcional)"),
						   QLineEdit::Normal, QString(), &ok);
		item.title = item.value;
		break;
	case ServiceItem::Announcement:
		item.value = QInputDialog::getMultiLineText(this, tr("Anuncio"),
							    tr("Texto (una linea en blanco separa las diapositivas)"),
							    QString(), &ok);
		if (!ok || item.value.trimmed().isEmpty())
			return;
		item.title = item.value.section(QLatin1Char('\n'), 0, 0).left(40);
		break;
	case ServiceItem::Scene: {
		QStringList scenes;
		char **names = obs_frontend_get_scene_names();
		for (char **name = names; name && *name; ++name)
			scenes << QT_UTF8(*name);
		bfree(names);
		if (scenes.isEmpty())
			return;
		item.value = QInputDialog::getItem(this, tr("Escena"), tr("Escena (cámara, video, pantalla...)"), scenes, 0, false, &ok);
		if (!ok)
			return;
		item.title = item.value;
		break;
	}
	case ServiceItem::Video:
		item.value = QFileDialog::getOpenFileName(this, tr("Video"), QDir::homePath(),
							  tr("Videos (*.mp4 *.mov *.m4v *.mkv *.webm *.avi)"));
		if (item.value.isEmpty())
			return;
		item.title = QFileInfo(item.value).completeBaseName();
		break;
	case ServiceItem::Sermon:
		item.title = QInputDialog::getText(this, tr("Prédica"), tr("Tema o título"), QLineEdit::Normal, QString(), &ok)
				     .trimmed();
		if (!ok)
			return;
		item.person = QInputDialog::getText(this, tr("Prédica"),
						    tr("Predicador (se muestra en el rótulo al llegar; opcional)"),
						    QLineEdit::Normal, QString(), &ok)
				      .trimmed();
		if (item.title.isEmpty())
			item.title = tr("Prédica");
		break;
	case ServiceItem::Prayer:
		item.title = tr("Momento de oración");
		item.value = tr("Oración");
		break;
	case ServiceItem::Custom:
		item.title = QInputDialog::getText(this, tr("Otro momento"), tr("Nombre (p. ej. Santa cena, Ofrenda)"),
						   QLineEdit::Normal, QString(), &ok)
				     .trimmed();
		if (!ok || item.title.isEmpty())
			return;
		break;
	}

	if (kind == ServiceItem::Countdown)
		item.minutes = item.number;
	insertItem(item);
}

/* After the chosen row (or at the end), with the usual length of its kind. */
void ServiceDock::insertItem(const ServiceItem &newItem)
{
	ServiceItem item = newItem;
	if (item.minutes <= 0)
		item.minutes = ServiceItem::defaultMinutes(item.kind);
	const int row = list->currentRow();
	const int at = row >= 0 ? row + 1 : (int)items.size();
	items.insert(at, item);
	if (current >= at)
		++current;
	changed();
	list->setCurrentRow(at);
}

void ServiceDock::addFromPanel(const QString &panel, const std::function<void()> &selectDragged)
{
	{
		/* Selecting the dragged row must not change what is on screen. */
		WorshipOutput::SourceOnlyScope sourceOnly;
		if (selectDragged)
			selectDragged();
	}
	ServiceItem item;
	if (panel == QStringLiteral("songsDock") && songs && !songs->selectedSongId().isEmpty()) {
		item.kind = ServiceItem::Song;
		item.value = songs->selectedSongId();
		item.title = songs->selectedSongTitle();
	} else if (panel == QStringLiteral("bibleDock") && bible && !bible->selectedReference().isEmpty()) {
		item.kind = ServiceItem::Bible;
		item.value = bible->selectedReference();
		item.title = item.value;
	} else if (panel == QStringLiteral("hymnalDock") && hymnal && hymnal->selectedHymnNumber() > 0) {
		item.kind = ServiceItem::Hymn;
		item.number = hymnal->selectedHymnNumber();
		item.extra = QString::number(hymnal->audioModeIndex());
		const QStringList modes = {tr("Cantado"), tr("Instrumental"), tr("Solo letra")};
		item.title = tr("Himno %1 (%2)").arg(item.number).arg(modes.value(hymnal->audioModeIndex()).toLower());
	} else if (panel == QStringLiteral("presentationDock") && presentations &&
		   QFileInfo::exists(presentations->selectedDeckKey())) {
		item.kind = ServiceItem::Presentation;
		item.value = presentations->selectedDeckKey();
		item.title = QFileInfo(item.value).fileName();
	} else {
		return;
	}
	insertItem(item);
}

/* Title, length, person in charge, notes and (for texts) what is shown. */
bool ServiceDock::editDetails(int row)
{
	if (row < 0 || row >= items.size())
		return false;
	ServiceItem &item = items[row];

	QDialog dialog(this);
	dialog.setWindowTitle(tr("Detalles: %1").arg(item.kindName()));
	dialog.setMinimumWidth(420);
	auto *form = new QFormLayout(&dialog);
	auto *titleEdit = new QLineEdit(item.title, &dialog);
	auto *minutesSpin = new QSpinBox(&dialog);
	minutesSpin->setRange(0, 600);
	minutesSpin->setSuffix(tr(" min"));
	minutesSpin->setValue(item.minutes);
	auto *personEdit = new QLineEdit(item.person, &dialog);
	personEdit->setPlaceholderText(item.kind == ServiceItem::Sermon ? tr("Predicador (sale en el rótulo)")
									: tr("Quién está a cargo (opcional)"));
	auto *notesEdit = new QPlainTextEdit(item.notes, &dialog);
	notesEdit->setPlaceholderText(tr("Notas para el equipo (no se proyectan)"));
	notesEdit->setMinimumHeight(80);
	QPlainTextEdit *textEdit = nullptr;
	form->addRow(tr("Título"), titleEdit);
	form->addRow(tr("Duración"), minutesSpin);
	form->addRow(tr("Responsable"), personEdit);
	if (showsText(item.kind)) {
		textEdit = new QPlainTextEdit(item.value, &dialog);
		textEdit->setPlaceholderText(tr("Texto en pantalla (vacío: no muestra nada)"));
		textEdit->setMinimumHeight(70);
		form->addRow(tr("En pantalla"), textEdit);
	}
	form->addRow(tr("Notas"), notesEdit);
	auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
	form->addRow(buttons);
	connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
	connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
	if (dialog.exec() != QDialog::Accepted)
		return false;

	item.title = titleEdit->text().trimmed();
	item.minutes = minutesSpin->value();
	item.person = personEdit->text().trimmed();
	item.notes = notesEdit->toPlainText().trimmed();
	if (textEdit)
		item.value = textEdit->toPlainText();
	changed();
	list->setCurrentRow(row);
	return true;
}

void ServiceDock::duplicateItem(int row)
{
	if (row < 0 || row >= items.size())
		return;
	list->setCurrentRow(row);
	insertItem(items.at(row));
}

void ServiceDock::showItemMenu(const QPoint &pos)
{
	const int row = list->row(list->itemAt(pos));
	if (row < 0)
		return;
	list->setCurrentRow(row);
	QMenu menu(this);
	menu.addAction(tr("Ir a este elemento"), this, [this, row]() { activate(row); });
	menu.addAction(tr("Detalles..."), this, [this, row]() { editDetails(row); });
	menu.addAction(tr("Duplicar"), this, [this, row]() { duplicateItem(row); });
	menu.addSeparator();
	menu.addAction(tr("Quitar"), this, [this, row]() {
		items.removeAt(row);
		if (current >= row)
			--current;
		changed();
	});
	menu.exec(list->viewport()->mapToGlobal(pos));
}

bool ServiceDock::activateNext()
{
	return activate(current + 1);
}

bool ServiceDock::activate(int index)
{
	if (index < 0 || index >= items.size())
		return false;
	const ServiceItem item = items.at(index);
	bool ok = false;

	switch (item.kind) {
	case ServiceItem::Hymn:
		ok = hymnal && hymnal->projectHymn(item.number, item.extra.toInt());
		break;
	case ServiceItem::Song:
		ok = songs && songs->projectSongById(item.value);
		break;
	case ServiceItem::Bible:
		ok = bible && bible->projectQuery(item.value, 0);
		break;
	case ServiceItem::Presentation:
		if (presentations) {
			presentations->openFiles({item.value});
			/* Office files may need a few seconds to convert. */
			auto attempts = std::make_shared<int>(0);
			auto tryProject = std::make_shared<std::function<void()>>();
			*tryProject = [this, attempts, tryProject]() {
				if (!presentations)
					return;
				if (presentations->isConverting() && ++*attempts < 360) {
					QTimer::singleShot(500, this, *tryProject);
					return;
				}
				presentations->project();
			};
			(*tryProject)();
			ok = true;
		}
		break;
	case ServiceItem::Countdown:
		if (liveTools) {
			liveTools->startCountdown(qMax(1, item.number), item.value);
			ok = true;
		}
		break;
	case ServiceItem::Name:
		if (liveTools) {
			liveTools->showName(item.value, item.extra);
			ok = true;
		}
		break;
	case ServiceItem::Announcement:
		if (songs) {
			Song song;
			song.title = item.title;
			song.fontFamily = WorshipRender::RecommendedFonts().value(0);
			song.background.preset = 11;
			song.slides = SongLibrary::parseText(item.value);
			ok = songs->projectTransient(song);
		}
		break;
	case ServiceItem::Scene: {
		OBSSourceAutoRelease source = obs_get_source_by_name(QT_TO_UTF8(item.value));
		if (obs_scene_t *scene = source ? obs_scene_from_source(source) : nullptr) {
			WorshipOutput::ShowScene(scene);
			ok = true;
		}
		break;
	}
	case ServiceItem::Video: {
		/* Full screen, from the start, and silent once it is off screen. */
		OBSScene scene = WorshipOutput::EnsureScene("WS Video");
		OBSDataAutoRelease settings = obs_data_create();
		obs_data_set_bool(settings, "is_local_file", true);
		obs_data_set_string(settings, "local_file", QT_TO_UTF8(item.value));
		obs_data_set_bool(settings, "restart_on_activate", true);
		obs_data_set_bool(settings, "close_when_inactive", true);
		bool created = false;
		OBSSource video = WorshipOutput::EnsureSource(scene, "WS Video - Archivo", "ffmpeg_source", settings, &created);
		if (video && QFileInfo::exists(item.value)) {
			if (created)
				WorshipOutput::FitItem(scene, video, 0.0f, 0.0f, 1.0f, 1.0f);
			else
				obs_source_update(video, settings);
			obs_source_media_restart(video);
			WorshipOutput::ShowScene(scene);
			ok = true;
		}
		break;
	}
	case ServiceItem::Sermon:
		/* The preacher's name, like a TV lower third. */
		if (liveTools && !item.person.isEmpty())
			liveTools->showName(item.person, item.title);
		ok = true;
		break;
	case ServiceItem::Prayer:
	case ServiceItem::Custom:
		ok = true;
		if (songs && !item.value.trimmed().isEmpty()) {
			Song song;
			song.title = item.title;
			song.fontFamily = WorshipRender::RecommendedFonts().value(0);
			song.background.preset = 11;
			song.slides = SongLibrary::parseText(item.value);
			ok = songs->projectTransient(song);
		}
		break;
	}

	current = index;
	currentStarted = QDateTime::currentDateTime();
	changed();
	return ok;
}
