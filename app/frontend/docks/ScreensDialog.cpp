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
#include <QFrame>
#include <QRadioButton>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QGuiApplication>
#include <QLabel>
#include <QPainter>
#include <QPushButton>
#include <QScreen>
#include <QTimer>
#include <QToolButton>
#include <QVBoxLayout>

#include <functional>

#include "moc_ScreensDialog.cpp"

namespace {
constexpr const char *CameraSceneName = "Solo cámara";
constexpr const char *ComboSceneName = "Cámara + proyector";
constexpr const char *ScreenSceneName = "Transmisión";
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
	case ScreensDialog::StreamLayout::CameraOnly:
		*camera = {0.0f, 0.0f, 1.0f, 1.0f};
		*screen = {0.0f, 0.0f, 0.0f, 0.0f};
		*screenOnTop = false;
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

bool ScreensDialog::CreateStreamScenes(StreamLayout layout, const QString &cameraName, QString *firstScene)
{
	OBSScene projector = WorshipOutput::EnsureScene(WorshipOutput::RoomScreenScene);
	if (!projector)
		return false;
	obs_source_t *projectorSource = obs_scene_get_source(projector);

	OBSSourceAutoRelease existing = cameraName.isEmpty() ? nullptr : obs_get_source_by_name(QT_TO_UTF8(cameraName));
	OBSSource camera = existing && isCamera(existing) ? OBSSource(existing.Get()) : OBSSource();
	if (!camera) {
		const QStringList cameras = Cameras();
		OBSSourceAutoRelease first = cameras.isEmpty() ? nullptr : obs_get_source_by_name(QT_TO_UTF8(cameras.first()));
		camera = first ? OBSSource(first.Get()) : createCamera();
	}

	/* Only scenes that do not exist yet are created: nothing is removed or
	 * rearranged in the scenes people already have. */
	const auto create = [](const char *name, std::function<void(obs_scene_t *)> fill) {
		OBSSourceAutoRelease taken = obs_get_source_by_name(name);
		if (taken)
			return false;
		OBSScene scene = WorshipOutput::EnsureScene(name);
		if (scene)
			fill(scene);
		return true;
	};
	const auto place = [](obs_scene_t *scene, obs_source_t *source, const Box &box, obs_bounds_type bounds) {
		obs_scene_add(scene, source);
		WorshipOutput::FitItem(scene, source, box.x, box.y, box.w, box.h, OBS_ALIGN_CENTER, bounds);
	};

	if (!camera) {
		/* No camera: the stream shows what the church sees. */
		create(ScreenSceneName, [&](obs_scene_t *scene) {
			place(scene, projectorSource, {0.0f, 0.0f, 1.0f, 1.0f}, OBS_BOUNDS_SCALE_INNER);
		});
		if (firstScene)
			*firstScene = QString::fromUtf8(ScreenSceneName);
		return true;
	}

	create(CameraSceneName, [&](obs_scene_t *scene) {
		place(scene, camera, {0.0f, 0.0f, 1.0f, 1.0f}, OBS_BOUNDS_SCALE_OUTER);
	});
	create(ComboSceneName, [&](obs_scene_t *scene) {
		Box cameraBox, screenBox;
		bool screenOnTop = true;
		boxesFor(layout, &cameraBox, &screenBox, &screenOnTop);
		const obs_bounds_type cameraBounds =
			layout == StreamLayout::CameraWithCorner ? OBS_BOUNDS_SCALE_OUTER : OBS_BOUNDS_SCALE_INNER;
		if (!screenOnTop)
			place(scene, projectorSource, screenBox, OBS_BOUNDS_SCALE_INNER);
		place(scene, camera, cameraBox, cameraBounds);
		if (screenOnTop)
			place(scene, projectorSource, screenBox, OBS_BOUNDS_SCALE_INNER);
	});
	if (firstScene)
		*firstScene = QString::fromUtf8(ComboSceneName);
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

namespace {
/* "● SALA" / "● EN VIVO": the two names used everywhere, with their colors. */
QString chip(const QString &color, const QString &name, const QString &text)
{
	return QStringLiteral("<span style=\"color:%1\">●</span> <b>%2</b> &nbsp;%3").arg(color, name.toHtmlEscaped(), text.toHtmlEscaped());
}

QWidget *choiceCard(QRadioButton *radio, const QString &text, QWidget *parent)
{
	auto *card = new QFrame(parent);
	card->setFrameShape(QFrame::StyledPanel);
	auto *layout = new QVBoxLayout(card);
	layout->setContentsMargins(14, 12, 14, 12);
	layout->setSpacing(4);
	QFont bold = radio->font();
	bold.setBold(true);
	radio->setFont(bold);
	radio->setParent(card);
	auto *label = new QLabel(text, card);
	label->setWordWrap(true);
	label->setContentsMargins(24, 0, 0, 0);
	idian::Utils::addClass(label, "text-muted");
	layout->addWidget(radio);
	layout->addWidget(label);
	return card;
}
} // namespace

QString ScreensDialog::ProjectorChip(const QString &text)
{
	return chip(QStringLiteral("#F2A65A"), tr("PROYECTOR"), text);
}

QString ScreensDialog::StreamChip(const QString &text)
{
	return chip(QStringLiteral("#E5484D"), tr("TRANSMISIÓN"), text);
}

ScreensDialog::ScreensDialog(QWidget *parent) : QDialog(parent)
{
	setWindowTitle(tr("Proyector y transmisión"));
	setMinimumWidth(640);
	auto *layout = new QVBoxLayout(this);
	layout->setSpacing(12);

	sameRadio = new QRadioButton(tr("Proyector y transmisión iguales"));
	separateRadio = new QRadioButton(tr("Proyector y transmisión por separado"));
	layout->addWidget(choiceCard(sameRadio, tr("Lo que mandás al proyector también se transmite."), this));
	layout->addWidget(choiceCard(separateRadio,
				     tr("Las escenas manejan la transmisión, como siempre. El proyector de la iglesia "
					"muestra los cantos y la Biblia, o la escena que elijas en el panel En vivo."),
				     this));
	(WorshipOutput::SeparateRoomScreen() ? separateRadio : sameRadio)->setChecked(true);

	details = new QWidget(this);
	auto *form = new QFormLayout(details);
	form->setContentsMargins(0, 4, 0, 0);
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
	monitorCombo->setCurrentIndex(monitorIndex >= 0 ? monitorIndex
							: (screens.size() > 1 ? 1 : monitorCombo->count() - 1));
	form->addRow(tr("Proyector"), monitorCombo);

	createCheck = new QCheckBox(tr("Crear las escenas \"Solo cámara\" y \"Cámara + proyector\""), details);
	createCheck->setChecked(true);
	form->addRow(QString(), createCheck);

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
	const QStringList designs{tr("Proyector en la esquina"), tr("Cámara en la esquina"), tr("Lado a lado")};
	for (int i = 0; i < designs.size(); ++i) {
		auto *button = new QToolButton(layouts);
		button->setCheckable(true);
		button->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);
		button->setIcon(layoutIcon(static_cast<StreamLayout>(i)));
		button->setIconSize(QSize(128, 72));
		button->setText(designs.at(i));
		button->setMinimumWidth(150);
		layoutGroup->addButton(button, i);
		layoutsRow->addWidget(button);
	}
	const int savedLayout = WorshipOutput::ConfigString(ConfigLayout).toInt();
	if (QAbstractButton *button = layoutGroup->button(qBound(0, savedLayout, 2)))
		button->setChecked(true);
	form->addRow(tr("Cámara + proyector"), layouts);
	layout->addWidget(details);

	status = new QLabel(this);
	status->setWordWrap(true);
	layout->addWidget(status);

	auto *buttons = new QDialogButtonBox(QDialogButtonBox::Cancel, this);
	QPushButton *applyButton = buttons->addButton(tr("Aplicar"), QDialogButtonBox::AcceptRole);
	idian::Utils::addClass(applyButton, "button-primary");
	layout->addWidget(buttons);

	const auto updateEnabled = [this]() {
		details->setVisible(separateRadio->isChecked());
		cameraCombo->setEnabled(createCheck->isChecked());
		for (QAbstractButton *button : layoutGroup->buttons())
			button->setEnabled(createCheck->isChecked());
		adjustSize();
	};
	connect(separateRadio, &QRadioButton::toggled, this, updateEnabled);
	connect(createCheck, &QCheckBox::toggled, this, updateEnabled);
	updateEnabled();
	connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
	connect(applyButton, &QPushButton::clicked, this, &ScreensDialog::apply);
}

void ScreensDialog::apply()
{
	/* What the church sees now stays on screen when switching. */
	const QString onAir = WorshipOutput::OnAirSceneName();
	OBSSourceAutoRelease onAirSource = onAir.isEmpty() ? nullptr : obs_get_source_by_name(QT_TO_UTF8(onAir));

	if (!separateRadio->isChecked()) {
		WorshipOutput::SetSeparateRoomScreen(false);
		/* Straight to the screen (also in Studio Mode): it was already on air. */
		if (onAirSource)
			obs_frontend_set_current_scene(onAirSource);
		accept();
		return;
	}

	WorshipOutput::SetSeparateRoomScreen(true);
	if (onAirSource)
		WorshipOutput::ShowOnProjector(onAirSource);

	const auto layout = static_cast<StreamLayout>(qMax(0, layoutGroup->checkedId()));
	if (createCheck->isChecked()) {
		QString first;
		if (CreateStreamScenes(layout, cameraCombo->currentData().toString(), &first)) {
			OBSSourceAutoRelease scene = obs_get_source_by_name(QT_TO_UTF8(first));
			if (scene) {
				if (obs_frontend_preview_program_mode_active())
					obs_frontend_set_current_preview_scene(scene);
				else
					obs_frontend_set_current_scene(scene);
			}
		}
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

namespace {
/* Live picture of the room screen, refreshed a few times per second. A
 * plain widget (not a native display) so it scrolls with the panel. */
class RoomMonitor : public QLabel {
public:
	explicit RoomMonitor(QWidget *parent) : QLabel(parent)
	{
		setMinimumHeight(90);
		/* Small enough that the EN VIVO buttons stay in view. */
		setMaximumHeight(150);
		setAlignment(Qt::AlignCenter);
		setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
		setStyleSheet(QStringLiteral("background:#000; border-radius:6px;"));
		timer.setInterval(200);
		connect(&timer, &QTimer::timeout, this, [this]() { refresh(); });
	}
	bool hasHeightForWidth() const override { return true; }
	int heightForWidth(int width) const override { return qMin(150, width * 9 / 16); }
	QSize sizeHint() const override { return {266, 150}; }

protected:
	void showEvent(QShowEvent *event) override
	{
		QLabel::showEvent(event);
		timer.start();
		refresh();
	}
	void hideEvent(QHideEvent *event) override
	{
		QLabel::hideEvent(event);
		timer.stop();
	}

private:
	QTimer timer;

	void refresh()
	{
		if (!isVisible() || width() < 20)
			return;
		OBSSourceAutoRelease room = obs_get_source_by_name(WorshipOutput::RoomScreenScene);
		const qreal ratio = devicePixelRatioF();
		const QSize size = QSize(16, 9).scaled(QSize(width() - 2, height() - 2) * ratio, Qt::KeepAspectRatio);
		QImage image = room ? WorshipOutput::RenderSource(room, (uint32_t)size.width(), (uint32_t)size.height())
				    : QImage();
		if (image.isNull()) {
			image = QImage(size, QImage::Format_RGB32);
			image.fill(Qt::black);
		}
		QPixmap pixmap = QPixmap::fromImage(image);
		pixmap.setDevicePixelRatio(ratio);
		setPixmap(pixmap);
	}
};
} // namespace

QWidget *ScreensDialog::CreateRoomMonitor(QWidget *parent)
{
	return new RoomMonitor(parent);
}
