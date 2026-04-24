#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include "dbmanager.h"
#include "scanner.h"
#include "thumbnailqueue.h"
#include <QCheckBox>
#include <QFileIconProvider>
#include <QHash>
#include <QMainWindow>
#include <QPointer>
#include <QPoint>

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

	enum FileListDataRole {
		SecondaryTextRole = Qt::UserRole + 1,
		EntryIdRole = Qt::UserRole + 2
	};

      private slots:
	void AddPath();
	void AddPathFast();
	void SaveAs();
	void OpenDB();
	void ShowSelectedCatalog();
	void ShowSelectedDirectory();
	void SelectCatalogByID(int id);
	void SearchFile();
	void ClearSearch();
	void ShowThumbnail();
	void Quit();
	void ShowAbout();
	void ShowSearchHelp();
	void createPathEntry(QString string);
	void catalogContextMenuRequested(QPoint);
	void rescanCatalog();
	void deleteCatalog();
	void updateThumbnailQueueStatus(int size);
	void toggleCatalogPanel(bool expanded);

      private:
	QString db_file_path;
	QString current_search_text;

	Scanner *scanner;
	DBManager *db;
	ThumbnailQueue *thumbQueue;
	QIcon folderIcon;
	QIcon driveIcon;
	QFileIconProvider iconProvider;
	QHash<QString, QIcon> fileIconCache;
	QHash<int, QString> catalogNameCache;
	QPointer<QCheckBox> previewToggle;
	QPointer<QDialog> previewPopup;
	QPoint previewPopupPosition;
	bool hasPreviewPopupPosition;
	int selected_catalog;
	bool in_search_mode;
	bool current_search_and_join;
	Ui::MainWindow *ui;

	void applyModernUi();
	void buildTree(QTreeWidgetItem *parent, int catalog_id, int parent_id);
	QIcon getCachedFileIcon(const QString &full_path);
	void closePreviewPopup();
	void executeSearch(const QString &text, bool and_join);
	void updateBrowseContext();
	void updateResultsSummary(int row_count);
};

class FileSizeColumn : public QTableWidgetItem {
      public:
	bool operator<(const QTableWidgetItem &other) const;
};

#endif // MAINWINDOW_H
