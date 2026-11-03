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

#include "SongEditor.hpp"
#include "WorshipOutput.hpp"

#include <QCheckBox>
#include <QColorDialog>
#include <QDialogButtonBox>
#include <QFileDialog>
#include <QFileInfo>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QPixmap>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QSpinBox>
#include <QSplitter>
#include <QTimer>
#include <QVBoxLayout>

#include "BackgroundLibrary.hpp"
#include "BackgroundSearch.hpp"

#include "moc_SongEditor.cpp"

namespace {
const QSize EditorThumb(192, 108);

enum ItemRole { KindRole = Qt::UserRole, PresetRole, PathRole };
enum ItemKind { InheritItem, PresetItem, ColorItem, AddItem, CustomItem, LibraryItem, SearchItem };
} // namespace

/* ------------------------------------------------------------------------ */

BackgroundPicker::BackgroundPicker(bool allowVideo_, bool allowInherit_, QWidget *parent)
	: QComboBox(parent),
	  allowVideo(allowVideo_),
	  allowInherit(allowInherit_)
{
	setToolTip(tr("Fondo de la pantalla"));
	setIconSize(QSize(48, 27));
	rebuild();
	connect(this, &QComboBox::activated, this, &BackgroundPicker::onActivated);
}

void BackgroundPicker::rebuild()
{
	QSignalBlocker blocker(this);
	clear();

	if (allowInherit)
		addItem(tr("(igual que el canto)"), InheritItem);

	/* Small swatches so the backgrounds can be recognised at a glance. */
	const QStringList presets = WorshipRender::BackgroundPresetNames();
	for (int i = 0; i < presets.size(); ++i) {
		WorshipRender::LyricsSlide swatch;
		swatch.background.preset = i;
		const QImage image = WorshipRender::Lyrics(swatch, QSize(48, 27));
		addItem(QIcon(QPixmap::fromImage(image)), presets.at(i), PresetItem);
		setItemData(count() - 1, i, PresetRole);
	}

	/* Mis fondos: bundled photos and everything the church added. */
	const QVector<BackgroundLibrary::Item> library = BackgroundLibrary::Items(allowVideo);
	if (!library.isEmpty())
		insertSeparator(count());
	bool customInLibrary = false;
	for (const BackgroundLibrary::Item &item : library) {
		addItem(BackgroundLibrary::Thumbnail(item.path), item.video ? tr("%1 (video)").arg(item.name) : item.name,
			LibraryItem);
		setItemData(count() - 1, item.path, PathRole);
		if (hasCustom && custom.path == item.path)
			customInLibrary = true;
	}
	insertSeparator(count());

	if (hasCustom && !customInLibrary) {
		QString label;
		switch (custom.kind) {
		case WorshipRender::BackgroundKind::Color:
			label = tr("Color %1").arg(custom.color);
			break;
		case WorshipRender::BackgroundKind::Image:
			label = tr("Imagen: %1").arg(QFileInfo(custom.path).fileName());
			break;
		case WorshipRender::BackgroundKind::Video:
			label = tr("Video: %1").arg(QFileInfo(custom.path).fileName());
			break;
		default:
			break;
		}
		if (!label.isEmpty())
			addItem(label, CustomItem);
	}

	addItem(tr("Color..."), ColorItem);
	addItem(allowVideo ? tr("Agregar imágenes o videos...") : tr("Agregar imágenes..."), AddItem);
	setItemData(count() - 1, AddItem, KindRole);
	addItem(tr("Buscar fondos en internet..."), SearchItem);
	setItemData(count() - 1, SearchItem, KindRole);
}

WorshipRender::Background BackgroundPicker::background(bool *inherit) const
{
	const int kind = currentData(KindRole).toInt();
	if (inherit)
		*inherit = kind == InheritItem;

	WorshipRender::Background result;
	if (kind == PresetItem) {
		result.kind = WorshipRender::BackgroundKind::Preset;
		result.preset = currentData(PresetRole).toInt();
	} else if (kind == CustomItem) {
		result = custom;
	} else if (kind == LibraryItem) {
		result.path = currentData(PathRole).toString();
		result.kind = BackgroundLibrary::IsVideo(result.path) ? WorshipRender::BackgroundKind::Video
								      : WorshipRender::BackgroundKind::Image;
	}
	return result;
}

