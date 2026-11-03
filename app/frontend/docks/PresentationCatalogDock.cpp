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
#include "WorshipLive.hpp"
#include "WorshipOutput.hpp"
#include "WorshipDrag.hpp"
#include "WorshipRender.hpp"

#include <obs-frontend-api.h>
#include <qt-wrappers.hpp>
#include <Idian/Utils.hpp>

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
	WorshipDrag::EnableDrag(resultsList, objectName);
	resultsList->setSelectionMode(QAbstractItemView::SingleSelection);

	audioModeCombo = new QComboBox(root);
	audioModeCombo->addItem(tr("Cantado"));
	audioModeCombo->addItem(tr("Instrumental"));
	audioModeCombo->addItem(tr("Solo letra"));
	audioModeCombo->setCurrentIndex(qBound(0, WorshipOutput::ConfigString(ConfigAudioMode).toInt(), 2));


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

	stopButton = new QPushButton(tr("Detener audio"), root);

	auto *hint = new QLabel(tr("Doble clic en un himno para mostrarlo. Con audio, la letra avanza sola."), root);
	hint->setWordWrap(true);
	idian::Utils::addClass(hint, "text-muted");

	liveLabel = new QLabel(root);
	idian::Utils::addClass(liveLabel, "text-bold");
	liveLabel->setWordWrap(true);

	statusLabel = new QLabel(root);
	idian::Utils::addClass(statusLabel, "text-muted");
	statusLabel->setWordWrap(true);

	auto *editionRow = new QHBoxLayout();
	editionRow->addWidget(editionCombo, 1);
	editionRow->addWidget(folderButton);
	layout->addLayout(editionRow);
	layout->addWidget(searchEdit);
	layout->addWidget(splitter, 1);
	layout->addLayout(projectRow);
	layout->addWidget(hint);
	layout->addWidget(stopButton);
	layout->addWidget(liveLabel);
	layout->addWidget(statusLabel);

	setWidget(root);

	syncTimer = new QTimer(this);
	syncTimer->setInterval(100);

	WorshipLive::instance().registerTarget(QStringLiteral("Himnario"), this);
	obs_frontend_add_event_callback(&PresentationCatalogDock::onFrontendEvent, this);
	connect(searchEdit, &QLineEdit::textChanged, this, &PresentationCatalogDock::onSearchChanged);
	connect(searchEdit, &QLineEdit::returnPressed, this, &PresentationCatalogDock::onProjectClicked);
	connect(resultsList, &QListWidget::currentItemChanged, this, &PresentationCatalogDock::onCurrentHymnChanged);
	connect(resultsList, &QListWidget::itemActivated, this, &PresentationCatalogDock::onProjectClicked);
	connect(slidesList, &QListWidget::itemClicked, this, &PresentationCatalogDock::onSlideClicked);
	connect(slidesList, &QListWidget::currentRowChanged, this, &PresentationCatalogDock::onSlideRowChanged);
	connect(folderButton, &QPushButton::clicked, this, &PresentationCatalogDock::onChooseFolderClicked);
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

void PresentationCatalogDock::onFrontendEvent(enum obs_frontend_event event, void *data)
{
	if (event != OBS_FRONTEND_EVENT_SCENE_CHANGED)
		return;
	auto *dock = static_cast<PresentationCatalogDock *>(data);
	QMetaObject::invokeMethod(dock, [dock]() { dock->stopAudioOffAir(); }, Qt::QueuedConnection);
}

/* A hymn that left the screen must not keep playing in the room. */
void PresentationCatalogDock::stopAudioOffAir()
{
	for (const QString &slot : {QString(), QStringLiteral(" B")}) {
		OBSSourceAutoRelease audio = obs_get_source_by_name(WorshipOutput::Named(AudioSourceName, slot).c_str());
		if (audio && !obs_source_active(audio) && obs_source_media_get_state(audio) == OBS_MEDIA_STATE_PLAYING)
			obs_source_media_stop(audio);
	}
}

