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

#include "GuidedTour.hpp"
#include "AppearanceDialog.hpp"
#include "WorshipOutput.hpp"

#include <obs-frontend-api.h>
#include <obs.hpp>
#include <Idian/Utils.hpp>

#include <QApplication>
#include <QDockWidget>
#include <QEvent>
#include <QHBoxLayout>
#include <QLabel>
#include <QListView>
#include <QListWidget>
#include <QMainWindow>
#include <QMenuBar>
#include <QPainter>
#include <QPainterPath>
#include <QPushButton>
#include <QTimer>
#include <QToolBar>
#include <QVBoxLayout>

#include "moc_GuidedTour.cpp"

namespace {
constexpr const char *ConfigGuide = "GuideShown";

QString sceneName(obs_source_t *scene)
{
	OBSSourceAutoRelease owned = scene;
	return scene ? QString::fromUtf8(obs_source_get_name(scene)) : QString();
}

/* Scenes made by people (WorshipStudio's own start with "WS "). */
int userSceneCount()
{
	obs_frontend_source_list scenes = {};
	obs_frontend_get_scenes(&scenes);
	int count = 0;
	for (size_t i = 0; i < scenes.sources.num; ++i)
		if (!QString::fromUtf8(obs_source_get_name(scenes.sources.array[i])).startsWith(QStringLiteral("WS ")))
			++count;
	obs_frontend_source_list_free(&scenes);
	return count;
}

/* Sources in all user scenes, so adding one anywhere counts. */
int userSourceCount()
{
	obs_frontend_source_list scenes = {};
	obs_frontend_get_scenes(&scenes);
	int count = 0;
	for (size_t i = 0; i < scenes.sources.num; ++i) {
		obs_source_t *source = scenes.sources.array[i];
		if (QString::fromUtf8(obs_source_get_name(source)).startsWith(QStringLiteral("WS ")))
			continue;
		obs_scene_enum_items(
			obs_scene_from_source(source),
			[](obs_scene_t *, obs_sceneitem_t *item, void *data) {
				const char *name = obs_source_get_name(obs_sceneitem_get_source(item));
				if (!QString::fromUtf8(name).startsWith(QStringLiteral("WS ")))
					++*static_cast<int *>(data);
				return true;
			},
			&count);
	}
	obs_frontend_source_list_free(&scenes);
	return count;
}

QString programName()
{
	return sceneName(obs_frontend_get_current_scene());
}

QString previewName()
{
	return obs_frontend_preview_program_mode_active() ? sceneName(obs_frontend_get_current_preview_scene())
							  : QString();
}

/* Brings a tabbed dock to the front so its widgets can be pointed at. */
void raiseDock(QMainWindow *window, const char *name)
{
	if (auto *dock = window->findChild<QDockWidget *>(QString::fromLatin1(name))) {
		dock->show();
		dock->raise();
	}
}
} // namespace

