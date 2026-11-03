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

#include "LanguageDialog.hpp"

#include <QDialog>
#include <QLabel>
#include <QListWidget>
#include <QPushButton>
#include <QVBoxLayout>

namespace {
struct Language {
	const char *code;
	const char *name;
	const char *suggested;
};

/* Languages WorshipStudio is fully translated to, in their own words. */
const Language Languages[] = {
	{"es-ES", "Español", "sugerido"},   {"en-US", "English", "suggested"},  {"pt-BR", "Português", "sugerido"},
	{"fr-FR", "Français", "suggéré"},   {"de-DE", "Deutsch", "empfohlen"},  {"it-IT", "Italiano", "consigliato"},
	{"ko-KR", "한국어", "추천"},
};
} // namespace

QString AskWorshipLanguage(const QString &current, const QStringList &preferred)
{
	/* Which entry to suggest: the saved one, else the first system language. */
	int suggested = -1;
	auto find = [](const QString &code) {
		const QString language = code.section(QLatin1Char('-'), 0, 0);
		for (int i = 0; i < int(std::size(Languages)); ++i)
			if (QString::fromLatin1(Languages[i].code).startsWith(language + QLatin1Char('-')))
				return i;
		return -1;
	};
	if (!current.isEmpty())
		suggested = find(current);
	for (int i = 0; suggested < 0 && i < preferred.size(); ++i)
		suggested = find(preferred.at(i));
	if (suggested < 0)
		suggested = 0;

	QDialog dialog;
	dialog.setWindowTitle(QStringLiteral("WorshipStudio"));
	dialog.setMinimumWidth(380);
	/* Shown before the theme is loaded: its own quiet ink-and-bone look. */
	dialog.setStyleSheet(QStringLiteral(
		"QDialog{background:#131211;color:#F4F1EC;}"
		"QLabel{color:#F4F1EC;font-size:15px;}"
		"QLabel#hint{color:#A39D95;font-size:13px;}"
		"QListWidget{background:#1A1817;border:1px solid #2D2A27;border-radius:10px;padding:6px;"
		"color:#F4F1EC;font-size:15px;outline:none;}"
		"QListWidget::item{padding:9px 10px;border-radius:7px;}"
		"QListWidget::item:selected{background:#5A544B;color:#FFFFFF;}"
		"QListWidget::item:hover:!selected{background:#22201E;}"
		"QPushButton{background:#DCC59C;color:#16130E;border:none;border-radius:8px;padding:10px 18px;"
		"font-size:15px;font-weight:600;}"
		"QPushButton:hover{background:#F1EBE0;}"));

	auto *layout = new QVBoxLayout(&dialog);
	layout->setContentsMargins(24, 22, 24, 22);
	layout->setSpacing(12);
	auto *title = new QLabel(QStringLiteral("<b>Idioma · Language · Idioma</b>"), &dialog);
	auto *hint = new QLabel(QStringLiteral("Elegí el idioma de WorshipStudio. Podés cambiarlo después en Ajustes.\n"
					       "Choose the language for WorshipStudio. You can change it later in Settings."),
				&dialog);
	hint->setObjectName(QStringLiteral("hint"));
	hint->setWordWrap(true);
	auto *list = new QListWidget(&dialog);
	for (int i = 0; i < int(std::size(Languages)); ++i) {
		QString label = QString::fromUtf8(Languages[i].name);
		if (i == suggested)
			label += QStringLiteral("   ·   ") + QString::fromUtf8(Languages[i].suggested);
		auto *item = new QListWidgetItem(label, list);
		item->setData(Qt::UserRole, QString::fromLatin1(Languages[i].code));
	}
	list->setCurrentRow(suggested);
	list->setMinimumHeight(list->sizeHintForRow(0) * int(std::size(Languages)) + 20);
	auto *button = new QPushButton(QStringLiteral("Continuar  ·  Continue  ➜"), &dialog);
	button->setDefault(true);

	layout->addWidget(title);
	layout->addWidget(hint);
	layout->addWidget(list, 1);
	layout->addWidget(button);

	QObject::connect(button, &QPushButton::clicked, &dialog, &QDialog::accept);
	QObject::connect(list, &QListWidget::itemActivated, &dialog, &QDialog::accept);
	dialog.exec();

	const QListWidgetItem *chosen = list->currentItem();
	return chosen ? chosen->data(Qt::UserRole).toString() : QString::fromLatin1(Languages[suggested].code);
}