void BackgroundPicker::setBackground(const WorshipRender::Background &background, bool inherit)
{
	if (inherit && allowInherit) {
		hasCustom = false;
		rebuild();
		setCurrentIndex(0);
		lastIndex = 0;
		return;
	}

	hasCustom = background.kind != WorshipRender::BackgroundKind::Preset;
	if (hasCustom)
		custom = background;
	rebuild();

	for (int i = 0; i < count(); ++i) {
		const int kind = itemData(i, KindRole).toInt();
		if ((hasCustom && (kind == CustomItem || (kind == LibraryItem && itemData(i, PathRole).toString() ==
										      custom.path))) ||
		    (!hasCustom && kind == PresetItem && itemData(i, PresetRole).toInt() == background.preset)) {
			setCurrentIndex(i);
			lastIndex = i;
			return;
		}
	}
}

void BackgroundPicker::onActivated(int index)
{
	const int kind = itemData(index, KindRole).toInt();
	WorshipRender::Background chosen;

	if (kind == ColorItem) {
		const QColor color = QColorDialog::getColor(Qt::black, this, tr("Color de fondo"));
		if (!color.isValid()) {
			setCurrentIndex(lastIndex);
			return;
		}
		chosen.kind = WorshipRender::BackgroundKind::Color;
		chosen.color = color.name();
	} else if (kind == AddItem || kind == SearchItem) {
		/* Added files stay in Mis fondos for every song and the Bible. */
		const QStringList added = kind == SearchItem ? BackgroundSearch::Run(this, allowVideo)
							     : BackgroundLibrary::AddWithDialog(this, allowVideo);
		if (added.isEmpty()) {
			setCurrentIndex(lastIndex);
			return;
		}
		chosen.path = added.first();
		chosen.kind = BackgroundLibrary::IsVideo(chosen.path) ? WorshipRender::BackgroundKind::Video
								      : WorshipRender::BackgroundKind::Image;
	} else {
		lastIndex = index;
		emit backgroundChanged();
		return;
	}

	setBackground(chosen);
	emit backgroundChanged();
}

/* ------------------------------------------------------------------------ */

