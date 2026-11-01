#pragma once

#include <QColor>
#include <QDialog>

class QButtonGroup;
class QLabel;
class QPushButton;

/* Accent color and light or dark mode for the whole interface. Changes are
 * applied right away so the result can be seen before closing. */
class AppearanceDialog : public QDialog {
	Q_OBJECT

public:
	explicit AppearanceDialog(QWidget *parent = nullptr);

	/* Saved accent (the brand ember by default), also used by the phone page. */
	static QColor savedAccent();
	static bool savedLight();

private:
	QColor accent;
	bool light = false;
	QPushButton *darkButton = nullptr;
	QPushButton *lightButton = nullptr;
	QLabel *status = nullptr;
	QList<QPushButton *> swatches;

	void apply();
	void refreshSelection();
};