GuidedTour::GuidedTour(QMainWindow *window_) : QWidget(window_), window(window_)
{
	setAttribute(Qt::WA_TransparentForMouseEvents, false);
	setAttribute(Qt::WA_DeleteOnClose);
	setGeometry(window->rect());
	window->installEventFilter(this);

	/* The bubble is its own small floating window: the video previews are
	 * native surfaces that would cover a normal child widget. */
	bubble = new QWidget(window, Qt::Tool | Qt::FramelessWindowHint);
	bubble->setObjectName(QStringLiteral("tourBubble"));
	bubble->setAttribute(Qt::WA_StyledBackground);
	bubble->setAttribute(Qt::WA_ShowWithoutActivating);
	bubble->setStyleSheet(QStringLiteral(
		"#tourBubble{background:#F4F1EC;border-radius:14px;}"
		"#tourBubble QLabel{color:#1B1915;background:transparent;}"
		"#tourBubble QLabel#tourCounter{color:#8A8276;font-size:12px;font-weight:600;}"
		"#tourBubble QLabel#tourTitle{font-size:17px;font-weight:700;}"
		"#tourBubble QLabel#tourText{font-size:14px;}"
		"#tourBubble QLabel#tourDone{color:#24966C;font-weight:700;}"
		"#tourBubble QPushButton{background:transparent;color:#5A544B;border:1px solid #D9D1C3;"
		"border-radius:8px;padding:7px 14px;font-size:13px;min-height:0px;max-height:none;margin:0px;}"
		"#tourBubble QPushButton:hover{background:#ECE6DB;}"
		"#tourBubble QPushButton#tourNext{background:#1B1915;color:#F4F1EC;border:1px solid #1B1915;font-weight:600;}"
		"#tourBubble QPushButton#tourNext:hover{background:#34302B;}"
		"#tourBubble QPushButton#tourExit{border:none;color:#8A8276;padding:7px 6px;}"));
	bubble->setFixedWidth(360);

	auto *layout = new QVBoxLayout(bubble);
	layout->setContentsMargins(20, 18, 20, 16);
	layout->setSpacing(8);
	counterLabel = new QLabel(bubble);
	counterLabel->setObjectName(QStringLiteral("tourCounter"));
	titleLabel = new QLabel(bubble);
	titleLabel->setObjectName(QStringLiteral("tourTitle"));
	titleLabel->setWordWrap(true);
	textLabel = new QLabel(bubble);
	textLabel->setObjectName(QStringLiteral("tourText"));
	textLabel->setWordWrap(true);
	textLabel->setTextFormat(Qt::RichText);
	doneLabel = new QLabel(QStringLiteral("✓ ") + tr("¡Bien hecho!"), bubble);
	doneLabel->setObjectName(QStringLiteral("tourDone"));
	doneLabel->hide();
	extraSlot = new QWidget(bubble);
	auto *extraLayout = new QVBoxLayout(extraSlot);
	extraLayout->setContentsMargins(0, 4, 0, 0);

	auto *buttons = new QHBoxLayout();
	auto *exitButton = new QPushButton(tr("Salir del recorrido"), bubble);
	exitButton->setObjectName(QStringLiteral("tourExit"));
	backButton = new QPushButton(tr("Atrás"), bubble);
	nextButton = new QPushButton(bubble);
	nextButton->setObjectName(QStringLiteral("tourNext"));
	buttons->addWidget(exitButton);
	buttons->addStretch(1);
	buttons->addWidget(backButton);
	buttons->addWidget(nextButton);

	layout->addWidget(counterLabel);
	layout->addWidget(titleLabel);
	layout->addWidget(textLabel);
	layout->addWidget(extraSlot);
	layout->addWidget(doneLabel);
	layout->addSpacing(4);
	layout->addLayout(buttons);

	connect(exitButton, &QPushButton::clicked, this, &GuidedTour::finish);
	connect(backButton, &QPushButton::clicked, this, [this]() { goTo(current - 1); });
	connect(nextButton, &QPushButton::clicked, this, [this]() { goTo(current + 1); });

	timer = new QTimer(this);
	timer->setInterval(400);
	connect(timer, &QTimer::timeout, this, &GuidedTour::refresh);

	buildSteps();
	show();
	raise();
	bubble->show();
	bubble->raise();
	timer->start();
	goTo(0);
}