SongEditor::SongEditor(const Song &song, QWidget *parent) : QDialog(parent), edited(song)
{
	setWindowTitle(song.id.isEmpty() ? tr("Nuevo canto") : tr("Editar canto"));
	resize(1100, 720);

	titleEdit = new QLineEdit(song.title, this);
	titleEdit->setPlaceholderText(tr("Titulo del canto"));
	authorEdit = new QLineEdit(song.author, this);
	authorEdit->setPlaceholderText(tr("Autor (opcional)"));
	copyrightEdit = new QLineEdit(song.copyright, this);
	copyrightEdit->setPlaceholderText(tr("Derechos / licencia CCLI (opcional)"));

	textEdit = new QPlainTextEdit(this);
	textEdit->setPlaceholderText(tr("[Estrofa 1]\nPrimera linea de la estrofa\nSegunda linea\n\n[Coro]\nLetra del coro\n\n"
					"[Estrofa 2]\nOtra estrofa\n\n[Coro]\n\n"
					"Cada linea en blanco empieza una diapositiva nueva.\n"
					"[Coro] nombra solo el bloque de abajo; escrito solo, repite el coro.\n"
					"Con los nombres ([Coro], [Puente]) saltás con una tecla."));
	textEdit->setPlainText(SongLibrary::toText(song));
	QFont mono = textEdit->font();
	mono.setPointSizeF(mono.pointSizeF() * 1.15);
	textEdit->setFont(mono);

	fontCombo = new QComboBox(this);
	for (const QString &family : WorshipRender::RecommendedFonts()) {
		fontCombo->addItem(family);
		fontCombo->setItemData(fontCombo->count() - 1, QFont(family), Qt::FontRole);
	}
	const int fontIndex = fontCombo->findText(song.fontFamily);
	fontCombo->setCurrentIndex(fontIndex >= 0 ? fontIndex : 0);

	sizeSpin = new QSpinBox(this);
	sizeSpin->setRange(30, 160);
	sizeSpin->setSuffix(tr(" px max"));
	sizeSpin->setValue(song.fontSize);

	uppercaseCheck = new QCheckBox(tr("MAYUSCULAS"), this);
	uppercaseCheck->setChecked(song.uppercase);
	titleCheck = new QCheckBox(tr("Titulo en la primera diapositiva"), this);
	titleCheck->setChecked(song.showTitle);

	layoutCombo = new QComboBox(this);
	layoutCombo->addItem(tr("Pantalla completa"));
	layoutCombo->addItem(tr("Tercio inferior (sobre la camara)"));
	layoutCombo->setCurrentIndex((int)song.layout);

	songBackground = new BackgroundPicker(true, false, this);
	songBackground->setBackground(song.background);

	auto *form = new QFormLayout();
	form->addRow(tr("Titulo"), titleEdit);
	form->addRow(tr("Autor"), authorEdit);
	form->addRow(tr("Derechos"), copyrightEdit);

	auto *styleForm = new QFormLayout();
	auto *fontRow = new QHBoxLayout();
	fontRow->addWidget(fontCombo, 1);
	fontRow->addWidget(sizeSpin);
	styleForm->addRow(tr("Letra"), fontRow);
	styleForm->addRow(tr("Fondo"), songBackground);
	styleForm->addRow(tr("Ubicacion"), layoutCombo);
	auto *checks = new QHBoxLayout();
	checks->addWidget(uppercaseCheck);
	checks->addWidget(titleCheck);
	checks->addStretch(1);
	styleForm->addRow(QString(), checks);

	auto *left = new QWidget(this);
	auto *leftLayout = new QVBoxLayout(left);
	leftLayout->setContentsMargins(0, 0, 0, 0);
	leftLayout->addLayout(form);
	leftLayout->addWidget(textEdit, 1);
	leftLayout->addLayout(styleForm);

	slidesList = new QListWidget(this);
	slidesList->setViewMode(QListView::IconMode);
	slidesList->setIconSize(EditorThumb);
	slidesList->setResizeMode(QListView::Adjust);
	slidesList->setMovement(QListView::Static);
	slidesList->setSpacing(6);
	slidesList->setUniformItemSizes(true);

	slideBackground = new BackgroundPicker(false, true, this);
	slideBackground->setEnabled(false);
	slideBackground->setToolTip(tr("Fondo solo para la diapositiva seleccionada"));

	previewLabel = new QLabel(this);
	previewLabel->setMinimumHeight(220);
	previewLabel->setAlignment(Qt::AlignCenter);
	previewLabel->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Ignored);

	auto *right = new QWidget(this);
	auto *rightLayout = new QVBoxLayout(right);
	rightLayout->setContentsMargins(0, 0, 0, 0);
	rightLayout->addWidget(new QLabel(tr("Diapositivas"), right));
	rightLayout->addWidget(slidesList, 3);
	auto *slideBgRow = new QHBoxLayout();
	slideBgRow->addWidget(new QLabel(tr("Fondo de esta diapositiva"), right));
	slideBgRow->addWidget(slideBackground, 1);
	rightLayout->addLayout(slideBgRow);
	rightLayout->addWidget(previewLabel, 2);

	auto *splitter = new QSplitter(Qt::Horizontal, this);
	splitter->addWidget(left);
	splitter->addWidget(right);
	splitter->setStretchFactor(0, 1);
	splitter->setStretchFactor(1, 1);

	auto *buttons = new QDialogButtonBox(QDialogButtonBox::Save | QDialogButtonBox::Cancel, this);
	buttons->button(QDialogButtonBox::Save)->setText(tr("Guardar"));
	buttons->button(QDialogButtonBox::Cancel)->setText(tr("Cancelar"));

	auto *layout = new QVBoxLayout(this);
	layout->addWidget(splitter, 1);
	layout->addWidget(buttons);

	refreshTimer = new QTimer(this);
	refreshTimer->setSingleShot(true);
	refreshTimer->setInterval(250);

	connect(buttons, &QDialogButtonBox::accepted, this, [this]() {
		readForm();
		if (edited.title.trimmed().isEmpty())
			edited.title = edited.slides.isEmpty() ? tr("Canto sin titulo")
							       : edited.slides.first().text.section(QLatin1Char('\n'), 0, 0);
		accept();
	});
	connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
	connect(textEdit, &QPlainTextEdit::textChanged, refreshTimer, qOverload<>(&QTimer::start));
	connect(refreshTimer, &QTimer::timeout, this, &SongEditor::refreshSlides);
	for (QComboBox *combo : {fontCombo, layoutCombo})
		connect(combo, &QComboBox::currentIndexChanged, refreshTimer, qOverload<>(&QTimer::start));
	connect(sizeSpin, &QSpinBox::valueChanged, refreshTimer, qOverload<>(&QTimer::start));
	connect(uppercaseCheck, &QCheckBox::toggled, refreshTimer, qOverload<>(&QTimer::start));
	connect(titleCheck, &QCheckBox::toggled, refreshTimer, qOverload<>(&QTimer::start));
	connect(songBackground, &BackgroundPicker::backgroundChanged, this, &SongEditor::refreshSlides);
	connect(slidesList, &QListWidget::currentRowChanged, this, &SongEditor::onSlideSelected);
	connect(slideBackground, &BackgroundPicker::backgroundChanged, this, &SongEditor::onSlideBackgroundChanged);

	refreshSlides();
	if (song.slides.isEmpty())
		textEdit->setFocus();
}

