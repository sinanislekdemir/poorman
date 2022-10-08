#ifndef SCANNER_H
#define SCANNER_H

#include "dbmanager.h"
#include <QThread>
#include <QtWidgets>

class Scanner : public QThread {
    Q_OBJECT

      public:
    explicit Scanner(QObject *parent, QString db_path);
    void setPath(QString path);
    bool running();
    void setCatalogName(QString cname);

      signals:
    void setProgressFilename(QString);

      public slots:
    void stop();

      private:
    QObject *parent;
    bool f_running;
    QString scan_path;
    QString db_path;
    QString catalog_name;
    QLabel *progress_label;
    void run();
    void processDirectory(QString path);
};

#endif // SCANNER_H
