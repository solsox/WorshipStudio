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

QString OutputSlot(const char *baseSceneName)
{
	const QString base = QString::fromUtf8(baseSceneName);
	const QString second = base + QStringLiteral(" B");

	OBSSourceAutoRelease program = obs_frontend_get_current_scene();
	const QString onAir = program ? QT_UTF8(obs_source_get_name(program)) : QString();

	if (!obs_frontend_preview_program_mode_active())
		return onAir == second ? QStringLiteral(" B") : QString();

	if (onAir == base)
		return QStringLiteral(" B");
	if (onAir == second)
		return QString();

	/* Neither copy is on air: keep using the one already in the preview. */
	OBSSourceAutoRelease preview = obs_frontend_get_current_preview_scene();
	const QString waiting = preview ? QT_UTF8(obs_source_get_name(preview)) : QString();
	return waiting == second ? QStringLiteral(" B") : QString();
}

std::string Named(const char *baseName, const QString &slot)
{
	return std::string(baseName) + slot.toStdString();
}

void ShowScene(obs_scene_t *scene)
{
	obs_source_t *source = obs_scene_get_source(scene);
	const bool studio = obs_frontend_preview_program_mode_active();

	/* Already there: a transition to the same scene would restart its
	 * media sources (hymn audio) for nothing. */
	OBSSourceAutoRelease current = studio ? obs_frontend_get_current_preview_scene()
					      : obs_frontend_get_current_scene();
	if (current == source)
		return;

	if (studio)
		obs_frontend_set_current_preview_scene(source);
	else
		obs_frontend_set_current_scene(source);
}

QImage RenderSource(obs_source_t *source, uint32_t width, uint32_t height)
{
	QImage image;
	if (!source || !width || !height)
		return image;

	obs_enter_graphics();
	gs_texrender_t *texrender = gs_texrender_create(GS_RGBA, GS_ZS_NONE);
	gs_stagesurf_t *stagesurf = gs_stagesurface_create(width, height, GS_RGBA);
	if (texrender && stagesurf && gs_texrender_begin(texrender, width, height)) {
		vec4 clear;
		vec4_zero(&clear);
		gs_clear(GS_CLEAR_COLOR, &clear, 0.0f, 0);
		/* Canvas coordinates, scaled down to the requested size. */
		gs_ortho(0.0f, (float)CanvasWidth(), 0.0f, (float)CanvasHeight(), -100.0f, 100.0f);
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
