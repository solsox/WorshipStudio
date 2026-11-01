#pragma once

#include <QString>

#include <functional>

class BibleDock;
class PresentationCatalogDock;
class PresentationDock;
class SongsDock;

/* Automated check of the hymnal and Bible projection. Started when the
 * WORSHIPSTUDIO_SELFTEST environment variable points to an output folder;
 * writes captures of the real OBS output plus reporte.txt there.
 * WORSHIPSTUDIO_SELFTEST_PRESENTATIONS lists files (separated by ';') to
 * open in the Presentaciones dock. WORSHIPSTUDIO_SELFTEST_ONLY limits the
 * run to some sections, comma separated: himnario, biblia, cantos,
 * presentaciones. */
void RunWorshipSelfTest(PresentationCatalogDock *hymnal, BibleDock *bible, SongsDock *songs,
			PresentationDock *presentations, const QString &outputDir, std::function<void()> finished);
