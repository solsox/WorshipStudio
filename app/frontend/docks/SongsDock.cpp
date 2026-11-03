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

#include "SongsDock.hpp"
#include "PresentationCatalogData.hpp"
#include "SongEditor.hpp"
#include "WorshipLive.hpp"
#include "WorshipOutput.hpp"
#include "WorshipDrag.hpp"
#include "WorshipRender.hpp"

#include <qt-wrappers.hpp>
#include <Idian/Utils.hpp>

#include <algorithm>
#include <QAbstractItemView>
#include <QCryptographicHash>
#include <QDateTime>
#include <QDir>
#include <QFileDialog>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QJsonDocument>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QListWidgetItem>
#include <QMessageBox>
#include <QPixmap>
#include <QPushButton>
#include <QSplitter>
#include <QTimer>
#include <QVBoxLayout>

#include "moc_SongsDock.cpp"

namespace {
constexpr const char *SceneName = "WS Cantos";
constexpr const char *ScreenSourceName = "WS Cantos - Pantalla";
constexpr const char *VideoSourceName = "WS Cantos - Video de fondo";
const QSize ThumbnailSize(160, 90);

/* Section names start with these words (in the languages WorshipStudio is
 * translated to, compared without accents); the key jumps to the next one. */
QStringList sectionPrefixes(QChar key)
{
	switch (key.toLower().unicode()) {
	case 'c':
		return {QStringLiteral("coro"), QStringLiteral("estribillo"), QStringLiteral("chorus"),
			QStringLiteral("refrao"), QStringLiteral("refrain"), QStringLiteral("ritornello"),
			QStringLiteral("refren")};
	case 'p':
		return {QStringLiteral("puente"), QStringLiteral("bridge"), QStringLiteral("ponte"),
			QStringLiteral("pont"), QStringLiteral("brucke")};
	case 'f':
		return {QStringLiteral("final"), QStringLiteral("ending"), QStringLiteral("outro"),
			QStringLiteral("fin"), QStringLiteral("schluss")};
	case 'i':
		return {QStringLiteral("intro"), QStringLiteral("introducao")};
	case 'e':
		return {QStringLiteral("estrofa"), QStringLiteral("estrofe"), QStringLiteral("verse"), QStringLiteral("verso"),
			QStringLiteral("couplet"), QStringLiteral("strophe"), QStringLiteral("strofa")};
	default:
		return {};
	}
}
} // namespace

