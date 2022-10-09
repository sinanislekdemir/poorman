#include "scanner.h"
#include "dbmanager.h"
#include <QDebug>
#include <QDirIterator>
#include <QFileInfo>
#include <QImage>
#include <QtWidgets>

Scanner::Scanner(QObject *parent, QString db_path) : QThread(parent) {
    this->f_running = false;
    this->parent = parent;
    this->db_path = db_path;
    catalog_id = -1;
}

void Scanner::setCatalogName(QString cname) { this->catalog_name = cname; }

void Scanner::setPath(QString path) { this->scan_path = path; }

bool Scanner::running() { return this->f_running; }

void Scanner::setCatalogId(int id) { catalog_id = id; }

void Scanner::run() {
    QDir dir(this->scan_path);
    if (!dir.exists()) {
        QMessageBox box;
        box.setText(tr("Directory does not exist"));
        box.setStandardButtons(QMessageBox::Ok);
        box.setIcon(QMessageBox::Warning);
        box.setWindowTitle("Warning");
        box.exec();
        this->stop();
        return;
    }
    qDebug() << "Into process directory";
    qDebug() << this->scan_path;
    this->processDirectory(this->scan_path);
    this->stop();
}

void Scanner::processDirectory(QString path) {
    DBManager *db = new DBManager(db_path);
    if (catalog_id == -1) {
        catalog_id = db->createCatalog(catalog_name, path, "");
    }
    QDir dir(path);
    QDirIterator it(dir, QDirIterator::Subdirectories);
    while (it.hasNext()) {
        QString filename = it.next();
        QFileInfo info(filename);
        QByteArray x;
        QBuffer buffer(&x);
        QString suffix = info.completeSuffix().toUpper();
        if (suffix == "JPG" || suffix == "PNG" || suffix == "JPEG") {
            QImage img(info.absoluteFilePath());
            QImage scaled = img.scaled(256, 256, Qt::KeepAspectRatio, Qt::SmoothTransformation);
            buffer.open(QIODevice::WriteOnly);
            scaled.save(&buffer, "JPG");
        }
        int parent = db->findParent(catalog_id, info.absolutePath());
        if (info.isDir()) {
            emit setProgressFilename(filename);
        }
        if (db->dirEntryExists(info.absoluteFilePath())) {
            continue;
        }
        db->createDirEntry(info.completeBaseName(), info.absolutePath(), info.absoluteFilePath(), info.size(), x, info.isDir(),
                   parent, catalog_id);
        if (buffer.isOpen()) {
            buffer.close();
        }
    }
    delete db;
    catalog_id = -1;
    emit setProgressFilename("finished");
}

void Scanner::stop() { this->f_running = false; }
