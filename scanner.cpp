#include "scanner.h"
#include "dbmanager.h"
#include "thumbnailqueue.h"
#include <QDebug>
#include <QDirIterator>
#include <QFileInfo>
#include <QMimeDatabase>
#include <QtWidgets>

Scanner::Scanner(QObject *parent, QString db_path) : QThread(parent) {
	this->f_running = false;
	this->parent = parent;
	this->db_path = db_path;
	catalog_id = -1;
	with_thumbs = true;
	thumb_queue = nullptr;
}

void Scanner::setCatalogName(QString cname) { this->catalog_name = cname; }

void Scanner::setPath(QString path) { this->scan_path = path; }

bool Scanner::running() { return this->f_running; }

void Scanner::setCatalogId(int id) { catalog_id = id; }

void Scanner::withThumbs(bool state) { with_thumbs = state; }

void Scanner::setThumbnailQueue(ThumbnailQueue *queue) { thumb_queue = queue; }

bool Scanner::needsThumbnail(const QFileInfo &info) {
	if (info.isDir())
		return false;

	QMimeDatabase mimeDb;
	QMimeType mimeType = mimeDb.mimeTypeForFile(info.filePath());
	QString mime = mimeType.name();

	return mime.startsWith("image/") || mime.startsWith("video/") || mime == "application/pdf";
}

void Scanner::run() {
	this->f_running = true;
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
	QString connectionName = QString("scanner_%1").arg((quintptr)QThread::currentThreadId());
	DBManager *db = new DBManager(db_path, connectionName);
	int current_catalog_id = catalog_id;
	if (current_catalog_id == -1) {
		current_catalog_id = db->createCatalog(catalog_name, path, "");
		qDebug() << "Created catalog with ID:" << current_catalog_id;
	}
	if (current_catalog_id == -1) {
		qDebug() << "ERROR: catalog_id is still -1 after createCatalog!";
		delete db;
		emit setProgressFilename("finished");
		return;
	}
	QDir dir(path);
	QDirIterator it(dir, QDirIterator::Subdirectories);
	while (it.hasNext()) {
		QString filename = it.next();
		QFileInfo info(filename);

		QString basename = info.fileName();
		if (basename == "." || basename == "..") {
			continue;
		}

		if (info.isDir()) {
			emit setProgressFilename(filename);
		}
		if (db->dirEntryExists(info.absoluteFilePath())) {
			continue;
		}

		int parent = db->findParent(current_catalog_id, info.absolutePath());
		int entry_id = db->createDirEntry(info.completeBaseName(), info.absolutePath(), info.absoluteFilePath(), info.size(),
						  QByteArray(), info.isDir(), parent, current_catalog_id);

		if (entry_id != -1 && with_thumbs && thumb_queue && needsThumbnail(info)) {
			ThumbnailRequest req;
			req.entry_id = entry_id;
			req.file_path = info.absoluteFilePath();
			req.max_size = 256;
			thumb_queue->addRequest(req);
		}
	}
	delete db;
	emit setProgressFilename("finished");
}

void Scanner::stop() { this->f_running = false; }