SongsDock::SongsDock(const QString &objectName, const QString &title, QWidget *parent) : OBSDock(title, parent)
{
	setObjectName(objectName);

	auto *root = new QWidget(this);
	auto *layout = new QVBoxLayout(root);
	layout->setContentsMargins(8, 8, 8, 8);
	layout->setSpacing(6);

	searchEdit = new QLineEdit(root);
	searchEdit->setPlaceholderText(tr("Buscar canto por titulo o letra"));
	searchEdit->setClearButtonEnabled(true);

	auto *newButton = new QPushButton(tr("Nuevo"), root);
	newButton->setObjectName(QStringLiteral("songsNew"));
	auto *editButton = new QPushButton(tr("Editar"), root);
	auto *deleteButton = new QPushButton(tr("Borrar"), root);
	auto *importButton = new QPushButton(tr("Importar..."), root);
	importButton->setToolTip(tr("OpenLyrics (OpenLP), OpenSong, ChordPro, texto (.txt) o cantos de WorshipStudio"));

	auto *libraryRow = new QHBoxLayout();
	libraryRow->addWidget(newButton);
	libraryRow->addWidget(editButton);
	libraryRow->addWidget(deleteButton);
	libraryRow->addWidget(importButton);

	songsList = new QListWidget(root);
	WorshipDrag::EnableDrag(songsList, objectName);
	songsList->setObjectName(QStringLiteral("songsList"));
	songsList->setSelectionMode(QAbstractItemView::SingleSelection);

	slidesList = new QListWidget(root);
	slidesList->setViewMode(QListView::IconMode);
	slidesList->setIconSize(ThumbnailSize);
	slidesList->setResizeMode(QListView::Adjust);
	slidesList->setMovement(QListView::Static);
	slidesList->setSpacing(4);
	slidesList->setUniformItemSizes(true);
	slidesList->setSelectionMode(QAbstractItemView::SingleSelection);
	slidesList->installEventFilter(this);
	slidesList->setToolTip(tr("Flechas o espacio: avanzar  |  C coro, P puente, 1-9 estrofa  |  L sin texto  |  B ocultar"));

	auto *splitter = new QSplitter(Qt::Vertical, root);
	splitter->addWidget(songsList);
	splitter->addWidget(slidesList);
	splitter->setStretchFactor(0, 2);
	splitter->setStretchFactor(1, 3);

	clearButton = new QPushButton(tr("Solo fondo"), root);
	clearButton->setCheckable(true);
	clearButton->setToolTip(tr("Deja solo el fondo en pantalla, sin la letra (tecla L)"));

	auto *hint = new QLabel(tr("Doble clic en un canto o en una diapositiva para mostrarlo. "
				   "Teclas: C coro, P puente, 1-9 estrofa."),
				root);
	hint->setWordWrap(true);
	idian::Utils::addClass(hint, "text-muted");

	liveLabel = new QLabel(root);
	idian::Utils::addClass(liveLabel, "text-bold");
	liveLabel->setWordWrap(true);
	statusLabel = new QLabel(root);
	idian::Utils::addClass(statusLabel, "text-muted");
	statusLabel->setWordWrap(true);

	layout->addWidget(searchEdit);
	layout->addLayout(libraryRow);
	layout->addWidget(splitter, 1);
	layout->addWidget(hint);
	layout->addWidget(clearButton);
	layout->addWidget(liveLabel);
	layout->addWidget(statusLabel);
	setWidget(root);

	connect(searchEdit, &QLineEdit::textChanged, this, &SongsDock::onSearchChanged);
	connect(searchEdit, &QLineEdit::returnPressed, this, &SongsDock::project);
	connect(songsList, &QListWidget::currentRowChanged, this, &SongsDock::onSongChanged);
	connect(songsList, &QListWidget::itemActivated, this, &SongsDock::project);
	connect(slidesList, &QListWidget::itemClicked, this, &SongsDock::onSlideClicked);
	connect(slidesList, &QListWidget::itemActivated, this, &SongsDock::onSlideClicked);
	connect(newButton, &QPushButton::clicked, this, &SongsDock::onNewClicked);
	connect(editButton, &QPushButton::clicked, this, &SongsDock::onEditClicked);
	connect(deleteButton, &QPushButton::clicked, this, &SongsDock::onDeleteClicked);
	connect(importButton, &QPushButton::clicked, this, &SongsDock::onImportClicked);
	connect(clearButton, &QPushButton::clicked, this, &SongsDock::toggleClearText);

	WorshipLive::instance().registerTarget(QStringLiteral("Cantos"), this);
	reload();
}


void SongsDock::reload(const QString &selectId)
{
	songs = SongLibrary::loadAll();
	rebuildList();
	if (!selectId.isEmpty()) {
		for (int row = 0; row < songsList->count(); ++row) {
			if (songs.at(songsList->item(row)->data(Qt::UserRole).toInt()).id == selectId) {
				songsList->setCurrentRow(row);
				break;
			}
		}
	}
}

void SongsDock::rebuildList()
{
	const QString filter = PresentationCatalogData::normalizeName(searchEdit->text());
	QSignalBlocker blocker(songsList);
	songsList->clear();
	for (int i = 0; i < songs.size(); ++i) {
		if (!filter.isEmpty() && !songs.at(i).searchBlob().contains(filter))
			continue;
		auto *item = new QListWidgetItem(songs.at(i).author.isEmpty()
							 ? songs.at(i).title
							 : QStringLiteral("%1  —  %2").arg(songs.at(i).title, songs.at(i).author));
		item->setData(Qt::UserRole, i);
		songsList->addItem(item);
	}
	blocker.unblock();

	statusLabel->setText(songs.isEmpty() ? tr("Todavia no hay cantos. Usa \"Nuevo\" para escribir uno.")
					     : tr("%1 cantos").arg(songsList->count()));
	songsList->setCurrentRow(songsList->count() > 0 ? 0 : -1);
	onSongChanged();
}

