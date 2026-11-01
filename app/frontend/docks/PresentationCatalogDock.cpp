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

#include "PresentationCatalogDock.hpp"
#include "WorshipOutput.hpp"
#include "WorshipRender.hpp"

#include <qt-wrappers.hpp>

#include <QAbstractItemView>
#include <QComboBox>
#include <QDir>
#include <QFileDialog>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QIcon>
#include <QImageReader>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QListWidgetItem>
#include <QPixmap>
#include <QPushButton>
#include <QSplitter>
#include <QTimer>
#include <QVBoxLayout>

#include "moc_PresentationCatalogDock.cpp"

namespace {
constexpr const char *SceneName = "WS Himnario";
constexpr const char *SlideSourceName = "WS Himno - Letra";
constexpr const char *AudioSourceName = "WS Himno - Audio";
constexpr const char *ConfigEdition = "HymnalEdition";

/* Each edition remembers its own folder. */
const char *rootConfigKey(HymnalEdition edition)
{
	return edition == HymnalEdition::New2009 ? "HymnalRoot" : "HymnalOldRoot";
}
constexpr const char *ConfigAudioMode = "HymnalAudioMode";
constexpr const char *ConfigFit = "HymnalFit";
constexpr int SeekLeadMs = 60;

QString formatTime(int64_t ms)
{
	if (ms < 0)
		ms = 0;
	const int64_t seconds = ms / 1000;
	return QStringLiteral("%1:%2").arg(seconds / 60).arg(seconds % 60, 2, 10, QLatin1Char('0'));
}
} // namespace

