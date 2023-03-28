#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include "dbmanager.h"
#include "scanner.h"
#include <QMainWindow>

QT_BEGIN_NAMESPACE
namespace Ui {
class MainWindow;
}
QT_END_NAMESPACE

class MainWindow : public QMainWindow {
	Q_OBJECT

      public:
	MainWindow(QWidget *parent = nullptr);
	~MainWindow();
	void refresh();
	void ShowFiles(QSqlQuery data, bool fullname);

      private slots:
	void AddPath();
	void SaveAs();
	void OpenDB();
	void ShowSelectedCatalog();
	void ShowSelectedDirectory();
	void SelectCatalogByID(int id);
	void SearchFile();
	void ShowThumbnail();
	void Quit();
	void ShowAbout();
	void createPathEntry(QString string);
	void catalogContextMenuRequested(QPoint);
	void rescanCatalog();
	void deleteCatalog();

      private:
	QString db_file_path;

	Scanner *scanner;
	DBManager *db;
	QIcon folderIcon;
	QIcon driveIcon;
	int selected_catalog;
	Ui::MainWindow *ui;

	void buildTree(QTreeWidgetItem *parent, int catalog_id, int parent_id);
};

class FileSizeColumn : public QTableWidgetItem {
      public:
	bool operator<(const QTableWidgetItem &other) const;
};

#endif // MAINWINDOW_H
