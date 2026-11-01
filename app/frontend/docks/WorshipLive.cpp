#include "WorshipLive.hpp"

#include "moc_WorshipLive.cpp"

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
	QObject *target = targets.value(current.source);
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
