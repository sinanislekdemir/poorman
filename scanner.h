#ifndef SCANNER_H
#define SCANNER_H

#include "dbmanager.h"
#include "thumbnailqueue.h"
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
	void setThumbnailQueue(ThumbnailQueue *queue);

      signals:
	void setProgressFilename(QString);
	void thumbnailQueueSize(int size);

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
	ThumbnailQueue *thumb_queue;
	void run();
	void processDirectory(QString path);
	bool needsThumbnail(const QFileInfo &info);
};

#endif // SCANNER_H
