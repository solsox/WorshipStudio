#pragma once

#include "SongLibrary.hpp"

#include <QComboBox>
#include <QDialog>

class QCheckBox;
class QLabel;
class QLineEdit;
class QListWidget;
class QPlainTextEdit;
class QSpinBox;
class QTimer;

/* Combo with the built-in backgrounds plus color, image and (optionally)
 * video. Remembers custom choices so they stay selectable. */
class BackgroundPicker : public QComboBox {
	Q_OBJECT

public:
	BackgroundPicker(bool allowVideo, bool allowInherit, QWidget *parent = nullptr);

	/* `inherit` is true when "(igual que el canto)" is selected. */
	WorshipRender::Background background(bool *inherit = nullptr) const;
	void setBackground(const WorshipRender::Background &background, bool inherit = false);

signals:
	void backgroundChanged();

private:
	bool allowVideo;
	bool allowInherit;
	WorshipRender::Background custom;
	bool hasCustom = false;
	int lastIndex = 0;

	void rebuild();
	void onActivated(int index);
};

class SongEditor : public QDialog {
	Q_OBJECT

public:
	SongEditor(const Song &song, QWidget *parent = nullptr);

	Song song() const;

private:
	Song edited;

	QLineEdit *titleEdit = nullptr;
	QLineEdit *authorEdit = nullptr;
	QLineEdit *copyrightEdit = nullptr;
	QPlainTextEdit *textEdit = nullptr;
	QComboBox *fontCombo = nullptr;
	QSpinBox *sizeSpin = nullptr;
	QCheckBox *uppercaseCheck = nullptr;
	QCheckBox *titleCheck = nullptr;
	QComboBox *layoutCombo = nullptr;
	BackgroundPicker *songBackground = nullptr;
	BackgroundPicker *slideBackground = nullptr;
	QListWidget *slidesList = nullptr;
	QLabel *previewLabel = nullptr;
	QTimer *refreshTimer = nullptr;

	void readForm();
	void refreshSlides();
	void refreshPreview();
	void onSlideSelected();
	void onSlideBackgroundChanged();
};
