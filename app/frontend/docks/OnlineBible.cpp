/******************************************************************************
    Copyright (C) 2026 by WorshipStudio Contributors

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
******************************************************************************/

#include "OnlineBible.hpp"
#include "WorshipOutput.hpp"

#include <QCoreApplication>
#include <QJsonArray>
#include <QJsonDocument>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QPointer>
#include <QRegularExpression>
#include <QTextDocumentFragment>
#include <QUrlQuery>

#include <memory>

namespace {
constexpr const char *ApiBase = "https://api.youversion.com/v1/";
constexpr const char *DataPrefix = "online:";

const char *const UsfmBooks[66] = {
	"GEN", "EXO", "LEV", "NUM", "DEU", "JOS", "JDG", "RUT", "1SA", "2SA", "1KI", "2KI", "1CH", "2CH",
	"EZR", "NEH", "EST", "JOB", "PSA", "PRO", "ECC", "SNG", "ISA", "JER", "LAM", "EZK", "DAN", "HOS",
	"JOL", "AMO", "OBA", "JON", "MIC", "NAM", "HAB", "ZEP", "HAG", "ZEC", "MAL", "MAT", "MRK", "LUK",
	"JHN", "ACT", "ROM", "1CO", "2CO", "GAL", "EPH", "PHP", "COL", "1TH", "2TH", "1TI", "2TI", "TIT",
	"PHM", "HEB", "JAS", "1PE", "2PE", "1JN", "2JN", "3JN", "JUD", "REV",
};

QNetworkAccessManager *network()
{
	static QNetworkAccessManager *manager = new QNetworkAccessManager();
	return manager;
}

/* GET on the API; `done` runs on the UI thread only while `context` lives. */
void get(const QString &path, const QUrlQuery &query, QObject *context,
	 std::function<void(const QByteArray &body, int status, const QString &error)> done)
{
	QUrl url(QString::fromLatin1(ApiBase) + path);
	url.setQuery(query);
	QNetworkRequest request(url);
	request.setRawHeader("X-YVP-App-Key", OnlineBible::AppKey().toUtf8());
	request.setRawHeader("Accept", "application/json");
	request.setTransferTimeout(15000);
	QNetworkReply *reply = network()->get(request);
	QPointer<QObject> guard(context);
	QObject::connect(reply, &QNetworkReply::finished, reply, [reply, guard, done]() {
		reply->deleteLater();
		if (!guard)
			return;
		const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
		QString error;
		if (reply->error() != QNetworkReply::NoError)
			error = status == 401 || status == 403
					? QCoreApplication::translate("WorshipStudio",
								      "Esta versión no está habilitada para WorshipStudio.")
					: QCoreApplication::translate("WorshipStudio",
								      "Sin conexión a internet: esta versión se lee en línea.");
		done(reply->readAll(), status, error);
	});
}

QString firstString(const QJsonObject &object, std::initializer_list<const char *> keys)
{
	for (const char *key : keys) {
		const QJsonValue value = object.value(QLatin1String(key));
		if (value.isString() && !value.toString().isEmpty())
			return value.toString();
		if (value.isDouble())
			return QString::number(value.toInteger());
	}
	return {};
}

QJsonArray firstArray(const QJsonObject &object, std::initializer_list<const char *> keys)
{
	for (const char *key : keys) {
		const QJsonValue value = object.value(QLatin1String(key));
		if (value.isArray())
			return value.toArray();
	}
	return {};
}
} // namespace

