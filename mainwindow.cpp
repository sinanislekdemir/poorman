#include "mainwindow.h"
#include "about.h"
#include "scanner.h"
#include "ui_mainwindow.h"
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QInputDialog>
#include <QSqlQuery>
#include <QtWidgets>
#include <cinttypes>
#include <cstdint>
#include <cstdlib>
#include <memory>

namespace {
class FileListDelegate : public QStyledItemDelegate {
      public:
	using QStyledItemDelegate::QStyledItemDelegate;

	QSize sizeHint(const QStyleOptionViewItem &option, const QModelIndex &index) const override {
		QSize size = QStyledItemDelegate::sizeHint(option, index);
		if (index.column() == 0) {
			size.setHeight(58);
		}
		return size;
	}

	void paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const override {
		if (index.column() != 0) {
			QStyledItemDelegate::paint(painter, option, index);
			return;
		}

		QStyleOptionViewItem opt(option);
		initStyleOption(&opt, index);
		const QString primary_text = opt.text;
		const QString secondary_text = index.data(MainWindow::SecondaryTextRole).toString();
		const QIcon icon = qvariant_cast<QIcon>(index.data(Qt::DecorationRole));
		const QWidget *widget = option.widget;
		QStyle *style = widget ? widget->style() : QApplication::style();

		opt.text.clear();
		opt.icon = QIcon();

		painter->save();
		style->drawPrimitive(QStyle::PE_PanelItemViewItem, &opt, painter, widget);

		QRect content_rect = option.rect.adjusted(14, 7, -14, -7);
		QRect icon_rect(content_rect.left(), content_rect.top() + 6, 18, 18);
		icon.paint(painter, icon_rect, Qt::AlignCenter,
			   option.state & QStyle::State_Enabled ? QIcon::Normal : QIcon::Disabled);

		QRect text_rect = content_rect.adjusted(30, 0, 0, 0);
		QColor primary_color =
		    option.state & QStyle::State_Selected ? opt.palette.color(QPalette::HighlightedText) : QColor("#E5EEF9");
		QColor secondary_color = option.state & QStyle::State_Selected ? QColor("#DBEAFE") : QColor("#8FA1B7");

		QFont primary_font = opt.font;
		primary_font.setWeight(QFont::DemiBold);
		painter->setFont(primary_font);
		painter->setPen(primary_color);
		const QString primary_line =
		    painter->fontMetrics().elidedText(primary_text, Qt::ElideMiddle, text_rect.width());
		painter->drawText(text_rect.adjusted(0, 1, 0, -18), Qt::AlignLeft | Qt::AlignVCenter, primary_line);

		QFont secondary_font = opt.font;
		secondary_font.setPointSizeF(qMax(8.0, secondary_font.pointSizeF() - 1.0));
		painter->setFont(secondary_font);
		painter->setPen(secondary_color);
		const QString secondary_line =
		    painter->fontMetrics().elidedText(secondary_text, Qt::ElideMiddle, text_rect.width());
		painter->drawText(text_rect.adjusted(0, 20, 0, 0), Qt::AlignLeft | Qt::AlignVCenter, secondary_line);
		painter->restore();
	}
};
} // namespace

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
	applyModernUi();
	connect(ui->actionAdd_path, &QAction::triggered, this, &MainWindow::AddPath);
	connect(ui->addPathNoThumb, &QAction::triggered, this, &MainWindow::AddPathFast);
	connect(ui->actionSave_catalog_file, &QAction::triggered, this, &MainWindow::SaveAs);
	connect(ui->catalogList, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
		[this](int) { ShowSelectedCatalog(); });
	connect(ui->directoryTree, &QTreeWidget::itemSelectionChanged, this, &MainWindow::ShowSelectedDirectory);
	connect(ui->fileList, &QTableWidget::itemSelectionChanged, this, &MainWindow::ShowThumbnail);
	connect(ui->actionOpen_catalog_file, &QAction::triggered, this, &MainWindow::OpenDB);
	connect(ui->searchButton, &QPushButton::clicked, this, &MainWindow::SearchFile);
	connect(ui->clearSearchButton, &QPushButton::clicked, this, &MainWindow::ClearSearch);
	connect(ui->searchHelpButton, &QPushButton::clicked, this, &MainWindow::ShowSearchHelp);
	connect(ui->actionQuit, &QAction::triggered, this, &MainWindow::Quit);
	connect(ui->actionAbout, &QAction::triggered, this, &MainWindow::ShowAbout);
	this->db_file_path = QDir::home().absolutePath() + "/poorman.sqlite";
	db = new DBManager(this->db_file_path);
	thumbQueue = new ThumbnailQueue(this, db_file_path);
	connect(thumbQueue, &ThumbnailQueue::queueSizeChanged, this, &MainWindow::updateThumbnailQueueStatus);
	this->scanner = new Scanner(this, db_file_path);
	this->scanner->setThumbnailQueue(thumbQueue);
	connect(this->scanner, SIGNAL(setProgressFilename(QString)), this, SLOT(createPathEntry(QString)));
	folderIcon = iconProvider.icon(QFileIconProvider::Folder);
	driveIcon = iconProvider.icon(QFileIconProvider::Drive);
	ui->catalogList->setContextMenuPolicy(Qt::CustomContextMenu);
	connect(ui->catalogList, SIGNAL(customContextMenuRequested(QPoint)), this, SLOT(catalogContextMenuRequested(QPoint)));
	selected_catalog = -2;
	in_search_mode = false;
	current_search_and_join = true;
	hasPreviewPopupPosition = false;
	refresh();
}

