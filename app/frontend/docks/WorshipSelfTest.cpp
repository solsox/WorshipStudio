#include "WorshipSelfTest.hpp"

#include "BibleDock.hpp"
#include "PresentationCatalogDock.hpp"
#include "PresentationDock.hpp"
#include "SongsDock.hpp"
#include "LiveToolsDock.hpp"
#include "ServiceDock.hpp"
#include "SongLibrary.hpp"
#include "WorshipLive.hpp"
#include "WorshipRemote.hpp"
#include "WelcomeGuide.hpp"
#include <OBSApp.hpp>
#include <QStackedWidget>
#include <obs-frontend-api.h>
#include <QJsonDocument>
#include <QJsonObject>
#include "WorshipOutput.hpp"
#include "WorshipRender.hpp"

#include <qt-wrappers.hpp>

#include <QDir>
#include <QPainter>
#include <QDockWidget>
#include <QMainWindow>
#include <QFile>
#include <QFileInfo>
#include <QImage>
#include <QPointer>
#include <QTextStream>
#include <QTimer>

#include <functional>
#include <memory>

namespace {
QImage renderSource(obs_source_t *source)
{
	return WorshipOutput::RenderSource(source, WorshipOutput::CanvasWidth(), WorshipOutput::CanvasHeight());
}

struct SelfTest {
	QPointer<PresentationCatalogDock> hymnal;
	QPointer<BibleDock> bible;
	QPointer<PresentationDock> presentations;
	QPointer<SongsDock> songs;
	QPointer<LiveToolsDock> liveTools;
	QPointer<ServiceDock> service;
	QDir outDir;
	QStringList report;
	int failures = 0;
	std::function<void()> finished;

	struct Step {
		int delayMs;
		std::function<void()> run;
		/* When set, the step is retried every 250 ms until it returns true
		 * or `delayMs` runs out. */
		std::function<bool()> until;
	};
	QVector<Step> steps;

	void log(const QString &line)
	{
		report << line;
		blog(LOG_INFO, "[WorshipStudio SelfTest] %s", QT_TO_UTF8(line));
	}

	void check(bool ok, const QString &what)
	{
		if (!ok)
			++failures;
		log(QStringLiteral("%1 %2").arg(ok ? QStringLiteral("[OK]  ") : QStringLiteral("[FAIL]"), what));
	}

	void capture(obs_scene_t *scene, const QString &name)
	{
		if (!scene) {
			check(false, QStringLiteral("captura %1: no hay escena").arg(name));
			return;
		}
		const QImage image = renderSource(obs_scene_get_source(scene));
		const QString path = outDir.absoluteFilePath(name + QStringLiteral(".png"));
		check(!image.isNull() && image.save(path), QStringLiteral("captura guardada %1").arg(path));
	}

	QString audioFile() const
	{
		if (!hymnal || !hymnal->audio())
			return {};
		OBSDataAutoRelease settings = obs_source_get_settings(hymnal->audio());
		return QT_UTF8(obs_data_get_string(settings, "local_file"));
	}

	QString mediaState() const
	{
		if (!hymnal || !hymnal->audio())
			return QStringLiteral("sin fuente");
		switch (obs_source_media_get_state(hymnal->audio())) {
		case OBS_MEDIA_STATE_PLAYING:
			return QStringLiteral("reproduciendo");
		case OBS_MEDIA_STATE_PAUSED:
			return QStringLiteral("pausado");
		case OBS_MEDIA_STATE_STOPPED:
			return QStringLiteral("detenido");
		case OBS_MEDIA_STATE_ENDED:
			return QStringLiteral("finalizado");
		case OBS_MEDIA_STATE_OPENING:
		case OBS_MEDIA_STATE_BUFFERING:
			return QStringLiteral("cargando");
		default:
			return QStringLiteral("ninguno");
		}
	}

	int64_t mediaTime() const { return hymnal && hymnal->audio() ? obs_source_media_get_time(hymnal->audio()) : -1; }

	void add(int delayMs, std::function<void()> run) { steps.push_back({delayMs, std::move(run), nullptr}); }
	void waitFor(int maxMs, std::function<bool()> until) { steps.push_back({maxMs, [] {}, std::move(until)}); }