void GuidedTour::buildSteps()
{
	QMainWindow *w = window;
	auto byName = [w](const char *name) { return [w, name]() -> QWidget * {
		return w->findChild<QWidget *>(QString::fromLatin1(name));
	}; };
	/* The "+" button of a list toolbar (scenes, sources). */
	auto addButton = [w](const char *toolbar, const char *action) {
		return [w, toolbar, action]() -> QWidget * {
			auto *bar = w->findChild<QToolBar *>(QString::fromLatin1(toolbar));
			auto *act = w->findChild<QAction *>(QString::fromLatin1(action));
			return bar && act ? bar->widgetForAction(act) : nullptr;
		};
	};

	steps.push_back({tr("Bienvenido a WorshipStudio"),
			 tr("En un par de minutos te muestro cómo armar tu reunión. Hacé cada paso en la ventana "
			    "real: lo que tenés que tocar queda iluminado. También podés avanzar con Siguiente."),
			 {}, {}, {}});

	steps.push_back({tr("1. Creá una escena"),
			 tr("Una escena es lo que se ve en pantalla: la cámara, un video, el púlpito... Tocá <b>+</b> "
			    "debajo de <b>Escenas</b> y ponele un nombre, por ejemplo <i>Cámara</i>."),
			 addButton("scenesToolbar", "actionAddScene"), {},
			 [this]() { return userSceneCount() > sceneCount; }});

	steps.push_back({tr("2. Agregale fuentes"),
			 tr("Con la escena elegida, tocá <b>+</b> debajo de <b>Fuentes</b> y elegí qué mostrar: "
			    "<b>Dispositivo de captura de video</b> para la cámara, <b>Imagen</b>, <b>Video</b> o "
			    "<b>Captura de pantalla</b>."),
			 addButton("sourcesToolbar", "actionAddSource"), {},
			 [this]() { return userSourceCount() > sourceCount; }});

	steps.push_back({tr("3. Cambiá de escena"),
			 tr("Hacé clic en otra escena de la lista. Primero aparece <b>EN ESPERA</b> (izquierda) y "
			    "nadie la ve; con <b>Pasar a pantalla</b> la ve la congregación."),
			 byName("scenes"), {},
			 [this]() { return previewName() != previewScene || programName() != programScene; }});

	steps.push_back({tr("4. Agregá un canto"),
			 tr("En <b>Cantos</b> tocá <b>Nuevo</b>, escribí el título y la letra. Una línea en blanco "
			    "separa las diapositivas, y <b>[Coro]</b> marca el coro para saltar con la tecla C."),
			 byName("songsNew"), [w]() { raiseDock(w, "songsDock"); },
			 [this, w]() {
				 auto *list = w->findChild<QListWidget *>(QStringLiteral("songsList"));
				 return list && list->count() > songCount;
			 }});

	steps.push_back({tr("5. Preparalo en espera"),
			 tr("Hacé <b>doble clic</b> en el canto: aparece <b>EN ESPERA</b>. La congregación todavía no "
			    "lo ve, así podés revisar que esté bien."),
			 byName("songsList"), [w]() { raiseDock(w, "songsDock"); },
			 []() { return previewName().startsWith(QStringLiteral("WS Cantos")); }});

	steps.push_back({tr("6. Pasalo a la pantalla"),
			 tr("Tocá <b>Pasar a pantalla</b>: ahora lo ve la gente. Avanzá con <b>Siguiente ▶</b>, las "
			    "flechas del teclado o el presentador inalámbrico."),
			 byName("liveGo"), {},
			 []() { return programName().startsWith(QStringLiteral("WS Cantos")); }});

	steps.push_back({tr("7. La Biblia, igual de fácil"),
			 tr("Escribí una cita como <i>Juan 3:16</i> o una frase, y hacé doble clic en el versículo. "
			    "<b>Más Biblias</b> descarga otras versiones gratis."),
			 byName("bibleSearch"), [w]() { raiseDock(w, "bibleDock"); }, {}});

	steps.push_back({tr("8. Organizá la reunión"),
			 tr("En <b>Orden del culto</b> agregá cantos, himnos, pasajes y anuncios en el orden de la "
			    "reunión. Durante el culto, <b>Siguiente elemento</b> lleva cada uno a la pantalla."),
			 byName("serviceDock"), [w]() { raiseDock(w, "serviceDock"); }, {}});

	steps.push_back({tr("9. Conectá el proyector"),
			 tr("Tocá <b>Pantalla del proyector</b> y elegí el proyector o la TV: ahí se verá a pantalla "
			    "completa lo que está EN PANTALLA."),
			 byName("liveProjector"), {}, {}});

	auto *customize = new QPushButton(tr("Personalizar ahora..."), extraSlot);
	customize->hide();
	connect(customize, &QPushButton::clicked, this, [this]() {
		auto *dialog = new AppearanceDialog(window);
		dialog->setAttribute(Qt::WA_DeleteOnClose);
		dialog->open();
	});
#ifdef __APPLE__
	const QString menuPlace = tr("En la barra de menú de arriba de la pantalla:");
	const std::function<QWidget *()> menuTarget;
#else
	const QString menuPlace = tr("En el menú de la ventana:");
	const std::function<QWidget *()> menuTarget = [w]() -> QWidget * { return w->menuBar(); };
#endif
	steps.push_back({tr("10. Hacelo tuyo"),
			 tr("Elegí modo oscuro o claro, el fondo y tu color principal.") + QStringLiteral("<br>") +
				 menuPlace + QStringLiteral(" <b>") + tr("Ver → Personalizar apariencia") +
				 QStringLiteral("</b>"),
			 menuTarget, {}, {}, customize});

	steps.push_back({tr("11. Manejalo desde el celular"),
			 tr("Tocá <b>Activar control desde el celular</b> y escaneá el código con la cámara del "
			    "teléfono: pasás diapositivas, cambiás escenas y más, sin instalar nada."),
			 byName("livePhone"), {}, {}});

	steps.push_back({tr("¡Listo!"),
			 tr("Ya sabés lo esencial. Podés volver a ver este recorrido cuando quieras en <b>Ayuda → Guía "
			    "de inicio de WorshipStudio</b>."),
			 {}, {}, {}});
}

