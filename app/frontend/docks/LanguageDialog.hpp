#pragma once

#include <QString>
#include <QStringList>

/* First start: asks for the interface language before anything is loaded.
 * `current` is preselected when set; otherwise the first of `preferred`
 * (the system languages) that WorshipStudio speaks is suggested.
 * Returns a locale code like "es-ES". */
QString AskWorshipLanguage(const QString &current, const QStringList &preferred);
