#pragma once

#include <QDialog>
#include <QStringList>

class QLabel;
class QLineEdit;
class QListWidget;
class QNetworkAccessManager;
class QPushButton;
class QButtonGroup;

/* "Buscar fondos en internet": free photos and moving backgrounds from
 * Pexels (landscapes, skies, lights...) added to Mis fondos with one click.
 * Uses the Pexels API key of WorshipStudio (WORSHIPSTUDIO_PEXELS_KEY) or the
 * PexelsKey setting; without one it points to the Biblioteca page. */
class BackgroundSearch : public QDialog {
	Q_OBJECT

public:
	/* Opens the search and returns the files added to Mis fondos. */
	static QStringList Run(QWidget *parent, bool allowVideo);
	static QString ApiKey();

private:
	BackgroundSearch(QWidget *parent, bool allowVideo);

	bool allowVideo;
	QNetworkAccessManager *network = nullptr;
	QLineEdit *searchEdit = nullptr;
	QButtonGroup *kindGroup = nullptr;
	QListWidget *results = nullptr;
	QLabel *status = nullptr;
	QPushButton *addButton = nullptr;
	QStringList added;
	int searchToken = 0;

	void search(const QString &query);
	void addSelected();
};
