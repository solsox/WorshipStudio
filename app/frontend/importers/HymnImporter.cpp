#include "HymnImporter.hpp"
#include <QDir>
#include <QFile>
#include <QTextStream>
#include <QDebug>

bool HymnImporter::importFromDirectory(const QString &path, QSqlDatabase &db)
{
    QDir baseDir(path);
    if (!baseDir.exists()) {
        qWarning() << "Directorio base no encontrado:" << path;
        return false;
    }

    // Buscar la carpeta "Letra" dentro del himnario
    QDir letraDir = baseDir.filePath("Letra");
    if (!letraDir.exists()) {
        qWarning() << "No se encontró la carpeta 'Letra' en" << path;
        return false;
    }

    QStringList subdirs = letraDir.entryList(QDir::Dirs | QDir::NoDotAndDotDot);
    int imported = 0;

    for (const QString &subdir : subdirs) {
        QDir hymnDir(letraDir.filePath(subdir));
        // Extraer número y título del nombre de la carpeta: "001 - Cantad alegres al Señor"
        QStringList parts = subdir.split(" - ");
        if (parts.size() < 2) continue;
        bool ok;
        int number = parts[0].toInt(&ok);
        if (!ok) continue;
        QString title = parts.mid(1).join(" - ").trimmed();

        // Buscar archivos de imagen (01.jpg, 02.jpg, ...)
        QStringList images = hymnDir.entryList({"*.jpg", "*.png", "*.jpeg"}, QDir::Files);
        if (images.isEmpty()) continue;

        // Tomar la primera imagen como preview
        QString firstImage = hymnDir.filePath(images.first());

        // Intentar leer la letra: si hay un archivo de texto con el mismo nombre, lo usamos
        QString lyrics;
        // Buscar un archivo .txt con el número o el título
        QStringList txtFiles = hymnDir.entryList({"*.txt"}, QDir::Files);
        if (!txtFiles.isEmpty()) {
            QFile file(hymnDir.filePath(txtFiles.first()));
            if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
                QTextStream stream(&file);
                lyrics = stream.readAll();
                file.close();
            }
        }

        // Si no hay .txt, podríamos extraer el título de la carpeta como letra
        if (lyrics.isEmpty()) {
            lyrics = title; // fallback
        }

        // Insertar en la BD
        QSqlQuery query(db);
        query.prepare("INSERT OR REPLACE INTO himnos (numero, titulo, letra, imagen_path) "
                      "VALUES (:num, :title, :lyrics, :img)");
        query.bindValue(":num", number);
        query.bindValue(":title", title);
        query.bindValue(":lyrics", lyrics);
        query.bindValue(":img", firstImage);

        if (query.exec()) {
            imported++;
        } else {
            qWarning() << "Error insertando himno" << number << ":" << query.lastError().text();
        }
    }

    qDebug() << "Importados" << imported << "himnos desde" << path;
    return true;
}

bool HymnImporter::isDatabasePopulated(QSqlDatabase &db)
{
    QSqlQuery query(db);
    query.exec("SELECT COUNT(*) FROM himnos");
    if (query.next()) {
        return query.value(0).toInt() > 0;
    }
    return false;
}