#include "WelcomeGuide.hpp"
#include "LiveToolsDock.hpp"
#include "WorshipOutput.hpp"

#include <obs-frontend-api.h>
#include <Idian/Utils.hpp>

#include <QCheckBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QPainter>
#include <QPainterPath>
#include <QPushButton>
#include <QStackedWidget>
#include <QTimer>
#include <QVBoxLayout>

#include "moc_WelcomeGuide.cpp"

namespace {
constexpr const char *ConfigGuide = "GuideShown";

enum Region { Everything, Screens, Content, LiveControl, Projector };

/* A small drawing of the WorshipStudio window with one part highlighted. */
class WindowDiagram : public QWidget {
public:
	WindowDiagram(int region, QWidget *parent) : QWidget(parent), region(region)
	{
		setMinimumSize(520, 270);
		setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
	}

protected:
	void paintEvent(QPaintEvent *) override
	{
		QPainter p(this);
		p.setRenderHint(QPainter::Antialiasing);
		const QColor frame(0x1D, 0x20, 0x29), panel(0x2E, 0x32, 0x41), text(0xC9, 0xCD, 0xD8);
		const QColor ember(0xE2, 0x58, 0x2B), preview(0x30, 0xA4, 0x6C), program(0xE5, 0x48, 0x4D);

		const QRectF win = QRectF(rect()).adjusted(6, 6, -6, -6);
		QPainterPath shape;
		shape.addRoundedRect(win, 12, 12);
		p.fillPath(shape, frame);

		const qreal W = win.width(), H = win.height();
		/* Same arrangement as the real window: scenes on the left, the two
		 * screens in the middle, controls below, content column on the right. */
		const QRectF scenes(win.left() + W * 0.02, win.top() + H * 0.05, W * 0.14, H * 0.91);
		const QRectF center(win.left() + W * 0.18, win.top() + H * 0.05, W * 0.50, H * 0.60);
		const QRectF left(center.left(), center.top(), center.width() * 0.49, center.height());
		const QRectF right(center.left() + center.width() * 0.51, center.top(), center.width() * 0.49, center.height());
		const QRectF bottom(win.left() + W * 0.18, win.top() + H * 0.69, W * 0.50, H * 0.27);
		const QRectF column(win.left() + W * 0.70, win.top() + H * 0.05, W * 0.28, H * 0.53);
		const QRectF live(win.left() + W * 0.70, win.top() + H * 0.61, W * 0.28, H * 0.35);

		auto box = [&](const QRectF &r, const QColor &fill, const QString &label, const QColor &labelColor,
			       qreal scale = 1.0) {
			QPainterPath path;
			path.addRoundedRect(r, 6, 6);
			p.fillPath(path, fill);
			p.setPen(labelColor);
			QFont font = p.font();
			font.setPixelSize(qMax(9, int(H * 0.048 * scale)));
			font.setBold(true);
			p.setFont(font);
			p.drawText(r.adjusted(4, 4, -4, -4), Qt::AlignCenter | Qt::TextWordWrap, label);
		};

		box(scenes, panel, tr("Escenas y fuentes"), text, 0.85);
		box(left, QColor(0x0E, 0x1F, 0x17), tr("EN ESPERA\nvista previa"), preview);
		box(right, QColor(0x24, 0x10, 0x12), tr("EN PANTALLA\nlo que ve la gente"), program);
		box(bottom, panel, tr("Audio · Transiciones · Transmitir y grabar"), text, 0.85);
		box(column, panel, tr("Orden · Cantos\nHimnario · Biblia\nPresentaciones"), text, 0.85);
		box(live, panel, tr("Control en vivo\nPasar a pantalla\n◀ ▶  Ocultar"), text, 0.85);

		/* Dim everything but the highlighted part. */
		QRectF focus;
		switch (region) {
		case Screens:
			focus = center;
			break;
		case Content:
			focus = column;
			break;
		case LiveControl:
		case Projector:
			focus = live;
			break;
		default:
			break;
		}
		if (!focus.isNull()) {
			QPainterPath dim;
			dim.addRoundedRect(win, 12, 12);
			QPainterPath hole;
			hole.addRoundedRect(focus.adjusted(-4, -4, 4, 4), 8, 8);
			p.fillPath(dim.subtracted(hole), QColor(0, 0, 0, 150));
			p.setPen(QPen(ember, 3));
			p.setBrush(Qt::NoBrush);
			p.drawRoundedRect(focus.adjusted(-4, -4, 4, 4), 8, 8);
		}
	}

private:
	int region;
};
} // namespace