	void addHymnSteps(int number, int mode)
	{
		const QString modeName = mode == 0 ? QStringLiteral("cantado")
					: mode == 1 ? QStringLiteral("instrumental")
						    : QStringLiteral("sololetra");
		const QString tag = QStringLiteral("himno%1-%2").arg(number, 3, 10, QLatin1Char('0')).arg(modeName);

		add(500, [=]() {
			log(QStringLiteral("--- Himno %1, modo %2 ---").arg(number).arg(modeName));
			check(hymnal->projectHymn(number, mode), QStringLiteral("proyectar himno %1").arg(number));
			const QString file = audioFile();
			if (mode == 0)
				check(file.contains(QStringLiteral("/Cantado/")), QStringLiteral("audio cantado: %1").arg(file));
			else if (mode == 1)
				check(file.contains(QStringLiteral("/Instrumental/")),
				      QStringLiteral("audio instrumental: %1").arg(file));
			else
				check(file.isEmpty(), QStringLiteral("sin audio (archivo vacio)"));
		});

		add(2000, [=]() {
			check(hymnal->currentSlide() == 0, QStringLiteral("diapositiva inicial 1/%1").arg(hymnal->slideCount()));
			if (mode < 2)
				check(mediaState() == QStringLiteral("reproduciendo") && mediaTime() > 0,
				      QStringLiteral("audio %1 en %2 ms").arg(mediaState()).arg(mediaTime()));
			else
				check(mediaState() != QStringLiteral("reproduciendo"),
				      QStringLiteral("audio no suena (%1)").arg(mediaState()));
			capture(hymnal->outputScene(), tag + QStringLiteral("-diapo1"));
		});

		if (mode < 2) {
			/* Auto-advance: jump the audio just before the 2nd change. */
			add(100, [=]() {
				const QVector<int> times = hymnal->slideTimes();
				if (times.size() > 1)
					obs_source_media_set_time(hymnal->audio(), times.at(1) - 800);
			});
			add(2000, [=]() {
				const QVector<int> times = hymnal->slideTimes();
				check(hymnal->currentSlide() == 2,
				      QStringLiteral("avance automatico a diapositiva 3 (actual %1, audio %2 ms, cambio en %3 ms)")
					      .arg(hymnal->currentSlide() + 1)
					      .arg(mediaTime())
					      .arg(times.value(1)));
				capture(hymnal->outputScene(), tag + QStringLiteral("-diapo3-auto"));
			});
		}

		/* Manual jump keeps lyrics and audio aligned. */
		/* Jump back to the second-to-last slide (after the auto-advance test). */
		auto target = std::make_shared<int>(0);
		add(100, [=]() {
			*target = qMax(1, hymnal->slideCount() - 2);
			hymnal->goToSlide(*target);
		});
		add(1200, [=]() {
			check(hymnal->currentSlide() == *target,
			      QStringLiteral("salto manual a diapositiva %1").arg(*target + 1));
			if (mode < 2) {
				const int expected = hymnal->slideTimes().value(*target - 1);
				const int64_t now = mediaTime();
				check(now >= expected && now < expected + 2500,
				      QStringLiteral("audio sincronizado tras salto: %1 ms (esperado desde %2 ms)")
					      .arg(now)
					      .arg(expected));
			}
			capture(hymnal->outputScene(), tag + QStringLiteral("-salto-manual"));
		});

		if (mode < 2) {
			/* End of the song: last slide stays and the audio ends cleanly. */
			add(100, [=]() {
				const int64_t duration = obs_source_media_get_duration(hymnal->audio());
				obs_source_media_set_time(hymnal->audio(), duration - 1200);
			});
			add(3500, [=]() {
				check(hymnal->currentSlide() == hymnal->slideCount() - 1,
				      QStringLiteral("final: ultima diapositiva %1/%2")
					      .arg(hymnal->currentSlide() + 1)
					      .arg(hymnal->slideCount()));
				check(mediaState() == QStringLiteral("finalizado") ||
					      mediaState() == QStringLiteral("detenido"),
				      QStringLiteral("final: audio %1").arg(mediaState()));
			});
		}

		add(100, [=]() { hymnal->stop(); });
	}

	void addBibleStep(const QString &query, int layout, const QString &name)
	{
		add(300, [=]() {
			log(QStringLiteral("--- Biblia: %1 (%2) ---")
				    .arg(query)
				    .arg(layout == 0 ? QStringLiteral("pantalla completa") : QStringLiteral("tercio inferior")));
			check(bible->projectQuery(query, layout), QStringLiteral("proyectar %1").arg(query));
		});
		add(900, [=]() {
			capture(bible->outputScene(), name);
			const QString rendered = bible->lastRenderedImage();
			const QString copy = outDir.absoluteFilePath(name + QStringLiteral("-imagen.png"));
			QFile::remove(copy);
			check(QFile::copy(rendered, copy), QStringLiteral("imagen generada %1").arg(copy));
		});
	}

	/* Opens a file, waits for a possible conversion and projects a few
	 * slides, normal and in book mode. */
	void addPresentationSteps(const QString &path, int index)
	{
		const QString tag = QStringLiteral("presentacion%1-%2")
					    .arg(index)
					    .arg(QFileInfo(path).suffix().toLower());
		add(300, [=]() {
			log(QStringLiteral("--- Presentacion: %1 ---").arg(path));
			presentations->setBookMode(false);
			presentations->openFiles({path});
		});
		/* Office files take a while to convert. */
		waitFor(180000, [=]() { return !presentations->isConverting(); });
		add(100, [=]() {
			check(!presentations->isConverting(), QStringLiteral("conversion terminada"));
			check(presentations->slideCount() > 0, QStringLiteral("%1 diapositivas (%2)")
								      .arg(presentations->slideCount())
								      .arg(presentations->deckError()));
			check(presentations->project(), QStringLiteral("proyectar"));
		});
		add(800, [=]() {
			check(presentations->currentSlide() == 0, QStringLiteral("diapositiva 1"));
			capture(presentations->outputScene(), tag + QStringLiteral("-diapo1"));
			presentations->nextSlide();
		});
		add(800, [=]() {
			check(presentations->slideCount() < 2 || presentations->currentSlide() == 1,
			      QStringLiteral("siguiente: diapositiva %1").arg(presentations->currentSlide() + 1));
			capture(presentations->outputScene(), tag + QStringLiteral("-diapo2"));
			presentations->goToSlide(presentations->slideCount() - 1);
		});
		add(800, [=]() {
			check(presentations->currentSlide() == presentations->slideCount() - 1,
			      QStringLiteral("ultima diapositiva %1").arg(presentations->slideCount()));
			capture(presentations->outputScene(), tag + QStringLiteral("-ultima"));
			presentations->setBookMode(true);
		});
		add(800, [=]() {
			log(QStringLiteral("modo libro: %1 diapositivas").arg(presentations->slideCount()));
			presentations->goToSlide(presentations->slideCount() > 20 ? 12 : 0);
		});
		add(800, [=]() {
			capture(presentations->outputScene(), tag + QStringLiteral("-libro-a"));
			presentations->nextSlide();
		});
		add(800, [=]() {
			capture(presentations->outputScene(), tag + QStringLiteral("-libro-b"));
			presentations->setBookMode(false);
		});
	}

