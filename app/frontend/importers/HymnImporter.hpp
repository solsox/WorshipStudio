#pragma once

#include <QString>
#include <QSqlDatabase>

class HymnImporter
{
public:
    // Importa todos los himnos desde el directorio dado (ruta a la carpeta "Himnario NUEVA VERSION PC 3")
    static bool importFromDirectory(const QString &path, QSqlDatabase &db);

    // Verifica si la tabla de himnos ya contiene datos
    static bool isDatabasePopulated(QSqlDatabase &db);
};