Song SongEditor::song() const
{
	return edited;
}

void SongEditor::readForm()
{
	edited.title = titleEdit->text().trimmed();
	edited.author = authorEdit->text().trimmed();
	edited.copyright = copyrightEdit->text().trimmed();
	edited.fontFamily = fontCombo->currentText();
	edited.fontSize = sizeSpin->value();
	edited.uppercase = uppercaseCheck->isChecked();
	edited.showTitle = titleCheck->isChecked();
	edited.layout = static_cast<WorshipRender::LyricsLayout>(layoutCombo->currentIndex());
	edited.background = songBackground->background();
	edited.slides = SongLibrary::parseText(textEdit->toPlainText(), edited.slides);
}

void SongEditor::refreshSlides()
{
	readForm();
	const int row = slidesList->currentRow();

	QSignalBlocker blocker(slidesList);
	slidesList->clear();
	for (int i = 0; i < edited.slides.size(); ++i) {
		const QImage image = WorshipRender::LyricsPreview(SongLibrary::lyricsSlide(edited, i), EditorThumb * 2);
		const QString label = edited.slides.at(i).label.isEmpty() ? QString::number(i + 1)
									 : QStringLiteral("%1. %2").arg(i + 1).arg(edited.slides.at(i).label);
		auto *item = new QListWidgetItem(
			QIcon(QPixmap::fromImage(image.scaled(EditorThumb, Qt::KeepAspectRatio, Qt::SmoothTransformation))),
			label);
		slidesList->addItem(item);
	}
	slidesList->setCurrentRow(qBound(-1, row < 0 ? 0 : row, (int)edited.slides.size() - 1));
	blocker.unblock();
	onSlideSelected();
}

void SongEditor::refreshPreview()
{
	const int row = slidesList->currentRow();
	if (row < 0) {
		previewLabel->setPixmap(QPixmap());
		previewLabel->setText(tr("Escribi la letra a la izquierda"));
		return;
	}
	const QSize canvas(WorshipOutput::CanvasWidth(), WorshipOutput::CanvasHeight());
	const QSize size = canvas.scaled(qMax(200, previewLabel->width() - 4), qMax(112, previewLabel->height() - 4),
					 Qt::KeepAspectRatio);
	/* Rendered at full size and scaled so it matches the screen exactly. */
	const QImage image = WorshipRender::LyricsPreview(SongLibrary::lyricsSlide(edited, row), canvas);
	previewLabel->setPixmap(QPixmap::fromImage(image.scaled(size, Qt::KeepAspectRatio, Qt::SmoothTransformation)));
}

void SongEditor::onSlideSelected()
{
	const int row = slidesList->currentRow();
	slideBackground->setEnabled(row >= 0);
	if (row >= 0) {
		const SongSlide &slide = edited.slides.at(row);
		slideBackground->setBackground(slide.background, !slide.hasBackground);
	}
	refreshPreview();
}

void SongEditor::onSlideBackgroundChanged()
{
	const int row = slidesList->currentRow();
	if (row < 0 || row >= edited.slides.size())
		return;
	bool inherit = false;
	const WorshipRender::Background background = slideBackground->background(&inherit);
	edited.slides[row].hasBackground = !inherit;
	edited.slides[row].background = background;

	/* parseText keeps the per-slide backgrounds by position. */
	refreshSlides();
}
