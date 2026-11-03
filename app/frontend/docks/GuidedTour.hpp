#pragma once

#include <QPointer>
#include <QVector>
#include <QWidget>

#include <functional>
#include <vector>

class QLabel;
class QMainWindow;
class QPushButton;
class QTimer;

/* Interactive first-run tour: dims the real window, leaves a hole over the
 * button or panel of each step and shows a bubble saying what to do. Clicks
 * go through the hole, so people do each step for real; when the step is
 * done (a scene created, a song on hold...) the tour moves on by itself. */
class GuidedTour : public QWidget {
	Q_OBJECT

public:
	explicit GuidedTour(QMainWindow *window);

	/* Starts the tour the first time WorshipStudio opens. */
	static void showOnFirstRun(QMainWindow *window);
	static GuidedTour *start(QMainWindow *window);

	int stepCount() const { return (int)steps.size(); }
	int currentStep() const { return current; }
	void goTo(int index);
	/* For the self-test: the step points at a real, visible widget. */
	bool targetVisible() const;
	QWidget *bubbleWidget() const;
	/* For the guide videos: the lit areas, in window coordinates. */
	QVector<QRect> litAreas() const;

protected:
	void paintEvent(QPaintEvent *event) override;
	bool eventFilter(QObject *watched, QEvent *event) override;

private:
	struct Step {
		QString title;
		QString text;
		std::function<QWidget *()> target;
		std::function<void()> prepare;
		std::function<bool()> done;
		QWidget *extra = nullptr;
		/* Tabbed dock whose tab button is lit too, so people see where to click. */
		const char *dock = nullptr;
	};

	QPointer<QMainWindow> window;
	std::vector<Step> steps;
	int current = -1;
	bool finishedStep = false;
	QVector<QRect> holes;

	QWidget *bubble = nullptr;
	QLabel *counterLabel = nullptr;
	QLabel *titleLabel = nullptr;
	QLabel *textLabel = nullptr;
	QLabel *doneLabel = nullptr;
	QWidget *extraSlot = nullptr;
	QPushButton *backButton = nullptr;
	QPushButton *nextButton = nullptr;
	QTimer *timer = nullptr;

	/* Baselines to notice what the person did during a step. */
	int sceneCount = 0;
	int sourceCount = 0;
	int songCount = 0;
	QString programScene;
	QString previewScene;

	void buildSteps();
	void refresh();
	void placeBubble();
	void snapshot();
	void finish();
	void scrollIntoView(QWidget *target);
	bool shouldShow() const;
};
