#include "dbmanager.h"
#include <QDebug>
#include <QSqlDriver>
#include <QSqlError>
#include <QSqlField>
#include <QSqlQuery>

/**
 * @brief DBManager::DBManager
 * @param dbpath
 *
 * Attention! Each connection is active only it's own thread!
 * For each separate thread (MainWindow vs Scanner) you need
 * to initiate a new connection using connect()
 */
DBManager::DBManager(QString &dbpath) {
	this->db_path = dbpath;
	this->connect();
}

DBManager::DBManager(QString &dbpath, QString connection_name) {
	this->db_path = dbpath;
	if (QSqlDatabase::contains(connection_name)) {
		m_db = QSqlDatabase::database(connection_name);
	} else {
		m_db = QSqlDatabase::addDatabase("QSQLITE", connection_name);
		m_db.setDatabaseName(db_path);
		if (!m_db.open()) {
			qDebug() << "Unable to open database " + db_path;
		} else {
			QSqlQuery query(m_db);
			query.exec("PRAGMA journal_mode=WAL");
			query.exec("PRAGMA synchronous=NORMAL");
		}
		createTables();
	}
}

/**
 * @brief Find the parent directory id.
 * @param catalog_id
 * @param full_path
 * @return
 */
int DBManager::findParent(int catalog_id, QString full_path) {
	QSqlQuery query(m_db);
	query.prepare("SELECT ids FROM direntry WHERE full_path = (:full_path) AND catalog_id = (:catalog_id)");
	query.bindValue(":full_path", full_path);
	query.bindValue(":catalog_id", catalog_id);
	query.exec();
	if (query.next()) {
		return query.value("ids").toInt();
	}
	return -1;
}

QString DBManager::formatSQL(QString keyword) {
	QSqlField f(QLatin1String(""), QVariant::String);
	f.setValue(keyword);
	return m_db.driver()->formatValue(f);
}

QSqlQuery DBManager::searchFiles(QString keyword, bool and_join, int cat_id) {
	QSqlQuery query(m_db);
	QString temp = "full_path LIKE '%%1%'";
	QStringList where;
	QStringList keywords = keyword.split(" ");
	for (QString k : keywords) {
		QString ck = formatSQL(k).replace("'", "");
		where.append(temp.arg(ck));
	}
	QString joiner = and_join ? " AND " : " OR ";
	if (cat_id == -1) {
		query.prepare("SELECT ids, directory, full_path, name, filesize, is_directory, catalog_id, parent_id, "
			      "thumbnail64 IS NOT NULL AS has_thumbnail FROM direntry WHERE (" +
			      where.join(joiner) + ")");
	} else {
		query.prepare("SELECT ids, directory, full_path, name, filesize, is_directory, catalog_id, parent_id, "
			      "thumbnail64 IS NOT NULL AS has_thumbnail FROM direntry WHERE catalog_id = (:catalog_id) AND (" +
			      where.join(joiner) + ")");
		query.bindValue(":catalog_id", cat_id);
	}
	query.exec();
	return query;
}

int DBManager::getRootId(int cat_id) {
	QSqlQuery query(m_db);
	query.prepare(
	    "SELECT ids FROM direntry WHERE catalog_id = (:catalog_id) AND parent_id = -1 AND name = (:name) AND is_directory = 1");
	query.bindValue(":catalog_id", cat_id);
	query.bindValue(":name", "");
	query.exec();
	if (query.next()) {
		return query.value("ids").toInt();
	}
	return -1;
}

QSqlQuery DBManager::allFiles(int cat_id) {
	QSqlQuery query(m_db);
	query.prepare("SELECT ids, full_path FROM direntry WHERE catalog_id = (:catalog_id)");
	query.bindValue(":catalog_id", cat_id);
	query.exec();
	return query;
}

QSqlQuery DBManager::fetchCatalogs() {
	QSqlQuery query(m_db);
	query.prepare("SELECT * FROM catalog ORDER BY name");
	query.exec();
	return query;
}

