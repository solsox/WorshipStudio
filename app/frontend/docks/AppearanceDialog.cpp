#include "AppearanceDialog.hpp"
#include "WorshipOutput.hpp"

#include <OBSApp.hpp>
#include <Idian/Utils.hpp>

#include <QColorDialog>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>

#include "moc_AppearanceDialog.cpp"

namespace {
constexpr const char *ConfigAccent = "AccentColor";
constexpr const char *ConfigLight = "AppearanceLight";

struct Swatch {
	const char *name;
	const char *color;
};

/* Saturated enough for buttons, readable with white text on top. */
const Swatch Swatches[] = {
	{"Brasa", "#E2582B"},  {"Indigo", "#5E66E0"}, {"Esmeralda", "#24966C"}, {"Azul", "#2C78DB"},
	{"Violeta", "#8752DE"}, {"Rosa", "#D63E73"},   {"Dorado", "#C08A1E"},    {"Turquesa", "#17919F"},
};
} // namespace

QColor AppearanceDialog::savedAccent()
{
	const QColor saved(WorshipOutput::ConfigString(ConfigAccent));
	return saved.isValid() ? saved : QColor(0xE2, 0x58, 0x2B);
}

bool AppearanceDialog::savedLight()
{
	return WorshipOutput::ConfigString(ConfigLight) == QStringLiteral("1");
}

AppearanceDialog::AppearanceDialog(QWidget *parent)
	: QDialog(parent),
	  accent(savedAccent()),
	  light(savedLight())
{
	setWindowTitle(tr("Personalizar apariencia"));
	setMinimumWidth(460);

	auto *layout = new QVBoxLayout(this);
	layout->setSpacing(10);

	auto *modeTitle = new QLabel(tr("Modo"), this);
	idian::Utils::addClass(modeTitle, "text-bold");
	darkButton = new QPushButton(tr("Oscuro"), this);
	lightButton = new QPushButton(tr("Claro"), this);
	for (QPushButton *button : {darkButton, lightButton}) {
		button->setCheckable(true);
		button->setMinimumHeight(44);
	}
	darkButton->setToolTip(tr("Recomendado para la cabina durante la reunion: menos brillo"));
	lightButton->setToolTip(tr("Para preparar en salas con mucha luz"));
	auto *modeRow = new QHBoxLayout();
	modeRow->addWidget(darkButton);
	modeRow->addWidget(lightButton);

	auto *colorTitle = new QLabel(tr("Color principal"), this);
	idian::Utils::addClass(colorTitle, "text-bold");
	auto *grid = new QGridLayout();
	grid->setSpacing(8);
	for (int i = 0; i < int(sizeof(Swatches) / sizeof(Swatches[0])); ++i) {
		const QColor color(QString::fromLatin1(Swatches[i].color));
		auto *button = new QPushButton(tr(Swatches[i].name), this);
		button->setMinimumHeight(40);
		button->setProperty("swatch", color);
		/* The swatch shows its own color whatever the current theme is. */
		button->setStyleSheet(QStringLiteral("QPushButton{background:%1;color:white;border-radius:8px;"
						     "border:2px solid transparent;font-weight:600;}"
						     "QPushButton[selected=\"true\"]{border:2px solid white;}")
					      .arg(color.name()));
		connect(button, &QPushButton::clicked, this, [this, color]() {
			accent = color;
			apply();
		});
		swatches << button;
		grid->addWidget(button, i / 4, i % 4);
	}
	auto *otherButton = new QPushButton(tr("Otro color..."), this);

	status = new QLabel(this);
	idian::Utils::addClass(status, "text-muted");
	auto *closeButton = new QPushButton(tr("Listo"), this);
	idian::Utils::addClass(closeButton, "button-primary");

	layout->addWidget(modeTitle);
	layout->addLayout(modeRow);
	layout->addSpacing(6);
	layout->addWidget(colorTitle);
	layout->addLayout(grid);
	layout->addWidget(otherButton);
	layout->addSpacing(6);
	layout->addWidget(status);
	layout->addWidget(closeButton);

	connect(darkButton, &QPushButton::clicked, this, [this]() {
		light = false;
		apply();
	});
	connect(lightButton, &QPushButton::clicked, this, [this]() {
		light = true;
		apply();
	});
	connect(otherButton, &QPushButton::clicked, this, [this]() {
		const QColor chosen = QColorDialog::getColor(accent, this, tr("Color principal"));
		if (chosen.isValid()) {
			accent = chosen;
			apply();
		}
	});
	connect(closeButton, &QPushButton::clicked, this, &QDialog::accept);

	refreshSelection();
}

void AppearanceDialog::apply()
{
	WorshipOutput::SetConfigString(ConfigAccent, accent.name());
	WorshipOutput::SetConfigString(ConfigLight, light ? QStringLiteral("1") : QStringLiteral("0"));
	const bool ok = App()->SetWorshipAppearance(accent, light);
	status->setText(ok ? tr("Aplicado. Tambien se usa en el control desde el celular.")
			   : tr("No se pudo aplicar el tema."));
	refreshSelection();
}

void AppearanceDialog::refreshSelection()
{
	darkButton->setChecked(!light);
	lightButton->setChecked(light);
	idian::Utils::toggleClass(darkButton, "button-primary", !light);
	idian::Utils::toggleClass(lightButton, "button-primary", light);
	for (QPushButton *button : swatches) {
		const bool selected = button->property("swatch").value<QColor>() == accent;
		button->setProperty("selected", selected);
		button->style()->unpolish(button);
		button->style()->polish(button);
	}
}