	void addAllBibleSteps()
	{
		addBibleStep(QStringLiteral("Juan 3:16"), 0, QStringLiteral("biblia-juan3-16"));
		addBibleStep(QStringLiteral("1 Cor 13:4-7"), 0, QStringLiteral("biblia-1cor13-4-7"));
		addBibleStep(QStringLiteral("Sal 23:1-6"), 0, QStringLiteral("biblia-salmo23-completo"));
		addBibleStep(QStringLiteral("Romanos 8:31-39"), 0, QStringLiteral("biblia-romanos8-largo"));
		addBibleStep(QStringLiteral("Jueces 1:1"), 0, QStringLiteral("biblia-jueces1-mayusculas"));
		addBibleStep(QStringLiteral("Juan 3:16"), 1, QStringLiteral("biblia-juan3-16-tercio"));
		addBibleStep(QStringLiteral("porque de tal manera amo"), 0, QStringLiteral("biblia-busqueda-frase"));

		/* Every bundled version shows the same passage. */
		for (const QString &abbrev : bible->versionAbbrevs()) {
			add(200, [this, abbrev]() {
				check(bible->selectVersion(abbrev), QStringLiteral("version %1 seleccionada").arg(abbrev));
			});
			addBibleStep(abbrev == QStringLiteral("KJV") ? QStringLiteral("John 3:16") : QStringLiteral("Juan 3:16"),
					   0, QStringLiteral("version-%1").arg(abbrev.toLower()));
		}
		add(200, [this]() { bible->selectVersion(QStringLiteral("RV1909")); });

		/* Fonts and backgrounds. */
		const QStringList fonts = WorshipRender::RecommendedFonts();
		const QStringList presets = WorshipRender::BackgroundPresetNames();
		for (int i = 0; i < presets.size(); ++i) {
			const QString font = fonts.value(i % qMax(1, (int)fonts.size()));
			add(200, [this, font, i]() {
				log(QStringLiteral("estilo: fondo %1, letra %2").arg(i).arg(font));
				bible->setStyle(font, i);
			});
			addBibleStep(QStringLiteral("Sal 46:1-3"), 0,
					   QStringLiteral("estilo-%1-%2").arg(i).arg(QString(font).replace(QLatin1Char(' '), QLatin1Char('_'))));
		}
		add(200, [this, fonts]() { bible->setStyle(fonts.value(0), 0); });
	}

	void addSongSteps()
	{
		const QString title = QStringLiteral("Autoprueba de cantos");
		add(300, [=]() {
			log(QStringLiteral("--- Cantos ---"));
			songs->deleteSong(title);
			check(songs->projectNewSong(title,
						    QStringLiteral("[Estrofa 1]\nCuan grande es tu amor\nque llena mi corazon\n\n"
								   "[Coro]\nTe alabare\npor siempre te alabare\n\n"
								   "[Estrofa 2]\nEn la manana y en la noche\ncantare de tu bondad\n\n"
								   "[Puente]\nSanto, santo, santo\n\n[Coro]\nTe alabare\npor siempre te alabare"),
						    11),
			      QStringLiteral("crear y proyectar canto"));
		});
		add(800, [=]() {
			check(songs->currentSlide() == 0 && songs->slideCount() == 5,
			      QStringLiteral("diapositiva 1/%1").arg(songs->slideCount()));
			capture(songs->outputScene(), QStringLiteral("canto-estrofa1"));
			songs->nextSlide();
		});
		add(600, [=]() {
			check(songs->currentSlide() == 1, QStringLiteral("siguiente: coro"));
			capture(songs->outputScene(), QStringLiteral("canto-coro"));
			check(songs->jumpToSection(QLatin1Char('p')) && songs->currentSlide() == 3,
			      QStringLiteral("tecla P: puente (diapositiva %1)").arg(songs->currentSlide() + 1));
		});
		add(600, [=]() {
			capture(songs->outputScene(), QStringLiteral("canto-puente"));
			check(songs->jumpToSection(QLatin1Char('c')) && songs->currentSlide() == 4,
			      QStringLiteral("tecla C: siguiente coro (diapositiva %1)").arg(songs->currentSlide() + 1));
			check(songs->jumpToSection(QLatin1Char('2')) && songs->currentSlide() == 2,
			      QStringLiteral("tecla 2: estrofa 2 (diapositiva %1)").arg(songs->currentSlide() + 1));
		});
		add(300, [=]() { songs->deleteSong(title); });
	}