void MainWindow::Quit() { QCoreApplication::quit(); }

void MainWindow::applyModernUi() {
	ui->catalogList->setIconSize(QSize(18, 18));
	ui->catalogList->setSizeAdjustPolicy(QComboBox::AdjustToContents);
	ui->catalogList->setInsertPolicy(QComboBox::NoInsert);
	ui->catalogList->setMinimumContentsLength(1);
	ui->horizontalLayout->setSpacing(2);
	ui->horizontalLayout->setContentsMargins(2, 2, 2, 2);
	ui->verticalLayout_4->setSpacing(2);
	ui->verticalLayout_4->setContentsMargins(2, 2, 2, 2);
	ui->verticalLayout_3->setSpacing(2);
	ui->verticalLayout_3->setContentsMargins(0, 0, 0, 0);
	ui->verticalLayout_2->setContentsMargins(0, 0, 0, 0);
	ui->verticalLayout->setSpacing(2);
	ui->verticalLayout->setContentsMargins(0, 0, 0, 0);
	ui->horizontalLayout_5->setSpacing(2);
	ui->horizontalLayout_5->setContentsMargins(3, 2, 3, 2);
	ui->horizontalLayout_3->setSpacing(2);
	ui->verticalLayout_5->setSpacing(2);
	ui->verticalLayout_5->setContentsMargins(3, 3, 3, 3);
	ui->verticalLayout_6->setSpacing(2);
	ui->verticalLayout_6->setContentsMargins(3, 3, 3, 3);
	ui->horizontalLayout_6->setSpacing(2);
	ui->toolbarFrame->setMaximumHeight(38);
	ui->horizontalLayout_5->insertWidget(1, ui->catalogList, 0);
	previewToggle = new QCheckBox(tr("Enable preview"), ui->toolbarFrame);
	previewToggle->setChecked(false);
	ui->horizontalLayout_5->insertWidget(3, previewToggle);
	connect(previewToggle, &QCheckBox::toggled, this, [this](bool enabled) {
		if (!enabled) {
			closePreviewPopup();
		}
	});
	ui->leftPanel->hide();
	ui->catalogToggleButton->hide();
	ui->directoryTree->setAnimated(true);
	ui->directoryTree->setIndentation(14);
	ui->directoryTree->setUniformRowHeights(true);
	ui->fileList->setAlternatingRowColors(true);
	ui->fileList->setShowGrid(false);
	ui->fileList->setSelectionBehavior(QAbstractItemView::SelectRows);
	ui->fileList->setSelectionMode(QAbstractItemView::SingleSelection);
	ui->fileList->setEditTriggers(QAbstractItemView::NoEditTriggers);
	ui->fileList->setIconSize(QSize(18, 18));
	ui->fileList->verticalHeader()->setVisible(false);
	ui->fileList->verticalHeader()->setDefaultSectionSize(58);
	ui->fileList->setItemDelegateForColumn(0, new FileListDelegate(ui->fileList));
	ui->clearSearchButton->setEnabled(false);
	ui->resultsSummaryLabel->setText(tr("Select a folder or run a search"));
	ui->foldersSubtitleLabel->setText(tr("Choose a catalog to browse"));
	ui->toolbarHintLabel->setText(tr("Catalog"));

	if (QHBoxLayout *mid_layout = qobject_cast<QHBoxLayout *>(ui->midSection->layout())) {
		QSplitter *browser_splitter = new QSplitter(Qt::Horizontal, ui->midSection);
		browser_splitter->setObjectName("browserSplitter");
		mid_layout->removeWidget(ui->directoryPanel);
		mid_layout->removeWidget(ui->filePanel);
		browser_splitter->addWidget(ui->directoryPanel);
		browser_splitter->addWidget(ui->filePanel);
		browser_splitter->setChildrenCollapsible(false);
		browser_splitter->setHandleWidth(10);
		browser_splitter->setStretchFactor(0, 0);
		browser_splitter->setStretchFactor(1, 1);
		browser_splitter->setSizes(QList<int>() << 220 << 720);
		mid_layout->addWidget(browser_splitter);
	}

	setStyleSheet(R"(
QMainWindow, QWidget#centralwidget {
	background-color: #0B1220;
	color: #E2E8F0;
}

QMenuBar {
	background-color: #0F172A;
	color: #E2E8F0;
	border-bottom: 1px solid #1E293B;
}

QMenuBar::item {
	background: transparent;
	padding: 6px 10px;
	border-radius: 8px;
}

QMenuBar::item:selected,
QMenu::item:selected {
	background-color: #1D4ED8;
	color: #F8FAFC;
}

QMenu {
	background-color: #111827;
	color: #E2E8F0;
	border: 1px solid #1F2937;
}

QFrame#directoryPanel,
QFrame#filePanel,
QFrame#toolbarFrame {
	background-color: #111827;
	border: 1px solid #1F2937;
	border-radius: 8px;
}