void SongsDock::onSearchChanged(const QString &)
{
	rebuildList();
}

void SongsDock::onSongChanged()
{
	QListWidgetItem *item = songsList->currentItem();
	selectedSong = item ? item->data(Qt::UserRole).toInt() : -1;
	rebuildSlides();
	updateControls();
}

void SongsDock::rebuildSlides()
{
	QSignalBlocker blocker(slidesList);
	slidesList->clear();
	if (selectedSong < 0)
		return;

	const Song &song = songs.at(selectedSong);
	for (int i = 0; i < song.slides.size(); ++i) {
		const QImage image = WorshipRender::LyricsPreview(SongLibrary::lyricsSlide(song, i), ThumbnailSize * 2);
		const QString label = song.slides.at(i).label.isEmpty() ? QString::number(i + 1) : song.slides.at(i).label;
		auto *item = new QListWidgetItem(
			QIcon(QPixmap::fromImage(image.scaled(ThumbnailSize, Qt::KeepAspectRatio, Qt::SmoothTransformation))),
			label);
		item->setData(Qt::UserRole, i);
		slidesList->addItem(item);
	}
	if (live && song.id == liveSong.id && liveSlide >= 0)
		slidesList->setCurrentRow(liveSlide);
}

int SongsDock::slideCount() const
{
	return live ? (int)liveSong.slides.size() : 0;
}

/* ------------------------------------------------------------------------ */
/* Library                                                                   */

bool SongsDock::editSong(Song song)
{
	SongEditor editor(song, this);
	if (editor.exec() != QDialog::Accepted)
		return false;

	Song saved = editor.song();
	QString error;
	if (!SongLibrary::save(saved, &error)) {
		statusLabel->setText(tr("No se pudo guardar el canto: %1").arg(error));
		return false;
	}
	reload(saved.id);

	/* Editing the song that is on screen updates the screen. */
	if (live && saved.id == liveSong.id) {
		liveSong = saved;
		showSlide(qMin(liveSlide, (int)liveSong.slides.size() - 1), true);
	}
	return true;
}

void SongsDock::onNewClicked()
{
	Song song;
	song.fontFamily = WorshipRender::RecommendedFonts().value(0);
	editSong(song);
}

void SongsDock::onEditClicked()
{
	if (selectedSong >= 0)
		editSong(songs.at(selectedSong));
}

void SongsDock::onDeleteClicked()
{
	if (selectedSong < 0)
		return;
	const Song song = songs.at(selectedSong);
	if (QMessageBox::question(this, tr("Borrar canto"), tr("¿Borrar \"%1\"? No se puede deshacer.").arg(song.title)) !=
	    QMessageBox::Yes)
		return;
	SongLibrary::remove(song);
	reload();
}

void SongsDock::onImportClicked()
{
	const QStringList files = QFileDialog::getOpenFileNames(this, tr("Importar cantos"), QDir::homePath(),
								 tr("Cantos (*.txt *.json *.xml *.cho *.chopro *.chordpro *.crd);;Todos los archivos (*)"));
	int imported = 0;
	QString lastId, errors;
	for (const QString &file : files) {
		Song song;
		QString error;
		if (SongLibrary::importFile(file, &song, &error) && SongLibrary::save(song, &error)) {
			++imported;
			lastId = song.id;
		} else {
			errors += QStringLiteral("\n%1: %2").arg(QFileInfo(file).fileName(), error);
		}
	}
	if (imported > 0)
		reload(lastId);
	if (!files.isEmpty())
		statusLabel->setText(tr("%1 cantos importados.").arg(imported) + errors);
}

void SongsDock::deleteSong(const QString &title)
{
	for (const Song &song : SongLibrary::loadAll()) {
		if (song.title == title)
			SongLibrary::remove(song);
	}
	reload();
}

bool SongsDock::projectNewSong(const QString &title, const QString &text, int preset)
{
	Song song;
	song.title = title;
	song.fontFamily = WorshipRender::RecommendedFonts().value(0);
	song.background.preset = preset;
	song.showTitle = true;
	song.slides = SongLibrary::parseText(text);
	if (!SongLibrary::save(song))
		return false;
	searchEdit->clear();
	reload(song.id);
	return project();
}

