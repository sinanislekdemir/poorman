#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include "dbmanager.h"
#include "scanner.h"
#include "thumbnailqueue.h"
#include <QCheckBox>
#include <QComboBox>
#include <QDockWidget>
#include <QFileIconProvider>
#include <QHash>
#include <QLabel>
#include <QLineEdit>
#include <QMainWindow>
#include <QPixmap>
#include <QPointer>
#include <QPoint>
#include <QProgressBar>
#include <QResizeEvent>
#include <QSettings>
#include <QSplitter>
#include <QToolBar>

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

      protected:
	void closeEvent(QCloseEvent *event) override;
	void resizeEvent(QResizeEvent *event) override;

      private slots:
	void AddPath();
	void AddPathFast();
	void SaveAs();
	void OpenDB();
	void ShowSelectedCatalog();
	void ShowSelectedDirectory();
	void SelectCatalogByID(int id);
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
	void updateScanState(bool running);
	void showScanError(QString message);
	void toggleTheme();
	void focusSearch();
	void fileListContextMenuRequested(QPoint);
	void openContainingFolder();
	void copyFullPath();
	void copyFileName();

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
	int selected_catalog;
	bool in_search_mode;
	bool current_search_and_join;
	Ui::MainWindow *ui;

	QToolBar *toolBar;
	QComboBox *catalogList;
	QLineEdit *searchInput;
	QComboBox *searchModeBox;
	QCheckBox *previewToggle;
	QLabel *toolbarHintLabel;
	QLabel *thumbStatusLabel;
	QProgressBar *scanProgress;
	QLabel *emptyStateLabel;
	QSplitter *browserSplitter;
	QAction *searchAction;
	QAction *browseAction;
	QAction *helpAction;

	QDockWidget *previewDock;
	QLabel *previewImage;
	QLabel *previewName;
	QLabel *previewMeta;
	QPixmap previewSource;

	void applyModernUi();
	void setupToolBar();
	void setupStatusBar();
	void setupPreviewDock();
	void setupIcons();
	void updateThemeAction();
	void buildTree(QTreeWidgetItem *parent, int catalog_id, int parent_id);
	QIcon getCachedFileIcon(const QString &full_path);
	void updatePreviewForCurrentRow();
	void updatePreviewImage();
	void clearPreview();
	void executeSearch(const QString &text, bool and_join);
	void updateBrowseContext();
	void updateResultsSummary(int row_count);
	void updateEmptyState(int row_count);
	void loadSettings();
	void saveSettings();
};

class FileSizeColumn : public QTableWidgetItem {
      public:
	bool operator<(const QTableWidgetItem &other) const;
};

#endif // MAINWINDOW_H