	/* Countdown, overlays, stage display, phone remote and service order. */
	void addLiveSteps()
	{
		add(300, [=]() {
			log(QStringLiteral("--- En vivo ---"));
			liveTools->startCountdown(5, QStringLiteral("El culto comienza en"));
		});
		add(1200, [=]() {
			check(liveTools->countdownRunning(), QStringLiteral("cuenta regresiva en marcha"));
			capture(liveTools->countdownScene(), QStringLiteral("envivo-cuenta-regresiva"));
			liveTools->stopCountdown();
			songs->projectNewSong(QStringLiteral("Autoprueba en vivo"),
					      QStringLiteral("[Coro]\nSanto es el Senor\nDios todopoderoso\n\n"
							     "[Estrofa 1]\nLa tierra llena esta\nde su gloria"),
					      12);
		});
		add(800, [=]() {
			const LiveState state = WorshipLive::instance().state();
			check(state.source == QStringLiteral("Cantos") && state.next.contains(QStringLiteral("tierra")),
			      QStringLiteral("hub en vivo: %1 / siguiente: %2").arg(state.source, state.next.left(30)));
			liveTools->showName(QStringLiteral("Pr. Daniel Gomez"), QStringLiteral("Mensaje: Fe que transforma"));
			liveTools->showUrgentMessage(QStringLiteral("Padres de Sofia, por favor acercarse a la recepcion"));
		});
		add(900, [=]() {
			OBSSourceAutoRelease program = obs_frontend_get_current_scene();
			obs_scene_t *scene = obs_scene_from_source(program);
			check(scene && obs_scene_find_source(scene, "WS Superposiciones"),
			      QStringLiteral("superposiciones sobre la escena al aire"));
			capture(scene, QStringLiteral("envivo-rotulo-y-mensaje"));
			liveTools->hideUrgentMessage();
			liveTools->hideName();
			WorshipLive::instance().setStageMessage(QStringLiteral("5 minutos"));
			QMetaObject::invokeMethod(liveTools, "refreshStage");
		});
		/* Stage display without opening a projector window. */
		add(300, [=]() { liveTools->ensureStage(); });
		add(600, [=]() {
			check(liveTools->stageScene() != nullptr, QStringLiteral("monitor de escenario creado"));
			capture(liveTools->stageScene(), QStringLiteral("envivo-monitor-escenario"));
		});
		add(300, [=]() {
			WorshipRemote *remote = liveTools->remote();
			const QString pin = remote->pin();
			const QByteArray page = remote->handleForTest("GET", QStringLiteral("/"));
			check(page.contains("WorshipStudio") && page.contains("Siguiente"), QStringLiteral("pagina del celular"));
			const QByteArray denied = remote->handleForTest("GET", QStringLiteral("/api/estado?pin=0000x"));
			check(denied.contains("error"), QStringLiteral("sin codigo no hay acceso"));
			const QJsonObject state = QJsonDocument::fromJson(remote->handleForTest(
				"GET", QStringLiteral("/api/estado?pin=%1").arg(pin))).object();
			check(state.value(QStringLiteral("source")).toString() == QStringLiteral("Cantos"),
			      QStringLiteral("estado por la API: %1").arg(state.value(QStringLiteral("title")).toString()));
			remote->handleForTest("POST", QStringLiteral("/api/accion?a=siguiente&pin=%1").arg(pin));
			remote->qrCode(240).save(outDir.absoluteFilePath(QStringLiteral("envivo-qr.png")));
			check(state.contains(QStringLiteral("scenes")) && state.contains(QStringLiteral("sources")),
			      QStringLiteral("estado con escenas y fuentes"));
		});
		add(700, [=]() {
			check(songs->currentSlide() == 1, QStringLiteral("siguiente desde el celular (diapositiva %1)").arg(songs->currentSlide() + 1));
			songs->deleteSong(QStringLiteral("Autoprueba en vivo"));

			ServiceItem countdown;
			countdown.kind = ServiceItem::Countdown;
			countdown.number = 3;
			countdown.value = QStringLiteral("Comenzamos en");
			ServiceItem bibleItem;
			bibleItem.kind = ServiceItem::Bible;
			bibleItem.value = QStringLiteral("Salmos 23:1-3");
			ServiceItem announcement;
			announcement.kind = ServiceItem::Announcement;
			announcement.title = QStringLiteral("Anuncio");
			announcement.value = QStringLiteral("Cena de jovenes\nViernes 19:00");
			service->setItems({countdown, bibleItem, announcement});
			check(service->activate(0), QStringLiteral("orden del culto: cuenta regresiva"));
		});
		add(800, [=]() {
			check(service->activateNext() && service->currentItem() == 1, QStringLiteral("orden del culto: pasaje"));
			check(WorshipLive::instance().serviceItems().size() == 3, QStringLiteral("orden visible en el celular"));
		});
		add(800, [=]() {
			capture(bible->outputScene(), QStringLiteral("envivo-orden-pasaje"));
			WorshipLive::instance().requestServiceItem(2);
		});
		add(800, [=]() {
			check(service->currentItem() == 2, QStringLiteral("orden del culto: anuncio pedido desde el celular"));
			capture(songs->outputScene(), QStringLiteral("envivo-orden-anuncio"));
			liveTools->stopCountdown();
			WorshipLive::instance().setStageMessage(QString());
			WorshipRemote *remote = liveTools->remote();
			remote->handleForTest("POST", QStringLiteral("/api/accion?a=biblia&cita=Juan%203%3A16&pin=%1").arg(remote->pin()));
		});
		add(800, [=]() {
			const LiveState state = WorshipLive::instance().state();
			check(state.source == QStringLiteral("Biblia") && state.title.contains(QStringLiteral("Juan 3:16")),
			      QStringLiteral("pasaje pedido desde el celular: %1").arg(state.title));
			WorshipRemote *remote = liveTools->remote();
			const QString pin = remote->pin();
			const QByteArray songsJson = remote->handleForTest("GET", QStringLiteral("/api/cantos?pin=%1").arg(pin));
			check(songsJson.startsWith('['), QStringLiteral("lista de cantos para el celular"));
			/* Switching to a user scene from the phone. */
			OBSSceneAutoRelease camera = obs_scene_create("Camara (prueba)");
			remote->handleForTest("POST", QStringLiteral("/api/accion?a=escena&nombre=Camara%20(prueba)&pin=%1").arg(pin));
			OBSSourceAutoRelease program = obs_frontend_get_current_scene();
			check(program && QT_UTF8(obs_source_get_name(program)) == QStringLiteral("Camara (prueba)"),
			      QStringLiteral("cambio de escena desde el celular"));
			obs_source_remove(obs_scene_get_source(camera));
			const QByteArray jpeg = remote->handleForTest("GET", QStringLiteral("/api/vista?cual=pantalla&pin=%1").arg(pin));
			check(jpeg.startsWith("\xFF\xD8"), QStringLiteral("miniatura en vivo para el celular (%1 bytes)").arg(jpeg.size()));
			QFile shot(outDir.absoluteFilePath(QStringLiteral("envivo-miniatura.jpg")));
			if (shot.open(QIODevice::WriteOnly))
				shot.write(jpeg);
			remote->handleForTest("POST", QStringLiteral("/api/accion?a=cuenta&min=3&pin=%1").arg(pin));
			check(liveTools->countdownRunning(), QStringLiteral("cuenta regresiva desde el celular"));
			liveTools->stopCountdown();
		});
	}

