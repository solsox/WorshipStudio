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

public slots:
	/* Studio Mode: sends what is waiting in the preview to the screen. */
	void goLive();
	/* Full screen projector of what the congregation sees. */
	void openProgramProjector();

public:
	void runHotkey(obs_hotkey_id id);

private:
	/* On air */
	QLabel *programLabel = nullptr;
	QLabel *previewLabel = nullptr;
	QLabel *nowText = nullptr;
	QLabel *nextText = nullptr;
	QPushButton *hideButton = nullptr;
	QPushButton *goLiveButton = nullptr;
	QPushButton *projectorButton = nullptr;

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

	obs_hotkey_id nextHotkey = OBS_INVALID_HOTKEY_ID;
	obs_hotkey_id previousHotkey = OBS_INVALID_HOTKEY_ID;
	obs_hotkey_id hideHotkey = OBS_INVALID_HOTKEY_ID;
	obs_hotkey_id goLiveHotkey = OBS_INVALID_HOTKEY_ID;
	void registerHotkeys();
	void saveHotkeys();

	OBSScene countdownSceneRef;
	OBSSource countdownSource;
	OBSScene stageSceneRef;
	OBSSource stageSource;
	OBSScene overlaySceneRef;
	OBSSource nameSource;
	OBSSource messageSource;

	void onLiveChanged();
	void refreshOnAir();
	QString describeScene(obs_source_t *sceneSource) const;
	void onTick();
	void renderCountdown();
	void ensureOverlays();
	void attachOverlays();
	void setOverlayVisible(const char *sourceName, bool visible);
	void openStageProjector();
	void updateRemoteInfo();
	static void onFrontendEvent(enum obs_frontend_event event, void *data);
};
