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

#include "ScreensDialog.hpp"
#include "WorshipOutput.hpp"

#include <obs-frontend-api.h>
#include <qt-wrappers.hpp>
#include <Idian/Utils.hpp>

#include <QButtonGroup>
#include <QCheckBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QGuiApplication>
#include <QLabel>
#include <QPainter>
#include <QPushButton>
#include <QScreen>
#include <QToolButton>
#include <QVBoxLayout>

#include "moc_ScreensDialog.cpp"

namespace {
constexpr const char *StreamSceneName = "Transmisión";
constexpr const char *NewCameraName = "Cámara";
constexpr const char *ConfigMonitor = "RoomScreenMonitor";
constexpr const char *ConfigLayout = "StreamLayout";
constexpr const char *ConfigCamera = "StreamCamera";

const char *const CameraIds[] = {"macos-avcapture", "macos-avcapture-fast", "av_capture_input", "dshow_input",
				 "v4l2_input"};

bool isCamera(obs_source_t *source)
{
	const char *id = obs_source_get_unversioned_id(source);
	for (const char *cameraId : CameraIds) {
		if (id && strcmp(id, cameraId) == 0)
			return true;
	}
	return false;
}

/* Where the camera and the room screen go on the stream (canvas fractions). */
struct Box {
	float x, y, w, h;
};

void boxesFor(ScreensDialog::StreamLayout layout, Box *camera, Box *screen, bool *screenOnTop)
{
	switch (layout) {
	case ScreensDialog::StreamLayout::CameraWithCorner:
		*camera = {0.0f, 0.0f, 1.0f, 1.0f};
		*screen = {0.63f, 0.63f, 0.34f, 0.34f};
		*screenOnTop = true;
		break;
	case ScreensDialog::StreamLayout::ScreenWithCamera:
		*screen = {0.0f, 0.0f, 1.0f, 1.0f};
		*camera = {0.03f, 0.63f, 0.34f, 0.34f};
		*screenOnTop = false;
		break;
	case ScreensDialog::StreamLayout::SideBySide:
		*camera = {0.02f, 0.235f, 0.47f, 0.53f};
		*screen = {0.51f, 0.235f, 0.47f, 0.53f};
		*screenOnTop = true;
		break;
	case ScreensDialog::StreamLayout::ScreenOnly:
		*camera = {0.0f, 0.0f, 0.0f, 0.0f};
		*screen = {0.0f, 0.0f, 1.0f, 1.0f};
		*screenOnTop = true;
		break;
	}
}

/* Small drawing of each stream layout for the buttons. */
QIcon layoutIcon(ScreensDialog::StreamLayout layout)
{
	const QSize size(128, 72);
	QPixmap pixmap(size * 2);
	pixmap.setDevicePixelRatio(2.0);
	pixmap.fill(Qt::transparent);
	QPainter painter(&pixmap);
	painter.setRenderHint(QPainter::Antialiasing);
	painter.setPen(Qt::NoPen);
	painter.setBrush(QColor(0x14, 0x13, 0x12));
	painter.drawRoundedRect(QRectF(0, 0, size.width(), size.height()), 6, 6);

	Box camera, screen;
	bool screenOnTop = true;
	boxesFor(layout, &camera, &screen, &screenOnTop);
	const auto rect = [&](const Box &box) {
		return QRectF(box.x * size.width(), box.y * size.height(), box.w * size.width(), box.h * size.height());
	};
	const auto drawCamera = [&]() {
		if (camera.w <= 0.0f)
			return;
		const QRectF r = rect(camera).adjusted(1, 1, -1, -1);
		painter.setBrush(QColor(0x5A, 0x54, 0x4B));
		painter.drawRoundedRect(r, 4, 4);
		/* A person: head and shoulders. */
		painter.setBrush(QColor(0x8A, 0x82, 0x76));
		const qreal unit = r.height() / 6.0;
		painter.drawEllipse(QPointF(r.center().x(), r.top() + unit * 2.3), unit, unit);
		painter.drawRoundedRect(QRectF(r.center().x() - unit * 1.8, r.top() + unit * 3.7, unit * 3.6, unit * 2.3),
					unit, unit);
	};
	const auto drawScreen = [&]() {
		const QRectF r = rect(screen).adjusted(1, 1, -1, -1);
		painter.setBrush(QColor(0xE6, 0xDE, 0xD0));
		painter.drawRoundedRect(r, 4, 4);
		painter.setBrush(QColor(0x34, 0x30, 0x2B));
		for (int line = 0; line < 2; ++line) {
			const qreal w = r.width() * (line ? 0.42 : 0.6);
			painter.drawRoundedRect(QRectF(r.center().x() - w / 2, r.center().y() - r.height() * 0.14 +
								 line * r.height() * 0.2,
						      w, r.height() * 0.08),
						2, 2);
		}
	};
	if (screenOnTop) {
		drawCamera();
		drawScreen();
	} else {
		drawScreen();
		drawCamera();
	}
	painter.end();
	return QIcon(pixmap);
}

/* A new camera source with the first device of the computer. */
OBSSource createCamera()
{
	const char *id = nullptr;
	for (const char *cameraId : CameraIds) {
		if (obs_source_get_display_name(cameraId)) {
			id = cameraId;
			break;
		}
	}
	if (!id)
		return {};
	OBSSourceAutoRelease camera = obs_source_create(id, NewCameraName, nullptr, nullptr);
	if (!camera)
		return {};
	obs_properties_t *props = obs_source_properties(camera);
	for (const char *name : {"device", "device_id", "video_device_id"}) {
		obs_property_t *list = props ? obs_properties_get(props, name) : nullptr;
		if (!list || obs_property_list_item_count(list) == 0)
			continue;
		for (size_t i = 0; i < obs_property_list_item_count(list); ++i) {
			const char *value = obs_property_list_item_string(list, i);
			if (value && *value) {
				OBSDataAutoRelease settings = obs_data_create();
				obs_data_set_string(settings, name, value);
				obs_source_update(camera, settings);
				break;
			}
		}
		break;
	}
	obs_properties_destroy(props);
	return OBSSource(camera.Get());
}
} // namespace

