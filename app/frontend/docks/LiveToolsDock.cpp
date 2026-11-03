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

#include "LiveToolsDock.hpp"
#include "SongEditor.hpp"
#include "WorshipLive.hpp"
#include "WorshipOutput.hpp"
#include "ScreensDialog.hpp"
#include "WorshipRemote.hpp"
#include "WorshipRender.hpp"

#include <qt-wrappers.hpp>
#include <Idian/Utils.hpp>

#include <QButtonGroup>
#include <QComboBox>
#include <QGridLayout>
#include <QDir>
#include <QFormLayout>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMenu>
#include <QPixmap>
#include <QPushButton>
#include <QScreen>
#include <QScrollArea>
#include <QSpinBox>
#include <QTimeEdit>
#include <QTimer>
#include <QVBoxLayout>

#include "moc_LiveToolsDock.cpp"

namespace {
constexpr const char *CountdownSceneName = "WS Cuenta regresiva";
constexpr const char *CountdownSourceName = "WS Cuenta regresiva - Pantalla";
constexpr const char *StageSceneName = "WS Escenario";
constexpr const char *StageSourceName = "WS Escenario - Monitor";
constexpr const char *OverlaySceneName = "WS Superposiciones";
constexpr const char *NameSourceName = "WS Rotulo de nombre";
constexpr const char *MessageSourceName = "WS Mensaje urgente";
constexpr const char *ConfigRemote = "RemoteEnabled";
constexpr const char *ConfigHotkeys = "LiveHotkeys";

/* Hotkeys run on another thread; the work is queued to the UI thread. */
void hotkeyCall(void *data, obs_hotkey_id id, obs_hotkey_t *, bool pressed)
{
	if (!pressed)
		return;
	auto *dock = static_cast<LiveToolsDock *>(data);
	QMetaObject::invokeMethod(dock, [dock, id]() { dock->runHotkey(id); }, Qt::QueuedConnection);
}

QLabel *heading(const QString &text, QWidget *parent)
{
	auto *label = new QLabel(text, parent);
	idian::Utils::addClass(label, "text-bold");
	label->setContentsMargins(0, 10, 0, 2);
	return label;
}

QString formatSeconds(qint64 seconds)
{
	seconds = qMax<qint64>(0, seconds);
	if (seconds >= 3600)
		return QStringLiteral("%1:%2:%3")
			.arg(seconds / 3600)
			.arg((seconds / 60) % 60, 2, 10, QLatin1Char('0'))
			.arg(seconds % 60, 2, 10, QLatin1Char('0'));
	return QStringLiteral("%1:%2").arg(seconds / 60, 2, 10, QLatin1Char('0')).arg(seconds % 60, 2, 10, QLatin1Char('0'));
}

/* Writes the image alternating two names so the image source reloads. */
QString saveFrame(const QImage &image, const QString &name, int *counter)
{
	*counter = (*counter + 1) % 2;
	const QString path = QDir(WorshipRender::CacheDir(QStringLiteral("live")))
				     .absoluteFilePath(QStringLiteral("%1-%2.png").arg(name).arg(*counter));
	image.save(path, "PNG");
	return path;
}

void setImage(obs_source_t *source, const QString &path)
{
	if (!source)
		return;
	OBSDataAutoRelease settings = obs_data_create();
	obs_data_set_string(settings, "file", QT_TO_UTF8(path));
	obs_source_update(source, settings);
}
} // namespace

