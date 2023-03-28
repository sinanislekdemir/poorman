#include "mainwindow.h"
#include "about.h"
#include "scanner.h"
#include "ui_mainwindow.h"
#include <QDir>
#include <QFile>
#include <QFileIconProvider>
#include <QFileInfo>
#include <QInputDialog>
#include <QSqlQuery>
#include <QtWidgets>
#include <cinttypes>
#include <cstdint>
#include <cstdlib>
#include <memory>

QString humanSize(uint64_t bytes) {
	QString suffix[] = {"B", "KB", "MB", "GB", "TB"};
	char length = sizeof(suffix) / sizeof(suffix[0]);

	int i = 0;
	double dblBytes = bytes;

	if (bytes > 1024) {
		for (i = 0; (bytes / 1024) > 0 && i < length - 1; i++, bytes /= 1024)
			dblBytes = bytes / 1024.0;
	}

	QString res = "%1 %2";
	return res.arg(QString::number(dblBytes), suffix[i]);
}

MainWindow::MainWindow(QWidget *parent) : QMainWindow(parent), ui(new Ui::MainWindow) {
	ui->setupUi(this);
	ui->imageLabel->hide();
	connect(ui->actionAdd_path, &QAction::triggered, this, &MainWindow::AddPath);
	connect(ui->actionSave_catalog_file, &QAction::triggered, this, &MainWindow::SaveAs);
	connect(ui->catalogList, &QListWidget::itemSelectionChanged, this, &MainWindow::ShowSelectedCatalog);
	connect(ui->directoryTree, &QTreeWidget::itemSelectionChanged, this, &MainWindow::ShowSelectedDirectory);
	connect(ui->fileList, &QTableWidget::itemSelectionChanged, this, &MainWindow::ShowThumbnail);
	connect(ui->actionOpen_catalog_file, &QAction::triggered, this, &MainWindow::OpenDB);
	connect(ui->searchLine, &QLineEdit::editingFinished, this, &MainWindow::SearchFile);
	connect(ui->actionQuit, &QAction::triggered, this, &MainWindow::Quit);
	connect(ui->actionAbout, &QAction::triggered, this, &MainWindow::ShowAbout);
	this->db_file_path = QDir::home().absolutePath() + "/poorman.sqlite";
	db = new DBManager(this->db_file_path);
	this->scanner = new Scanner(this, db_file_path);
	connect(this->scanner, SIGNAL(setProgressFilename(QString)), this, SLOT(createPathEntry(QString)));
	folderIcon = QIcon::fromTheme("folder");
	driveIcon = QIcon::fromTheme("accessories-dictionary");
	ui->searchCondition->addItem("Search all");
	ui->searchCondition->addItem("Search any");
	ui->catalogList->setContextMenuPolicy(Qt::CustomContextMenu);
	connect(ui->catalogList, SIGNAL(customContextMenuRequested(QPoint)), this, SLOT(catalogContextMenuRequested(QPoint)));
	selected_catalog = -2;
	refresh();
}

void MainWindow::Quit() { QCoreApplication::quit(); }

void MainWindow::catalogContextMenuRequested(QPoint pos) {
	if (ui->catalogList->currentItem() == NULL) {
		return;
	}
	QMenu *menu = new QMenu(this);
	QAction *rescanPath = new QAction(tr("Re-scan catalog for new files"), this);
	QAction *deleteCatalog = new QAction(tr("Re-scan catalog for deleted files"), this);
	connect(rescanPath, &QAction::triggered, this, &MainWindow::rescanCatalog);
	connect(deleteCatalog, &QAction::triggered, this, &MainWindow::deleteCatalog);
	menu->addAction(rescanPath);
	menu->addAction(deleteCatalog);
	menu->popup(ui->catalogList->viewport()->mapToGlobal(pos));
}