QSqlQuery DBManager::fetchFiles(int parent_id) {
	QSqlQuery query(m_db);
	query.prepare("SELECT ids, directory, full_path, name, filesize, is_directory, catalog_id, parent_id, "
		      "thumbnail64 IS NOT NULL AS has_thumbnail FROM direntry WHERE parent_id = (:parent_id) AND "
		      "is_directory = 0 ORDER BY name");
	query.bindValue(":parent_id", parent_id);
	query.exec();
	return query;
}

QSqlQuery DBManager::fetchFiles(int parent_id, int catalog_id) {
	QSqlQuery query(m_db);
	query.prepare("SELECT ids, directory, full_path, name, filesize, is_directory, catalog_id, parent_id, "
		      "thumbnail64 IS NOT NULL AS has_thumbnail FROM direntry WHERE parent_id = (:parent_id) AND "
		      "catalog_id = (:catalog_id) AND is_directory = 0 ORDER BY name");
	query.bindValue(":parent_id", parent_id);
	query.bindValue(":catalog_id", catalog_id);
	query.exec();
	return query;
}

QSqlQuery DBManager::fetchDirectoryTree(int cat_id, int parent_id) {
	QSqlQuery query(m_db);
	query.prepare(
	    "SELECT ids, name FROM direntry WHERE catalog_id = (:catalog_id) AND is_directory = 1 AND "
	    "parent_id = (:parent_id) ORDER BY ids;");
	query.bindValue(":catalog_id", cat_id);
	query.bindValue(":parent_id", parent_id);
	query.exec();
	return query;
}

/**
 * @brief DBManager::connect
 */
void DBManager::connect() {
	m_db = QSqlDatabase::addDatabase("QSQLITE");
	m_db.setDatabaseName(db_path);
	if (!m_db.open()) {
		qDebug() << "Unable to open database " + db_path;
	} else {
		qDebug() << "DB Connected";
		QSqlQuery query(m_db);
		query.exec("PRAGMA journal_mode=WAL");
		query.exec("PRAGMA synchronous=NORMAL");
	}
	createTables();
}

/**
 * @brief DBManager::~DBManager
 */
DBManager::~DBManager() {
	if (m_db.isOpen())
		m_db.close();
}

/**
 * @brief Check if the directory entry exists.
 * @param full_path
 * @return
 */
bool DBManager::dirEntryExists(QString full_path) {
	QSqlQuery query(m_db);
	query.prepare("SELECT ids FROM direntry WHERE full_path = (:full_path)");
	query.bindValue(":full_path", full_path);
	query.exec();
	if (query.next()) {
		return true;
	}
	return false;
}

int DBManager::createCatalog(Catalog &catalog) { return createCatalog(catalog.name, catalog.original_path, catalog.tags); }

int DBManager::createCatalog(QString name, QString original_path, QString tags) {
	QSqlQuery query(m_db);
	query.prepare("INSERT INTO catalog (name, original_path, tags) VALUES "
		      "(:name, :original_path, :tags);");
	query.bindValue(":name", name);
	query.bindValue(":original_path", original_path);
	query.bindValue(":tags", tags);
	if (query.exec()) {
		int id = query.lastInsertId().toInt();
		qDebug() << "Created catalog" << name << "with ID:" << id;
		return id;
	} else {
		qDebug() << "Unable to create the catalog" << query.lastError();
	}
	return -1;
}

int DBManager::createDirEntry(QString name, QString directory, QString full_path, int64_t filesize, QByteArray thumbnail, bool is_directory,
			      int parent_id, int catalog_id) {
	QSqlQuery query(m_db);
	query.prepare("INSERT INTO direntry ("
		      "directory, full_path, name, "
		      "filesize, thumbnail64, "
		      "is_directory, catalog_id, parent_id"
		      ") VALUES ("
		      ":directory, :full_path, :name, :filesize, "
		      ":thumbnail64, :is_directory, :catalog_id, :parent_id)");
	QVariant qfilesize((long long)filesize);
	query.bindValue(":directory", directory);
	query.bindValue(":full_path", full_path);
	query.bindValue(":name", name);
	query.bindValue(":filesize", qfilesize);
	query.bindValue(":thumbnail64", thumbnail);
	query.bindValue(":is_directory", (is_directory ? 1 : 0));
	query.bindValue(":catalog_id", catalog_id);
	query.bindValue(":parent_id", parent_id);
	if (!query.exec()) {
		qDebug() << "Unable to create direntry " << query.lastError();
		return -1;
	}

	return query.lastInsertId().toInt();
}

