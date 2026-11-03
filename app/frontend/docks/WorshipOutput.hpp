#pragma once

#include <obs.hpp>

#include <QImage>
#include <QString>

#include <string>

/* Helpers shared by the WorshipStudio docks to project content through
 * regular OBS scenes and sources, so everything stays editable by the user
 * (filters, transforms, transitions, multiview, streaming, recording). */
namespace WorshipOutput {

/* Returns the scene with the given name, creating it if needed. */
OBSScene EnsureScene(const char *sceneName);

/* Returns the source with the given name inside the scene, creating it
 * with the given id and settings if needed. When created, `created` is set
 * to true so the caller can apply an initial layout. */
OBSSource EnsureSource(obs_scene_t *scene, const char *sourceName, const char *sourceId, obs_data_t *settings,
		       bool *created = nullptr);

/* Fits the scene item into a box (relative to the canvas) using bounds. */
void FitItem(obs_scene_t *scene, obs_source_t *source, float x, float y, float width, float height,
	     uint32_t alignment = OBS_ALIGN_CENTER, obs_bounds_type boundsType = OBS_BOUNDS_SCALE_INNER);

/* Every WorshipStudio scene has two copies ("WS Cantos" and "WS Cantos B").
 * New content goes to the copy that is not on air, so in Studio Mode it
 * waits in the preview until the transition instead of jumping to the
 * screen. Returns the suffix to use: "" or " B". */
QString OutputSlot(const char *baseSceneName);

/* Name of a scene or source for a slot: Named("WS Cantos", " B"). */
std::string Named(const char *baseName, const QString &slot);

/* Shows the scene: in Studio Mode it goes to preview, otherwise to program.
 * With a separate room screen it goes to the room screen and the stream
 * keeps its own scene. The panel's "en vivo" scene always follows. */
void ShowScene(obs_scene_t *scene);

/* While alive, ShowScene only updates the "en vivo" scenes: content dropped
 * on the Sources panel is used inside a scene without changing the screen. */
class SourceOnlyScope {
public:
	SourceOnlyScope();
	~SourceOnlyScope();
};

/* Scenes that always show what a panel is showing ("Cantos en vivo"), to
 * use as sources inside any scene. Empty for scenes of no panel. */
QString FollowerFor(const QString &contentSceneName);
QStringList FollowerNames();

/* The room screen ("Pantalla de la sala") shows the content alone while the
 * stream shows its own scene, for example the camera with the content
 * small in a corner. */
constexpr const char *RoomScreenScene = "Pantalla de la sala";
bool SeparateRoomScreen();
void SetSeparateRoomScreen(bool separate);
/* Name of the content scene the congregation sees right now. */
QString OnAirSceneName();

/* WorshipStudio's own scenes, hidden from the scene list. */
bool IsInternalScene(const QString &name);

/* Picture of a source (usually a scene) at the given size, like OBS
 * screenshots. Must run on the UI thread. */
QImage RenderSource(obs_source_t *source, uint32_t width, uint32_t height);

uint32_t CanvasWidth();
uint32_t CanvasHeight();

QString ConfigString(const char *name);
void SetConfigString(const char *name, const QString &value);

} // namespace WorshipOutput