QStringList ScreensDialog::Cameras()
{
	QStringList names;
	obs_enum_sources(
		[](void *param, obs_source_t *source) {
			if (isCamera(source))
				static_cast<QStringList *>(param)->append(QT_UTF8(obs_source_get_name(source)));
			return true;
		},
		&names);
	names.sort(Qt::CaseInsensitive);
	return names;
}

bool ScreensDialog::BuildStreamScene(StreamLayout layout, const QString &cameraName, QString *error)
{
	OBSScene room = WorshipOutput::EnsureScene(WorshipOutput::RoomScreenScene);
	OBSScene stream = WorshipOutput::EnsureScene(StreamSceneName);
	if (!room || !stream)
		return false;

	OBSSource camera;
	if (layout != StreamLayout::ScreenOnly) {
		OBSSourceAutoRelease existing = cameraName.isEmpty() ? nullptr : obs_get_source_by_name(QT_TO_UTF8(cameraName));
		camera = existing ? OBSSource(existing.Get()) : createCamera();
		if (!camera) {
			if (error)
				*error = tr("No se encontró ninguna cámara en esta computadora.");
			return false;
		}
	}

	/* Start clean: the scene only has the camera and the room screen. */
	obs_scene_enum_items(
		stream,
		[](obs_scene_t *, obs_sceneitem_t *item, void *) {
			obs_sceneitem_remove(item);
			return true;
		},
		nullptr);

	Box cameraBox, screenBox;
	bool screenOnTop = true;
	boxesFor(layout, &cameraBox, &screenBox, &screenOnTop);
	obs_source_t *roomSource = obs_scene_get_source(room);
	const auto place = [&](obs_source_t *source, const Box &box, obs_bounds_type bounds) {
		obs_scene_add(stream, source);
		WorshipOutput::FitItem(stream, source, box.x, box.y, box.w, box.h, OBS_ALIGN_CENTER, bounds);
	};
	if (camera && !screenOnTop)
		place(roomSource, screenBox, OBS_BOUNDS_SCALE_INNER);
	if (camera)
		place(camera, cameraBox,
		      layout == StreamLayout::CameraWithCorner ? OBS_BOUNDS_SCALE_OUTER : OBS_BOUNDS_SCALE_INNER);
	if (!camera || screenOnTop)
		place(roomSource, screenBox, OBS_BOUNDS_SCALE_INNER);

	/* The stream goes to air with its own scene. */
	obs_frontend_set_current_scene(obs_scene_get_source(stream));
	if (obs_frontend_preview_program_mode_active())
		obs_frontend_set_current_preview_scene(obs_scene_get_source(stream));
	return true;
}