void MainWindow::rescanCatalog() {
	if (this->scanner->running()) {
		QMessageBox box;
		box.setText(tr("There is an active scanner running"));
		box.setIcon(QMessageBox::Warning);
		box.setStandardButtons(QMessageBox::Ok);
		box.exec();
		return;
	}
	QString selected = ui->catalogList->currentItem()->text();
	QString path = "";

	QSqlQuery catalogs = db->fetchCatalogs();
	while (catalogs.next()) {
		if (catalogs.value("name").toString() == selected) {
			path = catalogs.value("original_path").toString();
			QDir dir(path);
			if (!dir.exists()) {
				QMessageBox box;
				box.setText(tr("Catalog path is not reachable") + "\n" + path);
				box.setIcon(QMessageBox::Warning);
				box.setStandardButtons(QMessageBox::Ok);
				box.exec();
				return;
			}
			ui->statusbar->showMessage(tr("Scanning: ") + path);
			this->scanner->setPath(path);
			this->scanner->setCatalogId(catalogs.value("ids").toInt());
			this->scanner->start();
		}
	}
};

void MainWindow::deleteCatalog() {
	QString selected = ui->catalogList->currentItem()->text();
	QString path = "";
	QVector<int> ids;
	int catalog_id = -1;
	QSqlQuery catalogs = db->fetchCatalogs();
	while (catalogs.next()) {
		if (catalogs.value("name").toString() == selected) {
			path = catalogs.value("original_path").toString();
			QDir dir(path);
			if (!dir.exists()) {
				QMessageBox box;
				box.setText(tr("Catalog path is not reachable") + "\n" + path);
				box.setIcon(QMessageBox::Warning);
				box.setStandardButtons(QMessageBox::Ok);
				box.exec();
				return;
			}
			catalog_id = catalogs.value("ids").toInt();
			QSqlQuery files = db->allFiles(catalog_id);
			while (files.next()) {
				QString full_path = files.value("full_path").toString();
				QFile fpath(full_path);
				ui->statusbar->showMessage(tr("Check: ") + full_path);
				QApplication::processEvents();
				if (!fpath.exists()) {
					ui->statusbar->showMessage(tr("Gone: ") + full_path);
					ids.append(files.value("ids").toInt());
				}
			}
		}
	}
	if (catalog_id != -1) {
		if (ids.length() > 0) {
			ui->statusbar->showMessage(tr("Deleting old entries:") + QString::number(ids.length()));
			db->deleteFiles(catalog_id, ids);
			refresh();
		}
	}
};

void MainWindow::ShowAbout() {
	About about;
	about.setModal(true);
	about.exec();
}

void MainWindow::ShowThumbnail() {
	int row = ui->fileList->currentRow();
	QTableWidgetItem *w = ui->fileList->item(row, 2);
	int id = w->data(Qt::UserRole).toInt();

	QTableWidgetItem *fname_widget = ui->fileList->item(row, 0);
	int catalog_id = fname_widget->data(Qt::UserRole).toInt();

	if (catalog_id != selected_catalog)
		SelectCatalogByID(catalog_id);

	DirEntry d = db->getDirentry(id);
	ui->statusbar->showMessage(d.full_path);
	if (!d.thumbnail.isEmpty()) {
		QPixmap map = QPixmap();
		map.loadFromData(d.thumbnail);
		ui->imageLabel->setScaledContents(true);
		ui->imageLabel->setPixmap(map);
		ui->imageLabel->show();
	} else {
		ui->imageLabel->clear();
		ui->imageLabel->hide();
	}
}

void MainWindow::buildTree(QTreeWidgetItem *parent, int catalog_id, int parent_id) {
	if (parent->childCount() > 0) {
		return;
	}
	QSqlQuery dir_tree = db->fetchDirectoryTree(catalog_id, parent_id);
	while (dir_tree.next()) {
		QString label = dir_tree.value("name").toString();
		QTreeWidgetItem *item = new QTreeWidgetItem();
		item->setData(0, Qt::UserRole, dir_tree.value("ids").toInt());
		item->setText(0, label);
		item->setIcon(0, folderIcon);
		ui->statusbar->showMessage(label);
		parent->addChild(item);
	}
	QApplication::processEvents();
}

