#pragma once

#include "OBSDock.hpp"

#include <obs.hpp>
#include <obs-frontend-api.h>

#include <QDateTime>

class BackgroundPicker;
class QComboBox;
class QLabel;
class QLineEdit;
class QPushButton;
class QSpinBox;
class QTimeEdit;
class QTimer;
class WorshipRemote;

/* Everything that happens during the service besides the content itself:
 * what is on air, countdown, name and urgent message overlays, the stage
 * display and the phone remote. */
class LiveToolsDock : public OBSDock {
	Q_OBJECT

public:
	LiveToolsDock(const QString &objectName, const QString &title, QWidget *parent = nullptr);
	~LiveToolsDock() override;

	/* Used by the service order and the self test. */
	void startCountdown(int minutes, const QString &title);
	void stopCountdown();
	bool countdownRunning() const { return countdownActive; }
	void showName(const QString &name, const QString &role);
	void hideName();
	void showUrgentMessage(const QString &text);
	void hideUrgentMessage();
	obs_scene_t *countdownScene() const { return countdownSceneRef; }
	obs_scene_t *stageScene() const { return stageSceneRef; }
	obs_scene_t *overlayScene() const { return overlaySceneRef; }
	WorshipRemote *remote() const { return remoteServer; }
	void setRemoteEnabled(bool enabled);
	void refreshStage();
	/* Creates the WS Escenario scene (done when opening the projector). */
	void ensureStage();

private:
	/* On air */
	QLabel *nowTitle = nullptr;
	QLabel *nowText = nullptr;
	QLabel *nextText = nullptr;
	QPushButton *hideButton = nullptr;

	/* Countdown */
	QComboBox *countdownMode = nullptr;
	QSpinBox *countdownMinutes = nullptr;
	QTimeEdit *countdownTime = nullptr;
	QLineEdit *countdownTitle = nullptr;
	QLineEdit *countdownEnd = nullptr;
	BackgroundPicker *countdownBackground = nullptr;
	QPushButton *countdownButton = nullptr;
	QDateTime countdownTarget;
	bool countdownActive = false;
	int countdownCounter = 0;

	/* Overlays */
	QLineEdit *nameEdit = nullptr;
	QLineEdit *roleEdit = nullptr;
	QPushButton *nameButton = nullptr;
	QLineEdit *messageEdit = nullptr;
	QPushButton *messageButton = nullptr;
	bool nameVisible = false;
	bool messageVisible = false;

	/* Stage display */
	QLineEdit *stageMessageEdit = nullptr;
	QPushButton *stopwatchButton = nullptr;
	QDateTime stopwatchStart;
	int stageCounter = 0;

	/* Phone remote */
	QPushButton *remoteButton = nullptr;
	QLabel *remoteInfo = nullptr;
	QLabel *remoteQr = nullptr;
	WorshipRemote *remoteServer = nullptr;

	QTimer *clockTimer = nullptr;

	OBSScene countdownSceneRef;
	OBSSource countdownSource;
	OBSScene stageSceneRef;
	OBSSource stageSource;
	OBSScene overlaySceneRef;
	OBSSource nameSource;
	OBSSource messageSource;

	void onLiveChanged();
	void onTick();
	void renderCountdown();
	void ensureOverlays();
	void attachOverlays();
	void setOverlayVisible(const char *sourceName, bool visible);
	void openStageProjector();
	void updateRemoteInfo();
	static void onFrontendEvent(enum obs_frontend_event event, void *data);
};