bool SongsDock::projectSongById(const QString &id)
{
	searchEdit->clear();
	reload(id);
	if (selectedSong < 0 || songs.at(selectedSong).id != id)
		return false;
	QSignalBlocker blocker(slidesList);
	slidesList->setCurrentRow(0);
	return project();
}

bool SongsDock::projectTransient(const Song &song)
{
	if (song.slides.isEmpty())
		return false;
	liveSong = song;
	live = true;
	hidden = false;
	textCleared = false;
	clearButton->setChecked(false);
	freshOutput = true;
	showSlide(0, true);
	setItemVisible(true);
	WorshipOutput::ShowScene(scene);
	return liveSlide >= 0;
}

/* ------------------------------------------------------------------------ */
/* Live                                                                      */

bool SongsDock::prepareOutput(const WorshipRender::Background &background)
{
	/* New songs go to the copy of the scene that is not on air. */
	if (freshOutput || !scene) {
		outputSlot = WorshipOutput::OutputSlot(SceneName);
		scene = WorshipOutput::EnsureScene(WorshipOutput::Named(SceneName, outputSlot).c_str());
		freshOutput = false;
	}
	if (!scene)
		return false;
	const std::string videoName = WorshipOutput::Named(VideoSourceName, outputSlot);
	const std::string screenName = WorshipOutput::Named(ScreenSourceName, outputSlot);

	const bool useVideo = background.kind == WorshipRender::BackgroundKind::Video &&
			      liveSong.layout == WorshipRender::LyricsLayout::FullScreen;
	obs_sceneitem_t *videoItem = obs_scene_find_source(scene, videoName.c_str());
	if (useVideo) {
		OBSDataAutoRelease settings = obs_data_create();
		obs_data_set_bool(settings, "is_local_file", true);
		obs_data_set_string(settings, "local_file", QT_TO_UTF8(background.path));
		obs_data_set_bool(settings, "looping", true);
		obs_data_set_bool(settings, "restart_on_activate", false);
		bool created = false;
		OBSSource video = WorshipOutput::EnsureSource(scene, videoName.c_str(), "ffmpeg_source", settings, &created);
		if (video) {
			if (!created)
				obs_source_update(video, settings);
			WorshipOutput::FitItem(scene, video, 0.0f, 0.0f, 1.0f, 1.0f, OBS_ALIGN_CENTER,
					       OBS_BOUNDS_SCALE_OUTER);
			videoItem = obs_scene_find_source(scene, videoName.c_str());
			/* The video always stays under the lyrics. */
			obs_sceneitem_set_order(videoItem, OBS_ORDER_MOVE_BOTTOM);
		}
	}
	if (videoItem)
		obs_sceneitem_set_visible(videoItem, useVideo);

	bool created = false;
	OBSDataAutoRelease settings = obs_data_create();
	screenSource = WorshipOutput::EnsureSource(scene, screenName.c_str(), "image_source", settings, &created);
	if (created && screenSource)
		WorshipOutput::FitItem(scene, screenSource, 0.0f, 0.0f, 1.0f, 1.0f);
	return screenSource;
}

bool SongsDock::project()
{
	if (selectedSong < 0 || songs.at(selectedSong).slides.isEmpty())
		return false;

	liveSong = songs.at(selectedSong);
	live = true;
	hidden = false;
	textCleared = false;
	clearButton->setChecked(false);

	const int start = qMax(0, slidesList->currentRow());
	freshOutput = true;
	showSlide(start, true);
	setItemVisible(true);
	WorshipOutput::ShowScene(scene);
	return liveSlide >= 0;
}