QLabel#foldersTitleLabel,
QLabel#resultsTitleLabel {
	color: #F8FAFC;
	font-size: 14px;
	font-weight: 700;
}

QLabel#foldersSubtitleLabel {
	color: #8FA1B7;
}

QLabel#resultsSummaryLabel {
	color: #7DD3FC;
	font-weight: 600;
}

QLabel#toolbarHintLabel {
	color: #8FA1B7;
}

QLabel#imageLabel {
	background-color: #0F172A;
	border: 1px solid #1F2937;
	border-radius: 8px;
	padding: 2px;
}

QListWidget:focus,
QTreeWidget:focus,
QTableWidget:focus {
	border: 1px solid #3B82F6;
}

QPushButton {
	background-color: #1D4ED8;
	border: none;
	border-radius: 8px;
	color: #F8FAFC;
	padding: 3px 8px;
	font-weight: 600;
}

QPushButton:hover {
	background-color: #2563EB;
}

QPushButton#clearSearchButton {
	background-color: #172033;
}

QPushButton#searchHelpButton {
	background-color: #172033;
	min-width: 28px;
	max-width: 28px;
	padding: 5px 0;
}

QComboBox {
	background-color: #0F172A;
	border: 1px solid #1F2937;
	border-radius: 8px;
	padding: 2px 6px;
	color: #E2E8F0;
}

