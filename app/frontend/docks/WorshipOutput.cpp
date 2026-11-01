#include "WorshipOutput.hpp"

#include <OBSApp.hpp>

#include <obs-frontend-api.h>
#include <qt-wrappers.hpp>

namespace WorshipOutput {

static constexpr const char *ConfigSection = "WorshipStudio";

OBSScene EnsureScene(const char *sceneName)
{
	OBSSourceAutoRelease existing = obs_get_source_by_name(sceneName);
	if (existing) {
		obs_scene_t *scene = obs_scene_from_source(existing);
		if (scene)
			return OBSScene(scene);
	}

	/* The frontend picks up new scenes through the source_create signal and
	 * keeps its own reference, so it is safe to release ours afterwards. */
	OBSSceneAutoRelease scene = obs_scene_create(sceneName);
	return OBSScene(scene.Get());
}

OBSSource EnsureSource(obs_scene_t *scene, const char *sourceName, const char *sourceId, obs_data_t *settings,
		       bool *created)
{
	if (created)
		*created = false;

	obs_sceneitem_t *item = obs_scene_find_source(scene, sourceName);
	if (item)
		return OBSSource(obs_sceneitem_get_source(item));

	OBSSourceAutoRelease source = obs_get_source_by_name(sourceName);
	if (!source) {
		source = obs_source_create(sourceId, sourceName, settings, nullptr);
		if (!source)
			return OBSSource();
	}

	obs_scene_add(scene, source);
	if (created)
		*created = true;
	return OBSSource(source.Get());
}

void FitItem(obs_scene_t *scene, obs_source_t *source, float x, float y, float width, float height,
	     uint32_t alignment, obs_bounds_type boundsType)
{
	obs_sceneitem_t *item = obs_scene_find_source(scene, obs_source_get_name(source));
	if (!item)
		return;

	const float canvasW = (float)CanvasWidth();
	const float canvasH = (float)CanvasHeight();

	vec2 pos;
	vec2_set(&pos, canvasW * x, canvasH * y);
	vec2 bounds;
	vec2_set(&bounds, canvasW * width, canvasH * height);

	obs_sceneitem_defer_update_begin(item);
	obs_sceneitem_set_alignment(item, OBS_ALIGN_LEFT | OBS_ALIGN_TOP);
	obs_sceneitem_set_pos(item, &pos);
	obs_sceneitem_set_bounds_type(item, boundsType);
	obs_sceneitem_set_bounds_alignment(item, alignment);
	obs_sceneitem_set_bounds(item, &bounds);
	obs_sceneitem_defer_update_end(item);
}

void ShowScene(obs_scene_t *scene)
{
	obs_source_t *source = obs_scene_get_source(scene);
	if (obs_frontend_preview_program_mode_active())
		obs_frontend_set_current_preview_scene(source);
	else
		obs_frontend_set_current_scene(source);
}

uint32_t CanvasWidth()
{
	obs_video_info ovi;
	return obs_get_video_info(&ovi) ? ovi.base_width : 1920;
}

uint32_t CanvasHeight()
{
	obs_video_info ovi;
	return obs_get_video_info(&ovi) ? ovi.base_height : 1080;
}

QString ConfigString(const char *name)
{
	const char *value = config_get_string(App()->GetUserConfig(), ConfigSection, name);
	return value ? QT_UTF8(value) : QString();
}

void SetConfigString(const char *name, const QString &value)
{
	config_set_string(App()->GetUserConfig(), ConfigSection, name, QT_TO_UTF8(value));
	config_save_safe(App()->GetUserConfig(), "tmp", nullptr);
}

} // namespace WorshipOutput
