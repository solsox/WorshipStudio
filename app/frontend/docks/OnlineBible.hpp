#pragma once

#include <QJsonObject>
#include <QObject>
#include <QString>
#include <QStringList>
#include <QVector>

#include <functional>

/* Bibles read through the YouVersion Platform API (Reina Valera 1960, 1995,
 * NVI...). They appear in the version list like any other Bible, but the
 * text is only fetched for the chapter on screen and kept in memory: it is
 * never written to disk, as the publishers' licences require. */
namespace OnlineBible {

struct Version {
	QString id; /* YouVersion id, "149" */
	QString abbrev;
	QString title;
	QString language; /* ISO 639-3, "spa" */
};

/* App key: WORSHIPSTUDIO_YOUVERSION_KEY (environment or build), or
 * YouVersionKey in the [WorshipStudio] settings. Empty: no online Bibles. */
QString AppKey();
bool Available();

/* Version-list data for an online Bible ("online:149") and back. */
QString DataFor(const QString &id);
bool IsOnline(const QString &data);
QString IdOf(const QString &data);

/* Preferred order of the versions, when they are enabled for the app key. */
QVector<Version> Favorites();

/* The versions enabled for the app key in these languages (ISO 639-3). */
void FetchVersions(const QStringList &languages, QObject *context,
		   std::function<void(const QVector<Version> &)> done);

/* Copyright line of a version, for the status and the credits. */
void FetchInfo(const QString &id, QObject *context, std::function<void(const QJsonObject &)> done);

/* Verse counts per chapter for the 66 books, in canonical order. Empty
 * books when the service does not say; the caller falls back to a local
 * Bible with the same versification. */
void FetchIndex(const QString &id, QObject *context,
		std::function<void(const QVector<QVector<int>> &verseCounts, const QStringList &bookNames)> done);

/* The verses of one chapter (index 0 = verse 1). `book` is 0..65. */
void FetchChapter(const QString &id, int book, int chapter, QObject *context,
		  std::function<void(const QStringList &verses, const QString &error)> done);

/* USFM book id ("JHN") for a book 0..65. */
QString UsfmBook(int book);

/* Verses out of the passage HTML (yv-v markers); empty if none found. */
QStringList ParseChapterHtml(const QString &html);

} // namespace OnlineBible
