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

#include "WorshipTranslator.hpp"

#include <QDir>
#include <QFileInfo>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>

#include "moc_WorshipTranslator.cpp"

WorshipTranslator::WorshipTranslator(QObject *parent) : QTranslator(parent) {}

bool WorshipTranslator::load(const QString &directory, const QString &locale)
{
	texts.clear();
	const QString language = locale.section(QLatin1Char('-'), 0, 0);
	for (const QString &name : {locale, language}) {
		QFile file(QDir(directory).absoluteFilePath(name + QStringLiteral(".json")));
		if (name.isEmpty() || !file.open(QIODevice::ReadOnly))
			continue;
		const QJsonObject object = QJsonDocument::fromJson(file.readAll()).object();
		for (auto it = object.constBegin(); it != object.constEnd(); ++it) {
			const QString value = it.value().toString();
			if (!value.isEmpty())
				texts.insert(it.key(), value);
		}
		return !texts.isEmpty();
	}
	return false;
}

QString WorshipTranslator::translate(const char *, const char *sourceText, const char *, int) const
{
	if (!sourceText || texts.isEmpty())
		return {};
	return texts.value(QString::fromUtf8(sourceText));
}

QStringList WorshipTranslator::available(const QString &directory)
{
	QStringList codes;
	for (const QFileInfo &info : QDir(directory).entryInfoList({QStringLiteral("*.json")}, QDir::Files, QDir::Name))
		if (info.completeBaseName() != QStringLiteral("plantilla"))
			codes << info.completeBaseName();
	return codes;
}