namespace OnlineBible {

QString AppKey()
{
	const QByteArray environment = qgetenv("WORSHIPSTUDIO_YOUVERSION_KEY");
	if (!environment.isEmpty())
		return QString::fromUtf8(environment);
	const QString configured = WorshipOutput::ConfigString("YouVersionKey");
	if (!configured.isEmpty())
		return configured;
#ifdef WORSHIPSTUDIO_YOUVERSION_KEY
	return QStringLiteral(WORSHIPSTUDIO_YOUVERSION_KEY);
#else
	return {};
#endif
}

bool Available()
{
	return !AppKey().isEmpty();
}

QString DataFor(const QString &id)
{
	return QString::fromLatin1(DataPrefix) + id;
}

bool IsOnline(const QString &data)
{
	return data.startsWith(QLatin1String(DataPrefix));
}

QString IdOf(const QString &data)
{
	return IsOnline(data) ? data.mid((int)strlen(DataPrefix)) : QString();
}

QVector<Version> Favorites()
{
	return {
		{QStringLiteral("149"), QStringLiteral("RVR1960"), QStringLiteral("Reina Valera 1960"), QStringLiteral("spa")},
		{QStringLiteral("150"), QStringLiteral("RVR95"), QStringLiteral("Reina Valera 1995"), QStringLiteral("spa")},
		{QStringLiteral("147"), QStringLiteral("RVES"), QStringLiteral("Reina-Valera Antigua"), QStringLiteral("spa")},
		{QStringLiteral("128"), QStringLiteral("NVI"), QStringLiteral("Nueva Versión Internacional"), QStringLiteral("spa")},
	};
}

QString UsfmBook(int book)
{
	return book >= 0 && book < 66 ? QString::fromLatin1(UsfmBooks[book]) : QString();
}

void FetchVersions(const QStringList &languages, QObject *context, std::function<void(const QVector<Version> &)> done)
{
	if (!Available() || languages.isEmpty()) {
		done({});
		return;
	}
	/* The service answers with the first language that has Bibles, so each
	 * language is asked for on its own. */
	auto collected = std::make_shared<QVector<Version>>();
	auto pending = std::make_shared<int>((int)languages.size());
	for (const QString &language : languages) {
		QUrlQuery query;
		query.addQueryItem(QStringLiteral("language_ranges[]"), language);
		query.addQueryItem(QStringLiteral("page_size"), QStringLiteral("99"));
		get(QStringLiteral("bibles"), query, context, [collected, pending, language, done](const QByteArray &body, int, const QString &) {
			const QJsonArray data = QJsonDocument::fromJson(body).object().value(QStringLiteral("data")).toArray();
			for (const QJsonValue &value : data) {
				const QJsonObject object = value.toObject();
				Version version;
				version.id = firstString(object, {"id"});
				version.abbrev = firstString(object, {"localized_abbreviation", "abbreviation"});
				version.title = firstString(object, {"localized_title", "title"});
				version.language = firstString(object, {"language_tag"});
				if (version.language.isEmpty())
					version.language = language;
				if (!version.id.isEmpty() && !version.title.isEmpty())
					collected->push_back(version);
			}
			if (--*pending == 0)
				done(*collected);
		});
	}
}

void FetchInfo(const QString &id, QObject *context, std::function<void(const QJsonObject &)> done)
{
	get(QStringLiteral("bibles/%1").arg(id), {}, context, [done](const QByteArray &body, int, const QString &) {
		done(QJsonDocument::fromJson(body).object());
	});
}

void FetchIndex(const QString &id, QObject *context,
		std::function<void(const QVector<QVector<int>> &, const QStringList &)> done)
{
	get(QStringLiteral("bibles/%1/index").arg(id), {}, context, [done](const QByteArray &body, int, const QString &) {
		QVector<QVector<int>> counts(66);
		QStringList names;
		for (int i = 0; i < 66; ++i)
			names << QString();

		const QJsonDocument document = QJsonDocument::fromJson(body);
		QJsonArray booksArray = firstArray(document.object(), {"books", "data"});
		if (booksArray.isEmpty() && document.isArray())
			booksArray = document.array();
		for (const QJsonValue &bookValue : booksArray) {
			const QJsonObject bookObject = bookValue.toObject();
			const QString usfm = firstString(bookObject, {"id", "usfm", "book_id"}).toUpper();
			int book = -1;
			for (int i = 0; i < 66; ++i) {
				if (usfm == QLatin1String(UsfmBooks[i]))
					book = i;
			}
			if (book < 0)
				continue;
			names[book] = firstString(bookObject, {"title", "full_title", "name", "human"});
			for (const QJsonValue &chapterValue : firstArray(bookObject, {"chapters"})) {
				const QJsonObject chapterObject = chapterValue.toObject();
				int verses = 0;
				const QJsonValue list = chapterObject.value(QStringLiteral("verses"));
				if (list.isArray())
					verses = (int)list.toArray().size();
				else if (list.isDouble())
					verses = list.toInt();
				else
					verses = chapterObject.value(QStringLiteral("verse_count")).toInt();
				/* Introductions and other non numbered chapters are skipped. */
				bool numbered = false;
				firstString(chapterObject, {"id", "title"}).section(QLatin1Char('.'), -1).toInt(&numbered);
				if (numbered && verses > 0)
					counts[book].push_back(verses);
			}
		}
		done(counts, names);
	});
}

QStringList ParseChapterHtml(const QString &html)
{
	static const QRegularExpression marker(
		QStringLiteral("<span\\b[^>]*class=\"[^\"]*\\byv-v\\b[^\"]*\"[^>]*>"));
	static const QRegularExpression number(QStringLiteral("\\bv=\"(\\d+)"));
	static const QRegularExpression label(QStringLiteral("<span\\b[^>]*class=\"[^\"]*\\byv-vlbl\\b[^\"]*\"[^>]*>.*?</span>"),
					      QRegularExpression::DotMatchesEverythingOption);
	static const QRegularExpression notes(
		QStringLiteral("<(span|div)\\b[^>]*class=\"[^\"]*\\b(yv-n|yv-h|note|heading)\\b[^\"]*\"[^>]*>.*?</\\1>"),
		QRegularExpression::DotMatchesEverythingOption);
	static const QRegularExpression blockEnd(QStringLiteral("</(div|p)>|<br\\s*/?>"));

	QStringList verses;
	QRegularExpressionMatchIterator it = marker.globalMatch(html);
	QVector<QPair<int, qsizetype>> starts; /* verse number, text start */
	QVector<qsizetype> tagStarts;
	while (it.hasNext()) {
		const QRegularExpressionMatch match = it.next();
		const int verse = number.match(match.captured(0)).captured(1).toInt();
		if (verse <= 0)
			continue;
		starts.push_back({verse, match.capturedEnd(0)});
		tagStarts.push_back(match.capturedStart(0));
	}
	for (int i = 0; i < starts.size(); ++i) {
		const qsizetype end = i + 1 < starts.size() ? tagStarts.at(i + 1) : html.size();
		QString part = html.mid(starts.at(i).second, end - starts.at(i).second);
		part.remove(label).remove(notes).replace(blockEnd, QStringLiteral(" "));
		const QString text = QTextDocumentFragment::fromHtml(part).toPlainText().simplified();
		const int verse = starts.at(i).first;
		while (verses.size() < verse)
			verses << QString();
		/* A verse split across poetry lines continues in the same slot. */
		verses[verse - 1] = (verses.at(verse - 1) + QLatin1Char(' ') + text).trimmed();
	}
	return verses;
}

void FetchChapter(const QString &id, int book, int chapter, QObject *context,
		  std::function<void(const QStringList &, const QString &)> done)
{
	QUrlQuery query;
	query.addQueryItem(QStringLiteral("format"), QStringLiteral("html"));
	query.addQueryItem(QStringLiteral("include_headings"), QStringLiteral("false"));
	query.addQueryItem(QStringLiteral("include_notes"), QStringLiteral("false"));
	const QString passage = QStringLiteral("%1.%2").arg(UsfmBook(book)).arg(chapter);
	get(QStringLiteral("bibles/%1/passages/%2").arg(id, passage), query, context,
	    [done](const QByteArray &body, int, const QString &error) {
		    if (!error.isEmpty()) {
			    done({}, error);
			    return;
		    }
		    const QString html = QJsonDocument::fromJson(body).object().value(QStringLiteral("content")).toString();
		    const QStringList verses = ParseChapterHtml(html);
		    done(verses, verses.isEmpty() ? QCoreApplication::translate("WorshipStudio",
										"No se pudo leer este capítulo.")
						  : QString());
	    });
}

} // namespace OnlineBible
