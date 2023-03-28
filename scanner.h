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
	void withThumbs(bool state);
	bool running();
	void setCatalogName(QString cname);
	void setCatalogId(int id);

      signals:
	void setProgressFilename(QString);

      public slots:
	void stop();

      private:
	bool with_thumbs;
	QObject *parent;
	bool f_running;
	QString scan_path;
	QString db_path;
	QString catalog_name;
	QLabel *progress_label;
	int catalog_id;
	void run();
	void processDirectory(QString path);
};

#endif // SCANNER_H