QComboBox::drop-down {
	border: none;
	width: 22px;
}

QCheckBox {
	color: #CBD5E1;
	spacing: 5px;
	padding: 0 4px;
}

QCheckBox::indicator {
	width: 14px;
	height: 14px;
}

QListWidget,
QTreeWidget,
QTableWidget {
	background-color: #0F172A;
	alternate-background-color: #131C2E;
	border: 1px solid #1F2937;
	border-radius: 8px;
	padding: 1px;
	selection-background-color: #1D4ED8;
	selection-color: #F8FAFC;
	outline: 0;
}

QListWidget::item,
QTreeWidget::item,
QTableWidget::item {
	border-radius: 6px;
	padding: 2px 3px;
}

QHeaderView::section {
	background-color: #111827;
	color: #8FA1B7;
	border: none;
	border-bottom: 1px solid #1F2937;
	padding: 4px 6px;
	font-weight: 600;
}

QSplitter::handle {
	background-color: #334155;
	border-radius: 1px;
}

QSplitter::handle:horizontal {
	width: 10px;
}

QScrollBar:vertical,
QScrollBar:horizontal {
	background: transparent;
	border: none;
	margin: 4px;
}

QScrollBar::handle:vertical,
QScrollBar::handle:horizontal {
	background-color: #334155;
	border-radius: 6px;
	min-height: 28px;
	min-width: 28px;
}

QScrollBar::add-line,
QScrollBar::sub-line,
QScrollBar::add-page,
QScrollBar::sub-page {
	background: transparent;
	border: none;
}

QStatusBar {
	background-color: #0F172A;
	color: #8FA1B7;
	border-top: 1px solid #1F2937;
}
)");
}

