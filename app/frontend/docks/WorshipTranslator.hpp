#pragma once

#include <QHash>
#include <QString>
#include <QTranslator>

/* WorshipStudio's own texts are written in Spanish in the code. This
 * translator maps them to the interface language from
 * data/worship/idiomas/<idioma>.json ({"texto en español": "traducción"}).
 * Texts without a translation stay in Spanish. */
class WorshipTranslator : public QTranslator {
	Q_OBJECT

public:
	explicit WorshipTranslator(QObject *parent = nullptr);

	/* locale like "pt-BR"; tries pt-BR.json, then pt.json. */
	bool load(const QString &directory, const QString &locale);
	bool isEmpty() const override { return texts.isEmpty(); }
	QString translate(const char *context, const char *sourceText, const char *disambiguation = nullptr,
			  int n = -1) const override;

	/* Languages with a WorshipStudio translation, as locale codes. */
	static QStringList available(const QString &directory);

private:
	QHash<QString, QString> texts;
};