PresentationCatalogDock::~PresentationCatalogDock()
{
	obs_frontend_remove_event_callback(&PresentationCatalogDock::onFrontendEvent, this);
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

std::string PresentationCatalogDock::slideItemName() const
{
	return WorshipOutput::Named(SlideSourceName, outputSlot);
}

bool PresentationCatalogDock::prepareOutput()
{
	/* A new hymn goes to the copy of the scene that is not on air. */
	outputSlot = WorshipOutput::OutputSlot(SceneName);
	scene = WorshipOutput::EnsureScene(WorshipOutput::Named(SceneName, outputSlot).c_str());
	if (!scene)
		return false;

	bool created = false;
	OBSDataAutoRelease slideSettings = obs_data_create();
	slideSource = WorshipOutput::EnsureSource(scene, slideItemName().c_str(), "image_source", slideSettings,
						  &created);
	if (created && slideSource)
		WorshipOutput::FitItem(scene, slideSource, 0.0f, 0.0f, 1.0f, 1.0f);

	OBSDataAutoRelease audioSettings = obs_data_create();
	obs_data_set_bool(audioSettings, "is_local_file", true);
	obs_data_set_bool(audioSettings, "looping", false);
	/* A hymn waiting in the preview stays silent: it starts when it goes on
	 * air (restart_on_activate) and stops when it leaves (onSceneChanged). */
	obs_data_set_bool(audioSettings, "restart_on_activate", true);
	obs_data_set_bool(audioSettings, "close_when_inactive", false);
	audioSource = WorshipOutput::EnsureSource(scene, WorshipOutput::Named(AudioSourceName, outputSlot).c_str(),
						  "ffmpeg_source", audioSettings, &created);
	if (!created && audioSource) {
		/* Sources made by older builds still play while waiting in the preview. */
		OBSDataAutoRelease current = obs_source_get_settings(audioSource);
		if (!obs_data_get_bool(current, "restart_on_activate") || obs_data_get_bool(current, "close_when_inactive"))
			obs_source_update(audioSource, audioSettings);
	}
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
		/* In Studio Mode it plays once the transition puts it on air. */
		if (obs_source_active(audioSource))
			obs_source_media_restart(audioSource);
		else
			obs_source_media_stop(audioSource);
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

	LiveState state;
	state.source = QStringLiteral("Himnario");
	state.title = entry.displayLabel;
	state.section = tr("Diapositiva %1 de %2").arg(slide + 1).arg(entry.slidePaths.size());
	state.index = slide;
	state.count = (int)entry.slidePaths.size();
	if (obs_sceneitem_t *item = scene ? obs_scene_find_source(scene, slideItemName().c_str()) : nullptr)
		state.hidden = !obs_sceneitem_visible(item);
	WorshipLive::instance().publish(state);
}

void PresentationCatalogDock::toggleHidden()
{
	if (liveEntry < 0 || !scene)
		return;
	if (obs_sceneitem_t *item = obs_scene_find_source(scene, slideItemName().c_str())) {
		obs_sceneitem_set_visible(item, !obs_sceneitem_visible(item));
		LiveState state = WorshipLive::instance().state();
		state.hidden = !obs_sceneitem_visible(item);
		WorshipLive::instance().publish(state);
	}
}

QString PresentationCatalogDock::renderedSlide(int entryIndex, int slide) const
{
	const QString &path = allEntries.at(entryIndex).slidePaths.at(slide);
	const QSize canvas(WorshipOutput::CanvasWidth(), WorshipOutput::CanvasHeight());
	return WorshipRender::HymnSlide(path, static_cast<WorshipRender::SlideFit>(fitCombo->currentIndex()), canvas);
}

int PresentationCatalogDock::audioModeIndex() const
{
	return audioModeCombo->currentIndex();
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
	WorshipLive::instance().clear(QStringLiteral("Himnario"));
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

bool PresentationCatalogDock::useAsSource()
{
	onProjectClicked();
	return liveEntry >= 0;
}
