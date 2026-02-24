#ifndef THUMBNAILQUEUE_H
#define THUMBNAILQUEUE_H

#include <QMutex>
#include <QObject>
#include <QQueue>
#include <QThread>
#include <QThreadPool>

struct ThumbnailRequest {
	int entry_id;
	QString file_path;
	int max_size;
};

class ThumbnailWorker : public QObject, public QRunnable {
	Q_OBJECT
      public:
	ThumbnailWorker(ThumbnailRequest request, QString db_path);
	void run() override;

      signals:
	void thumbnailReady(int entry_id, QByteArray data);
	void thumbnailFailed(int entry_id);

      private:
	ThumbnailRequest request;
	QString db_path;
};

class ThumbnailQueue : public QObject {
	Q_OBJECT
      public:
	ThumbnailQueue(QObject *parent, QString db_path);
	~ThumbnailQueue();
	void addRequest(ThumbnailRequest request);
	int queueSize();
	void stop();

      signals:
	void queueSizeChanged(int size);
	void allComplete();

      private slots:
	void onThumbnailReady(int entry_id, QByteArray data);
	void onThumbnailFailed(int entry_id);

      private:
	QThreadPool *pool;
	QString db_path;
	QMutex mutex;
	int pending;
	int completed;
	int failed;
};

#endif // THUMBNAILQUEUE_H