	void addImportSteps()
	{
		add(200, [=]() {
			log(QStringLiteral("--- Importar cantos ---"));
			const QString openLyrics = outDir.absoluteFilePath(QStringLiteral("canto-openlyrics.xml"));
			QFile f1(openLyrics);
			if (f1.open(QIODevice::WriteOnly))
				f1.write(R"(<?xml version="1.0" encoding="UTF-8"?>
<song xmlns="http://openlyrics.info/namespace/2009/song" version="0.9"><properties><titles><title>Canto OpenLyrics</title></titles><authors><author>Autor</author></authors></properties>
<lyrics><verse name="v1"><lines>Primera linea<br/>Segunda linea</lines></verse><verse name="c1"><lines>Coro uno<br/>Coro dos</lines></verse></lyrics></song>)");
			f1.close();
			Song song;
			QString error;
			check(SongLibrary::importFile(openLyrics, &song, &error) && song.slides.size() == 2 &&
				      song.slides.at(1).label == QStringLiteral("Coro 1") && song.title == QStringLiteral("Canto OpenLyrics"),
			      QStringLiteral("OpenLyrics: %1 diapositivas, %2").arg(song.slides.size()).arg(error));

			const QString openSong = outDir.absoluteFilePath(QStringLiteral("canto-opensong"));
			QFile f2(openSong);
			if (f2.open(QIODevice::WriteOnly))
				f2.write("<?xml version=\"1.0\"?><song><title>Canto OpenSong</title><author>X</author><lyrics>[V1]\n.G   D\n Linea uno\n Linea dos\n[C]\n Coro aqui</lyrics></song>");
			f2.close();
			Song song2;
			check(SongLibrary::importFile(openSong, &song2, &error) && song2.slides.size() == 2 &&
				      song2.slides.at(0).text == QStringLiteral("Linea uno\nLinea dos"),
			      QStringLiteral("OpenSong: %1 diapositivas").arg(song2.slides.size()));

			const QString chordPro = outDir.absoluteFilePath(QStringLiteral("canto.cho"));
			QFile f3(chordPro);
			if (f3.open(QIODevice::WriteOnly))
				f3.write("{title: Canto ChordPro}\n{artist: Y}\n[G]Linea [D]uno\nLinea dos\n\n{soc}\n[C]Coro final\n{eoc}\n");
			f3.close();
			Song song3;
			check(SongLibrary::importFile(chordPro, &song3, &error) && song3.slides.size() == 2 &&
				      song3.slides.at(0).text == QStringLiteral("Linea uno\nLinea dos") &&
				      song3.slides.at(1).label == QStringLiteral("Coro"),
			      QStringLiteral("ChordPro: %1 diapositivas, titulo %2").arg(song3.slides.size()).arg(song3.title));
		});
	}

	/* Showcase images for the website, drawn by the real renderer. */
	/* Studio Mode: new content waits in the preview; the screen only
	 * changes with the transition. */
	void addStudioSteps()
	{
		auto sceneName = [](bool preview) {
			OBSSourceAutoRelease source = preview ? obs_frontend_get_current_preview_scene()
							      : obs_frontend_get_current_scene();
			return source ? QT_UTF8(obs_source_get_name(source)) : QString();
		};
		add(300, [=]() {
			log(QStringLiteral("--- Modo estudio ---"));
			obs_frontend_set_preview_program_mode(true);
		});
		add(800, [=]() {
			songs->projectNewSong(QStringLiteral("Prueba estudio A"), QStringLiteral("Canto A\nprimera"), 11);
		});
		add(800, [=]() {
			check(sceneName(true).startsWith(QStringLiteral("WS Cantos")),
			      QStringLiteral("canto A en espera (%1)").arg(sceneName(true)));
			liveTools->goLive();
		});
		add(1500, [=]() {
			const QString onAir = sceneName(false);
			check(onAir.startsWith(QStringLiteral("WS Cantos")), QStringLiteral("canto A en pantalla (%1)").arg(onAir));
			songs->projectNewSong(QStringLiteral("Prueba estudio B"), QStringLiteral("Canto B\nsegunda"), 13);
		});
		add(800, [=]() {
			const QString onAir = sceneName(false), waiting = sceneName(true);
			check(onAir != waiting && waiting.startsWith(QStringLiteral("WS Cantos")),
			      QStringLiteral("canto B en espera sin tocar la pantalla (pantalla %1, espera %2)").arg(onAir, waiting));
			OBSSourceAutoRelease program = obs_frontend_get_current_scene();
			capture(obs_scene_from_source(program), QStringLiteral("estudio-pantalla-sigue-A"));
			OBSSourceAutoRelease preview = obs_frontend_get_current_preview_scene();
			capture(obs_scene_from_source(preview), QStringLiteral("estudio-espera-B"));
			liveTools->goLive();
		});
		add(1500, [=]() {
			check(sceneName(false).startsWith(QStringLiteral("WS Cantos")), QStringLiteral("canto B paso a pantalla"));
			songs->deleteSong(QStringLiteral("Prueba estudio A"));
			songs->deleteSong(QStringLiteral("Prueba estudio B"));
			hymnal->useEdition(0, QString());
			check(hymnal->projectHymn(1, 0), QStringLiteral("himno 1 en espera"));
		});
		add(2000, [=]() {
			check(mediaState() != QStringLiteral("reproduciendo"),
			      QStringLiteral("himno en espera no suena (%1)").arg(mediaState()));
			liveTools->goLive();
		});
		add(2500, [=]() {
			check(mediaState() == QStringLiteral("reproduciendo"),
			      QStringLiteral("al pasar a pantalla el himno suena (%1)").arg(mediaState()));
			hymnal->stop();
			obs_frontend_set_preview_program_mode(false);
		});
	}

