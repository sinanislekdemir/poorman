#include "thumbnailmanager.h"
#include <QBuffer>
#include <QDebug>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QImage>
#include <QMimeDatabase>
#include <QMimeType>
#include <QProcess>
#include <QTemporaryFile>

ThumbnailManager::ThumbnailManager() {}

QString ThumbnailManager::detectMimeType(const QString &filePath) {
	QMimeType mimeType = mimeDb.mimeTypeForFile(filePath);
	return mimeType.name();
}

QByteArray ThumbnailManager::generateWithQt(const QString &filePath, int maxSize) {
	QImage img(filePath);
	if (img.isNull()) {
		return QByteArray();
	}

	QImage scaled = img.scaled(maxSize, maxSize, Qt::KeepAspectRatio, Qt::SmoothTransformation);
	QByteArray ba;
	QBuffer buffer(&ba);
	buffer.open(QIODevice::WriteOnly);
	scaled.save(&buffer, "PNG");
	return ba;
}

#ifdef Q_OS_LINUX
QString ThumbnailManager::findThumbnailer(const QString &mimeType) {
	QStringList thumbnailerDirs = {"/usr/share/thumbnailers", "/usr/local/share/thumbnailers"};

	for (const QString &dir : thumbnailerDirs) {
		QDir thumbDir(dir);
		if (!thumbDir.exists())
			continue;

		QStringList thumbnailerFiles = thumbDir.entryList(QStringList() << "*.thumbnailer", QDir::Files);
		for (const QString &file : thumbnailerFiles) {
			QString fullPath = thumbDir.absoluteFilePath(file);
			QFile f(fullPath);
			if (f.open(QIODevice::ReadOnly | QIODevice::Text)) {
				QString content = f.readAll();
				f.close();

				if (content.contains(mimeType)) {
					QStringList lines = content.split('\n');
					for (const QString &line : lines) {
						if (line.startsWith("Exec=")) {
							QString exec = line.mid(5).trimmed();
							QString cmd = exec.split(' ').first();
							if (QFile::exists(cmd)) {
								return exec;
							}
						}
					}
				}
			}
		}
	}
	return QString();
}

QByteArray ThumbnailManager::generateWithNative(const QString &filePath, const QString &mimeType, int maxSize) {
	QString thumbnailerExec = findThumbnailer(mimeType);
	if (thumbnailerExec.isEmpty()) {
		return QByteArray();
	}

	QTemporaryFile tempFile;
	if (!tempFile.open()) {
		return QByteArray();
	}
	QString outputPath = tempFile.fileName() + ".png";
	tempFile.close();

	QString exec = thumbnailerExec;
	exec.replace("%s", QString::number(maxSize));
	exec.replace("%u", filePath);
	exec.replace("%i", filePath);
	exec.replace("%o", outputPath);

	QStringList parts = exec.split(' ', Qt::SkipEmptyParts);
	if (parts.isEmpty()) {
		return QByteArray();
	}

	QString program = parts.takeFirst();
	QProcess process;
	process.start(program, parts);

	if (!process.waitForFinished(4000)) {
		process.kill();
		process.waitForFinished(1000);
		QFile::remove(outputPath);
		return QByteArray();
	}

	if (process.exitCode() != 0) {
		QFile::remove(outputPath);
		return QByteArray();
	}

	QFile outFile(outputPath);
	if (!outFile.open(QIODevice::ReadOnly)) {
		return QByteArray();
	}

	QByteArray result = outFile.readAll();
	outFile.close();
	QFile::remove(outputPath);

	return result;
}
#endif

QByteArray ThumbnailManager::generateThumbnail(const QString &filePath, int maxSize) {
	QString mimeType = detectMimeType(filePath);

#ifdef Q_OS_LINUX
	QByteArray nativeThumb = generateWithNative(filePath, mimeType, maxSize);
	if (!nativeThumb.isEmpty()) {
		return nativeThumb;
	}
#endif

	return generateWithQt(filePath, maxSize);
}
