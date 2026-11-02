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
#include "WorshipLive.hpp"
#include "WorshipOutput.hpp"

#include <obs-frontend-api.h>
#include <qt-wrappers.hpp>
#include <Idian/Utils.hpp>

#include <QDir>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QInputDialog>
#include <QJsonArray>
#include <QJsonDocument>
#include <QLabel>
#include <QListWidget>
#include <QMenu>
#include <QPushButton>
#include <QSaveFile>
#include <QStandardPaths>
#include <QTimer>
#include <QVBoxLayout>

#include "moc_ServiceDock.cpp"

namespace {
QString fileFilter()
{
	return QCoreApplication::translate("WorshipStudio", "Orden del culto (*.wsservice)");
}
} // namespace

QJsonObject ServiceItem::toJson() const
{
	return QJsonObject{{QStringLiteral("kind"), (int)kind},
			   {QStringLiteral("title"), title},
			   {QStringLiteral("value"), value},
			   {QStringLiteral("extra"), extra},
			   {QStringLiteral("number"), number}};
}

ServiceItem ServiceItem::fromJson(const QJsonObject &object)
{
	ServiceItem item;
	item.kind = static_cast<Kind>(qBound(0, object.value(QStringLiteral("kind")).toInt(), (int)Scene));
	item.title = object.value(QStringLiteral("title")).toString();
	item.value = object.value(QStringLiteral("value")).toString();
	item.extra = object.value(QStringLiteral("extra")).toString();
	item.number = object.value(QStringLiteral("number")).toInt();
	return item;
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
	auto *fileRow = new QHBoxLayout();
	fileRow->addWidget(newButton);
	fileRow->addWidget(openButton);
	fileRow->addWidget(saveButton);

	list = new QListWidget(root);
	list->setDragDropMode(QAbstractItemView::InternalMove);
	list->setDefaultDropAction(Qt::MoveAction);
	list->setSelectionMode(QAbstractItemView::SingleSelection);
	list->setToolTip(tr("Arrastra para cambiar el orden. Doble clic para pasar a ese elemento."));

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
	};
	for (const auto &[kind, label] : kinds) {
		const ServiceItem::Kind itemKind = kind;
		addMenu->addAction(label, this, [this, itemKind]() { addItem(itemKind); });
	}
	addButton->setMenu(addMenu);
	auto *removeButton = new QPushButton(tr("Quitar"), root);
	auto *editRow = new QHBoxLayout();
	editRow->addWidget(addButton, 1);
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
	connect(goButton, &QPushButton::clicked, this, [this]() { activate(list->currentRow()); });
	connect(nextButton, &QPushButton::clicked, this, &ServiceDock::activateNext);
	connect(list, &QListWidget::itemActivated, this, [this](QListWidgetItem *item) { activate(list->row(item)); });
	connect(list->model(), &QAbstractItemModel::rowsMoved, this, [this]() { QTimer::singleShot(0, this, &ServiceDock::syncFromList); });
	connect(&WorshipLive::instance(), &WorshipLive::serviceItemRequested, this, &ServiceDock::activate);

	load(autosavePath());
	rebuild();
	WorshipLive::instance().setService({}, -1);
	changed();
}

QString ServiceDock::autosavePath()
{
	const QString base = QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation);
	const QString dir = QDir(base).absoluteFilePath(QStringLiteral("WorshipStudio/servicios"));
	QDir().mkpath(dir);
	return QDir(dir).absoluteFilePath(QStringLiteral("actual.wsservice"));
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
		auto *entry = new QListWidgetItem(QStringLiteral("%1.  %2  —  %3")
							  .arg(i + 1)
							  .arg(item.kindName(), item.title.isEmpty() ? item.value : item.title));
		entry->setData(Qt::UserRole, i);
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
	}

	const int row = list->currentRow();
	items.insert(row >= 0 ? row + 1 : items.size(), item);
	changed();
	list->setCurrentRow(row >= 0 ? row + 1 : (int)items.size() - 1);
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
	}

	current = index;
	changed();
	return ok;
}
