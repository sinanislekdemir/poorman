/**
 * Main Database management
 */

#ifndef DBMANAGER_H
#define DBMANAGER_H

#include <QSqlDatabase>

struct Catalog {
    int id;
    QString name;
    QString original_path;
    QString tags;
};

struct DirEntry {
    int id;
    QString directory;
    QString full_path;
    QString name;
    int64_t filesize;
    QByteArray thumbnail;
    bool is_directory;
    int catalog_id;
    int parent_id;
};

class DBManager {
      public:
    DBManager(QString &dbpath);
    ~DBManager();
    // Create stuff
    int createDirEntry(QString name, QString directory, QString full_path, int64_t filesize, QByteArray thumbnail, bool is_directory,
               int parent_id, int catalog_id);
    int createDirEntry(DirEntry &dir_entry);
    int createCatalog(QString name, QString original_path, QString tags);
    int createCatalog(Catalog &catalog);
    // Find stuff
    int findParent(int catalog_id, QString full_path);
    bool dirEntryExists(QString full_path);
    void connect();
    QSqlQuery fetchCatalogs();
    QSqlQuery fetchDirectoryTree(int cat_id, int parent_id);
    QSqlQuery fetchFiles(int parent_id);
    QSqlQuery allFiles(int cat_id);
    QSqlQuery searchFiles(QString keyword, bool and_join, int cat_id);
    bool deleteFiles(int cat_id, QVector<int> files);
    QString formatSQL(QString keyword);
    DirEntry getDirentry(int id);
    int getRootId(int cat_id);

      private:
    QSqlDatabase m_db;
    QString db_path;
    void createTables();
};

#endif // DBMANAGER_H
