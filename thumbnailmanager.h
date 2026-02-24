#ifndef THUMBNAILMANAGER_H
#define THUMBNAILMANAGER_H

#include <QByteArray>
#include <QMimeDatabase>
#include <QString>

class ThumbnailManager {
      public:
	ThumbnailManager();
	QByteArray generateThumbnail(const QString &filePath, int maxSize = 256);

      private:
	QMimeDatabase mimeDb;
	QString detectMimeType(const QString &filePath);
	QByteArray generateWithQt(const QString &filePath, int maxSize);
#ifdef Q_OS_LINUX
	QByteArray generateWithNative(const QString &filePath, const QString &mimeType, int maxSize);
	QString findThumbnailer(const QString &mimeType);
#endif
};

#endif // THUMBNAILMANAGER_H