GuidedTour *GuidedTour::start(QMainWindow *window)
{
	for (GuidedTour *existing : window->findChildren<GuidedTour *>())
		existing->finish();
	/* The tour teaches the on hold -> on screen flow. */
	if (!obs_frontend_preview_program_mode_active())
		obs_frontend_set_preview_program_mode(true);
	return new GuidedTour(window);
}

void GuidedTour::showOnFirstRun(QMainWindow *window)
{
	if (WorshipOutput::ConfigString(ConfigGuide) == QStringLiteral("1"))
		return;
	/* The setup wizard opens first on a new install: wait until it closes. */
	if (QApplication::activeModalWidget()) {
		QTimer::singleShot(1000, window, [window]() { showOnFirstRun(window); });
		return;
	}
	WorshipOutput::SetConfigString(ConfigGuide, QStringLiteral("1"));
	start(window);
}

void GuidedTour::snapshot()
{
	sceneCount = userSceneCount();
	sourceCount = userSourceCount();
	auto *list = window ? window->findChild<QListWidget *>(QStringLiteral("songsList")) : nullptr;
	songCount = list ? list->count() : 0;
	programScene = programName();
	previewScene = previewName();
}

void GuidedTour::goTo(int index)
{
	if (index >= stepCount()) {
		finish();
		return;
	}
	current = qBound(0, index, stepCount() - 1);
	finishedStep = false;
	const Step &step = steps[current];
	if (step.prepare)
		step.prepare();
	snapshot();

	counterLabel->setText(current == 0 || current == stepCount() - 1
				      ? QString()
				      : tr("Paso %1 de %2").arg(current).arg(stepCount() - 2));
	counterLabel->setVisible(!counterLabel->text().isEmpty());
	titleLabel->setText(step.title);
	textLabel->setText(step.text);
	doneLabel->hide();
	for (QWidget *child : extraSlot->findChildren<QWidget *>(Qt::FindDirectChildrenOnly))
		child->hide();
	if (step.extra) {
		step.extra->setParent(extraSlot);
		extraSlot->layout()->addWidget(step.extra);
		step.extra->show();
	}
	extraSlot->setVisible(step.extra != nullptr);
	backButton->setVisible(current > 0);
	nextButton->setText(current == 0 ? tr("Empezar") : current == stepCount() - 1 ? tr("Terminar") : tr("Siguiente"));
	refresh();
}

