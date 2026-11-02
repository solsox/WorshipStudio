#pragma once

#include <QDialog>
#include <QJsonArray>
#include <QJsonObject>
#include <QPointer>
#include <QString>

class QComboBox;
class QLabel;
class QLineEdit;
class QListWidget;
class QNetworkAccessManager;
class QNetworkReply;
class QProgressBar;
class QPushButton;

namespace BibleCatalog {
/* The 66 books in Protestant canon order. */
QString BookId(int index);
QString SpanishBookName(int index);
QString EnglishBookName(int index);

/* Human readable license of a translation from the free catalog. */
QString LicenseText(const QJsonObject &translation);

/* Converts a "complete.simple.json" translation from the Free Use Bible API
 * into a WorshipStudio Bible. Books keep canon order (missing ones are left
 * empty) so references and abbreviations work in every language. */
QByteArray Convert(const QJsonObject &complete, const QJsonObject &translation, QString *error,
		   int *verseCount = nullptr);

/* Bibles the church already has in open formats: Zefania XML or OSIS XML.
 * Returns a WorshipStudio Bible, empty (with `error`) when the file is not one. */
QByteArray ImportOpenFormat(const QByteArray &data, const QString &fallbackName, QString *error,
			    int *verseCount = nullptr);

/* File name the converted translation is saved under. */
QString FileName(const QString &translationId);
} // namespace BibleCatalog

class BibleCatalogDialog : public QDialog {
	Q_OBJECT

public:
	BibleCatalogDialog(const QString &bibleDir, QWidget *parent = nullptr);

signals:
	void installed(const QString &path);

private slots:
	void refill();
	void onCurrentChanged();
	void onDownloadClicked();

private:
	QString bibleDir;
	QNetworkAccessManager *network = nullptr;
	QPointer<QNetworkReply> reply;
	QJsonArray translations;

	QLineEdit *searchEdit = nullptr;
	QComboBox *languageCombo = nullptr;
	QListWidget *list = nullptr;
	QLabel *infoLabel = nullptr;
	QProgressBar *progress = nullptr;
	QPushButton *downloadButton = nullptr;

	static QString cachePath();
	void loadCatalog();
	bool setCatalog(const QByteArray &data);
	void fillLanguages();
	QJsonObject currentTranslation() const;
	bool isInstalled(const QString &id) const;
	void setBusy(bool busy, const QString &message);
};