LiveToolsDock::LiveToolsDock(const QString &objectName, const QString &title, QWidget *parent)
	: OBSDock(title, parent)
{
	setObjectName(objectName);

	auto *scroll = new QScrollArea(this);
	scroll->setWidgetResizable(true);
	scroll->setFrameShape(QFrame::NoFrame);
	auto *root = new QWidget(scroll);
	auto *layout = new QVBoxLayout(root);
	layout->setContentsMargins(8, 4, 8, 8);
	layout->setSpacing(6);

	/* --- On air ------------------------------------------------------- */
	/* --- The one place to drive what the congregation sees ------------ */
	layout->addWidget(heading(tr("Control en vivo"), root));
	programLabel = new QLabel(root);
	programLabel->setWordWrap(true);
	programLabel->setTextFormat(Qt::RichText);
	previewLabel = new QLabel(root);
	previewLabel->setWordWrap(true);
	previewLabel->setTextFormat(Qt::RichText);
	nowText = new QLabel(root);
	nowText->setWordWrap(true);
	nextText = new QLabel(root);
	nextText->setWordWrap(true);
	idian::Utils::addClass(nextText, "text-muted");

	goLiveButton = new QPushButton(tr("Pasar a pantalla  ➜"), root);
	goLiveButton->setObjectName(QStringLiteral("liveGo"));
	idian::Utils::addClass(goLiveButton, "button-primary");
	goLiveButton->setMinimumHeight(44);
	goLiveButton->setToolTip(tr("Lo que esta EN ESPERA pasa a la pantalla de la congregacion"));
	auto *previousButton = new QPushButton(tr("◀  Anterior"), root);
	auto *nextButton = new QPushButton(tr("Siguiente  ▶"), root);
	hideButton = new QPushButton(tr("Ocultar"), root);
	hideButton->setToolTip(tr("Deja la pantalla sin contenido sin perder el lugar"));
	auto *nowRow = new QHBoxLayout();
	nowRow->addWidget(previousButton);
	nowRow->addWidget(nextButton, 1);
	nowRow->addWidget(hideButton);
	/* Projector and stream separately (Herramientas > Proyector y transmisión). */
	roomMonitor = ScreensDialog::CreateRoomMonitor(root);
	roomMonitor->setToolTip(tr("Lo que ve la iglesia en el proyector"));
	/* What the church projector shows: the panels' content, any scene or black. */
	projectorScenes = new QWidget(root);
	projectorScenes->setObjectName(QStringLiteral("liveProjectorScenes"));
	auto *scenesGrid = new QGridLayout(projectorScenes);
	scenesGrid->setContentsMargins(0, 0, 0, 0);
	scenesGrid->setSpacing(4);
	projectorGroup = new QButtonGroup(this);
	projectorGroup->setExclusive(true);
	connect(projectorGroup, &QButtonGroup::buttonClicked, this, [](QAbstractButton *button) {
		const QString scene = button->property("scene").toString();
		OBSSourceAutoRelease source = scene.isEmpty() ? nullptr : obs_get_source_by_name(QT_TO_UTF8(scene));
		WorshipOutput::ShowOnProjector(source);
	});

	projectorButton = new QPushButton(tr("Pantalla del proyector..."), root);
	projectorButton->setObjectName(QStringLiteral("liveProjector"));
	projectorButton->setToolTip(tr("Muestra la salida a pantalla completa en el proyector o TV"));

	layout->addWidget(programLabel);
	layout->addWidget(roomMonitor);
	layout->addWidget(projectorScenes);
	layout->addWidget(previewLabel);
	layout->addWidget(nowText);
	layout->addWidget(nextText);
	layout->addWidget(goLiveButton);
	layout->addLayout(nowRow);
	layout->addWidget(projectorButton);
	separateButton = new QPushButton(tr("Proyector y transmisión..."), root);
	separateButton->setObjectName(QStringLiteral("liveSeparate"));
	separateButton->setToolTip(tr("Mostrar en el proyector algo distinto de lo que se transmite"));
	connect(separateButton, &QPushButton::clicked, this, [this]() {
		auto *dialog = new ScreensDialog(window());
		dialog->setAttribute(Qt::WA_DeleteOnClose);
		connect(dialog, &QDialog::finished, this, &LiveToolsDock::refreshOnAir);
		dialog->open();
	});
	layout->addWidget(separateButton);

	/* --- Countdown ---------------------------------------------------- */
	layout->addWidget(heading(tr("Cuenta regresiva"), root));
	countdownMode = new QComboBox(root);
	countdownMode->addItem(tr("Durante"));
	countdownMode->addItem(tr("Hasta la hora"));
	countdownMinutes = new QSpinBox(root);
	countdownMinutes->setRange(1, 240);
	countdownMinutes->setValue(5);
	countdownMinutes->setSuffix(tr(" min"));
	countdownTime = new QTimeEdit(QTime::currentTime().addSecs(15 * 60), root);
	countdownTime->setDisplayFormat(QStringLiteral("HH:mm"));
	countdownTime->setVisible(false);
	auto *modeRow = new QHBoxLayout();
	modeRow->addWidget(countdownMode, 1);
	modeRow->addWidget(countdownMinutes, 1);
	modeRow->addWidget(countdownTime, 1);
	countdownTitle = new QLineEdit(tr("El culto comienza en"), root);
	countdownEnd = new QLineEdit(tr("Bienvenidos"), root);
	countdownEnd->setPlaceholderText(tr("Texto al llegar a cero"));
	countdownBackground = new BackgroundPicker(false, false, root);
	WorshipRender::Background defaultBackground;
	defaultBackground.preset = 9;
	countdownBackground->setBackground(defaultBackground);
	countdownButton = new QPushButton(tr("Proyectar cuenta regresiva"), root);
	auto *countdownForm = new QFormLayout();
	countdownForm->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);
	countdownForm->addRow(tr("Tiempo"), modeRow);
	countdownForm->addRow(tr("Titulo"), countdownTitle);
	countdownForm->addRow(tr("Al final"), countdownEnd);
	countdownForm->addRow(tr("Fondo"), countdownBackground);
	layout->addLayout(countdownForm);
	layout->addWidget(countdownButton);

	/* --- Name lower third --------------------------------------------- */
	layout->addWidget(heading(tr("Rotulo de nombre"), root));
	nameEdit = new QLineEdit(root);
	nameEdit->setPlaceholderText(tr("Nombre (p. ej. Pr. Juan Perez)"));
	roleEdit = new QLineEdit(root);
	roleEdit->setPlaceholderText(tr("Cargo o tema (opcional)"));
	nameButton = new QPushButton(tr("Mostrar rotulo"), root);
	nameButton->setCheckable(true);
	layout->addWidget(nameEdit);
	layout->addWidget(roleEdit);
	layout->addWidget(nameButton);

	/* --- Urgent message ----------------------------------------------- */
	layout->addWidget(heading(tr("Mensaje urgente"), root));
	messageEdit = new QLineEdit(root);
	messageEdit->setPlaceholderText(tr("Se muestra arriba de todo lo que este al aire"));
	messageButton = new QPushButton(tr("Mostrar mensaje"), root);
	messageButton->setCheckable(true);
	layout->addWidget(messageEdit);
	layout->addWidget(messageButton);

	/* --- Stage display ------------------------------------------------ */
	layout->addWidget(heading(tr("Monitor de escenario"), root));
	auto *stageHelp = new QLabel(tr("Pantalla para cantantes y predicador: diapositiva actual, siguiente, reloj y "
					"mensajes."),
				     root);
	stageHelp->setWordWrap(true);
	idian::Utils::addClass(stageHelp, "text-muted");
	stageMessageEdit = new QLineEdit(root);
	stageMessageEdit->setPlaceholderText(tr("Mensaje al escenario (p. ej. \"5 minutos\")"));
	auto *sendButton = new QPushButton(tr("Enviar"), root);
	auto *clearMessage = new QPushButton(tr("Borrar"), root);
	auto *stageRow = new QHBoxLayout();
	stageRow->addWidget(stageMessageEdit, 1);
	stageRow->addWidget(sendButton);
	stageRow->addWidget(clearMessage);
	auto *openStage = new QPushButton(tr("Abrir en una pantalla..."), root);
	stopwatchButton = new QPushButton(tr("Iniciar cronometro"), root);
	stopwatchButton->setCheckable(true);
	auto *stageButtons = new QHBoxLayout();
	stageButtons->addWidget(openStage, 1);
	stageButtons->addWidget(stopwatchButton, 1);
	layout->addWidget(stageHelp);
	layout->addLayout(stageRow);
	layout->addLayout(stageButtons);

	/* --- Phone remote ------------------------------------------------- */
	layout->addWidget(heading(tr("Control desde el celular"), root));
	remoteButton = new QPushButton(tr("Activar control desde el celular"), root);
	remoteButton->setObjectName(QStringLiteral("livePhone"));
	remoteButton->setCheckable(true);
	remoteInfo = new QLabel(root);
	remoteInfo->setWordWrap(true);
	remoteInfo->setTextInteractionFlags(Qt::TextSelectableByMouse);
	remoteQr = new QLabel(root);
	remoteQr->setAlignment(Qt::AlignCenter);
	auto *newPin = new QPushButton(tr("Nuevo codigo"), root);
	layout->addWidget(remoteButton);
	layout->addWidget(remoteInfo);
	layout->addWidget(remoteQr);
	layout->addWidget(newPin);
	layout->addStretch(1);

	scroll->setWidget(root);
	setWidget(scroll);

	remoteServer = new WorshipRemote(this);
	clockTimer = new QTimer(this);
	clockTimer->setInterval(1000);
	clockTimer->start();

	WorshipLive &live = WorshipLive::instance();
	connect(&live, &WorshipLive::changed, this, &LiveToolsDock::onLiveChanged);
	connect(&live, &WorshipLive::stageMessageChanged, this, &LiveToolsDock::refreshStage);
	connect(previousButton, &QPushButton::clicked, &live, &WorshipLive::previous);
	connect(nextButton, &QPushButton::clicked, &live, &WorshipLive::next);
	connect(hideButton, &QPushButton::clicked, &live, &WorshipLive::toggleHidden);
	connect(goLiveButton, &QPushButton::clicked, this, &LiveToolsDock::goLive);
	connect(projectorButton, &QPushButton::clicked, this, &LiveToolsDock::openProgramProjector);
	connect(clockTimer, &QTimer::timeout, this, &LiveToolsDock::onTick);

	connect(countdownMode, &QComboBox::currentIndexChanged, this, [this](int index) {
		countdownMinutes->setVisible(index == 0);
		countdownTime->setVisible(index == 1);
	});
	connect(countdownButton, &QPushButton::clicked, this, [this]() {
		if (countdownActive) {
			stopCountdown();
			return;
		}
		if (countdownMode->currentIndex() == 0) {
			startCountdown(countdownMinutes->value(), countdownTitle->text());
		} else {
			QDateTime target(QDate::currentDate(), countdownTime->time());
			if (target < QDateTime::currentDateTime())
				target = target.addDays(1);
			startCountdown(qMax<qint64>(1, (QDateTime::currentDateTime().secsTo(target) + 59) / 60),
				       countdownTitle->text());
			countdownTarget = target;
		}
	});
	connect(countdownBackground, &BackgroundPicker::backgroundChanged, this, [this]() {
		if (countdownActive)
			renderCountdown();
	});

	connect(nameButton, &QPushButton::clicked, this, [this](bool checked) {
		if (checked)
			showName(nameEdit->text(), roleEdit->text());
		else
			hideName();
	});
	connect(messageButton, &QPushButton::clicked, this, [this](bool checked) {
		if (checked)
			showUrgentMessage(messageEdit->text());
		else
			hideUrgentMessage();
	});

	connect(sendButton, &QPushButton::clicked, this,
		[this]() { WorshipLive::instance().setStageMessage(stageMessageEdit->text().trimmed()); });
	connect(stageMessageEdit, &QLineEdit::returnPressed, sendButton, &QPushButton::click);
	connect(clearMessage, &QPushButton::clicked, this, [this]() {
		stageMessageEdit->clear();
		WorshipLive::instance().setStageMessage(QString());
	});
	connect(openStage, &QPushButton::clicked, this, &LiveToolsDock::openStageProjector);
	connect(stopwatchButton, &QPushButton::clicked, this, [this](bool checked) {
		stopwatchStart = checked ? QDateTime::currentDateTime() : QDateTime();
		stopwatchButton->setText(checked ? tr("Detener cronometro") : tr("Iniciar cronometro"));
		refreshStage();
	});

	connect(remoteButton, &QPushButton::clicked, this, &LiveToolsDock::setRemoteEnabled);
	connect(newPin, &QPushButton::clicked, this, [this]() {
		remoteServer->regeneratePin();
		updateRemoteInfo();
	});

	obs_frontend_add_event_callback(&LiveToolsDock::onFrontendEvent, this);
	registerHotkeys();

	updateRemoteInfo();
	if (WorshipOutput::ConfigString(ConfigRemote) == QStringLiteral("1"))
		QTimer::singleShot(0, this, [this]() { setRemoteEnabled(true); });
}

