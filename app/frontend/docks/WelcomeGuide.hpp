#pragma once

#include <QDialog>
#include <QPointer>

class LiveToolsDock;
class QCheckBox;
class QLabel;
class QPushButton;
class QStackedWidget;

/* First run guide: what each part of the window does, in six short steps,
 * each with a diagram of the window that highlights the part it explains. */
class WelcomeGuide : public QDialog {
	Q_OBJECT

public:
	WelcomeGuide(LiveToolsDock *liveTools, QWidget *parent = nullptr);

	/* Shows the guide the first time WorshipStudio opens. */
	static void showOnFirstRun(LiveToolsDock *liveTools, QWidget *parent);

private:
	QPointer<LiveToolsDock> liveTools;
	QStackedWidget *pages = nullptr;
	QLabel *stepLabel = nullptr;
	QPushButton *backButton = nullptr;
	QPushButton *nextButton = nullptr;
	QCheckBox *studioCheck = nullptr;

	void addPage(int region, const QString &title, const QString &text, QWidget *extra = nullptr);
	void go(int delta);
};