	/* Changing colors and light/dark mode must work without restarting. */
	void addAppearanceSteps()
	{
		struct Look {
			const char *color;
			bool light;
			const char *name;
		};
		const Look looks[] = {{"#2C78DB", true, "claro-azul"},
				      {"#24966C", false, "oscuro-esmeralda"},
				      {"#D63E73", true, "claro-rosa"},
				      {"#E2582B", false, "oscuro-brasa"}};
		auto previous = std::make_shared<QString>();
		add(200, [=]() {
			log(QStringLiteral("--- Apariencia ---"));
			if (OBSTheme *theme = App()->GetTheme())
				*previous = theme->id;
		});
		for (const Look &look : looks) {
			const QColor color(QString::fromLatin1(look.color));
			const bool light = look.light;
			const QString name = QString::fromLatin1(look.name);
			add(300, [=]() {
				check(App()->SetWorshipAppearance(color, light), QStringLiteral("aplicar %1").arg(name));
				check(App()->IsThemeDark() == !light, QStringLiteral("modo %1 aplicado").arg(light ? "claro" : "oscuro"));
				log(QStringLiteral("resaltado de la paleta: %1 (pedido %2)")
					    .arg(qApp->palette().color(QPalette::Highlight).name(), color.name()));
			});
			add(900, [=]() {
				QWidget *window = hymnal->window();
				const QString path = outDir.absoluteFilePath(QStringLiteral("apariencia-%1.png").arg(name));
				check(window && window->grab().save(path), QStringLiteral("captura %1").arg(name));
			});
		}
		add(300, [=]() {
			if (!previous->isEmpty())
				App()->SetTheme(*previous);
		});
	}

	void addGuideSteps()
	{
		auto guide = std::make_shared<QPointer<WelcomeGuide>>();
		add(300, [=]() {
			*guide = new WelcomeGuide(liveTools, hymnal->window());
			(*guide)->resize(760, 660);
			(*guide)->show();
		});
		for (int page = 0; page < 5; ++page) {
			add(500, [=]() {
				if (!*guide)
					return;
				auto *pages = (*guide)->findChild<QStackedWidget *>();
				if (pages)
					pages->setCurrentIndex(page);
			});
			add(400, [=]() {
				if (!*guide)
					return;
				const QString path = outDir.absoluteFilePath(QStringLiteral("guia-paso%1.png").arg(page + 1));
				check((*guide)->grab().save(path), QStringLiteral("guia paso %1").arg(page + 1));
			});
		}
		add(200, [=]() {
			if (*guide)
				(*guide)->reject();
		});
	}