WelcomeGuide::WelcomeGuide(LiveToolsDock *liveTools_, QWidget *parent) : QDialog(parent), liveTools(liveTools_)
{
	setWindowTitle(tr("Bienvenido a WorshipStudio"));
	setMinimumSize(640, 560);

	pages = new QStackedWidget(this);
	stepLabel = new QLabel(this);
	idian::Utils::addClass(stepLabel, "text-muted");
	backButton = new QPushButton(tr("Atrás"), this);
	nextButton = new QPushButton(this);
	idian::Utils::addClass(nextButton, "button-primary");
	nextButton->setDefault(true);
	auto *skipButton = new QPushButton(tr("Saltar guía"), this);

	addPage(Everything, tr("Todo lo que va en la pantalla, en un solo lugar"),
		tr("WorshipStudio proyecta canciones, Biblia, himnarios y presentaciones, y si querés también "
		   "transmite y graba en vivo. Todo desde esta misma ventana."));
	addPage(Screens, tr("Dos pantallas: lo que preparás y lo que ve la gente"),
		tr("<b style='color:#30A46C'>EN ESPERA</b> (izquierda) es tu borrador: ahí aparece lo que elegís, "
		   "y nadie lo ve todavía.<br><b style='color:#E5484D'>EN PANTALLA</b> (derecha) es exactamente lo "
		   "que ve la congregación en el proyector y en la transmisión."));
	addPage(Content, tr("Elegí qué mostrar"),
		tr("En la columna de la derecha están el <b>Orden del culto</b>, <b>Cantos</b>, <b>Himnario</b>, "
		   "<b>Biblia</b> y <b>Presentaciones</b>.<br>Hacé <b>doble clic</b> en un canto, un himno, un "
		   "versículo o una diapositiva y aparece EN ESPERA."));
	addPage(LiveControl, tr("Pasalo a la pantalla"),
		tr("Abajo a la derecha está el <b>Control en vivo</b>: <b>Pasar a pantalla</b> manda lo que está en "
		   "espera a la congregación. <b>Siguiente</b>, <b>Anterior</b> y <b>Ocultar</b> sirven para "
		   "cualquier contenido.<br>También funcionan las flechas y el presentador inalámbrico."));

	auto *projectorBox = new QWidget(this);
	auto *projectorLayout = new QVBoxLayout(projectorBox);
	projectorLayout->setContentsMargins(0, 0, 0, 0);
	auto *projectorButton = new QPushButton(tr("Elegir la pantalla del proyector..."), projectorBox);
	studioCheck = new QCheckBox(tr("Preparar EN ESPERA antes de mostrar (recomendado)"), projectorBox);
	studioCheck->setChecked(true);
	projectorLayout->addWidget(projectorButton);
	projectorLayout->addWidget(studioCheck);
	addPage(Projector, tr("Conectá el proyector"),
		tr("Elegí la pantalla donde se proyecta: ahí se mostrará a pantalla completa lo que está EN "
		   "PANTALLA. Podés cambiarla después con el botón <b>Pantalla del proyector</b>.<br>En "
		   "<b>Ver → Personalizar apariencia</b> elegís modo claro u oscuro y tu color."),
		projectorBox);

	auto *buttons = new QHBoxLayout();
	buttons->addWidget(stepLabel);
	buttons->addStretch(1);
	buttons->addWidget(skipButton);
	buttons->addWidget(backButton);
	buttons->addWidget(nextButton);

	auto *layout = new QVBoxLayout(this);
	layout->addWidget(pages, 1);
	layout->addLayout(buttons);

	connect(backButton, &QPushButton::clicked, this, [this]() { go(-1); });
	connect(nextButton, &QPushButton::clicked, this, [this]() { go(1); });
	connect(skipButton, &QPushButton::clicked, this, &QDialog::reject);
	connect(projectorButton, &QPushButton::clicked, this, [this]() {
		if (liveTools)
			QMetaObject::invokeMethod(liveTools, "openProgramProjector");
	});
	connect(this, &QDialog::accepted, this, [this]() {
		obs_frontend_set_preview_program_mode(studioCheck->isChecked());
	});

	go(0);
}

void WelcomeGuide::addPage(int region, const QString &title, const QString &text, QWidget *extra)
{
	auto *page = new QWidget(pages);
	auto *layout = new QVBoxLayout(page);
	layout->setSpacing(12);
	layout->addWidget(new WindowDiagram(region, page), 1);
	auto *heading = new QLabel(title, page);
	idian::Utils::addClass(heading, "text-large");
	idian::Utils::addClass(heading, "text-bold");
	heading->setWordWrap(true);
	auto *body = new QLabel(text, page);
	body->setWordWrap(true);
	body->setTextFormat(Qt::RichText);
	layout->addWidget(heading);
	layout->addWidget(body);
	if (extra)
		layout->addWidget(extra);
	pages->addWidget(page);
}

void WelcomeGuide::go(int delta)
{
	const int index = pages->currentIndex() + delta;
	if (index >= pages->count()) {
		accept();
		return;
	}
	pages->setCurrentIndex(qMax(0, index));
	const int current = pages->currentIndex();
	stepLabel->setText(tr("Paso %1 de %2").arg(current + 1).arg(pages->count()));
	backButton->setEnabled(current > 0);
	nextButton->setText(current == pages->count() - 1 ? tr("Empezar") : tr("Siguiente"));
}

void WelcomeGuide::showOnFirstRun(LiveToolsDock *liveTools, QWidget *parent)
{
	if (WorshipOutput::ConfigString(ConfigGuide) == QStringLiteral("1"))
		return;
	WorshipOutput::SetConfigString(ConfigGuide, QStringLiteral("1"));
	auto *guide = new WelcomeGuide(liveTools, parent);
	guide->setAttribute(Qt::WA_DeleteOnClose);
	guide->open();
}
