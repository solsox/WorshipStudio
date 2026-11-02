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
constexpr const char *ConfigBase = "AppearanceBase";

/* Background bases: grey1 (lightest detail) to grey8 (deepest). */
struct Base {
	const char *id;
	const char *name;
	bool light;
	const char *greys[8];
	const char *muted;
};

const Base Bases[] = {
	{"ciruela", "Ciruela", false,
	 {"#6E5A78", "#58465F", "#46374E", "#382B3F", "#2C2132", "#221A27", "#1A131E", "#110C14"}, "#B4A5BC"},
	{"mate", "Negro mate", false,
	 {"#6B6B6B", "#555555", "#444444", "#363636", "#2B2B2B", "#222222", "#1A1A1A", "#121212"}, "#A8A8A8"},
	{"profundo", "Negro profundo", false,
	 {"#5E5E63", "#47474C", "#333337", "#26262A", "#1B1B1E", "#121214", "#0A0A0B", "#000000"}, "#9C9CA3"},
	{"grafito", "Negro brillante", false,
	 {"#7A7A82", "#64646C", "#515158", "#424248", "#36363B", "#2C2C30", "#232326", "#18181A"}, "#B0B0B8"},
	{"blanco", "Blanco", true,
	 {"#8C8C92", "#FFFFFF", "#FFFFFF", "#F7F7F8", "#EFEFF1", "#E8E8EB", "#DCDCE0", "#C8C8CD"}, nullptr},
	{"vainilla", "Vainilla", true,
	 {"#968C78", "#FFFDF7", "#FFFCF3", "#FBF5E6", "#F4ECD8", "#EEE4CC", "#E3D6B8", "#D3C49F"}, nullptr},
	{"hueso", "Hueso", true,
	 {"#908A80", "#FDFCFA", "#FBFAF6", "#F3F0EA", "#EAE6DD", "#E2DDD2", "#D2CCBE", "#BDB6A6"}, nullptr},
};

const Base *findBase(const QString &id, bool light)
{
	for (const Base &base : Bases) {
		if (base.light == light && id == QLatin1String(base.id))
			return &base;
	}
	/* First of each mode is the default: ciruela or blanco. */
	for (const Base &base : Bases) {
		if (base.light == light)
			return &base;
	}
	return nullptr;
}

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

QString AppearanceDialog::savedBase()
{
	return WorshipOutput::ConfigString(ConfigBase);
}

QMap<QString, QString> AppearanceDialog::baseVariables(const QString &id)
{
	QMap<QString, QString> variables;
	for (bool light : {false, true}) {
		const Base *base = findBase(id, light);
		if (!base || id != QLatin1String(base->id))
			continue;
		for (int i = 0; i < 8; ++i)
			variables.insert(QStringLiteral("grey%1").arg(i + 1), QString::fromLatin1(base->greys[i]));
		if (base->muted)
			variables.insert(QStringLiteral("text_muted"), QString::fromLatin1(base->muted));
	}
	return variables;
}

AppearanceDialog::AppearanceDialog(QWidget *parent)
	: QDialog(parent),
	  accent(savedAccent()),
	  light(savedLight()),
	  base(QString::fromLatin1(findBase(savedBase(), savedLight())->id))
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

	auto *baseTitle = new QLabel(tr("Fondo"), this);
	idian::Utils::addClass(baseTitle, "text-bold");
	baseRow = new QHBoxLayout();
	baseRow->setSpacing(8);

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
	layout->addWidget(baseTitle);
	layout->addLayout(baseRow);
	layout->addSpacing(6);
	layout->addWidget(colorTitle);
	layout->addLayout(grid);
	layout->addWidget(otherButton);
	layout->addSpacing(6);
	layout->addWidget(status);
	layout->addWidget(closeButton);

	/* Each mode remembers nothing of the other: switching starts from its default base. */
	connect(darkButton, &QPushButton::clicked, this, [this]() {
		if (light)
			base = QString::fromLatin1(findBase(QString(), false)->id);
		light = false;
		apply();
	});
	connect(lightButton, &QPushButton::clicked, this, [this]() {
		if (!light)
			base = QString::fromLatin1(findBase(QString(), true)->id);
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

	rebuildBases();
	refreshSelection();
}

/* Tiles with the real background color of each base for the current mode. */
void AppearanceDialog::rebuildBases()
{
	for (QPushButton *button : baseButtons)
		button->deleteLater();
	baseButtons.clear();
	for (const Base &option : Bases) {
		if (option.light != light)
			continue;
		const QString id = QString::fromLatin1(option.id);
		auto *button = new QPushButton(tr(option.name), this);
		button->setMinimumHeight(52);
		button->setProperty("base", id);
		button->setStyleSheet(QStringLiteral("QPushButton{background:%1;color:%2;border-radius:8px;"
						     "border:2px solid %3;font-weight:600;}"
						     "QPushButton[selected=\"true\"]{border:2px solid %4;}")
					      .arg(QLatin1String(option.greys[6]),
						   option.light ? QStringLiteral("#1A1A1A") : QStringLiteral("#F2F2F2"),
						   QLatin1String(option.greys[4]), accent.name()));
		connect(button, &QPushButton::clicked, this, [this, id]() {
			base = id;
			apply();
		});
		baseRow->addWidget(button);
		baseButtons << button;
	}
}

void AppearanceDialog::apply()
{
	WorshipOutput::SetConfigString(ConfigAccent, accent.name());
	WorshipOutput::SetConfigString(ConfigLight, light ? QStringLiteral("1") : QStringLiteral("0"));
	WorshipOutput::SetConfigString(ConfigBase, base);
	const bool ok = App()->SetWorshipAppearance(accent, light, baseVariables(base));
	rebuildBases();
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
	for (QPushButton *button : baseButtons) {
		button->setProperty("selected", button->property("base").toString() == base);
		button->style()->unpolish(button);
		button->style()->polish(button);
	}
	for (QPushButton *button : swatches) {
		const bool selected = button->property("swatch").value<QColor>() == accent;
		button->setProperty("selected", selected);
		button->style()->unpolish(button);
		button->style()->polish(button);
	}
}