QWidget *GuidedTour::bubbleWidget() const
{
	return bubble;
}

bool GuidedTour::targetVisible() const
{
	if (current < 0 || !steps[current].target)
		return true;
	QWidget *target = steps[current].target();
	return target && target->isVisible() && target->width() > 0;
}

void GuidedTour::refresh()
{
	if (!window || current < 0)
		return;
	setGeometry(window->rect());

	const Step &step = steps[current];
	QWidget *target = step.target ? step.target() : nullptr;
	if (target && target->isVisible())
		hole = QRect(target->mapTo(window, QPoint(0, 0)), target->size()).adjusted(-6, -6, 6, 6);
	else
		hole = QRect();

	/* Clicks inside the hole reach the real window. */
	QRegion mask(rect());
	if (!hole.isNull())
		mask = mask.subtracted(QRegion(hole));
	setMask(mask);

	if (!finishedStep && step.done && step.done()) {
		finishedStep = true;
		doneLabel->show();
		bubble->adjustSize();
		const int step_ = current;
		QTimer::singleShot(1100, this, [this, step_]() {
			if (current == step_)
				goTo(current + 1);
		});
	}
	placeBubble();
	update();
}

void GuidedTour::placeBubble()
{
	bubble->adjustSize();
	const QSize size = bubble->size();
	const QRect area = window->rect().adjusted(16, 16, -16, -16);
	QPoint pos;
	if (hole.isNull()) {
		pos = area.center() - QPoint(size.width() / 2, size.height() / 2);
	} else if (hole.right() + 16 + size.width() <= area.right()) {
		pos = QPoint(hole.right() + 16, hole.center().y() - size.height() / 2);
	} else if (hole.left() - 16 - size.width() >= area.left()) {
		pos = QPoint(hole.left() - 16 - size.width(), hole.center().y() - size.height() / 2);
	} else if (hole.bottom() + 16 + size.height() <= area.bottom()) {
		pos = QPoint(hole.center().x() - size.width() / 2, hole.bottom() + 16);
	} else {
		pos = QPoint(hole.center().x() - size.width() / 2, hole.top() - 16 - size.height());
	}
	pos.setX(qBound(area.left(), pos.x(), area.right() - size.width()));
	pos.setY(qBound(area.top(), pos.y(), area.bottom() - size.height()));
	bubble->move(window->mapToGlobal(pos));
	bubble->raise();
}

void GuidedTour::paintEvent(QPaintEvent *)
{
	QPainter p(this);
	p.setRenderHint(QPainter::Antialiasing);
	QPainterPath dim;
	dim.addRect(rect());
	if (!hole.isNull()) {
		QPainterPath cut;
		cut.addRoundedRect(hole, 10, 10);
		dim = dim.subtracted(cut);
	}
	p.fillPath(dim, QColor(8, 7, 6, 165));
	if (!hole.isNull()) {
		p.setPen(QPen(QColor(0xE6, 0xDE, 0xD0), 2.5));
		p.setBrush(Qt::NoBrush);
		p.drawRoundedRect(QRectF(hole).adjusted(1, 1, -1, -1), 10, 10);
	}
}

bool GuidedTour::eventFilter(QObject *watched, QEvent *event)
{
	if (watched == window && (event->type() == QEvent::Resize || event->type() == QEvent::Move ||
				  event->type() == QEvent::LayoutRequest))
		QTimer::singleShot(0, this, &GuidedTour::refresh);
	return QWidget::eventFilter(watched, event);
}

void GuidedTour::finish()
{
	timer->stop();
	if (window)
		window->removeEventFilter(this);
	bubble->deleteLater();
	close();
}
