#include "WorshipLive.hpp"
#include "WorshipOutput.hpp"

#include <obs-frontend-api.h>
#include <obs.hpp>

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

QString ownerOfScene(obs_source_t *scene)
{
	return scene ? ownerOfName(QString::fromUtf8(obs_source_get_name(scene))) : QString();
}
} // namespace

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
	emit changed();
}

void WorshipLive::clear(const QString &source)
{
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
	/* Next/previous drive what the congregation sees. In Studio Mode, content
	 * that was just chosen and is still waiting in the preview is prepared
	 * first; otherwise the dock that owns the scene on screen is used. */
	OBSSourceAutoRelease program = obs_frontend_get_current_scene();
	QString source = ownerOfScene(program);
	if (WorshipOutput::SeparateRoomScreen()) {
		source = ownerOfName(WorshipOutput::OnAirSceneName());
	} else if (obs_frontend_preview_program_mode_active()) {
		OBSSourceAutoRelease preview = obs_frontend_get_current_preview_scene();
		const QString waiting = ownerOfScene(preview);
		if (!waiting.isEmpty() && waiting == current.source && preview != program)
			source = waiting;
	}
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