	void addGallerySteps()
	{
		struct Shot {
			const char *query;
			int preset;
			const char *font;
			const char *name;
		};
		const Shot shots[] = {
			{"Salmos 23:1", 12, "Playfair Display", "galeria-salmo23"},
			{"Salmos 121:1-2", 8, "Montserrat", "galeria-salmo121"},
			{"Salmos 150:6", 10, "DM Serif Display", "galeria-salmo150"},
			{"Isaias 40:31", 7, "Lora", "galeria-isaias40"},
			{"Salmos 46:10", 9, "Raleway", "galeria-salmo46"},
		};
		for (const Shot &shot : shots) {
			const QString query = QString::fromLatin1(shot.query);
			const QString font = QString::fromLatin1(shot.font);
			const QString name = QString::fromLatin1(shot.name);
			const int preset = shot.preset;
			add(200, [=]() {
				bible->setStyle(font, preset);
				check(bible->projectQuery(query, 0), QStringLiteral("galeria %1").arg(query));
			});
			add(500, [=]() {
				const QString copy = outDir.absoluteFilePath(name + QStringLiteral(".png"));
				QFile::remove(copy);
				check(QFile::copy(bible->lastRenderedImage(), copy), QStringLiteral("galeria guardada %1").arg(name));
			});
		}
		add(200, [=]() {
			const QSize canvas(1920, 1080);
			struct Lyric {
				const char *text;
				int preset;
				const char *font;
				const char *name;
			};
			const Lyric lyrics[] = {
				{"Cantad al Senor un cantico nuevo\ncantad al Senor toda la tierra", 11, "Montserrat", "galeria-canto1"},
				{"Alabad su nombre\nanunciad de dia en dia su salvacion", 13, "Poppins", "galeria-canto2"},
				{"Dad gloria y poder\ntributad la honra debida a su nombre", 15, "Josefin Sans", "galeria-canto3"},
			};
			for (const Lyric &lyric : lyrics) {
				WorshipRender::LyricsSlide slide;
				slide.text = QString::fromUtf8(lyric.text)
						     .replace(QStringLiteral("Senor"), QString::fromUtf8("Señor"))
						     .replace(QStringLiteral("cantico"), QString::fromUtf8("cántico"))
						     .replace(QStringLiteral("dia en dia"), QString::fromUtf8("día en día"))
						     .replace(QStringLiteral("salvacion"), QString::fromUtf8("salvación"));
				slide.background.preset = lyric.preset;
				slide.fontFamily = QString::fromLatin1(lyric.font);
				slide.maxFontSize = 96;
				WorshipRender::Lyrics(slide, canvas).save(outDir.absoluteFilePath(QString::fromLatin1(lyric.name) + QStringLiteral(".png")));
			}

			/* Lower third and urgent message over a clean background. */
			WorshipRender::LyricsSlide base;
			base.background.preset = 6;
			QImage composite = WorshipRender::Lyrics(base, canvas).convertToFormat(QImage::Format_ARGB32_Premultiplied);
			QPainter painter(&composite);
			painter.drawImage(0, 0, WorshipRender::NameLowerThird(QString::fromUtf8("Ana Martínez"),
									      QString::fromUtf8("Conferencia: Liderazgo con propósito"),
									      QStringLiteral("Montserrat"), canvas));
			painter.drawImage(0, 0, WorshipRender::UrgentMessage(QString::fromUtf8("Los padres de Sofía, por favor acercarse a la recepción"),
									     QStringLiteral("Montserrat"), canvas));
			painter.end();
			check(composite.save(outDir.absoluteFilePath(QStringLiteral("galeria-rotulo.png"))),
			      QStringLiteral("galeria rotulo"));

			WorshipRender::StageInfo stage;
			stage.clock = QStringLiteral("10:42");
			stage.title = QStringLiteral("Cantad al Señor");
			stage.section = QStringLiteral("Coro");
			stage.current = QString::fromUtf8("Cantad al Señor un cántico nuevo\ncantad al Señor toda la tierra");
			stage.next = QString::fromUtf8("Alabad su nombre\nanunciad de día en día su salvación");
			stage.timer = QStringLiteral("Cronometro 12:05");
			stage.message = QStringLiteral("5 minutos");
			WorshipRender::StageDisplay(stage, canvas).save(outDir.absoluteFilePath(QStringLiteral("galeria-escenario.png")));

			WorshipRender::Background night;
			night.preset = 9;
			WorshipRender::Countdown(QStringLiteral("Comenzamos en"), QStringLiteral("04:59"), QString(), night,
						 QStringLiteral("Montserrat"), canvas)
				.save(outDir.absoluteFilePath(QStringLiteral("galeria-cuenta.png")));
		});
		add(200, [=]() { bible->setStyle(WorshipRender::RecommendedFonts().value(0), 0); });
	}

	/* Screenshots of the main window with each WorshipStudio dock in front. */
	void addInterfaceSteps()
	{
		const QList<QPointer<QDockWidget>> docks = {hymnal.data(), songs.data(), bible.data(),
							     presentations.data()};
		const QStringList names = {QStringLiteral("himnario"), QStringLiteral("cantos"), QStringLiteral("biblia"),
					   QStringLiteral("presentaciones")};
		auto studioWas = std::make_shared<bool>(false);
		/* Show the real flow: a passage on screen, a song waiting. */
		add(300, [=]() {
			*studioWas = obs_frontend_preview_program_mode_active();
			obs_frontend_set_preview_program_mode(true);
		});
		add(800, [=]() { bible->projectQuery(QStringLiteral("Salmos 46:10"), 0); });
		add(800, [=]() { liveTools->goLive(); });
		add(1500, [=]() {
			songs->projectNewSong(QStringLiteral("Cantad al Señor"),
					      QString::fromUtf8("Cantad al Señor un cántico nuevo\ncantad al Señor toda la tierra"),
					      11);
		});
		for (int i = 0; i < docks.size(); ++i) {
			add(400, [=]() {
				if (docks.at(i)) {
					docks.at(i)->show();
					docks.at(i)->raise();
				}
			});
			add(700, [=]() {
				QWidget *window = hymnal->window();
				const QString path = outDir.absoluteFilePath(QStringLiteral("interfaz-%1.png").arg(names.at(i)));
				check(window && window->grab().save(path), QStringLiteral("captura de la interfaz %1").arg(path));
			});
		}
		add(200, [=]() {
			songs->deleteSong(QStringLiteral("Cantad al Señor"));
			obs_frontend_set_preview_program_mode(*studioWas);
		});
	}

	/* Opening another file while one is on screen must not stop it. */
	void addDeckSwitchSteps(const QString &first, const QString &second)
	{
		add(300, [=]() {
			log(QStringLiteral("--- Cambiar de presentacion con otra en vivo ---"));
			presentations->setBookMode(false);
			presentations->openFiles({first});
		});
		waitFor(180000, [=]() { return !presentations->isConverting(); });
		add(300, [=]() { check(presentations->project(), QStringLiteral("primera presentacion en vivo")); });
		add(600, [=]() { presentations->nextSlide(); });
		add(600, [=]() {
			check(presentations->currentSlide() == 1, QStringLiteral("primera: diapositiva 2"));
			presentations->openFiles({second});
		});
		waitFor(180000, [=]() { return !presentations->isConverting(); });
		add(600, [=]() {
			check(presentations->currentSlide() == 1, QStringLiteral("al elegir otra, la que esta en vivo sigue"));
			WorshipLive::instance().next();
		});
		add(800, [=]() {
			check(presentations->currentSlide() == 2,
			      QStringLiteral("Siguiente avanza la que esta en pantalla (diapositiva %1)").arg(presentations->currentSlide() + 1));
		});
	}