QString SongsDock::renderedSlide(int slide) const
{
	const QSize canvas(WorshipOutput::CanvasWidth(), WorshipOutput::CanvasHeight());
	WorshipRender::LyricsSlide lyrics = SongLibrary::lyricsSlide(liveSong, slide);
	if (textCleared) {
		lyrics.text.clear();
		lyrics.footer.clear();
	}

	const QByteArray key = QJsonDocument(SongLibrary::backgroundToJson(lyrics.background)).toJson() +
			       QStringLiteral("|%1|%2|%3|%4|%5|%6|%7|%8x%9")
				       .arg(lyrics.text, lyrics.footer, lyrics.fontFamily)
				       .arg(lyrics.maxFontSize)
				       .arg(lyrics.uppercase)
				       .arg((int)lyrics.layout)
				       .arg(QFileInfo(lyrics.background.path).lastModified().toMSecsSinceEpoch())
				       .arg(canvas.width())
				       .arg(canvas.height())
				       .toUtf8();
	const QString hash = QString::fromLatin1(QCryptographicHash::hash(key, QCryptographicHash::Sha1).toHex().left(20));
	const QString path = QDir(WorshipRender::CacheDir(QStringLiteral("songs"))).absoluteFilePath(hash + ".png");
	if (!QFileInfo::exists(path))
		WorshipRender::Lyrics(lyrics, canvas).save(path, "PNG");
	return path;
}

void SongsDock::showSlide(int slide, bool force)
{
	if (!live) {
		if (selectedSong >= 0) {
			QSignalBlocker blocker(slidesList);
			slidesList->setCurrentRow(slide);
			project();
		}
		return;
	}
	if (liveSong.slides.isEmpty())
		return;

	slide = qBound(0, slide, (int)liveSong.slides.size() - 1);
	if (slide == liveSlide && !force)
		return;

	const WorshipRender::LyricsSlide lyrics = SongLibrary::lyricsSlide(liveSong, slide);
	if (!prepareOutput(lyrics.background))
		return;

	OBSDataAutoRelease settings = obs_data_create();
	obs_data_set_string(settings, "file", QT_TO_UTF8(renderedSlide(slide)));
	obs_source_update(screenSource, settings);
	liveSlide = slide;

	/* Prepare the next slide so the change is instant. */
	if (slide + 1 < liveSong.slides.size()) {
		const QString id = liveSong.id;
		QTimer::singleShot(0, this, [this, id, slide]() {
			if (live && liveSong.id == id && slide + 1 < liveSong.slides.size())
				renderedSlide(slide + 1);
		});
	}

	if (selectedSong >= 0 && songs.at(selectedSong).id == liveSong.id) {
		QSignalBlocker blocker(slidesList);
		slidesList->setCurrentRow(slide);
	}
	updateControls();
	updateLiveLabel();
	publishLive();
}

void SongsDock::publishLive()
{
	if (!live || liveSlide < 0)
		return;
	LiveState state;
	state.source = QStringLiteral("Cantos");
	state.title = liveSong.title;
	state.current = textCleared ? QString() : liveSong.slides.at(liveSlide).text;
	state.next = liveSlide + 1 < liveSong.slides.size() ? liveSong.slides.at(liveSlide + 1).text : QString();
	state.section = liveSong.slides.at(liveSlide).label;
	state.index = liveSlide;
	state.count = (int)liveSong.slides.size();
	state.hidden = hidden;
	WorshipLive::instance().publish(state);
}

void SongsDock::onSlideClicked(QListWidgetItem *item)
{
	if (!item)
		return;
	/* Clicking a slide of another song starts that song there. */
	if (!live || selectedSong < 0 || songs.at(selectedSong).id != liveSong.id) {
		QSignalBlocker blocker(slidesList);
		slidesList->setCurrentItem(item);
		project();
		return;
	}
	showSlide(item->data(Qt::UserRole).toInt());
}

void SongsDock::nextSlide()
{
	if (live)
		showSlide(liveSlide + 1);
}

void SongsDock::previousSlide()
{
	if (live)
		showSlide(liveSlide - 1);
}