PresentationCatalogDock::PresentationCatalogDock(const QString &objectName, const QString &title, QWidget *parent)
	: OBSDock(title, parent)
{
	setObjectName(objectName);

	auto *root = new QWidget(this);
	auto *layout = new QVBoxLayout(root);
	layout->setContentsMargins(8, 8, 8, 8);
	layout->setSpacing(6);

	searchEdit = new QLineEdit(root);
	searchEdit->setPlaceholderText(tr("Buscar himno por numero o titulo"));
	searchEdit->setClearButtonEnabled(true);

	resultsList = new QListWidget(root);
	resultsList->setSelectionMode(QAbstractItemView::SingleSelection);

	audioModeCombo = new QComboBox(root);
	audioModeCombo->addItem(tr("Cantado"));
	audioModeCombo->addItem(tr("Instrumental"));
	audioModeCombo->addItem(tr("Solo letra"));
	audioModeCombo->setCurrentIndex(qBound(0, WorshipOutput::ConfigString(ConfigAudioMode).toInt(), 2));

	projectButton = new QPushButton(tr("Proyectar"), root);
	projectButton->setDefault(true);

	auto *folderButton = new QPushButton(tr("Carpeta..."), root);
	folderButton->setToolTip(tr("Elegir la carpeta del himnario seleccionado"));

	editionCombo = new QComboBox(root);
	editionCombo->addItem(PresentationCatalogData::editionName(HymnalEdition::New2009));
	editionCombo->addItem(PresentationCatalogData::editionName(HymnalEdition::Old1962));
	editionCombo->setCurrentIndex(qBound(0, WorshipOutput::ConfigString(ConfigEdition).toInt(), 1));

	fitCombo = new QComboBox(root);
	fitCombo->addItem(tr("Fondo difuminado"));
	fitCombo->addItem(tr("Estirar"));
	fitCombo->addItem(tr("Barras negras"));
	fitCombo->setToolTip(tr("Como llenar la pantalla 16:9 con las diapositivas 4:3"));
	fitCombo->setCurrentIndex(qBound(0, WorshipOutput::ConfigString(ConfigFit).toInt(), 2));

	auto *projectRow = new QHBoxLayout();
	projectRow->addWidget(audioModeCombo, 1);
	projectRow->addWidget(fitCombo, 1);
	projectRow->addWidget(projectButton);

	slidesList = new QListWidget(root);
	slidesList->setViewMode(QListView::IconMode);
	slidesList->setIconSize(QSize(160, 90));
	slidesList->setResizeMode(QListView::Adjust);
	slidesList->setMovement(QListView::Static);
	slidesList->setSpacing(4);
	slidesList->setUniformItemSizes(true);
	slidesList->setSelectionMode(QAbstractItemView::SingleSelection);

	auto *splitter = new QSplitter(Qt::Vertical, root);
	splitter->addWidget(resultsList);
	splitter->addWidget(slidesList);
	splitter->setStretchFactor(0, 2);
	splitter->setStretchFactor(1, 3);

	previousButton = new QPushButton(tr("< Anterior"), root);
	nextButton = new QPushButton(tr("Siguiente >"), root);
	stopButton = new QPushButton(tr("Detener"), root);

	auto *liveRow = new QHBoxLayout();
	liveRow->addWidget(previousButton);
	liveRow->addWidget(nextButton);
	liveRow->addWidget(stopButton);

	liveLabel = new QLabel(root);
	liveLabel->setWordWrap(true);

	statusLabel = new QLabel(root);
	statusLabel->setWordWrap(true);

	auto *editionRow = new QHBoxLayout();
	editionRow->addWidget(editionCombo, 1);
	editionRow->addWidget(folderButton);
	layout->addLayout(editionRow);
	layout->addWidget(searchEdit);
	layout->addWidget(splitter, 1);
	layout->addLayout(projectRow);
	layout->addLayout(liveRow);
	layout->addWidget(liveLabel);
	layout->addWidget(statusLabel);

	setWidget(root);

	syncTimer = new QTimer(this);
	syncTimer->setInterval(100);

	connect(searchEdit, &QLineEdit::textChanged, this, &PresentationCatalogDock::onSearchChanged);
	connect(searchEdit, &QLineEdit::returnPressed, this, &PresentationCatalogDock::onProjectClicked);
	connect(resultsList, &QListWidget::currentItemChanged, this, &PresentationCatalogDock::onCurrentHymnChanged);
	connect(resultsList, &QListWidget::itemActivated, this, &PresentationCatalogDock::onProjectClicked);
	connect(slidesList, &QListWidget::itemClicked, this, &PresentationCatalogDock::onSlideClicked);
	connect(slidesList, &QListWidget::currentRowChanged, this, &PresentationCatalogDock::onSlideRowChanged);
	connect(projectButton, &QPushButton::clicked, this, &PresentationCatalogDock::onProjectClicked);
	connect(folderButton, &QPushButton::clicked, this, &PresentationCatalogDock::onChooseFolderClicked);
	connect(previousButton, &QPushButton::clicked, this, &PresentationCatalogDock::onPreviousClicked);
	connect(nextButton, &QPushButton::clicked, this, &PresentationCatalogDock::onNextClicked);
	connect(stopButton, &QPushButton::clicked, this, &PresentationCatalogDock::onStopClicked);
	connect(audioModeCombo, &QComboBox::currentIndexChanged, this,
		[](int index) { WorshipOutput::SetConfigString(ConfigAudioMode, QString::number(index)); });
	connect(fitCombo, &QComboBox::currentIndexChanged, this, [this](int index) {
		WorshipOutput::SetConfigString(ConfigFit, QString::number(index));
		if (liveEntry >= 0) {
			const int slide = liveSlide;
			liveSlide = -1;
			showSlide(slide, false);
		}
	});
	connect(syncTimer, &QTimer::timeout, this, &PresentationCatalogDock::onSyncTick);

	connect(editionCombo, &QComboBox::currentIndexChanged, this, [this](int index) {
		WorshipOutput::SetConfigString(ConfigEdition, QString::number(index));
		loadCatalog();
	});

	loadCatalog();
}

PresentationCatalogDock::~PresentationCatalogDock()
{
	syncTimer->stop();
}

HymnalEdition PresentationCatalogDock::edition() const
{
	return static_cast<HymnalEdition>(editionCombo->currentIndex());
}

QString PresentationCatalogDock::useEdition(int editionIndex, const QString &root)
{
	const HymnalEdition target = static_cast<HymnalEdition>(editionIndex);
	const QString previous = WorshipOutput::ConfigString(rootConfigKey(target));
	if (!root.isNull())
		WorshipOutput::SetConfigString(rootConfigKey(target), root);
	if (editionCombo->currentIndex() != editionIndex)
		editionCombo->setCurrentIndex(editionIndex);
	else
		loadCatalog();
	return previous;
}