LiveToolsDock::~LiveToolsDock()
{
	obs_frontend_remove_event_callback(&LiveToolsDock::onFrontendEvent, this);
	saveHotkeys();
	for (obs_hotkey_id id : {nextHotkey, previousHotkey, hideHotkey, goLiveHotkey}) {
		if (id != OBS_INVALID_HOTKEY_ID)
			obs_hotkey_unregister(id);
	}
}

/* ------------------------------------------------------------------------ */

void LiveToolsDock::onLiveChanged()
{
	const LiveState state = WorshipLive::instance().state();
	nowText->setText(state.current);
	nextText->setText(state.next.isEmpty()
				  ? QString()
				  : tr("Sigue: %1").arg(QString(state.next).replace(QLatin1Char('\n'), QStringLiteral(" / "))));
	hideButton->setText(state.hidden ? tr("Mostrar") : tr("Ocultar"));
	refreshOnAir();
	refreshStage();
}

/* "WS Cantos B" -> "Cantos", plus the title of what the dock is showing. */
QString LiveToolsDock::describeScene(obs_source_t *sceneSource) const
{
	if (!sceneSource)
		return tr("nada");
	QString name = QT_UTF8(obs_source_get_name(sceneSource));
	if (!name.startsWith(QStringLiteral("WS ")))
		return tr("escena \"%1\"").arg(name);
	name = name.mid(3);
	if (name.endsWith(QStringLiteral(" B")))
		name.chop(2);
	const LiveState state = WorshipLive::instance().state();
	if (!state.title.isEmpty() && state.source.startsWith(name.left(4)))
		name += QStringLiteral(" · ") + state.title;
	return name;
}

