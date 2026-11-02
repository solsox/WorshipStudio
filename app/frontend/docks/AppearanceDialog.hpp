#pragma once

#include <QColor>
#include <QDialog>
#include <QMap>

class QButtonGroup;
class QHBoxLayout;
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
	/* Background shades of the saved base ("ciruela", "mate", "blanco"...). */
	static QString savedBase();
	static QMap<QString, QString> baseVariables(const QString &id);

private:
	QColor accent;
	bool light = false;
	QPushButton *darkButton = nullptr;
	QPushButton *lightButton = nullptr;
	QString base;
	QLabel *status = nullptr;
	QHBoxLayout *baseRow = nullptr;
	QList<QPushButton *> baseButtons;
	QList<QPushButton *> swatches;

	void rebuildBases();
	void apply();
	void refreshSelection();
};
