#include "WorshipOutput.hpp"
#include "WorshipLive.hpp"

#include <OBSApp.hpp>

#include <obs-frontend-api.h>
#include <qt-wrappers.hpp>

namespace WorshipOutput {

static constexpr const char *ConfigSection = "WorshipStudio";
static int sourceOnlyDepth = 0;

namespace {
/* The visible content scene inside a follower scene. */
QString visibleIn(const char *followerName)
{
	OBSSourceAutoRelease follower = obs_get_source_by_name(followerName);
	obs_scene_t *scene = follower ? obs_scene_from_source(follower) : nullptr;
	if (!scene)
		return {};
	QString name;
	obs_scene_enum_items(
		scene,
		[](obs_scene_t *, obs_sceneitem_t *item, void *param) {
			if (obs_sceneitem_visible(item))
				*static_cast<QString *>(param) =
					QT_UTF8(obs_source_get_name(obs_sceneitem_get_source(item)));
			return true;
		},
		&name);
	return name;
}

/* Shows `content` (a scene) in the follower scene, with a short fade, and
 * hides the rest. */
void follow(const char *followerName, obs_source_t *content)
{
	OBSScene follower = EnsureScene(followerName);
	if (!follower || !content || obs_scene_get_source(follower) == content)
		return;
	const char *contentName = obs_source_get_name(content);
	obs_sceneitem_t *target = obs_scene_find_source(follower, contentName);
	if (!target) {
		target = obs_scene_add(follower, content);
		if (!target)
			return;
		FitItem(follower, content, 0.0f, 0.0f, 1.0f, 1.0f);
		for (bool show : {true, false}) {
			OBSSourceAutoRelease fade = obs_source_create_private("fade_transition", "WS Fundido", nullptr);
			obs_sceneitem_set_transition(target, show, fade);
			obs_sceneitem_set_transition_duration(target, show, 350);
		}
	}
	obs_sceneitem_set_order(target, OBS_ORDER_MOVE_TOP);
	obs_scene_enum_items(
		follower,
		[](obs_scene_t *, obs_sceneitem_t *item, void *param) {
			obs_sceneitem_set_visible(item, item == static_cast<obs_sceneitem_t *>(param));
			return true;
		},
		target);
}
} // namespace

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

	const QString onAir = OnAirSceneName();
	/* Used as a source: never change what the congregation is seeing. */
	if (sourceOnlyDepth > 0)
		return onAir == base ? QStringLiteral(" B") : QString();

	if (!obs_frontend_preview_program_mode_active() || SeparateRoomScreen())
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

SourceOnlyScope::SourceOnlyScope()
{
	++sourceOnlyDepth;
}

SourceOnlyScope::~SourceOnlyScope()
{
	--sourceOnlyDepth;
}

QStringList FollowerNames()
{
	return {QStringLiteral("Cantos en vivo"), QStringLiteral("Biblia en vivo"), QStringLiteral("Himnario en vivo"),
		QStringLiteral("Presentación en vivo")};
}

QString FollowerFor(const QString &name)
{
	if (name.startsWith(QStringLiteral("WS Cantos")))
		return FollowerNames().at(0);
	if (name.startsWith(QStringLiteral("WS Biblia")))
		return FollowerNames().at(1);
	if (name.startsWith(QStringLiteral("WS Himnario")))
		return FollowerNames().at(2);
	if (name.startsWith(QStringLiteral("WS Presentacion")))
		return FollowerNames().at(3);
	return {};
}

bool SeparateRoomScreen()
{
	return ConfigString("SeparateRoomScreen") == QStringLiteral("1");
}

void SetSeparateRoomScreen(bool separate)
{
	SetConfigString("SeparateRoomScreen", separate ? QStringLiteral("1") : QStringLiteral("0"));
	if (separate)
		EnsureScene(RoomScreenScene);
	/* Panels and labels that name SALA / EN VIVO refresh. */
	QMetaObject::invokeMethod(&WorshipLive::instance(), "changed", Qt::QueuedConnection);
}

QString OnAirSceneName()
{
	if (SeparateRoomScreen())
		return visibleIn(RoomScreenScene);
	OBSSourceAutoRelease program = obs_frontend_get_current_scene();
	return program ? QT_UTF8(obs_source_get_name(program)) : QString();
}

bool IsInternalScene(const QString &name)
{
	return name.startsWith(QStringLiteral("WS ")) || name == QLatin1String(RoomScreenScene) ||
	       FollowerNames().contains(name);
}

void ShowScene(obs_scene_t *scene)
{
	obs_source_t *source = obs_scene_get_source(scene);
	const QString name = QT_UTF8(obs_source_get_name(source));
	const QString follower = FollowerFor(name);
	if (!follower.isEmpty())
		follow(QT_TO_UTF8(follower), source);
	if (sourceOnlyDepth > 0)
		return;
	if (SeparateRoomScreen()) {
		follow(RoomScreenScene, source);
		return;
	}

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