void LiveToolsDock::rebuildProjectorScenes(const QString &onAir)
{
	/* "Contenido" (the panels), every scene of the user, and black. */
	const QStringList scenes = WorshipOutput::ProjectorScenes();
	const QString content = WorshipOutput::LastContentScene();
	QStringList wanted{QStringLiteral("\x01content")};
	wanted << scenes << QStringLiteral("\x01black");
	if (wanted != projectorSceneKeys) {
		projectorSceneKeys = wanted;
		for (QAbstractButton *button : projectorGroup->buttons()) {
			projectorGroup->removeButton(button);
			button->deleteLater();
		}
		auto *grid = static_cast<QGridLayout *>(projectorScenes->layout());
		for (int i = 0; i < wanted.size(); ++i) {
			const QString &key = wanted.at(i);
			const bool special = key.startsWith(QLatin1Char('\x01'));
			auto *button = new QPushButton(key == QStringLiteral("\x01content") ? tr("Cantos, Biblia...")
						       : key == QStringLiteral("\x01black")  ? tr("Negro")
											      : key,
						       projectorScenes);
			button->setCheckable(true);
			button->setToolTip(special ? (key == QStringLiteral("\x01black") ? tr("El proyector queda en negro")
											 : tr("Lo último de Cantos, Biblia, Himnario o Presentaciones"))
						   : tr("Mostrar la escena \"%1\" en el proyector").arg(key));
			button->setProperty("key", key);
			projectorGroup->addButton(button);
			grid->addWidget(button, i / 2, i % 2);
		}
	}
	for (QAbstractButton *button : projectorGroup->buttons()) {
		const QString key = button->property("key").toString();
		const QString scene = key == QStringLiteral("\x01content") ? content
				      : key == QStringLiteral("\x01black")  ? QString()
									     : key;
		button->setProperty("scene", scene);
		const bool current = key == QStringLiteral("\x01black") ? onAir.isEmpty()
				     : key == QStringLiteral("\x01content") ? onAir.startsWith(QStringLiteral("WS "))
									     : onAir == key;
		button->setChecked(current);
		if (key == QStringLiteral("\x01content"))
			button->setEnabled(!content.isEmpty());
	}
}

