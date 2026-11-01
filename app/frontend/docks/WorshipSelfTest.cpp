#include "WorshipSelfTest.hpp"

#include "BibleDock.hpp"
#include "PresentationCatalogDock.hpp"
#include "PresentationDock.hpp"
#include "WorshipOutput.hpp"
#include "WorshipRender.hpp"

#include <qt-wrappers.hpp>

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QImage>
#include <QPointer>
#include <QTextStream>
#include <QTimer>

#include <functional>
#include <memory>

namespace {
/* Renders a source (usually a scene) to an image, like OBS screenshots do. */
QImage renderSource(obs_source_t *source)
{
	const uint32_t width = WorshipOutput::CanvasWidth();
	const uint32_t height = WorshipOutput::CanvasHeight();
	QImage image;

	obs_enter_graphics();
	gs_texrender_t *texrender = gs_texrender_create(GS_RGBA, GS_ZS_NONE);
	gs_stagesurf_t *stagesurf = gs_stagesurface_create(width, height, GS_RGBA);
	if (texrender && stagesurf && gs_texrender_begin(texrender, width, height)) {
		vec4 clear;
		vec4_zero(&clear);
		gs_clear(GS_CLEAR_COLOR, &clear, 0.0f, 0);
		gs_ortho(0.0f, (float)width, 0.0f, (float)height, -100.0f, 100.0f);
		gs_blend_state_push();
		gs_blend_function(GS_BLEND_ONE, GS_BLEND_ZERO);
		obs_source_inc_showing(source);
		obs_source_video_render(source);
		obs_source_dec_showing(source);
		gs_blend_state_pop();
		gs_texrender_end(texrender);

		gs_stage_texture(stagesurf, gs_texrender_get_texture(texrender));
		uint8_t *data = nullptr;
		uint32_t linesize = 0;
		if (gs_stagesurface_map(stagesurf, &data, &linesize)) {
			image = QImage(data, width, height, linesize, QImage::Format_RGBA8888).copy();
			gs_stagesurface_unmap(stagesurf);
		}
	}
	gs_stagesurface_destroy(stagesurf);
	gs_texrender_destroy(texrender);
	obs_leave_graphics();
	return image;
}

struct SelfTest {
	QPointer<PresentationCatalogDock> hymnal;
	QPointer<BibleDock> bible;
	QPointer<PresentationDock> presentations;
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
			if (!hymnal || !bible || !presentations)
				return;
			steps.at(index).run();
			runNext(index + 1);
		});
	}

	void waitStep(int index, int waitedMs)
	{
		QTimer::singleShot(250, [this, index, waitedMs]() {
			if (!hymnal || !bible || !presentations)
				return;
			if (steps.at(index).until() || waitedMs + 250 >= steps.at(index).delayMs)
				runNext(index + 1);
			else
				waitStep(index, waitedMs + 250);
		});
	}
};
} // namespace

void RunWorshipSelfTest(PresentationCatalogDock *hymnal, BibleDock *bible, PresentationDock *presentations,
			const QString &outputDir, std::function<void()> finished)
{
	auto *test = new SelfTest();
	test->hymnal = hymnal;
	test->bible = bible;
	test->presentations = presentations;
	QDir().mkpath(outputDir);
	test->outDir = QDir(outputDir);
	test->finished = [test, finished]() {
		if (finished)
			finished();
		delete test;
	};

	test->log(QStringLiteral("=== WorshipStudio autoprueba, lienzo %1x%2 ===")
			  .arg(WorshipOutput::CanvasWidth())
			  .arg(WorshipOutput::CanvasHeight()));

	const QStringList presentationFiles =
		qEnvironmentVariable("WORSHIPSTUDIO_SELFTEST_PRESENTATIONS").split(QLatin1Char(';'), Qt::SkipEmptyParts);
	for (int i = 0; i < presentationFiles.size(); ++i)
		test->addPresentationSteps(presentationFiles.at(i), i + 1);
	if (qEnvironmentVariable("WORSHIPSTUDIO_SELFTEST_ONLY") == QStringLiteral("presentaciones")) {
		test->runNext(0);
		return;
	}

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

	test->addBibleStep(QStringLiteral("Juan 3:16"), 0, QStringLiteral("biblia-juan3-16"));
	test->addBibleStep(QStringLiteral("1 Cor 13:4-7"), 0, QStringLiteral("biblia-1cor13-4-7"));
	test->addBibleStep(QStringLiteral("Sal 23:1-6"), 0, QStringLiteral("biblia-salmo23-completo"));
	test->addBibleStep(QStringLiteral("Romanos 8:31-39"), 0, QStringLiteral("biblia-romanos8-largo"));
	test->addBibleStep(QStringLiteral("Jueces 1:1"), 0, QStringLiteral("biblia-jueces1-mayusculas"));
	test->addBibleStep(QStringLiteral("Juan 3:16"), 1, QStringLiteral("biblia-juan3-16-tercio"));
	test->addBibleStep(QStringLiteral("porque de tal manera amo"), 0, QStringLiteral("biblia-busqueda-frase"));

	/* Every bundled version shows the same passage. */
	for (const QString &abbrev : bible->versionAbbrevs()) {
		test->add(200, [test, abbrev]() {
			test->check(test->bible->selectVersion(abbrev), QStringLiteral("version %1 seleccionada").arg(abbrev));
		});
		test->addBibleStep(abbrev == QStringLiteral("KJV") ? QStringLiteral("John 3:16") : QStringLiteral("Juan 3:16"),
				   0, QStringLiteral("version-%1").arg(abbrev.toLower()));
	}
	test->add(200, [test]() { test->bible->selectVersion(QStringLiteral("RV1909")); });

	/* Fonts and backgrounds. */
	const QStringList fonts = WorshipRender::RecommendedFonts();
	const QStringList presets = WorshipRender::BackgroundPresetNames();
	for (int i = 0; i < presets.size(); ++i) {
		const QString font = fonts.value(i % qMax(1, (int)fonts.size()));
		test->add(200, [test, font, i]() {
			test->log(QStringLiteral("estilo: fondo %1, letra %2").arg(i).arg(font));
			test->bible->setStyle(font, i);
		});
		test->addBibleStep(QStringLiteral("Sal 46:1-3"), 0,
				   QStringLiteral("estilo-%1-%2").arg(i).arg(QString(font).replace(QLatin1Char(' '), QLatin1Char('_'))));
	}
	test->add(200, [test, fonts]() { test->bible->setStyle(fonts.value(0), 0); });

	test->runNext(0);
}