void MainWindow::catalogContextMenuRequested(QPoint pos) {
	if (ui->catalogList->currentIndex() < 0) {
		return;
	}
	QMenu *menu = new QMenu(this);
	QAction *rescanPath = new QAction(tr("Re-scan catalog for new files"), this);
	QAction *deleteCatalog = new QAction(tr("Re-scan catalog for deleted files"), this);
	connect(rescanPath, &QAction::triggered, this, &MainWindow::rescanCatalog);
	connect(deleteCatalog, &QAction::triggered, this, &MainWindow::deleteCatalog);
	menu->addAction(rescanPath);
	menu->addAction(deleteCatalog);
	menu->popup(ui->catalogList->mapToGlobal(pos));
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
	QString selected = ui->catalogList->currentText();
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
	QString selected = ui->catalogList->currentText();
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

void MainWindow::ShowSearchHelp() {
	QMessageBox helpBox;
	helpBox.setWindowTitle(tr("Search Help"));
	helpBox.setTextFormat(Qt::RichText);
	helpBox.setText(
	    tr("<h3>Search Syntax</h3>"
	       "<p><b>Basic Search:</b><br>"
	       "Type keywords separated by spaces to search file names and paths.</p>"
	       "<p><b>Search Modes:</b><br>"
	       "• <b>Search all</b> - Files must contain ALL keywords<br>"
	       "• <b>Search any</b> - Files containing ANY keyword</p>"
	       "<p><b>Examples:</b><br>"
	       "• <code>vacation 2023</code> - finds files with both 'vacation' AND '2023'<br>"
	       "• <code>jpg png</code> - with 'Search any' finds all .jpg OR .png files<br>"
	       "• <code>report final</code> - finds files containing both words</p>"
	       "<p><b>Tips:</b><br>"
	       "• Search is case-insensitive<br>"
	       "• Searches in full file path (directory + filename)<br>"
	       "• Use specific keywords for better results</p>"));
	helpBox.setIcon(QMessageBox::Information);
	helpBox.setStandardButtons(QMessageBox::Ok);
	helpBox.exec();
}

void MainWindow::ShowThumbnail() {
	int row = ui->fileList->currentRow();
	if (row < 0) {
		closePreviewPopup();
		return;
	}
	QTableWidgetItem *fname_widget = ui->fileList->item(row, 0);
	if (fname_widget == NULL) {
		closePreviewPopup();
		return;
	}
	int id = fname_widget->data(EntryIdRole).toInt();
	int catalog_id = fname_widget->data(Qt::UserRole).toInt();

	if (in_search_mode && catalog_id != selected_catalog)
		SelectCatalogByID(catalog_id);

	DirEntry d = db->getDirentry(id);
	ui->statusbar->showMessage(d.full_path);
	closePreviewPopup();
	if (!previewToggle || !previewToggle->isChecked()) {
		return;
	}
	if (d.thumbnail.isEmpty()) {
		return;
	}

	QPixmap map;
	map.loadFromData(d.thumbnail);
	if (map.isNull()) {
		return;
	}

	QDialog *popup = new QDialog(this, Qt::Tool | Qt::WindowTitleHint | Qt::WindowCloseButtonHint | Qt::WindowStaysOnTopHint);
	popup->setAttribute(Qt::WA_DeleteOnClose);
	popup->setWindowTitle(QFileInfo(d.full_path).fileName());
	QVBoxLayout *layout = new QVBoxLayout(popup);
	layout->setContentsMargins(4, 4, 4, 4);

	QLabel *preview = new QLabel(popup);
	preview->setAlignment(Qt::AlignCenter);
	preview->setMinimumSize(240, 180);
	preview->setPixmap(map.scaled(720, 540, Qt::KeepAspectRatio, Qt::SmoothTransformation));
	layout->addWidget(preview);

	if (hasPreviewPopupPosition) {
		popup->move(previewPopupPosition);
	}
	previewPopup = popup;
	connect(popup, &QDialog::finished, this, [this, popup](int) {
		previewPopupPosition = popup->pos();
		hasPreviewPopupPosition = true;
		previewPopup = nullptr;
	});
	popup->show();
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
	QDialog dialog(this);
	dialog.setWindowTitle(tr("Search files"));
	dialog.setModal(true);

	QVBoxLayout *layout = new QVBoxLayout(&dialog);
	layout->setContentsMargins(10, 10, 10, 10);
	layout->setSpacing(6);

	QLabel *label = new QLabel(tr("Search file names and paths"), &dialog);
	QLineEdit *search_input = new QLineEdit(&dialog);
	search_input->setText(current_search_text);
	search_input->setPlaceholderText(tr("vacation 2023"));

	QComboBox *condition_box = new QComboBox(&dialog);
	condition_box->addItem(tr("Search all"));
	condition_box->addItem(tr("Search any"));
	condition_box->setCurrentIndex(current_search_and_join ? 0 : 1);

	layout->addWidget(label);
	layout->addWidget(search_input);
	layout->addWidget(condition_box);

	QDialogButtonBox *button_box = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
	QPushButton *clear_button = button_box->addButton(tr("Clear"), QDialogButtonBox::ResetRole);
	QPushButton *help_button = button_box->addButton(tr("Help"), QDialogButtonBox::HelpRole);
	layout->addWidget(button_box);

	connect(button_box, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
	connect(button_box, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
	connect(help_button, &QPushButton::clicked, this, &MainWindow::ShowSearchHelp);
	connect(clear_button, &QPushButton::clicked, &dialog, [&]() {
		search_input->clear();
		ClearSearch();
		dialog.reject();
	});

	search_input->setFocus();
	search_input->selectAll();
	if (dialog.exec() != QDialog::Accepted) {
		return;
	}

	executeSearch(search_input->text().trimmed(), condition_box->currentIndex() == 0);
}

void MainWindow::ClearSearch() {
	closePreviewPopup();
	current_search_text.clear();
	current_search_and_join = true;
	in_search_mode = false;
	ui->clearSearchButton->setEnabled(false);
	ui->toolbarHintLabel->setText(tr("Browse folders or run a search"));
	if (ui->directoryTree->currentItem() != NULL) {
		ShowSelectedDirectory();
	} else {
		ShowSelectedCatalog();
	}
}

void MainWindow::ShowSelectedDirectory() {
	if (ui->directoryTree->currentItem() == NULL) {
		return;
	}
	int dir_id = ui->directoryTree->currentItem()->data(0, Qt::UserRole).toInt();

	buildTree(ui->directoryTree->currentItem(), selected_catalog, dir_id);
	closePreviewPopup();
	QSqlQuery files = db->fetchFiles(dir_id, selected_catalog);
	in_search_mode = false;
	updateBrowseContext();
	ShowFiles(files, false);
}

void MainWindow::SelectCatalogByID(int id) {
	ui->directoryTree->clear();
	selected_catalog = id;
	for (int i = 0; i < ui->catalogList->count(); i++) {
		if (ui->catalogList->itemData(i, Qt::UserRole).toInt() == id) {
			ui->catalogList->setCurrentIndex(i);
			break;
		}
	}
}

void MainWindow::ShowSelectedCatalog() {
	int catalog_id = -1;
	if (ui->catalogList->currentIndex() >= 0) {
		catalog_id = ui->catalogList->currentData(Qt::UserRole).toInt();
	}
	ui->directoryTree->clear();
	ui->fileList->clearContents();
	ui->fileList->setRowCount(0);
	closePreviewPopup();
	in_search_mode = false;
	selected_catalog = catalog_id;
	ui->clearSearchButton->setEnabled(false);
	updateBrowseContext();
	updateResultsSummary(0);
	if (catalog_id < 0) {
		return;
	}
	QTreeWidgetItem *it = new QTreeWidgetItem();
	it->setText(0, ui->catalogList->currentText().isEmpty() ? "Root" : ui->catalogList->currentText());
	it->setIcon(0, driveIcon);
	int root_id = db->getRootId(catalog_id);
	it->setData(0, Qt::UserRole, root_id);
	buildTree(it, catalog_id, root_id);
	ui->directoryTree->addTopLevelItem(it);
	it->setExpanded(true);
}

void MainWindow::refresh() {
	ui->catalogList->clear();
	catalogNameCache.clear();
	QSqlQuery catalogs = db->fetchCatalogs();
	while (catalogs.next()) {
		const int catalog_id = catalogs.value("ids").toInt();
		const QString catalog_name = catalogs.value("name").toString();
		ui->catalogList->addItem(driveIcon, catalog_name, catalog_id);
		catalogNameCache.insert(catalog_id, catalog_name);
	}
	ui->directoryTree->clear();
	ui->fileList->clear();
	ui->fileList->setRowCount(0);
	closePreviewPopup();
	ui->resultsSummaryLabel->setText(ui->catalogList->count() > 0 ? tr("Pick a folder or search across a catalog")
								   : tr("Add a catalog to start browsing"));
	ui->foldersSubtitleLabel->setText(tr("Choose a catalog to browse"));
	ui->toolbarHintLabel->setText(tr("Catalog"));
	if (ui->catalogList->count() > 0) {
		ui->catalogList->setCurrentIndex(0);
		ShowSelectedCatalog();
	}
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
	fileIconCache.clear();
	delete thumbQueue;
	thumbQueue = new ThumbnailQueue(this, db_file_path);
	connect(thumbQueue, &ThumbnailQueue::queueSizeChanged, this, &MainWindow::updateThumbnailQueueStatus);
	delete this->scanner;
	this->scanner = new Scanner(this, db_file_path);
	this->scanner->setThumbnailQueue(thumbQueue);
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
	ui->fileList->clearContents();
	ui->fileList->setColumnCount(3);
	ui->fileList->setRowCount(0);

	ui->fileList->setSortingEnabled(false);
	QHeaderView *headerView = ui->fileList->horizontalHeader();
	headerView->setSectionResizeMode(0, QHeaderView::Stretch);
	headerView->setSectionResizeMode(1, QHeaderView::ResizeToContents);
	headerView->setSectionResizeMode(2, QHeaderView::ResizeToContents);
	headerView->sortIndicatorOrder();
	headerView->setSortIndicatorShown(true);

	QTableWidgetItem *h_filename = new QTableWidgetItem();
	h_filename->setText("File");
	ui->fileList->setHorizontalHeaderItem(0, h_filename);

	QTableWidgetItem *h_filesize = new FileSizeColumn();
	h_filesize->setText("Size");
	h_filesize->setData(Qt::UserRole, -1);
	ui->fileList->setHorizontalHeaderItem(1, h_filesize);

	QTableWidgetItem *h_thumbnail = new QTableWidgetItem();
	h_thumbnail->setText("Preview");
	ui->fileList->setHorizontalHeaderItem(2, h_thumbnail);

	int row = 0;
	while (data.next()) {
		ui->fileList->setRowCount(row + 1);
		QTableWidgetItem *fname = new QTableWidgetItem();
		QString full_path = data.value("full_path").toString();
		QFileInfo inf(full_path);
		QString catalog_name = catalogNameCache.value(data.value("catalog_id").toInt());
		QString secondary_text;

		if (fullname) {
			secondary_text = catalog_name.isEmpty()
					     ? QDir::toNativeSeparators(full_path)
					     : tr("%1  •  %2").arg(catalog_name, QDir::toNativeSeparators(full_path));
		} else {
			QString type_label = inf.suffix().isEmpty() ? tr("File") : inf.suffix().toUpper();
			secondary_text = tr("%1  •  %2").arg(type_label, QDir::toNativeSeparators(inf.absolutePath()));
		}

		fname->setIcon(getCachedFileIcon(full_path));
		fname->setData(Qt::UserRole, data.value("catalog_id"));
		fname->setData(EntryIdRole, data.value("ids").toInt());
		fname->setData(SecondaryTextRole, secondary_text);
		fname->setText(inf.fileName());
		fname->setToolTip(QDir::toNativeSeparators(full_path));

		ui->fileList->setItem(row, 0, fname);

		QTableWidgetItem *fsize = new FileSizeColumn();
		fsize->setData(Qt::UserRole, data.value("filesize").toInt());
		fsize->setText(humanSize(data.value("filesize").toInt()));
		fsize->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
		ui->fileList->setItem(row, 1, fsize);

		QTableWidgetItem *fthumb = new QTableWidgetItem();
		const bool has_thumbnail = data.value("has_thumbnail").toBool();
		fthumb->setText(has_thumbnail ? tr("Ready") : tr("None"));
		fthumb->setForeground(has_thumbnail ? QColor("#86EFAC") : QColor("#64748B"));
		fthumb->setTextAlignment(Qt::AlignCenter);
		ui->fileList->setItem(row, 2, fthumb);

		row++;
	}
	ui->fileList->setSortingEnabled(true);
	updateResultsSummary(row);
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
		this->scanner->setCatalogId(-1);
		this->scanner->setCatalogName(catalog_name);
		this->scanner->withThumbs(true);
		this->scanner->start();
	} else {
		ui->statusbar->showMessage("Cancelled");
	}
}

void MainWindow::AddPathFast() {
	QString filename = QFileDialog::getExistingDirectory(this, tr("Choose directory"));
	if (filename.isEmpty()) {
		QMessageBox box;
		box.setText(tr("No directory selected"));
		box.setIcon(QMessageBox::Information);
		box.setStandardButtons(QMessageBox::Ok);
		box.exec();
		return;
	}
	if (this->scanner->running()) {
		QMessageBox box;
		box.setText(tr("There is an active scanner running"));
		box.setIcon(QMessageBox::Warning);
		box.setStandardButtons(QMessageBox::Ok);
		box.exec();
		return;
	}
	this->scanner->setPath(filename);
	QFileInfo finfo(filename);
	bool ok;
	QString catalog_name = QInputDialog::getText(this, tr("Catalog Name"), tr("Give a name to this catalog"),
						     QLineEdit::EchoMode::Normal, finfo.baseName(), &ok);
	if (ok && !catalog_name.isEmpty()) {
		this->scanner->setCatalogId(-1);
		this->scanner->setCatalogName(catalog_name);
		this->scanner->withThumbs(false);
		this->scanner->start();
	} else {
		ui->statusbar->showMessage("Cancelled");
	}
}

MainWindow::~MainWindow() {
	closePreviewPopup();
	delete thumbQueue;
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

void MainWindow::updateThumbnailQueueStatus(int size) {
	if (size > 0) {
		ui->statusbar->showMessage(tr("Thumbnail queue: %1 pending").arg(size));
	} else {
		ui->statusbar->showMessage(tr("All thumbnails generated"));
	}
}

QIcon MainWindow::getCachedFileIcon(const QString &full_path) {
	auto cached_icon = fileIconCache.constFind(full_path);
	if (cached_icon != fileIconCache.constEnd()) {
		return cached_icon.value();
	}

	QIcon icon = iconProvider.icon(QFileInfo(full_path));
	fileIconCache.insert(full_path, icon);
	return icon;
}

void MainWindow::closePreviewPopup() {
	if (previewPopup) {
		previewPopupPosition = previewPopup->pos();
		hasPreviewPopupPosition = true;
		previewPopup->close();
		previewPopup = nullptr;
	}
}

void MainWindow::executeSearch(const QString &text, bool and_join) {
	if (text.isEmpty()) {
		ClearSearch();
		return;
	}

	current_search_text = text;
	current_search_and_join = and_join;
	ui->clearSearchButton->setEnabled(true);
	ui->toolbarHintLabel->setText(tr("Catalog"));
	closePreviewPopup();
	QSqlQuery files = db->searchFiles(text, and_join, selected_catalog >= 0 ? selected_catalog : -1);
	in_search_mode = true;
	ShowFiles(files, true);
}

void MainWindow::updateBrowseContext() {
	QTreeWidgetItem *folder_item = ui->directoryTree->currentItem();
	if (folder_item != NULL) {
		ui->foldersSubtitleLabel->setText(tr("Browsing %1").arg(folder_item->text(0)));
	} else if (ui->catalogList->currentIndex() >= 0) {
		ui->foldersSubtitleLabel->setText(tr("Catalog: %1").arg(ui->catalogList->currentText()));
	} else {
		ui->foldersSubtitleLabel->setText(tr("Choose a catalog to browse"));
	}
	ui->toolbarHintLabel->setText(tr("Catalog"));
}

void MainWindow::updateResultsSummary(int row_count) {
	if (in_search_mode) {
		ui->resultsTitleLabel->setText(tr("Search results"));
		QString scope = selected_catalog >= 0 ? catalogNameCache.value(selected_catalog) : tr("all catalogs");
		if (current_search_text.isEmpty()) {
			ui->resultsSummaryLabel->setText(tr("%1 item(s) found in %2").arg(row_count).arg(scope));
		} else {
			ui->resultsSummaryLabel->setText(
			    tr("%1 match(es) for \"%2\" in %3").arg(row_count).arg(current_search_text, scope));
		}
		return;
	}

	ui->resultsTitleLabel->setText(tr("Files"));
	QTreeWidgetItem *folder_item = ui->directoryTree->currentItem();
	QString scope = folder_item != NULL ? folder_item->text(0) : tr("the selected folder");
	ui->resultsSummaryLabel->setText(tr("%1 file(s) in %2").arg(row_count).arg(scope));
}

void MainWindow::toggleCatalogPanel(bool) {}
