#pragma once

#include <QImage>
#include <QObject>
#include <QString>

class QTcpServer;
class QTcpSocket;

/* Control from a phone or tablet on the same network: a small web server
 * that serves a touch page and a JSON API. Protected by a 4 digit PIN. */
class WorshipRemote : public QObject {
	Q_OBJECT

public:
	explicit WorshipRemote(QObject *parent = nullptr);
	~WorshipRemote() override;

	bool start(quint16 port = 8765);
	void stop();
	bool isRunning() const;

	QString url() const;
	QString pin() const { return accessPin; }
	void regeneratePin();

	/* QR code of url() + pin, so the phone opens it ready to use. */
	QImage qrCode(int size) const;

	/* Used by the self test. */
	QByteArray handleForTest(const QByteArray &method, const QString &target);

signals:
	void runningChanged(bool running);

private:
	QTcpServer *server = nullptr;
	QString accessPin;
	quint16 listenPort = 0;

	void onConnection();
	void onReadyRead(QTcpSocket *socket);
	QByteArray respond(const QByteArray &method, const QString &target, int *status, QByteArray *contentType);
	QByteArray stateJson() const;
};