void MainWindow::SearchFile() {
	int catalog_id = selected_catalog >= 0 ? selected_catalog : -1;
	QString text = ui->searchLine->text();
	QString condition = ui->searchCondition->currentText();
	bool and_join = condition == "Search all";

	QSqlQuery files = db->searchFiles(text, and_join, catalog_id);
	ShowFiles(files, true);
}

void MainWindow::ShowSelectedDirectory() {
	int dir_id = ui->directoryTree->currentItem()->data(0, Qt::UserRole).toInt();

	buildTree(ui->directoryTree->currentItem(), selected_catalog, dir_id);
	QSqlQuery files = db->fetchFiles(dir_id);
	ShowFiles(files, false);
}

void MainWindow::SelectCatalogByID(int id) {
	QSqlQuery catalogs = db->fetchCatalogs();
	ui->directoryTree->clear();
	selected_catalog = id;
	for (int i = 0; i < ui->catalogList->count(); i++) {
		QListWidgetItem *item = ui->catalogList->item(i);
		if (item->data(Qt::UserRole).toInt() == id) {
			item->setSelected(true);
			ui->catalogList->setCurrentItem(item);
		} else {
			item->setSelected(false);
		}
	}

	ShowSelectedCatalog();
}

void MainWindow::ShowSelectedCatalog() {
	QSqlQuery catalogs = db->fetchCatalogs();
	QListWidgetItem *item = ui->catalogList->currentItem();
	int catalog_id = -1;
	if (item != NULL && item->isSelected()) {
		catalog_id = item->data(Qt::UserRole).toInt();
	}
	ui->directoryTree->clear();
	selected_catalog = catalog_id;
	QTreeWidgetItem *it = new QTreeWidgetItem();
	it->setText(0, item == NULL ? "Root" : item->text());
	it->setIcon(0, driveIcon);
	int root_id = db->getRootId(catalog_id);
	it->setData(0, Qt::UserRole, root_id);
	buildTree(it, catalog_id, root_id);
	ui->directoryTree->addTopLevelItem(it);
}

void MainWindow::refresh() {
	ui->catalogList->clear();
	QSqlQuery catalogs = db->fetchCatalogs();
	while (catalogs.next()) {
		QListWidgetItem *item = new QListWidgetItem();
		item->setText(catalogs.value("name").toString());
		item->setIcon(driveIcon);
		item->setData(Qt::UserRole, catalogs.value("ids").toInt());
		ui->catalogList->addItem(item);
	}
	ui->directoryTree->clear();
	ui->fileList->clear();
	ui->fileList->setRowCount(0);
}

void MainWindow::SaveAs() {
	QString filename = QFileDialog::getSaveFileName(this, tr("Save catalog database"), "", "SQLite DB (*.sqlite)");
	if (filename.isEmpty()) {
		return;
	}
	delete db;
	QFile::copy(db_file_path, filename);
	this->db_file_path = filename;
	db = new DBManager(this->db_file_path);
	this->refresh();
}

void MainWindow::OpenDB() {
	QString filename = QFileDialog::getOpenFileName(this, tr("Open catalog database"), "", "SQLite DB (*.sqlite)");
	if (filename.isEmpty()) {
		return;
	}
	delete db;
	this->db_file_path = filename;
	db = new DBManager(this->db_file_path);
	delete this->scanner;
	this->scanner = new Scanner(this, db_file_path);
	connect(this->scanner, SIGNAL(setProgressFilename(QString)), this, SLOT(createPathEntry(QString)));
	this->refresh();
}

bool FileSizeColumn::operator<(const QTableWidgetItem &other) const {
	if (text() == "")
		return data(Qt::UserRole).toInt() > other.data(Qt::UserRole).toInt();
	else
		return data(Qt::UserRole).toInt() < other.data(Qt::UserRole).toInt();
};