bool SongsDock::jumpToSection(QChar key)
{
	if (!live)
		return false;

	int target = -1;
	if (key.isDigit() && key != QLatin1Char('0')) {
		/* "3" goes to Estrofa 3 (or the 3rd verse when labels have no number). */
		const QString number = QString(key);
		int verse = 0;
		for (int i = 0; i < liveSong.slides.size() && target < 0; ++i) {
			const QString label = PresentationCatalogData::normalizeName(liveSong.slides.at(i).label);
			if (label.startsWith(QStringLiteral("estrofa")) || label.startsWith(QStringLiteral("verso"))) {
				const bool newVerse = i == 0 || liveSong.slides.at(i - 1).label != liveSong.slides.at(i).label;
				if (newVerse)
					++verse;
				if (newVerse && (label.endsWith(QLatin1Char(' ') + number) || verse == number.toInt()))
					target = i;
			}
		}
	} else {
		const QStringList prefixes = sectionPrefixes(key);
		if (prefixes.isEmpty())
			return false;
		/* Next matching section after the current slide, wrapping around. */
		const int count = (int)liveSong.slides.size();
		for (int step = 1; step <= count && target < 0; ++step) {
			const int i = (liveSlide + step) % count;
			const QString label = PresentationCatalogData::normalizeName(liveSong.slides.at(i).label);
			const bool sectionStart = i == 0 || liveSong.slides.at(i - 1).label != liveSong.slides.at(i).label;
			if (sectionStart && std::any_of(prefixes.begin(), prefixes.end(),
							[&](const QString &prefix) { return label.startsWith(prefix); }))
				target = i;
		}
	}

	if (target < 0)
		return false;
	showSlide(target);
	return true;
}

void SongsDock::setItemVisible(bool visible)
{
	if (!scene)
		return;
	for (const char *base : {ScreenSourceName, VideoSourceName}) {
		const std::string name = WorshipOutput::Named(base, outputSlot);
		if (obs_sceneitem_t *item = obs_scene_find_source(scene, name.c_str())) {
			const bool isVideo = base == VideoSourceName;
			const bool videoWanted = SongLibrary::lyricsSlide(liveSong, liveSlide).background.kind ==
						 WorshipRender::BackgroundKind::Video;
			obs_sceneitem_set_visible(item, visible && (!isVideo || videoWanted));
		}
	}
}

void SongsDock::toggleHidden()
{
	if (!live)
		return;
	hidden = !hidden;
	setItemVisible(!hidden);
	updateLiveLabel();
	publishLive();
}

void SongsDock::toggleClearText()
{
	if (!live) {
		clearButton->setChecked(false);
		return;
	}
	textCleared = !textCleared;
	clearButton->setChecked(textCleared);
	showSlide(liveSlide, true);
}

bool SongsDock::eventFilter(QObject *watched, QEvent *event)
{
	if (watched == slidesList && event->type() == QEvent::KeyPress) {
		auto *keyEvent = static_cast<QKeyEvent *>(event);
		switch (keyEvent->key()) {
		case Qt::Key_Right:
		case Qt::Key_Down:
		case Qt::Key_PageDown:
		case Qt::Key_Space:
			if (!live)
				project();
			else
				nextSlide();
			return true;
		case Qt::Key_Left:
		case Qt::Key_Up:
		case Qt::Key_PageUp:
			previousSlide();
			return true;
		case Qt::Key_Return:
		case Qt::Key_Enter:
			showSlide(slidesList->currentRow());
			return true;
		case Qt::Key_B:
		case Qt::Key_Period:
			toggleHidden();
			return true;
		case Qt::Key_L:
			toggleClearText();
			return true;
		default:
			if (!keyEvent->text().isEmpty() && jumpToSection(keyEvent->text().at(0)))
				return true;
			break;
		}
	}
	return OBSDock::eventFilter(watched, event);
}

void SongsDock::updateControls()
{
	clearButton->setEnabled(live);
}

void SongsDock::updateLiveLabel()
{
	if (!live || liveSlide < 0) {
		liveLabel->setText(tr("Sin proyeccion activa"));
		return;
	}
	const SongSlide &slide = liveSong.slides.at(liveSlide);
	QString text = tr("En vivo: %1  |  %2 (%3/%4)")
			       .arg(liveSong.title, slide.label.isEmpty() ? tr("Diapositiva") : slide.label)
			       .arg(liveSlide + 1)
			       .arg(liveSong.slides.size());
	if (textCleared)
		text += tr("  |  SIN TEXTO");
	if (hidden)
		text += tr("  |  OCULTO");
	liveLabel->setText(text);
}

bool SongsDock::useAsSource()
{
	return project();
}
