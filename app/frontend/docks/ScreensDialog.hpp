#pragma once

#include <QDialog>

class QButtonGroup;
class QCheckBox;
class QRadioButton;
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

	enum class StreamLayout { CameraWithCorner = 0, ScreenWithCamera = 1, SideBySide = 2, ScreenOnly = 3, CameraOnly = 4 };

	/* "● PROYECTOR ..." and "● TRANSMISIÓN ...": the same names and colors everywhere. */
	static QString ProjectorChip(const QString &text);
	static QString StreamChip(const QString &text);
	/* Live picture of the room screen, for the En vivo panel. */
	static QWidget *CreateRoomMonitor(QWidget *parent);

	/* Creates the stream scenes "Solo cámara" and "Cámara + proyector" (or
	 * "Transmisión" without a camera) when they do not exist yet; existing
	 * scenes are never changed. `firstScene` is the one to put on air. */
	static bool CreateStreamScenes(StreamLayout layout, const QString &cameraName, QString *firstScene = nullptr);
	/* Opens the room screen full screen on a display (-1: in a window). */
	static void OpenRoomScreen(int monitor);
	/* Reopens the room screen after a restart, when it was configured. */
	static void RestoreRoomScreen();
	/* Video capture sources (cameras) that exist in the collection. */
	static QStringList Cameras();

private:
	QRadioButton *sameRadio = nullptr;
	QRadioButton *separateRadio = nullptr;
	QCheckBox *createCheck = nullptr;
	QComboBox *monitorCombo = nullptr;
	QComboBox *cameraCombo = nullptr;
	QButtonGroup *layoutGroup = nullptr;
	QWidget *details = nullptr;
	QLabel *status = nullptr;

	void apply();
};