void ScreensDialog::OpenRoomScreen(int monitor)
{
	WorshipOutput::EnsureScene(WorshipOutput::RoomScreenScene);
	obs_frontend_open_projector("Scene", monitor, "", WorshipOutput::RoomScreenScene);
}

void ScreensDialog::RestoreRoomScreen()
{
	if (!WorshipOutput::SeparateRoomScreen())
		return;
	const QString saved = WorshipOutput::ConfigString(ConfigMonitor);
	bool ok = false;
	const int monitor = saved.toInt(&ok);
	/* Only on a second display: never cover the operator's own screen. */
	if (ok && monitor > 0 && monitor < QGuiApplication::screens().size())
		OpenRoomScreen(monitor);
}

ScreensDialog::ScreensDialog(QWidget *parent) : QDialog(parent)
{
	setWindowTitle(tr("Pantallas y transmisión"));
	setMinimumWidth(620);
	auto *layout = new QVBoxLayout(this);
	layout->setSpacing(12);

	auto *intro = new QLabel(tr("Como en las transmisiones profesionales: la pantalla de la sala muestra solo la "
				    "letra, la Biblia o la presentación, y la transmisión muestra la cámara con eso "
				    "mismo más chico."),
				 this);
	intro->setWordWrap(true);
	layout->addWidget(intro);

	separateCheck = new QCheckBox(tr("La sala y la transmisión muestran cosas distintas"), this);
	separateCheck->setChecked(WorshipOutput::SeparateRoomScreen());
	layout->addWidget(separateCheck);

	details = new QWidget(this);
	auto *form = new QFormLayout(details);
	form->setContentsMargins(0, 0, 0, 0);
	form->setVerticalSpacing(10);

	monitorCombo = new QComboBox(details);
	const QList<QScreen *> screens = QGuiApplication::screens();
	for (int i = 0; i < screens.size(); ++i) {
		const QSize size = screens.at(i)->geometry().size();
		monitorCombo->addItem(tr("Pantalla %1: %2 (%3x%4)")
					      .arg(i + 1)
					      .arg(screens.at(i)->name())
					      .arg(size.width())
					      .arg(size.height()),
				      i);
	}
	monitorCombo->addItem(tr("En una ventana (para probar)"), -1);
	const QString savedMonitor = WorshipOutput::ConfigString(ConfigMonitor);
	const int monitorIndex = savedMonitor.isEmpty() ? -1 : monitorCombo->findData(savedMonitor.toInt());
	monitorCombo->setCurrentIndex(monitorIndex >= 0 ? monitorIndex : qMin(1, monitorCombo->count() - 1));
	form->addRow(tr("Proyector de la sala"), monitorCombo);

	cameraCombo = new QComboBox(details);
	for (const QString &camera : Cameras())
		cameraCombo->addItem(camera, camera);
	cameraCombo->addItem(tr("Agregar la cámara de esta computadora"), QString());
	const int cameraIndex = cameraCombo->findData(WorshipOutput::ConfigString(ConfigCamera));
	cameraCombo->setCurrentIndex(cameraIndex >= 0 ? cameraIndex : 0);
	form->addRow(tr("Cámara"), cameraCombo);

	auto *layouts = new QWidget(details);
	auto *layoutsRow = new QHBoxLayout(layouts);
	layoutsRow->setContentsMargins(0, 0, 0, 0);
	layoutGroup = new QButtonGroup(this);
	const QStringList names{tr("Cámara y pantalla en la esquina"), tr("Pantalla y cámara en la esquina"),
				tr("Lado a lado"), tr("Igual que la sala")};
	for (int i = 0; i < names.size(); ++i) {
		auto *button = new QToolButton(layouts);
		button->setCheckable(true);
		button->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);
		button->setIcon(layoutIcon(static_cast<StreamLayout>(i)));
		button->setIconSize(QSize(128, 72));
		button->setText(names.at(i));
		button->setMinimumWidth(140);
		layoutGroup->addButton(button, i);
		layoutsRow->addWidget(button);
	}
	const int savedLayout = WorshipOutput::ConfigString(ConfigLayout).toInt();
	if (QAbstractButton *button = layoutGroup->button(qBound(0, savedLayout, 3)))
		button->setChecked(true);
	form->addRow(tr("Transmisión"), layouts);
	layout->addWidget(details);

	auto *hint = new QLabel(tr("Se crea la escena \"Transmisión\". Podés moverla y agrandar cada parte como "
				   "cualquier escena, y usar \"Pantalla de la sala\" como fuente en tus otras escenas."),
				this);
	hint->setWordWrap(true);
	idian::Utils::addClass(hint, "text-muted");
	layout->addWidget(hint);

	status = new QLabel(this);
	status->setWordWrap(true);
	layout->addWidget(status);

	auto *buttons = new QDialogButtonBox(QDialogButtonBox::Cancel, this);
	QPushButton *applyButton = buttons->addButton(tr("Aplicar"), QDialogButtonBox::AcceptRole);
	idian::Utils::addClass(applyButton, "button-primary");
	layout->addWidget(buttons);

	const auto updateEnabled = [this]() { details->setEnabled(separateCheck->isChecked()); };
	connect(separateCheck, &QCheckBox::toggled, this, updateEnabled);
	updateEnabled();
	connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
	connect(applyButton, &QPushButton::clicked, this, &ScreensDialog::apply);
}