void LiveToolsDock::refreshOnAir()
{
	const bool separate = WorshipOutput::SeparateRoomScreen();
	roomMonitor->setVisible(separate);
	projectorScenes->setVisible(separate);
	projectorButton->setText(separate ? tr("Abrir el proyector...") : tr("Pantalla del proyector..."));
	if (separate) {
		const QString onAir = WorshipOutput::OnAirSceneName();
		OBSSourceAutoRelease roomSource = onAir.isEmpty() ? nullptr : obs_get_source_by_name(QT_TO_UTF8(onAir));
		OBSSourceAutoRelease stream = obs_frontend_get_current_scene();
		programLabel->setText(ScreensDialog::ProjectorChip(roomSource ? describeScene(roomSource) : tr("negro")));
		const QString state = obs_frontend_streaming_active() ? tr("al aire") : tr("sin transmitir");
		previewLabel->setText(ScreensDialog::StreamChip(QStringLiteral("%1 · %2").arg(describeScene(stream), state)));
		previewLabel->setVisible(true);
		goLiveButton->setVisible(false);
		rebuildProjectorScenes(onAir);
		return;
	}
	const bool studio = obs_frontend_preview_program_mode_active();
	OBSSourceAutoRelease program = obs_frontend_get_current_scene();
	programLabel->setText(QStringLiteral("<span style=\"color:#E5484D\">●</span> <b>%1</b> %2")
				      .arg(tr("EN PANTALLA:"), describeScene(program).toHtmlEscaped()));
	previewLabel->setVisible(studio);
	goLiveButton->setVisible(studio);
	if (studio) {
		OBSSourceAutoRelease preview = obs_frontend_get_current_preview_scene();
		previewLabel->setText(QStringLiteral("<span style=\"color:#30A46C\">●</span> <b>%1</b> %2")
					      .arg(tr("EN ESPERA:"), describeScene(preview).toHtmlEscaped()));
	}
}

void LiveToolsDock::registerHotkeys()
{
	/* One set for everything on screen: songs, Bible, hymns, presentations.
	 * They also work with presentation clickers and while the app is in the
	 * background (Settings > Hotkeys). */
	nextHotkey = obs_hotkey_register_frontend("WorshipStudio.Next", "WorshipStudio: siguiente", hotkeyCall, this);
	previousHotkey =
		obs_hotkey_register_frontend("WorshipStudio.Previous", "WorshipStudio: anterior", hotkeyCall, this);
	hideHotkey =
		obs_hotkey_register_frontend("WorshipStudio.Hide", "WorshipStudio: ocultar / mostrar", hotkeyCall, this);
	goLiveHotkey = obs_hotkey_register_frontend("WorshipStudio.GoLive", "WorshipStudio: pasar a pantalla",
						    hotkeyCall, this);

	const QString json = WorshipOutput::ConfigString(ConfigHotkeys);
	OBSDataAutoRelease data = json.isEmpty() ? nullptr : obs_data_create_from_json(QT_TO_UTF8(json));
	if (!data)
		return;
	const std::pair<const char *, obs_hotkey_id> entries[] = {
		{"next", nextHotkey}, {"previous", previousHotkey}, {"hide", hideHotkey}, {"golive", goLiveHotkey}};
	for (const auto &[name, id] : entries) {
		OBSDataArrayAutoRelease array = obs_data_get_array(data, name);
		obs_hotkey_load(id, array);
	}
}