void MainWindow::ShowFiles(QSqlQuery data, bool fullname) {
	ui->fileList->clear();
	ui->fileList->setColumnCount(3);
	ui->fileList->setRowCount(0);

	ui->fileList->setSortingEnabled(true);
	QHeaderView *headerView = ui->fileList->horizontalHeader();
	headerView->setSectionResizeMode(0, QHeaderView::Stretch);
	headerView->sortIndicatorOrder();
	headerView->setSortIndicatorShown(true);

	QTableWidgetItem *h_filename = new QTableWidgetItem();
	h_filename->setText("File name");
	ui->fileList->setHorizontalHeaderItem(0, h_filename);

	QTableWidgetItem *h_filesize = new FileSizeColumn();
	h_filesize->setText("File size");
	h_filesize->setData(Qt::UserRole, -1);
	ui->fileList->setHorizontalHeaderItem(1, h_filesize);

	QTableWidgetItem *h_thumbnail = new QTableWidgetItem();
	h_thumbnail->setText("Has thumbnail");
	ui->fileList->setHorizontalHeaderItem(2, h_thumbnail);

	int row = 0;

	QFileIconProvider ip;
	while (data.next()) {
		ui->fileList->setRowCount(row + 1);
		QTableWidgetItem *fname = new QTableWidgetItem();
		QFileInfo inf(data.value("full_path").toString());
		fname->setIcon(ip.icon(inf));
		fname->setData(Qt::UserRole, data.value("catalog_id"));
		if (fullname) {
			fname->setText(inf.absoluteFilePath());
		} else {
			fname->setText(inf.completeBaseName());
		}

		ui->fileList->setItem(row, 0, fname);

		QTableWidgetItem *fsize = new FileSizeColumn();
		fsize->setData(Qt::UserRole, data.value("filesize").toInt());
		fsize->setText(humanSize(data.value("filesize").toInt()));
		ui->fileList->setItem(row, 1, fsize);

		QTableWidgetItem *fthumb = new QTableWidgetItem();
		if (data.value("thumbnail64").isNull()) {
			fthumb->setCheckState(Qt::CheckState::Unchecked);
		} else {
			fthumb->setCheckState(Qt::CheckState::Checked);
		}
		fthumb->setData(Qt::UserRole, data.value("ids").toInt());
		ui->fileList->setItem(row, 2, fthumb);

		row++;
	}
}

void MainWindow::AddPath() {
	QString filename = QFileDialog::getExistingDirectory(this, tr("Choose directory"));
	if (filename.isEmpty()) {
		QMessageBox box;
		box.setText(tr("No directory selected"));
		box.setIcon(QMessageBox::Information);
		box.setStandardButtons(QMessageBox::Ok);
		box.exec();
		return;
	}
	this->scanner->setPath(filename);
	if (this->scanner->running()) {
		QMessageBox box;
		box.setText(tr("There is an active scanner running"));
		box.setIcon(QMessageBox::Warning);
		box.setStandardButtons(QMessageBox::Ok);
		box.exec();
		return;
	}
	QFileInfo finfo(filename);
	bool ok;
	QString catalog_name = QInputDialog::getText(this, tr("Catalog Name"), tr("Give a name to this catalog"),
						     QLineEdit::EchoMode::Normal, finfo.baseName(), &ok);
	if (ok && !catalog_name.isEmpty()) {
		this->scanner->setCatalogName(catalog_name);
		this->scanner->start();
	} else {
		ui->statusbar->showMessage("Cancelled");
	}
}

MainWindow::~MainWindow() {
	delete db;
	delete ui;
	delete scanner;
}

void MainWindow::createPathEntry(QString string) {
	if (string.endsWith("/.") || string.endsWith("/..")) {
		return;
	}
	ui->statusbar->showMessage(string);
	if (string == "finished") {
		delete db;
		db = new DBManager(this->db_file_path);
		refresh();
	}
}