	void runNext(int index)
	{
		if (index >= steps.size()) {
			log(QStringLiteral("=== Resultado: %1 fallos ===").arg(failures));
			QFile file(outDir.absoluteFilePath(QStringLiteral("reporte.txt")));
			if (file.open(QIODevice::WriteOnly | QIODevice::Text))
				QTextStream(&file) << report.join(QLatin1Char('\n')) << '\n';
			if (finished)
				finished();
			return;
		}
		if (steps.at(index).until) {
			waitStep(index, 0);
			return;
		}
		QTimer::singleShot(steps.at(index).delayMs, [this, index]() {
			if (!hymnal || !bible || !presentations || !songs)
				return;
			steps.at(index).run();
			runNext(index + 1);
		});
	}

	void waitStep(int index, int waitedMs)
	{
		QTimer::singleShot(250, [this, index, waitedMs]() {
			if (!hymnal || !bible || !presentations || !songs)
				return;
			if (steps.at(index).until() || waitedMs + 250 >= steps.at(index).delayMs)
				runNext(index + 1);
			else
				waitStep(index, waitedMs + 250);
		});
	}
};
} // namespace

void RunWorshipSelfTest(PresentationCatalogDock *hymnal, BibleDock *bible, SongsDock *songs,
			PresentationDock *presentations, LiveToolsDock *liveTools, ServiceDock *service,
			const QString &outputDir, std::function<void()> finished)
{
	auto *test = new SelfTest();
	test->liveTools = liveTools;
	test->service = service;
	test->hymnal = hymnal;
	test->bible = bible;
	test->songs = songs;
	test->presentations = presentations;
	QDir().mkpath(outputDir);
	test->outDir = QDir(outputDir);

	test->log(QStringLiteral("=== WorshipStudio autoprueba, lienzo %1x%2 ===")
			  .arg(WorshipOutput::CanvasWidth())
			  .arg(WorshipOutput::CanvasHeight()));

	const QStringList only =
		qEnvironmentVariable("WORSHIPSTUDIO_SELFTEST_ONLY").split(QLatin1Char(','), Qt::SkipEmptyParts);
	const auto wanted = [&only](const char *section) {
		return only.isEmpty() || only.contains(QString::fromLatin1(section));
	};

	if (wanted("presentaciones")) {
		const QStringList presentationFiles = qEnvironmentVariable("WORSHIPSTUDIO_SELFTEST_PRESENTATIONS")
							      .split(QLatin1Char(';'), Qt::SkipEmptyParts);
		for (int i = 0; i < presentationFiles.size(); ++i)
			test->addPresentationSteps(presentationFiles.at(i), i + 1);
		if (presentationFiles.size() >= 2)
			test->addDeckSwitchSteps(presentationFiles.at(0), presentationFiles.at(1));
	}
	/* The checks expect content to go straight to the screen. */
	const bool studioWas = obs_frontend_preview_program_mode_active();
	obs_frontend_set_preview_program_mode(false);
	test->finished = [test, finished, studioWas]() {
		obs_frontend_set_preview_program_mode(studioWas);
		if (finished)
			finished();
		delete test;
	};

	if (wanted("galeria"))
		test->addGallerySteps();
	if (wanted("estudio"))
		test->addStudioSteps();
	if (wanted("guia"))
		test->addGuideSteps();
	if (wanted("apariencia"))
		test->addAppearanceSteps();
	if (wanted("envivo"))
		test->addLiveSteps();
	if (wanted("importar"))
		test->addImportSteps();
	if (wanted("interfaz"))
		test->addInterfaceSteps();
	if (wanted("cantos"))
		test->addSongSteps();
	if (wanted("biblia"))
		test->addAllBibleSteps();
	if (!wanted("himnario")) {
		test->runNext(0);
		return;
	}

	/* Hymn checks use the new hymnal (2009). */
	test->add(100, [test]() { test->hymnal->useEdition(0, QString()); });
	test->addHymnSteps(1, 0);
	test->addHymnSteps(1, 1);
	test->addHymnSteps(1, 2);
	test->addHymnSteps(100, 0);

	/* Old hymnal: a plain folder of hymns (NNN - Titulo with images and mp3). */
	const QString oldFolder = qEnvironmentVariable("WORSHIPSTUDIO_SELFTEST_OLD_HYMNAL");
	if (!oldFolder.isEmpty()) {
		auto previousRoot = std::make_shared<QString>();
		test->add(300, [test, oldFolder, previousRoot]() {
			test->log(QStringLiteral("--- Himnario antiguo (carpeta simple) ---"));
			*previousRoot = test->hymnal->useEdition(1, oldFolder);
			test->check(test->hymnal->hymnCount() > 0,
				    QStringLiteral("himnario antiguo cargado: %1 himnos").arg(test->hymnal->hymnCount()));
			test->check(test->hymnal->projectHymn(2, 0), QStringLiteral("proyectar himno 2 del antiguo"));
		});
		test->add(2000, [test]() {
			test->check(test->mediaState() == QStringLiteral("reproduciendo"),
				    QStringLiteral("audio del antiguo %1").arg(test->mediaState()));
			test->check(test->hymnal->currentSlide() == 0,
				    QStringLiteral("sin tiempos: se queda en la diapositiva 1 hasta avanzar a mano"));
			test->hymnal->goToSlide(1);
		});
		test->add(600, [test]() {
			test->check(test->hymnal->currentSlide() == 1, QStringLiteral("avance manual a diapositiva 2"));
			test->capture(test->hymnal->outputScene(), QStringLiteral("antiguo-himno2-diapo2"));
			test->hymnal->stop();
		});
		test->add(300, [test, previousRoot]() {
			test->hymnal->useEdition(1, *previousRoot);
			test->hymnal->useEdition(0, QString());
			test->check(test->hymnal->hymnCount() > 600,
				    QStringLiteral("de vuelta al himnario nuevo: %1 himnos").arg(test->hymnal->hymnCount()));
		});
	}

	test->runNext(0);
}