void PresentationCatalogDock::loadCatalog()
{
	/* Changing hymnal while a hymn is live would mix both catalogs. */
	if (liveEntry >= 0)
		onStopClicked();

	QString error;
	allEntries = PresentationCatalogData::loadHymnCatalog(
		edition(), WorshipOutput::ConfigString(rootConfigKey(edition())), &error, &hymnRoot);
	if (allEntries.isEmpty()) {
		blog(LOG_WARNING, "[WorshipStudio] Himnario not loaded: %s", QT_TO_UTF8(error));
		statusLabel->setText(error + QStringLiteral(" ") + tr("Usa \"Carpeta...\" para elegirla."));
	} else {
		blog(LOG_INFO, "[WorshipStudio] Himnario loaded: %d hymns from %s", (int)allEntries.size(),
		     QT_TO_UTF8(hymnRoot));
	}

	rebuildResults(searchEdit->text());
}

void PresentationCatalogDock::rebuildResults(const QString &filter)
{
	resultsList->clear();

	const QString normalizedFilter = PresentationCatalogData::normalizeName(filter);
	bool numeric = false;
	const int number = normalizedFilter.toInt(&numeric);

	for (int i = 0; i < allEntries.size(); ++i) {
		const PresentationEntry &entry = allEntries.at(i);
		if (numeric) {
			if (!QString::number(entry.number).startsWith(QString::number(number)))
				continue;
		} else if (!normalizedFilter.isEmpty() && !entry.searchBlob.contains(normalizedFilter)) {
			continue;
		}

		auto *item = new QListWidgetItem(entry.displayLabel);
		item->setData(Qt::UserRole, i);
		if (entry.slidePaths.isEmpty())
			item->setForeground(palette().brush(QPalette::Disabled, QPalette::Text));
		resultsList->addItem(item);
	}

	if (!allEntries.isEmpty())
		statusLabel->setText(tr("%1 himnos").arg(resultsList->count()));

	if (resultsList->count() > 0)
		resultsList->setCurrentRow(0);
	else
		onCurrentHymnChanged(nullptr, nullptr);
}

void PresentationCatalogDock::loadSlideThumbnails(int entryIndex)
{
	QSignalBlocker blocker(slidesList);
	slidesList->clear();

	if (entryIndex < 0 || entryIndex >= allEntries.size())
		return;

	const PresentationEntry &entry = allEntries.at(entryIndex);
	for (int i = 0; i < entry.slidePaths.size(); ++i) {
		QImageReader reader(entry.slidePaths.at(i));
		const QSize size = reader.size();
		if (size.isValid())
			reader.setScaledSize(size.scaled(slidesList->iconSize(), Qt::KeepAspectRatio));

		auto *item = new QListWidgetItem(QIcon(QPixmap::fromImage(reader.read())), QString::number(i + 1));
		item->setData(Qt::UserRole, i);
		slidesList->addItem(item);
	}

	if (entryIndex == liveEntry && liveSlide >= 0)
		slidesList->setCurrentRow(liveSlide);
}

void PresentationCatalogDock::onSearchChanged(const QString &text)
{
	rebuildResults(text);
}

void PresentationCatalogDock::onCurrentHymnChanged(QListWidgetItem *current, QListWidgetItem *)
{
	selectedEntry = current ? current->data(Qt::UserRole).toInt() : -1;
	loadSlideThumbnails(selectedEntry);
	updateControls();
}

PresentationCatalogDock::AudioMode PresentationCatalogDock::audioMode() const
{
	return static_cast<AudioMode>(audioModeCombo->currentIndex());
}

QString PresentationCatalogDock::audioPathFor(const PresentationEntry &entry) const
{
	switch (audioMode()) {
	case AudioMode::Sung:
		return entry.cantadoPath.isEmpty() ? entry.instrumentalPath : entry.cantadoPath;
	case AudioMode::Instrumental:
		return entry.instrumentalPath.isEmpty() ? entry.cantadoPath : entry.instrumentalPath;
	case AudioMode::None:
		break;
	}
	return {};
}

