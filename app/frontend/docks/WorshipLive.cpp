#include "WorshipLive.hpp"
#include "WorshipOutput.hpp"

#include <obs-frontend-api.h>
#include <obs.hpp>

#include <QVector>

#include "moc_WorshipLive.cpp"

namespace {
/* "WS Presentacion B" -> "Presentaciones": which dock owns a scene. */
QString ownerOfName(const QString &name)
{
	if (name.startsWith(QStringLiteral("WS Cantos")))
		return QStringLiteral("Cantos");
	if (name.startsWith(QStringLiteral("WS Biblia")))
		return QStringLiteral("Biblia");
	if (name.startsWith(QStringLiteral("WS Himnario")))
		return QStringLiteral("Himnario");
	if (name.startsWith(QStringLiteral("WS Presentacion")))
		return QStringLiteral("Presentaciones");
	return {};
}

/* The panel a scene shows: its own scenes ("WS Biblia B"), or a scene of the
 * user with "Biblia en vivo" inside (nested scenes and groups too). The
 * topmost visible one wins, as that is what people see. */
QString ownerOfScene(obs_source_t *scene, int depth = 0)
{
	if (!scene || depth > 8)
		return {};
	const QString name = QString::fromUtf8(obs_source_get_name(scene));
	QString owner = ownerOfName(name);
	if (!owner.isEmpty())
		return owner;
	const QStringList followers = WorshipOutput::FollowerNames();
	const int follower = followers.indexOf(name);
	if (follower >= 0)
		return QStringList{QStringLiteral("Cantos"), QStringLiteral("Biblia"), QStringLiteral("Himnario"),
				   QStringLiteral("Presentaciones")}
			.value(follower);
	obs_scene_t *s = obs_scene_from_source(scene);
	if (!s)
		s = obs_group_from_source(scene);
	if (!s)
		return {};
	QVector<OBSSource> visible;
	obs_scene_enum_items(
		s,
		[](obs_scene_t *, obs_sceneitem_t *item, void *param) {
			if (obs_sceneitem_visible(item))
				static_cast<QVector<OBSSource> *>(param)->push_back(OBSSource(obs_sceneitem_get_source(item)));
			return true;
		},
		&visible);
	for (int i = (int)visible.size() - 1; i >= 0; --i) {
		owner = ownerOfScene(visible.at(i), depth + 1);
		if (!owner.isEmpty())
			return owner;
	}
	return {};
}
} // namespace

QString WorshipLive::activeSource() const
{
	/* With the projector separate, what the church sees is driven; otherwise
	 * the screen, or in Studio Mode the content waiting in the preview. */
	if (WorshipOutput::SeparateRoomScreen()) {
		const QString onAir = WorshipOutput::OnAirSceneName();
		OBSSourceAutoRelease scene = onAir.isEmpty() ? nullptr : obs_get_source_by_name(onAir.toUtf8().constData());
		return ownerOfScene(scene);
	}
	OBSSourceAutoRelease program = obs_frontend_get_current_scene();
	QString source = ownerOfScene(program);
	if (obs_frontend_preview_program_mode_active()) {
		OBSSourceAutoRelease preview = obs_frontend_get_current_preview_scene();
		const QString waiting = ownerOfScene(preview);
		if (!waiting.isEmpty() && waiting == current.source && preview != program)
			source = waiting;
	}
	return source;
}

LiveState WorshipLive::state() const
{
	const QString source = activeSource();
	return source.isEmpty() ? current : states.value(source, current);
}

WorshipLive &WorshipLive::instance()
{
	static WorshipLive live;
	return live;
}

void WorshipLive::registerTarget(const QString &source, QObject *dock)
{
	targets.insert(source, dock);
}

void WorshipLive::publish(const LiveState &state)
{
	current = state;
	states.insert(state.source, state);
	emit changed();
}

void WorshipLive::clear(const QString &source)
{
	states.remove(source);
	if (current.source != source)
		return;
	current = LiveState();
	emit changed();
}

void WorshipLive::setStageMessage(const QString &text)
{
	if (message == text)
		return;
	message = text;
	emit stageMessageChanged();
}

void WorshipLive::setService(const QStringList &items, int currentItem)
{
	service = items;
	serviceIndex = currentItem;
	emit serviceChanged();
}

void WorshipLive::invoke(const char *method)
{
	/* Next/previous drive the panel whose content people see; when nothing
	 * of a panel is on screen, the last one used. */
	QString source = activeSource();
	if (source.isEmpty())
		source = current.source;

	QObject *target = targets.value(source);
	if (target)
		QMetaObject::invokeMethod(target, method, Qt::QueuedConnection);
}

void WorshipLive::next()
{
	invoke("nextSlide");
}

void WorshipLive::previous()
{
	invoke("previousSlide");
}

void WorshipLive::toggleHidden()
{
	invoke("toggleHidden");
}