void LiveToolsDock::saveHotkeys()
{
	OBSDataAutoRelease data = obs_data_create();
	const std::pair<const char *, obs_hotkey_id> entries[] = {
		{"next", nextHotkey}, {"previous", previousHotkey}, {"hide", hideHotkey}, {"golive", goLiveHotkey}};
	for (const auto &[name, id] : entries) {
		OBSDataArrayAutoRelease array = obs_hotkey_save(id);
		obs_data_set_array(data, name, array);
	}
	WorshipOutput::SetConfigString(ConfigHotkeys, QT_UTF8(obs_data_get_json(data)));
}

void LiveToolsDock::runHotkey(obs_hotkey_id id)
{
	WorshipLive &live = WorshipLive::instance();
	if (id == nextHotkey)
		live.next();
	else if (id == previousHotkey)
		live.previous();
	else if (id == hideHotkey)
		live.toggleHidden();
	else if (id == goLiveHotkey)
		goLive();
}

void LiveToolsDock::goLive()
{
	if (obs_frontend_preview_program_mode_active())
		obs_frontend_preview_program_trigger_transition();
}

void LiveToolsDock::openProgramProjector()
{
	/* The congregation sees the program (or the room screen when it is
	 * separate from the stream): full screen on the chosen display. */
	const bool separate = WorshipOutput::SeparateRoomScreen();
	const char *type = separate ? "Scene"
				    : obs_frontend_preview_program_mode_active() ? "StudioProgram" : "Preview";
	const char *name = separate ? WorshipOutput::RoomScreenScene : nullptr;
	QMenu menu(this);
	const QList<QScreen *> screens = QGuiApplication::screens();
	for (int i = 0; i < screens.size(); ++i) {
		const QSize size = screens.at(i)->geometry().size();
		menu.addAction(tr("Pantalla %1: %2 (%3x%4)").arg(i + 1).arg(screens.at(i)->name()).arg(size.width()).arg(size.height()),
			       this, [i, type, name]() { obs_frontend_open_projector(type, i, "", name); });
	}
	menu.addSeparator();
	menu.addAction(tr("En una ventana"), this, [type, name]() { obs_frontend_open_projector(type, -1, "", name); });
	menu.addSeparator();
	menu.addAction(tr("Proyector y transmisión..."), this, [this]() {
		auto *dialog = new ScreensDialog(window());
		dialog->setAttribute(Qt::WA_DeleteOnClose);
		connect(dialog, &QDialog::finished, this, &LiveToolsDock::refreshOnAir);
		dialog->open();
	});
	menu.exec(QCursor::pos());
}

void LiveToolsDock::onTick()
{
	if (countdownActive)
		renderCountdown();
	if (stageSceneRef)
		refreshStage();
}

/* ------------------------------------------------------------------------ */
/* Countdown                                                                 */

void LiveToolsDock::startCountdown(int minutes, const QString &title)
{
	countdownSceneRef = WorshipOutput::EnsureScene(CountdownSceneName);
	bool created = false;
	OBSDataAutoRelease settings = obs_data_create();
	countdownSource = WorshipOutput::EnsureSource(countdownSceneRef, CountdownSourceName, "image_source", settings,
						      &created);
	if (created && countdownSource)
		WorshipOutput::FitItem(countdownSceneRef, countdownSource, 0.0f, 0.0f, 1.0f, 1.0f);

	if (!title.isNull())
		countdownTitle->setText(title);
	countdownTarget = QDateTime::currentDateTime().addSecs(qint64(minutes) * 60);
	countdownActive = true;
	countdownButton->setText(tr("Detener cuenta regresiva"));
	renderCountdown();
	WorshipOutput::ShowScene(countdownSceneRef);
}

void LiveToolsDock::stopCountdown()
{
	countdownActive = false;
	countdownButton->setText(tr("Proyectar cuenta regresiva"));
	refreshStage();
}

void LiveToolsDock::renderCountdown()
{
	const qint64 remaining = QDateTime::currentDateTime().secsTo(countdownTarget);
	const QSize canvas(WorshipOutput::CanvasWidth(), WorshipOutput::CanvasHeight());
	QImage image;
	if (remaining <= 0) {
		/* At zero the end text replaces the clock and the countdown stops. */
		WorshipRender::LyricsSlide end;
		end.text = countdownEnd->text();
		end.background = countdownBackground->background();
		end.maxFontSize = 120;
		image = WorshipRender::Lyrics(end, canvas);
		countdownActive = false;
		countdownButton->setText(tr("Proyectar cuenta regresiva"));
	} else {
		image = WorshipRender::Countdown(countdownTitle->text(), formatSeconds(remaining), QString(),
						 countdownBackground->background(), WorshipRender::RecommendedFonts().value(0),
						 canvas);
	}
	setImage(countdownSource, saveFrame(image, QStringLiteral("cuenta"), &countdownCounter));
}

