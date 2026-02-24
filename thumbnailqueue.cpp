#include "thumbnailqueue.h"
#include "dbmanager.h"
#include "thumbnailmanager.h"
#include <QDebug>
#include <QThread>

ThumbnailWorker::ThumbnailWorker(ThumbnailRequest request, QString db_path) : request(request), db_path(db_path) {
	setAutoDelete(true);
}

void ThumbnailWorker::run() {
	ThumbnailManager mgr;
	QByteArray thumbnail = mgr.generateThumbnail(request.file_path, request.max_size);

	if (thumbnail.isEmpty()) {
		emit thumbnailFailed(request.entry_id);
		return;
	}

	QString connectionName = QString("thumb_worker_%1").arg((quintptr)QThread::currentThreadId());
	DBManager db(db_path, connectionName);
	if (db.updateThumbnail(request.entry_id, thumbnail)) {
		emit thumbnailReady(request.entry_id, thumbnail);
	} else {
		emit thumbnailFailed(request.entry_id);
	}
}

ThumbnailQueue::ThumbnailQueue(QObject *parent, QString db_path) : QObject(parent), db_path(db_path), pending(0), completed(0), failed(0) {
	int cores = QThread::idealThreadCount();
	int maxThreads = cores > 0 ? cores / 2 : 2;
	if (maxThreads < 1)
		maxThreads = 1;

	pool = new QThreadPool(this);
	pool->setMaxThreadCount(maxThreads);

	qDebug() << "ThumbnailQueue initialized with" << maxThreads << "worker threads";
}

ThumbnailQueue::~ThumbnailQueue() {
	stop();
	delete pool;
}

void ThumbnailQueue::addRequest(ThumbnailRequest request) {
	QMutexLocker locker(&mutex);

	ThumbnailWorker *worker = new ThumbnailWorker(request, db_path);
	connect(worker, &ThumbnailWorker::thumbnailReady, this, &ThumbnailQueue::onThumbnailReady);
	connect(worker, &ThumbnailWorker::thumbnailFailed, this, &ThumbnailQueue::onThumbnailFailed);

	pending++;
	pool->start(worker);

	emit queueSizeChanged(pending);
}

int ThumbnailQueue::queueSize() {
	QMutexLocker locker(&mutex);
	return pending;
}

void ThumbnailQueue::stop() { pool->waitForDone(); }

void ThumbnailQueue::onThumbnailReady(int entry_id, QByteArray data) {
	QMutexLocker locker(&mutex);
	pending--;
	completed++;

	emit queueSizeChanged(pending);

	if (pending == 0) {
		qDebug() << "Thumbnail queue complete:" << completed << "generated," << failed << "failed";
		emit allComplete();
	}
}

void ThumbnailQueue::onThumbnailFailed(int entry_id) {
	QMutexLocker locker(&mutex);
	pending--;
	failed++;

	qDebug() << "Thumbnail generation failed for entry" << entry_id;

	emit queueSizeChanged(pending);

	if (pending == 0) {
		qDebug() << "Thumbnail queue complete:" << completed << "generated," << failed << "failed";
		emit allComplete();
	}
}
