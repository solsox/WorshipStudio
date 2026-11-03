#pragma once

#include <QDialog>

class QButtonGroup;
class QCheckBox;
class QComboBox;
class QLabel;
class QWidget;

/* "Pantallas y transmisión": the room screen shows the content alone (full
 * screen lyrics, Bible...) and the stream shows its own scene, for example
 * the camera with the same content small in a corner, as in professional
 * broadcasts. */
class ScreensDialog : public QDialog {
	Q_OBJECT

public:
	explicit ScreensDialog(QWidget *parent = nullptr);

	enum class StreamLayout { CameraWithCorner = 0, ScreenWithCamera = 1, SideBySide = 2, ScreenOnly = 3 };

	/* Builds (or rebuilds) the "Transmisión" scene; returns false without a
	 * camera when one is needed. Also used by the self test. */
	static bool BuildStreamScene(StreamLayout layout, const QString &cameraName, QString *error = nullptr);
	/* Opens the room screen full screen on a display (-1: in a window). */
	static void OpenRoomScreen(int monitor);
	/* Reopens the room screen after a restart, when it was configured. */
	static void RestoreRoomScreen();
	/* Video capture sources (cameras) that exist in the collection. */
	static QStringList Cameras();

private:
	QCheckBox *separateCheck = nullptr;
	QComboBox *monitorCombo = nullptr;
	QComboBox *cameraCombo = nullptr;
	QButtonGroup *layoutGroup = nullptr;
	QWidget *details = nullptr;
	QLabel *status = nullptr;

	void apply();
};