/* ------------------------------------------------------------------------ */
/* Overlays: one scene nested on top of whatever is on air                   */

void LiveToolsDock::ensureOverlays()
{
	overlaySceneRef = WorshipOutput::EnsureScene(OverlaySceneName);
	OBSDataAutoRelease settings = obs_data_create();
	bool created = false;
	nameSource = WorshipOutput::EnsureSource(overlaySceneRef, NameSourceName, "image_source", settings, &created);
	if (created && nameSource)
		WorshipOutput::FitItem(overlaySceneRef, nameSource, 0.0f, 0.0f, 1.0f, 1.0f);
	messageSource = WorshipOutput::EnsureSource(overlaySceneRef, MessageSourceName, "image_source", settings, &created);
	if (created && messageSource)
		WorshipOutput::FitItem(overlaySceneRef, messageSource, 0.0f, 0.0f, 1.0f, 1.0f);
}

void LiveToolsDock::attachOverlays()
{
	if (!overlaySceneRef || (!nameVisible && !messageVisible))
		return;
	obs_source_t *overlay = obs_scene_get_source(overlaySceneRef);

	/* The program scene, and the preview one in Studio Mode. */
	OBSSourceAutoRelease program = obs_frontend_get_current_scene();
	OBSSourceAutoRelease preview = obs_frontend_preview_program_mode_active()
					       ? obs_frontend_get_current_preview_scene()
					       : nullptr;
	for (obs_source_t *target : {program.Get(), preview.Get()}) {
		obs_scene_t *scene = target ? obs_scene_from_source(target) : nullptr;
		if (!scene || target == overlay)
			continue;
		obs_sceneitem_t *item = obs_scene_find_source(scene, OverlaySceneName);
		if (!item) {
			item = obs_scene_add(scene, overlay);
			WorshipOutput::FitItem(scene, overlay, 0.0f, 0.0f, 1.0f, 1.0f);
		}
		obs_sceneitem_set_visible(item, true);
		obs_sceneitem_set_order(item, OBS_ORDER_MOVE_TOP);
	}
}

void LiveToolsDock::setOverlayVisible(const char *sourceName, bool visible)
{
	ensureOverlays();
	if (obs_sceneitem_t *item = obs_scene_find_source(overlaySceneRef, sourceName))
		obs_sceneitem_set_visible(item, visible);
	attachOverlays();
}

void LiveToolsDock::showName(const QString &name, const QString &role)
{
	if (name.trimmed().isEmpty()) {
		nameButton->setChecked(false);
		return;
	}
	ensureOverlays();
	nameEdit->setText(name);
	roleEdit->setText(role);
	const QSize canvas(WorshipOutput::CanvasWidth(), WorshipOutput::CanvasHeight());
	static int counter = 0;
	setImage(nameSource, saveFrame(WorshipRender::NameLowerThird(name, role, WorshipRender::RecommendedFonts().value(0),
								    canvas),
				       QStringLiteral("rotulo"), &counter));
	nameVisible = true;
	nameButton->setChecked(true);
	nameButton->setText(tr("Ocultar rotulo"));
	setOverlayVisible(NameSourceName, true);
}

void LiveToolsDock::hideName()
{
	nameVisible = false;
	nameButton->setChecked(false);
	nameButton->setText(tr("Mostrar rotulo"));
	setOverlayVisible(NameSourceName, false);
}

void LiveToolsDock::showUrgentMessage(const QString &text)
{
	if (text.trimmed().isEmpty()) {
		messageButton->setChecked(false);
		return;
	}
	ensureOverlays();
	messageEdit->setText(text);
	const QSize canvas(WorshipOutput::CanvasWidth(), WorshipOutput::CanvasHeight());
	static int counter = 0;
	setImage(messageSource,
		 saveFrame(WorshipRender::UrgentMessage(text, WorshipRender::RecommendedFonts().value(0), canvas),
			   QStringLiteral("mensaje"), &counter));
	messageVisible = true;
	messageButton->setChecked(true);
	messageButton->setText(tr("Ocultar mensaje"));
	setOverlayVisible(MessageSourceName, true);
}

void LiveToolsDock::hideUrgentMessage()
{
	messageVisible = false;
	messageButton->setChecked(false);
	messageButton->setText(tr("Mostrar mensaje"));
	setOverlayVisible(MessageSourceName, false);
}

