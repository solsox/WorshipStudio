#pragma once

#include <obs.hpp>

#include <QDialog>
#include <QJsonArray>
#include <QJsonObject>
#include <QString>

#include <functional>

class QLabel;
class QLineEdit;
class QListWidget;
class QNetworkAccessManager;
class QPushButton;
class QSlider;
class QTcpServer;
class QTcpSocket;

/* Small web server on 127.0.0.1 with the page that holds the YouTube player.
 * The browser source of the scene loads that page; this way the player has a
 * real http origin (YouTube refuses to be driven from file://). The page also
 * reports back how the video is going, so the window can show the time. */
class YouTubePlayerServer : public QObject {
	Q_OBJECT

public:
	static YouTubePlayerServer &instance();

	/* Starts listening the first time it is needed. */
	bool ensureRunning();
	QString pageUrl(const QString &videoId) const;

	/* Used by the self test. */
	QByteArray handleForTest(const QString &target);

	/* How the video on screen is going, as the page last reported it:
	 * {video, tiempo, duracion, estado}. Empty before the first report. */
	QJsonObject lastProgress() const { return progressSeen; }

signals:
	/* state: -1 unknown, 0 ended, 1 playing, 2 paused, 3 buffering. */
	void progress(const QString &videoId, double time, double duration, int state);

private:
	void onConnection();
	void onReadyRead(QTcpSocket *socket);
	QByteArray respond(const QString &target, int *status, QByteArray *contentType);

	QTcpServer *server = nullptr;
	quint16 listenPort = 0;
	QJsonObject progressSeen;
};

/* "Conexión con YouTube": a window to look for a video on YouTube, put it on
 * screen as a source of the scene and drive it from here. It has no seek bar
 * on purpose, so nobody drags the video by mistake in the middle of the
 * service: instead of next and previous there are -10 and +10 seconds. */
class YouTubeConnect : public QDialog {
	Q_OBJECT

public:
	/* Opens the window (only one at a time) and brings it to the front. */
	static void Run(QWidget *parent);

	/* Called once when the app starts: the videos saved in the scenes point
	 * at the address the player page has in this run. */
	static void Install();

	/* "https://youtu.be/abc", "watch?v=abc", "abc" -> "abc" ("" if none). */
	static QString VideoId(const QString &text);

	/* The window, the phone and the tablet all go through these. */

	/* Searches on YouTube and hands over [{id, titulo, canal, duracion}];
	 * an empty list when it could not search. */
	static void SearchVideos(QObject *context, const QString &query,
				 std::function<void(const QJsonArray &)> done);
	/* Puts the video on screen (creates or reuses the source of the scene). */
	static bool Show(const QString &videoId, const QString &title);
	/* "play", "pause", "restart", "seek" (with seconds), "mute", "volume". */
	static void Command(const QString &cmd, double value = 0);
	/* {video, titulo, tiempo, duracion, estado} of the video on screen. */
	static QJsonObject Status();

protected:
	void keyPressEvent(QKeyEvent *event) override;

private:
	explicit YouTubeConnect(QWidget *parent);

	void search(const QString &text);
	void showResults(const QJsonArray &videos);
	/* Puts the video on screen: creates or reuses the browser source of the
	 * scene being edited, fits it to the screen and leaves it paused. */
	void showVideo(const QString &videoId, const QString &title);
	void showSelected();
	void command(const QString &cmd, const QJsonObject &extra = {});
	void seek(double seconds);
	void togglePlay();
	void fitToScreen();
	void removeFromScene();
	void updatePlayer();
	OBSSource liveSource() const;
	/* A video already on screen from a previous time the window was open. */
	void adoptExistingSource();

	QNetworkAccessManager *network = nullptr;
	QLineEdit *searchEdit = nullptr;
	QListWidget *results = nullptr;
	QLabel *status = nullptr;
	QLabel *nowLabel = nullptr;
	QLabel *timeLabel = nullptr;
	QPushButton *showButton = nullptr;
	QPushButton *playButton = nullptr;
	QPushButton *backButton = nullptr;
	QPushButton *forwardButton = nullptr;
	QPushButton *restartButton = nullptr;
	QPushButton *muteButton = nullptr;
	QPushButton *fitButton = nullptr;
	QPushButton *removeButton = nullptr;
	QSlider *volume = nullptr;

	OBSWeakSource live;
	QString liveId;
	QString liveTitle;
	int searchToken = 0;
	int state = -1;
	bool muted = false;
};