bool PresentationCatalogDock::prepareOutput()
{
	scene = WorshipOutput::EnsureScene(SceneName);
	if (!scene)
		return false;

	bool created = false;
	OBSDataAutoRelease slideSettings = obs_data_create();
	slideSource = WorshipOutput::EnsureSource(scene, SlideSourceName, "image_source", slideSettings, &created);
	if (created && slideSource)
		WorshipOutput::FitItem(scene, slideSource, 0.0f, 0.0f, 1.0f, 1.0f);

	OBSDataAutoRelease audioSettings = obs_data_create();
	obs_data_set_bool(audioSettings, "is_local_file", true);
	obs_data_set_bool(audioSettings, "looping", false);
	obs_data_set_bool(audioSettings, "restart_on_activate", false);
	obs_data_set_bool(audioSettings, "close_when_inactive", false);
	audioSource = WorshipOutput::EnsureSource(scene, AudioSourceName, "ffmpeg_source", audioSettings, &created);
	if (created && audioSource) {
		/* Hymns are played to the room, not only to the stream. */
		obs_source_set_monitoring_type(audioSource, OBS_MONITORING_TYPE_MONITOR_AND_OUTPUT);
	}

	return slideSource && audioSource;
}

void PresentationCatalogDock::onProjectClicked()
{
	if (selectedEntry < 0 || selectedEntry >= allEntries.size())
		return;

	const PresentationEntry &entry = allEntries.at(selectedEntry);
	if (entry.slidePaths.isEmpty()) {
		statusLabel->setText(tr("El himno %1 no tiene diapositivas.").arg(entry.number));
		return;
	}

	if (!prepareOutput()) {
		statusLabel->setText(tr("No se pudieron crear las fuentes de proyeccion."));
		return;
	}

	liveEntry = selectedEntry;
	liveSlide = -1;

	const QString audioPath = audioPathFor(entry);
	liveWithAudio = !audioPath.isEmpty();

	OBSDataAutoRelease settings = obs_data_create();
	obs_data_set_string(settings, "local_file", liveWithAudio ? QT_TO_UTF8(audioPath) : "");
	obs_source_update(audioSource, settings);

	showSlide(0, false);
	WorshipOutput::ShowScene(scene);

	if (liveWithAudio) {
		obs_source_media_restart(audioSource);
		syncTimer->start();
	} else {
		obs_source_media_stop(audioSource);
		syncTimer->stop();
	}

	loadSlideThumbnails(selectedEntry);
	updateControls();
	updateLiveLabel();
}

void PresentationCatalogDock::showSlide(int slide, bool seekAudio)
{
	if (liveEntry < 0 || !slideSource)
		return;

	const PresentationEntry &entry = allEntries.at(liveEntry);
	slide = qBound(0, slide, (int)entry.slidePaths.size() - 1);

	if (slide != liveSlide) {
		OBSDataAutoRelease settings = obs_data_create();
		obs_data_set_string(settings, "file", QT_TO_UTF8(renderedSlide(liveEntry, slide)));
		obs_source_update(slideSource, settings);
		liveSlide = slide;

		/* Prepare the next slide ahead of time so auto-advance never waits. */
		if (slide + 1 < entry.slidePaths.size()) {
			const int entryIndex = liveEntry;
			QTimer::singleShot(0, this, [this, entryIndex, slide]() {
				if (entryIndex == liveEntry)
					renderedSlide(entryIndex, slide + 1);
			});
		}
	}

	if (seekAudio && liveWithAudio && audioSource) {
		const int64_t target = slide == 0 ? 0 : entry.slideTimes.value(slide - 1, -1);
		if (target >= 0) {
			obs_media_state state = obs_source_media_get_state(audioSource);
			if (state == OBS_MEDIA_STATE_ENDED || state == OBS_MEDIA_STATE_STOPPED) {
				obs_source_media_restart(audioSource);
				syncTimer->start();
			}
			obs_source_media_set_time(audioSource, target + SeekLeadMs);
		}
	}

	if (selectedEntry == liveEntry)
		selectSlideRow(slide);
	updateControls();
	updateLiveLabel();
}

QString PresentationCatalogDock::renderedSlide(int entryIndex, int slide) const
{
	const QString &path = allEntries.at(entryIndex).slidePaths.at(slide);
	const QSize canvas(WorshipOutput::CanvasWidth(), WorshipOutput::CanvasHeight());
	return WorshipRender::HymnSlide(path, static_cast<WorshipRender::SlideFit>(fitCombo->currentIndex()), canvas);
}

bool PresentationCatalogDock::projectHymn(int number, int mode)
{
	searchEdit->clear();
	for (int row = 0; row < resultsList->count(); ++row) {
		if (allEntries.at(resultsList->item(row)->data(Qt::UserRole).toInt()).number == number) {
			resultsList->setCurrentRow(row);
			audioModeCombo->setCurrentIndex(mode);
			onProjectClicked();
			return liveEntry >= 0;
		}
	}
	return false;
}