DirEntry DBManager::getDirentry(int id) {
	QSqlQuery query(m_db);
	query.prepare("SELECT * FROM direntry WHERE ids = (:ids)");
	query.bindValue(":ids", id);
	if (query.exec() && query.next()) {
		return DirEntry{
		    id,
		    query.value("directory").toString(),
		    query.value("full_path").toString(),
		    query.value("name").toString(),
		    query.value("filesize").toInt(),
		    query.value("thumbnail64").toByteArray(),
		    query.value("is_directory").toInt() == 1,
		    query.value("catalog_id").toInt(),
		    query.value("parent_id").toInt(),
		};
	}
	return DirEntry{};
}

int DBManager::createDirEntry(DirEntry &dir_entry) {
	return createDirEntry(dir_entry.name, dir_entry.directory, dir_entry.full_path, dir_entry.filesize, dir_entry.thumbnail,
			      dir_entry.is_directory, dir_entry.parent_id, dir_entry.catalog_id);
}

bool DBManager::deleteFiles(int cat_id, QVector<int> files) {
	if (files.isEmpty()) {
		return true;
	}
	QStringList placeholders;
	for (int i = 0; i < files.size(); i++) {
		placeholders.append("?");
	}
	QString queryStr = QString("DELETE FROM direntry WHERE catalog_id = ? AND ids IN (%1)").arg(placeholders.join(", "));
	QSqlQuery query(m_db);
	query.prepare(queryStr);
	query.addBindValue(cat_id);
	for (int id : files) {
		query.addBindValue(id);
	}
	if (!query.exec()) {
		qDebug() << query.executedQuery();
		qDebug() << query.lastError();
		return false;
	}
	return true;
}

bool DBManager::updateThumbnail(int entry_id, QByteArray thumbnail) {
	QSqlQuery query(m_db);
	query.prepare("UPDATE direntry SET thumbnail64 = :thumbnail WHERE ids = :id");
	query.bindValue(":thumbnail", thumbnail);
	query.bindValue(":id", entry_id);
	if (!query.exec()) {
		qDebug() << "Failed to update thumbnail for id" << entry_id << query.lastError();
		return false;
	}
	return true;
}

void DBManager::createTables() {
	QSqlQuery query(m_db);
	query.prepare("CREATE TABLE IF NOT EXISTS direntry ("
		      "ids integer primary key, directory "
		      "text, full_path text, name text, filesize integer, "
		      "thumbnail64 blob, is_directory integer, catalog_id integer, "
		      "parent_id integer);");
	if (query.exec()) {
		qDebug() << "Created direntry table";
	} else {
		qDebug() << "Failed to create direntry";
	}
	query.prepare("CREATE INDEX IF NOT EXISTS direntry_fullpath ON direntry (catalog_id, full_path)");
	if (query.exec()) {
		qDebug("Created full_path index");
	} else {
		qDebug("Full_path index failed");
	}
	query.prepare("CREATE TABLE IF NOT EXISTS catalog(ids integer primary key, name text, "
		      "original_path text, tags text);");
	if (query.exec()) {
		qDebug() << "Created catalog table";
	} else {
		qDebug() << "Failed to create the catalog table";
	}
	createIndexes();
}

void DBManager::createIndexes() {
	QSqlQuery query(m_db);
	query.prepare("CREATE INDEX IF NOT EXISTS direntry_catalog_parent_type_name ON direntry "
		      "(catalog_id, parent_id, is_directory, name)");
	if (!query.exec()) {
		qDebug() << "Failed to create browse name index" << query.lastError();
	}

	query.prepare("CREATE INDEX IF NOT EXISTS direntry_catalog_parent_type_ids ON direntry "
		      "(catalog_id, parent_id, is_directory, ids)");
	if (!query.exec()) {
		qDebug() << "Failed to create browse ids index" << query.lastError();
	}
}