void LiveToolsDock::onFrontendEvent(enum obs_frontend_event event, void *data)
{
	auto *dock = static_cast<LiveToolsDock *>(data);
	/* Overlays follow the operator to whatever scene goes on air. */
	if (event == OBS_FRONTEND_EVENT_SCENE_CHANGED || event == OBS_FRONTEND_EVENT_PREVIEW_SCENE_CHANGED)
		QMetaObject::invokeMethod(dock, [dock]() { dock->attachOverlays(); }, Qt::QueuedConnection);
	if (event == OBS_FRONTEND_EVENT_SCENE_CHANGED || event == OBS_FRONTEND_EVENT_PREVIEW_SCENE_CHANGED ||
	    event == OBS_FRONTEND_EVENT_STUDIO_MODE_ENABLED || event == OBS_FRONTEND_EVENT_STUDIO_MODE_DISABLED ||
	    event == OBS_FRONTEND_EVENT_STREAMING_STARTED || event == OBS_FRONTEND_EVENT_STREAMING_STOPPED ||
	    event == OBS_FRONTEND_EVENT_SCENE_LIST_CHANGED)
		QMetaObject::invokeMethod(dock, [dock]() { dock->refreshOnAir(); }, Qt::QueuedConnection);
}

/* ------------------------------------------------------------------------ */
/* Stage display                                                             */

void LiveToolsDock::refreshStage()
{
	if (!stageSceneRef)
		return;
	const LiveState state = WorshipLive::instance().state();
	WorshipRender::StageInfo info;
	info.clock = QTime::currentTime().toString(QStringLiteral("HH:mm"));
	info.title = state.title;
	info.section = state.section;
	info.current = state.current;
	info.next = state.next;
	info.hidden = state.hidden;
	info.message = WorshipLive::instance().stageMessage();
	if (countdownActive)
		info.timer = tr("Cuenta regresiva %1").arg(formatSeconds(QDateTime::currentDateTime().secsTo(countdownTarget)));
	else if (stopwatchStart.isValid())
		info.timer = tr("Cronometro %1").arg(formatSeconds(stopwatchStart.secsTo(QDateTime::currentDateTime())));

	const QSize canvas(WorshipOutput::CanvasWidth(), WorshipOutput::CanvasHeight());
	setImage(stageSource, saveFrame(WorshipRender::StageDisplay(info, canvas), QStringLiteral("escenario"),
					&stageCounter));
}

void LiveToolsDock::ensureStage()
{
	stageSceneRef = WorshipOutput::EnsureScene(StageSceneName);
	OBSDataAutoRelease settings = obs_data_create();
	bool created = false;
	stageSource = WorshipOutput::EnsureSource(stageSceneRef, StageSourceName, "image_source", settings, &created);
	if (created && stageSource)
		WorshipOutput::FitItem(stageSceneRef, stageSource, 0.0f, 0.0f, 1.0f, 1.0f);
	refreshStage();
}

void LiveToolsDock::openStageProjector()
{
	ensureStage();

	/* Full screen on the chosen display, or a window. */
	QMenu menu(this);
	const QList<QScreen *> screens = QGuiApplication::screens();
	for (int i = 0; i < screens.size(); ++i) {
		const QSize size = screens.at(i)->geometry().size();
		menu.addAction(tr("Pantalla %1: %2 (%3x%4)").arg(i + 1).arg(screens.at(i)->name()).arg(size.width()).arg(size.height()),
			       this, [i]() { obs_frontend_open_projector("Scene", i, "", StageSceneName); });
	}
	menu.addSeparator();
	menu.addAction(tr("En una ventana"), this, []() { obs_frontend_open_projector("Scene", -1, "", StageSceneName); });
	menu.exec(QCursor::pos());
}

/* ------------------------------------------------------------------------ */
/* Phone remote                                                              */

void LiveToolsDock::setRemoteEnabled(bool enabled)
{
	if (enabled && !remoteServer->start())
		enabled = false;
	if (!enabled)
		remoteServer->stop();
	WorshipOutput::SetConfigString(ConfigRemote, enabled ? QStringLiteral("1") : QStringLiteral("0"));
	remoteButton->setChecked(enabled);
	updateRemoteInfo();
}

void LiveToolsDock::updateRemoteInfo()
{
	if (!remoteServer->isRunning()) {
		remoteButton->setText(tr("Activar control desde el celular"));
		remoteInfo->setText(tr("El celular debe estar en la misma red Wi-Fi que esta computadora. No hace falta "
				       "instalar nada."));
		remoteQr->clear();
		remoteQr->setVisible(false);
		return;
	}
	remoteButton->setText(tr("Desactivar control desde el celular"));
	remoteInfo->setText(tr("Escanea el codigo o abre <b>%1</b> en el navegador del celular.<br>Codigo de acceso: "
			       "<b>%2</b>")
				    .arg(remoteServer->url(), remoteServer->pin()));
	remoteQr->setPixmap(QPixmap::fromImage(remoteServer->qrCode(180)));
	remoteQr->setVisible(true);
}