void PresentationCatalogDock::selectSlideRow(int slide)
{
	QSignalBlocker blocker(slidesList);
	slidesList->setCurrentRow(slide);
}

void PresentationCatalogDock::onSlideClicked(QListWidgetItem *item)
{
	if (!item)
		return;

	if (selectedEntry != liveEntry) {
		onProjectClicked();
	}
	showSlide(item->data(Qt::UserRole).toInt(), true);
}

void PresentationCatalogDock::onSlideRowChanged(int row)
{
	/* Keyboard navigation inside the thumbnails of the live hymn. */
	if (row >= 0 && selectedEntry == liveEntry && row != liveSlide)
		showSlide(row, true);
}

void PresentationCatalogDock::onPreviousClicked()
{
	if (liveEntry >= 0)
		showSlide(liveSlide - 1, true);
}

void PresentationCatalogDock::onNextClicked()
{
	if (liveEntry >= 0)
		showSlide(liveSlide + 1, true);
}

void PresentationCatalogDock::onStopClicked()
{
	syncTimer->stop();
	if (audioSource)
		obs_source_media_stop(audioSource);

	liveEntry = -1;
	liveSlide = -1;
	liveWithAudio = false;
	slidesList->clearSelection();
	updateControls();
	updateLiveLabel();
}

void PresentationCatalogDock::onSyncTick()
{
	if (liveEntry < 0 || !audioSource) {
		syncTimer->stop();
		return;
	}

	const obs_media_state state = obs_source_media_get_state(audioSource);
	if (state == OBS_MEDIA_STATE_ENDED) {
		syncTimer->stop();
		updateLiveLabel();
		return;
	}
	if (state != OBS_MEDIA_STATE_PLAYING)
		return;

	/* Same rule as the original hymnal: slide N+1 starts at times[N].
	 * Hymns without timings are advanced by hand while the audio plays. */
	const PresentationEntry &entry = allEntries.at(liveEntry);
	if (entry.slideTimes.isEmpty()) {
		updateLiveLabel();
		return;
	}
	const int64_t now = obs_source_media_get_time(audioSource);
	int slide = 0;
	while (slide < entry.slideTimes.size() && entry.slideTimes.at(slide) <= now)
		++slide;

	if (slide != liveSlide)
		showSlide(slide, false);
	else
		updateLiveLabel();
}

void PresentationCatalogDock::onChooseFolderClicked()
{
	const QString folder = QFileDialog::getExistingDirectory(
		this, tr("Carpeta del %1").arg(PresentationCatalogData::editionName(edition()).toLower()),
		hymnRoot.isEmpty() ? QDir::homePath() : hymnRoot);
	if (folder.isEmpty())
		return;

	if (!PresentationCatalogData::isHymnRoot(folder)) {
		statusLabel->setText(tr("Esa carpeta no tiene himnos: se esperan carpetas \"NNN - Titulo\" con "
					"imagenes (o la carpeta del programa Himnario Adventista)."));
		return;
	}

	WorshipOutput::SetConfigString(rootConfigKey(edition()), folder);
	loadCatalog();
}

void PresentationCatalogDock::updateControls()
{
	const bool live = liveEntry >= 0;
	projectButton->setEnabled(selectedEntry >= 0);
	previousButton->setEnabled(live && liveSlide > 0);
	nextButton->setEnabled(live && liveSlide < allEntries.at(liveEntry).slidePaths.size() - 1);
	stopButton->setEnabled(live);
}

void PresentationCatalogDock::updateLiveLabel()
{
	if (liveEntry < 0) {
		liveLabel->setText(tr("Sin proyeccion activa"));
		return;
	}

	const PresentationEntry &entry = allEntries.at(liveEntry);
	QString text = tr("En vivo: %1  |  Diapositiva %2/%3")
			       .arg(entry.displayLabel)
			       .arg(liveSlide + 1)
			       .arg(entry.slidePaths.size());

	if (liveWithAudio && audioSource) {
		const obs_media_state state = obs_source_media_get_state(audioSource);
		if (state == OBS_MEDIA_STATE_ENDED)
			text += tr("  |  Audio finalizado");
		else
			text += QStringLiteral("  |  %1 / %2")
					.arg(formatTime(obs_source_media_get_time(audioSource)))
					.arg(formatTime(obs_source_media_get_duration(audioSource)));
	}

	liveLabel->setText(text);
}