void ScreensDialog::apply()
{
	const bool separate = separateCheck->isChecked();
	if (!separate) {
		WorshipOutput::SetSeparateRoomScreen(false);
		accept();
		return;
	}

	/* What is on screen now stays there when switching. */
	const QString onAir = WorshipOutput::OnAirSceneName();
	WorshipOutput::SetSeparateRoomScreen(true);
	if (!onAir.isEmpty() && WorshipOutput::IsInternalScene(onAir)) {
		OBSSourceAutoRelease source = obs_get_source_by_name(QT_TO_UTF8(onAir));
		if (obs_scene_t *scene = source ? obs_scene_from_source(source) : nullptr)
			WorshipOutput::ShowScene(scene);
	}

	const auto layout = static_cast<StreamLayout>(qMax(0, layoutGroup->checkedId()));
	QString error;
	if (!BuildStreamScene(layout, cameraCombo->currentData().toString(), &error)) {
		status->setText(error);
		return;
	}
	const int monitor = monitorCombo->currentData().toInt();
	WorshipOutput::SetConfigString(ConfigMonitor, QString::number(monitor));
	WorshipOutput::SetConfigString(ConfigLayout, QString::number((int)layout));
	WorshipOutput::SetConfigString(ConfigCamera, cameraCombo->currentData().toString().isEmpty()
							     ? QString::fromUtf8(NewCameraName)
							     : cameraCombo->currentData().toString());
	OpenRoomScreen(monitor);
	accept();
}
