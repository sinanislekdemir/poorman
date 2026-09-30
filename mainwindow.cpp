#include "mainwindow.h"
#include "about.h"
#include "scanner.h"
#include "theme.h"
#include "ui_mainwindow.h"
#include <QApplication>
#include <QClipboard>
#include <QCloseEvent>
#include <QDesktopServices>
#include <QDir>
#include <QDockWidget>
#include <QEventLoop>
#include <QFile>
#include <QFileInfo>
#include <QInputDialog>
#include <QMessageBox>
#include <QPixmap>
#include <QResizeEvent>
#include <QSettings>
#include <QShortcut>
#include <QSignalBlocker>
#include <QSqlQuery>
#include <QToolButton>
#include <QUrl>
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
		const Theme::Colors &colors = Theme::colors();
		const bool selected = option.state & QStyle::State_Selected;

		opt.text.clear();
		opt.icon = QIcon();

		painter->save();
		style->drawPrimitive(QStyle::PE_PanelItemViewItem, &opt, painter, widget);

		QRect content_rect = option.rect.adjusted(14, 7, -14, -7);
		QRect icon_rect(content_rect.left(), content_rect.top() + 6, 18, 18);
		icon.paint(painter, icon_rect, Qt::AlignCenter,
			   option.state & QStyle::State_Enabled ? QIcon::Normal : QIcon::Disabled);

		QRect text_rect = content_rect.adjusted(30, 0, 0, 0);
		QColor primary_color = selected ? opt.palette.color(QPalette::HighlightedText) : colors.text;
		QColor secondary_color = selected ? colors.selectionText : colors.muted;

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

	selected_catalog = -2;
	in_search_mode = false;
	current_search_and_join = true;
	this->db_file_path = QDir::home().absolutePath() + "/poorman.sqlite";

	setupToolBar();
	setupStatusBar();
	applyModernUi();
	setupPreviewDock();
	setupIcons();
	updateThemeAction();

	connect(ui->actionAdd_path, &QAction::triggered, this, &MainWindow::AddPath);
	connect(ui->addPathNoThumb, &QAction::triggered, this, &MainWindow::AddPathFast);
	connect(ui->actionSave_catalog_file, &QAction::triggered, this, &MainWindow::SaveAs);
	connect(ui->actionOpen_catalog_file, &QAction::triggered, this, &MainWindow::OpenDB);
	connect(ui->actionQuit, &QAction::triggered, this, &MainWindow::Quit);
	connect(ui->actionAbout, &QAction::triggered, this, &MainWindow::ShowAbout);
	connect(ui->actionFocus_search, &QAction::triggered, this, &MainWindow::focusSearch);
	connect(ui->actionSearch_help, &QAction::triggered, this, &MainWindow::ShowSearchHelp);
	connect(ui->actionToggle_theme, &QAction::triggered, this, &MainWindow::toggleTheme);
	connect(ui->actionGithub_Pages, &QAction::triggered, this, []() {
		QDesktopServices::openUrl(QUrl("https://github.com/sinanislekdemir/poorman"));
	});

	connect(catalogList, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int) { ShowSelectedCatalog(); });
	connect(ui->directoryTree, &QTreeWidget::itemSelectionChanged, this, &MainWindow::ShowSelectedDirectory);
	connect(ui->fileList, &QTableWidget::itemSelectionChanged, this, &MainWindow::ShowThumbnail);
	connect(ui->fileList, &QWidget::customContextMenuRequested, this, &MainWindow::fileListContextMenuRequested);

	connect(searchInput, &QLineEdit::returnPressed, this,
		[this]() { executeSearch(searchInput->text().trimmed(), searchModeBox->currentIndex() == 0); });
	connect(searchModeBox, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int index) {
		if (in_search_mode) {
			executeSearch(searchInput->text().trimmed(), index == 0);
		}
	});
	QShortcut *escape = new QShortcut(QKeySequence(Qt::Key_Escape), searchInput);
	connect(escape, &QShortcut::activated, this, [this]() {
		searchInput->clear();
		ClearSearch();
	});

	catalogList->setContextMenuPolicy(Qt::CustomContextMenu);
	connect(catalogList, &QWidget::customContextMenuRequested, this, &MainWindow::catalogContextMenuRequested);

	folderIcon = iconProvider.icon(QFileIconProvider::Folder);
	driveIcon = iconProvider.icon(QFileIconProvider::Drive);

	db = new DBManager(this->db_file_path);
	thumbQueue = new ThumbnailQueue(this, db_file_path);
	connect(thumbQueue, &ThumbnailQueue::queueSizeChanged, this, &MainWindow::updateThumbnailQueueStatus);
	this->scanner = new Scanner(this, db_file_path);
	this->scanner->setThumbnailQueue(thumbQueue);
	connect(this->scanner, SIGNAL(setProgressFilename(QString)), this, SLOT(createPathEntry(QString)));
	connect(this->scanner, SIGNAL(scanError(QString)), this, SLOT(showScanError(QString)));
	connect(this->scanner, &QThread::started, this, [this]() { updateScanState(true); });
	connect(this->scanner, &QThread::finished, this, [this]() { updateScanState(false); });

	loadSettings();
	refresh();
}

void MainWindow::Quit() { QCoreApplication::quit(); }

void MainWindow::setupToolBar() {
	toolBar = addToolBar(tr("Main"));
	toolBar->setObjectName("mainToolBar");
	toolBar->setMovable(false);
	toolBar->setFloatable(false);
	toolBar->setIconSize(QSize(18, 18));
	toolBar->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);

	toolbarHintLabel = new QLabel(tr("Catalog:"), toolBar);
	toolBar->addWidget(toolbarHintLabel);

	catalogList = new QComboBox(toolBar);
	catalogList->setIconSize(QSize(18, 18));
	catalogList->setSizeAdjustPolicy(QComboBox::AdjustToContents);
	catalogList->setMinimumContentsLength(14);
	catalogList->setToolTip(tr("Select a catalog. Right-click for options."));
	toolBar->addWidget(catalogList);

	toolBar->addSeparator();
	toolBar->addAction(ui->actionAdd_path);
	toolBar->addAction(ui->addPathNoThumb);
	toolBar->addAction(ui->actionOpen_catalog_file);
	toolBar->addAction(ui->actionSave_catalog_file);
	toolBar->addSeparator();

	for (QAction *action : {ui->actionAdd_path, ui->addPathNoThumb, ui->actionOpen_catalog_file, ui->actionSave_catalog_file}) {
		if (QToolButton *button = qobject_cast<QToolButton *>(toolBar->widgetForAction(action))) {
			button->setToolButtonStyle(Qt::ToolButtonIconOnly);
		}
	}

	searchInput = new QLineEdit(toolBar);
	searchInput->setClearButtonEnabled(true);
	searchInput->setPlaceholderText(tr("Search file names…  (e.g. vacation 2023)"));
	searchInput->setMinimumWidth(220);
	searchInput->setMaximumWidth(460);
	searchInput->setToolTip(tr("Search file names only. Press Enter to search, Esc to clear."));
	searchInput->addAction(Theme::icon("edit-find"), QLineEdit::LeadingPosition);

	searchModeBox = new QComboBox(toolBar);
	searchModeBox->addItem(tr("Search all"));
	searchModeBox->addItem(tr("Search any"));
	searchModeBox->setToolTip(tr("Require all keywords or any keyword"));

	searchAction = new QAction(Theme::icon("edit-find"), tr("Search"), this);
	browseAction = new QAction(Theme::icon("edit-clear"), tr("Browse"), this);
	browseAction->setEnabled(false);
	helpAction = new QAction(Theme::icon("help-browser"), tr("Search help"), this);

	toolBar->addWidget(searchInput);
	toolBar->addWidget(searchModeBox);
	toolBar->addAction(searchAction);
	toolBar->addAction(browseAction);
	toolBar->addAction(helpAction);
	toolBar->addSeparator();

	previewToggle = new QCheckBox(tr("Preview"), toolBar);
	previewToggle->setToolTip(tr("Show the preview panel for the selected file"));
	toolBar->addWidget(previewToggle);

	connect(searchAction, &QAction::triggered, this,
		[this]() { executeSearch(searchInput->text().trimmed(), searchModeBox->currentIndex() == 0); });
	connect(browseAction, &QAction::triggered, this, &MainWindow::ClearSearch);
	connect(helpAction, &QAction::triggered, this, &MainWindow::ShowSearchHelp);
	connect(previewToggle, &QCheckBox::toggled, ui->actionToggle_preview, &QAction::setChecked);
	connect(ui->actionToggle_preview, &QAction::toggled, previewToggle, &QCheckBox::setChecked);
}

void MainWindow::setupStatusBar() {
	scanProgress = new QProgressBar(this);
	scanProgress->setRange(0, 0);
	scanProgress->setTextVisible(false);
	scanProgress->setFixedWidth(130);
	scanProgress->setFixedHeight(10);
	scanProgress->hide();

	thumbStatusLabel = new QLabel(tr("Thumbnails: idle"), this);
	thumbStatusLabel->setMinimumWidth(150);
	thumbStatusLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);

	ui->statusbar->addPermanentWidget(scanProgress);
	ui->statusbar->addPermanentWidget(thumbStatusLabel);
	ui->statusbar->showMessage(tr("Ready"));
}

void MainWindow::setupPreviewDock() {
	previewDock = new QDockWidget(tr("Preview"), this);
	previewDock->setObjectName("previewDock");
	previewDock->setAllowedAreas(Qt::RightDockWidgetArea | Qt::LeftDockWidgetArea);
	previewDock->setFeatures(QDockWidget::DockWidgetClosable | QDockWidget::DockWidgetMovable);

	QWidget *panel = new QWidget(previewDock);
	QVBoxLayout *layout = new QVBoxLayout(panel);
	layout->setContentsMargins(8, 8, 8, 8);
	layout->setSpacing(8);

	previewImage = new QLabel(panel);
	previewImage->setObjectName("previewImage");
	previewImage->setAlignment(Qt::AlignCenter);
	previewImage->setMinimumSize(220, 180);
	previewImage->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);

	previewName = new QLabel(panel);
	previewName->setObjectName("previewName");
	previewName->setWordWrap(true);

	previewMeta = new QLabel(panel);
	previewMeta->setObjectName("previewMeta");
	previewMeta->setWordWrap(true);

	layout->addWidget(previewImage, 1);
	layout->addWidget(previewName);
	layout->addWidget(previewMeta);
	previewDock->setWidget(panel);
	addDockWidget(Qt::RightDockWidgetArea, previewDock);
	previewDock->hide();

	connect(ui->actionToggle_preview, &QAction::toggled, previewDock, &QWidget::setVisible);
	connect(previewDock, &QDockWidget::visibilityChanged, this, [this](bool visible) {
		QSignalBlocker block_action(ui->actionToggle_preview);
		QSignalBlocker block_toggle(previewToggle);
		ui->actionToggle_preview->setChecked(visible);
		previewToggle->setChecked(visible);
		if (visible) {
			updatePreviewForCurrentRow();
		}
	});
}

void MainWindow::setupIcons() {
	ui->actionAdd_path->setIcon(Theme::icon("folder-add"));
	ui->addPathNoThumb->setIcon(Theme::icon("flash"));
	ui->actionOpen_catalog_file->setIcon(Theme::icon("folder-open"));
	ui->actionSave_catalog_file->setIcon(Theme::icon("document-save"));
	ui->actionQuit->setIcon(Theme::icon("application-exit"));
	ui->actionAbout->setIcon(Theme::icon("dialog-information"));
	ui->actionSearch_help->setIcon(Theme::icon("help-browser"));
	ui->actionFocus_search->setIcon(Theme::icon("edit-find"));
	ui->actionToggle_theme->setIcon(Theme::icon("preferences-system"));
	ui->actionToggle_preview->setIcon(Theme::icon("view-list"));
	if (searchAction) searchAction->setIcon(Theme::icon("edit-find"));
	if (browseAction) browseAction->setIcon(Theme::icon("edit-clear"));
	if (helpAction) helpAction->setIcon(Theme::icon("help-browser"));
	if (searchInput) {
		for (QAction *action : searchInput->actions()) {
			action->setIcon(Theme::icon("edit-find"));
		}
	}
}

void MainWindow::updateThemeAction() {
	ui->actionToggle_theme->setText(Theme::current() == Theme::Mode::Dark ? tr("Switch to light theme")
									     : tr("Switch to dark theme"));
}

void MainWindow::applyModernUi() {
	ui->directoryTree->setAnimated(true);
	ui->directoryTree->setIndentation(16);
	ui->directoryTree->setUniformRowHeights(true);
	ui->directoryTree->setIconSize(QSize(18, 18));

	ui->fileList->setAlternatingRowColors(true);
	ui->fileList->setShowGrid(false);
	ui->fileList->setSelectionBehavior(QAbstractItemView::SelectRows);
	ui->fileList->setSelectionMode(QAbstractItemView::SingleSelection);
	ui->fileList->setEditTriggers(QAbstractItemView::NoEditTriggers);
	ui->fileList->setIconSize(QSize(18, 18));
	ui->fileList->setItemDelegateForColumn(0, new FileListDelegate(ui->fileList));
	ui->fileList->verticalHeader()->setVisible(false);
	ui->fileList->verticalHeader()->setDefaultSectionSize(58);
	ui->fileList->horizontalHeader()->setHighlightSections(false);
	ui->fileList->horizontalHeader()->setStretchLastSection(false);

	if (QHBoxLayout *mid_layout = qobject_cast<QHBoxLayout *>(ui->midSection->layout())) {
		browserSplitter = new QSplitter(Qt::Horizontal, ui->midSection);
		browserSplitter->setObjectName("browserSplitter");
		browserSplitter->setChildrenCollapsible(false);
		browserSplitter->setHandleWidth(10);
		mid_layout->removeWidget(ui->directoryPanel);
		mid_layout->removeWidget(ui->filePanel);
		browserSplitter->addWidget(ui->directoryPanel);
		browserSplitter->addWidget(ui->filePanel);
		browserSplitter->setStretchFactor(0, 0);
		browserSplitter->setStretchFactor(1, 1);
		browserSplitter->setSizes(QList<int>() << 240 << 900);
		mid_layout->addWidget(browserSplitter);
	}

	emptyStateLabel = new QLabel(tr("No files to show.\nSelect a folder or run a search."), ui->filePanel);
	emptyStateLabel->setObjectName("emptyStateLabel");
	emptyStateLabel->setAlignment(Qt::AlignCenter);
	emptyStateLabel->setWordWrap(true);
	if (QVBoxLayout *file_layout = qobject_cast<QVBoxLayout *>(ui->filePanel->layout())) {
		file_layout->addWidget(emptyStateLabel);
	}

	ui->resultsSummaryLabel->setText(tr("Select a folder or run a search"));
	ui->foldersSubtitleLabel->setText(tr("Choose a catalog to browse"));
}

void MainWindow::loadSettings() {
	QSettings settings;
	restoreGeometry(settings.value("window/geometry").toByteArray());

	const bool preview = settings.value("view/preview", false).toBool();
	{
		QSignalBlocker blocker(previewToggle);
		previewToggle->setChecked(preview);
	}
	{
		QSignalBlocker blocker(ui->actionToggle_preview);
		ui->actionToggle_preview->setChecked(preview);
	}
	if (previewDock) {
		previewDock->setVisible(preview);
	}

	const bool and_join = settings.value("search/and_join", true).toBool();
	{
		QSignalBlocker blocker(searchModeBox);
		searchModeBox->setCurrentIndex(and_join ? 0 : 1);
	}
	current_search_and_join = and_join;

	if (browserSplitter) {
		browserSplitter->restoreState(settings.value("window/splitter").toByteArray());
	}
}

void MainWindow::saveSettings() {
	QSettings settings;
	settings.setValue("window/geometry", saveGeometry());
	if (browserSplitter) {
		settings.setValue("window/splitter", browserSplitter->saveState());
	}
	settings.setValue("view/preview", previewToggle && previewToggle->isChecked());
	settings.setValue("search/and_join", searchModeBox && searchModeBox->currentIndex() == 0);
	Theme::saveMode(Theme::current());
}

void MainWindow::closeEvent(QCloseEvent *event) {
	saveSettings();
	QMainWindow::closeEvent(event);
}

void MainWindow::resizeEvent(QResizeEvent *event) {
	QMainWindow::resizeEvent(event);
	updatePreviewImage();
}

void MainWindow::toggleTheme() {
	Theme::Mode next = Theme::current() == Theme::Mode::Dark ? Theme::Mode::Light : Theme::Mode::Dark;
	Theme::apply(next);
	setupIcons();
	updateThemeAction();
	ui->fileList->viewport()->update();
	ui->directoryTree->viewport()->update();
}

void MainWindow::focusSearch() {
	searchInput->setFocus();
	searchInput->selectAll();
}

void MainWindow::catalogContextMenuRequested(QPoint pos) {
	if (catalogList->currentIndex() < 0) {
		return;
	}
	QMenu menu(this);
	QAction *rescanPath = menu.addAction(tr("Re-scan catalog for new files"));
	QAction *removeGone = menu.addAction(tr("Re-scan catalog for deleted files"));
	QAction *chosen = menu.exec(catalogList->mapToGlobal(pos));
	if (!chosen) {
		return;
	}
	if (chosen == rescanPath) {
		rescanCatalog();
	} else if (chosen == removeGone) {
		deleteCatalog();
	}
}

void MainWindow::rescanCatalog() {
	if (this->scanner->running()) {
		QMessageBox::warning(this, tr("Scan"), tr("There is an active scanner running"));
		return;
	}
	QString selected = catalogList->currentText();
	QString path = "";

	QSqlQuery catalogs = db->fetchCatalogs();
	while (catalogs.next()) {
		if (catalogs.value("name").toString() == selected) {
			path = catalogs.value("original_path").toString();
			QDir dir(path);
			if (!dir.exists()) {
				QMessageBox::warning(this, tr("Scan"), tr("Catalog path is not reachable") + "\n" + path);
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
	QString selected = catalogList->currentText();
	QString path = "";
	QVector<int> ids;
	int catalog_id = -1;
	QSqlQuery catalogs = db->fetchCatalogs();
	while (catalogs.next()) {
		if (catalogs.value("name").toString() == selected) {
			path = catalogs.value("original_path").toString();
			QDir dir(path);
			if (!dir.exists()) {
				QMessageBox::warning(this, tr("Scan"), tr("Catalog path is not reachable") + "\n" + path);
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
		} else {
			ui->statusbar->showMessage(tr("No deleted files found"));
		}
	}
};

void MainWindow::ShowAbout() {
	About about(this);
	about.setModal(true);
	about.exec();
}

void MainWindow::ShowSearchHelp() {
	QMessageBox helpBox(this);
	helpBox.setWindowTitle(tr("Search Help"));
	helpBox.setTextFormat(Qt::RichText);
	helpBox.setText(
	    tr("<h3>Search Syntax</h3>"
	       "<p><b>Basic Search:</b><br>"
	       "Type keywords separated by spaces to search file names.</p>"
	       "<p><b>Search Modes:</b><br>"
	       "• <b>Search all</b> - File names must contain ALL keywords<br>"
	       "• <b>Search any</b> - File names containing ANY keyword</p>"
	       "<p><b>Examples:</b><br>"
	       "• <code>vacation 2023</code> - finds files with both 'vacation' AND '2023'<br>"
	       "• <code>jpg png</code> - with 'Search any' finds all .jpg OR .png files<br>"
	       "• <code>report final.pdf</code> - finds files containing both words</p>"
	       "<p><b>Tips:</b><br>"
	       "• Search is case-insensitive<br>"
	       "• Only file names are matched, not the folders in their path<br>"
	       "• Use specific keywords for better results</p>"));
	helpBox.setIcon(QMessageBox::Information);
	helpBox.setStandardButtons(QMessageBox::Ok);
	helpBox.exec();
}

void MainWindow::ShowThumbnail() { updatePreviewForCurrentRow(); }

void MainWindow::updatePreviewForCurrentRow() {
	int row = ui->fileList->currentRow();
	QTableWidgetItem *item = row >= 0 ? ui->fileList->item(row, 0) : nullptr;
	if (!item) {
		clearPreview();
		return;
	}

	int id = item->data(EntryIdRole).toInt();
	int catalog_id = item->data(Qt::UserRole).toInt();
	if (in_search_mode && catalog_id != selected_catalog) {
		SelectCatalogByID(catalog_id);
	}

	DirEntry d = db->getDirentry(id);
	ui->statusbar->showMessage(d.full_path);
	if (!previewDock || !previewDock->isVisible()) {
		return;
	}

	QFileInfo info(d.full_path);
	const QString type_label = d.is_directory
				       ? tr("Folder")
				       : (info.suffix().isEmpty() ? tr("File") : info.suffix().toUpper());
	previewName->setText(info.fileName().isEmpty() ? d.full_path : info.fileName());
	previewMeta->setText(tr("%1  •  %2\n%3").arg(type_label, humanSize(d.filesize), QDir::toNativeSeparators(d.full_path)));

	if (!d.thumbnail.isEmpty()) {
		QPixmap map;
		if (map.loadFromData(d.thumbnail)) {
			previewSource = map;
			updatePreviewImage();
			return;
		}
	}
	previewSource = getCachedFileIcon(d.full_path).pixmap(160, 160);
	updatePreviewImage();
}

void MainWindow::updatePreviewImage() {
	if (!previewImage) {
		return;
	}
	if (previewSource.isNull()) {
		previewImage->clear();
		return;
	}
	const QSize target = previewImage->size() - QSize(10, 10);
	if (target.width() <= 0 || target.height() <= 0) {
		previewImage->setPixmap(previewSource);
		return;
	}
	previewImage->setPixmap(previewSource.scaled(target, Qt::KeepAspectRatio, Qt::SmoothTransformation));
}

void MainWindow::clearPreview() {
	previewSource = QPixmap();
	if (previewImage) previewImage->clear();
	if (previewName) previewName->clear();
	if (previewMeta) previewMeta->clear();
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

void MainWindow::ClearSearch() {
	current_search_text.clear();
	current_search_and_join = true;
	in_search_mode = false;
	if (browseAction) {
		browseAction->setEnabled(false);
	}
	if (!searchInput->text().isEmpty()) {
		QSignalBlocker blocker(searchInput);
		searchInput->clear();
	}
	toolbarHintLabel->setText(tr("Catalog:"));
	clearPreview();
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
	clearPreview();
	QSqlQuery files = db->fetchFiles(dir_id, selected_catalog);
	in_search_mode = false;
	updateBrowseContext();
	ShowFiles(files, false);
}

void MainWindow::SelectCatalogByID(int id) {
	ui->directoryTree->clear();
	selected_catalog = id;
	for (int i = 0; i < catalogList->count(); i++) {
		if (catalogList->itemData(i, Qt::UserRole).toInt() == id) {
			catalogList->setCurrentIndex(i);
			break;
		}
	}
}

void MainWindow::ShowSelectedCatalog() {
	int catalog_id = -1;
	if (catalogList->currentIndex() >= 0) {
		catalog_id = catalogList->currentData(Qt::UserRole).toInt();
	}
	ui->directoryTree->clear();
	ui->fileList->clearContents();
	ui->fileList->setRowCount(0);
	clearPreview();
	in_search_mode = false;
	selected_catalog = catalog_id;
	if (browseAction) {
		browseAction->setEnabled(false);
	}
	updateBrowseContext();
	updateResultsSummary(0);
	if (catalog_id < 0) {
		return;
	}
	QTreeWidgetItem *it = new QTreeWidgetItem();
	it->setText(0, catalogList->currentText().isEmpty() ? "Root" : catalogList->currentText());
	it->setIcon(0, driveIcon);
	int root_id = db->getRootId(catalog_id);
	it->setData(0, Qt::UserRole, root_id);
	buildTree(it, catalog_id, root_id);
	ui->directoryTree->addTopLevelItem(it);
	it->setExpanded(true);
}

void MainWindow::refresh() {
	catalogList->clear();
	catalogNameCache.clear();
	QSqlQuery catalogs = db->fetchCatalogs();
	while (catalogs.next()) {
		const int catalog_id = catalogs.value("ids").toInt();
		const QString catalog_name = catalogs.value("name").toString();
		catalogList->addItem(driveIcon, catalog_name, catalog_id);
		catalogNameCache.insert(catalog_id, catalog_name);
	}
	ui->directoryTree->clear();
	ui->fileList->clear();
	ui->fileList->setRowCount(0);
	clearPreview();
	ui->resultsSummaryLabel->setText(catalogList->count() > 0 ? tr("Pick a folder or search across a catalog")
								  : tr("Add a catalog to start browsing"));
	ui->foldersSubtitleLabel->setText(tr("Choose a catalog to browse"));
	toolbarHintLabel->setText(tr("Catalog:"));
	updateEmptyState(0);
	if (catalogList->count() > 0) {
		catalogList->setCurrentIndex(0);
		ShowSelectedCatalog();
	}
}

void MainWindow::SaveAs() {
	QString filename = QFileDialog::getSaveFileName(this, tr("Save catalog database"), "", "SQLite DB (*.sqlite)",
							 nullptr, QFileDialog::DontUseNativeDialog);
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
	QString filename = QFileDialog::getOpenFileName(this, tr("Open catalog database"), "", "SQLite DB (*.sqlite)",
							nullptr, QFileDialog::DontUseNativeDialog);
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
	connect(this->scanner, SIGNAL(scanError(QString)), this, SLOT(showScanError(QString)));
	connect(this->scanner, &QThread::started, this, [this]() { updateScanState(true); });
	connect(this->scanner, &QThread::finished, this, [this]() { updateScanState(false); });
	this->refresh();
}

bool FileSizeColumn::operator<(const QTableWidgetItem &other) const {
	if (text() == "")
		return data(Qt::UserRole).toInt() > other.data(Qt::UserRole).toInt();
	else
		return data(Qt::UserRole).toInt() < other.data(Qt::UserRole).toInt();
};

void MainWindow::ShowFiles(QSqlQuery data, bool fullname) {
	ui->fileList->setSortingEnabled(false);
	ui->fileList->setUpdatesEnabled(false);
	ui->fileList->blockSignals(true);
	ui->fileList->clearContents();
	ui->fileList->setColumnCount(3);
	ui->fileList->setRowCount(0);

	QHeaderView *headerView = ui->fileList->horizontalHeader();
	headerView->setSectionResizeMode(0, QHeaderView::Stretch);
	headerView->setSectionResizeMode(1, QHeaderView::ResizeToContents);
	headerView->setSectionResizeMode(2, QHeaderView::ResizeToContents);

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

	const Theme::Colors &colors = Theme::colors();
	int row = 0;
	int processed = 0;
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
		fthumb->setForeground(has_thumbnail ? colors.positive : colors.negative);
		fthumb->setTextAlignment(Qt::AlignCenter);
		ui->fileList->setItem(row, 2, fthumb);

		row++;
		if ((++processed % 200) == 0) {
			ui->resultsSummaryLabel->setText(tr("Loading… %1").arg(row));
			QApplication::processEvents(QEventLoop::ExcludeUserInputEvents);
		}
	}
	ui->fileList->blockSignals(false);
	ui->fileList->setUpdatesEnabled(true);
	ui->fileList->setSortingEnabled(true);
	updateResultsSummary(row);
	updateEmptyState(row);
}

void MainWindow::AddPath() {
	QString filename = QFileDialog::getExistingDirectory(this, tr("Choose directory"), QString(),
							     QFileDialog::ShowDirsOnly | QFileDialog::DontUseNativeDialog);
	if (filename.isEmpty()) {
		QMessageBox::information(this, tr("Scan"), tr("No directory selected"));
		return;
	}
	this->scanner->setPath(filename);
	if (this->scanner->running()) {
		QMessageBox::warning(this, tr("Scan"), tr("There is an active scanner running"));
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
	QString filename = QFileDialog::getExistingDirectory(this, tr("Choose directory"), QString(),
							     QFileDialog::ShowDirsOnly | QFileDialog::DontUseNativeDialog);
	if (filename.isEmpty()) {
		QMessageBox::information(this, tr("Scan"), tr("No directory selected"));
		return;
	}
	if (this->scanner->running()) {
		QMessageBox::warning(this, tr("Scan"), tr("There is an active scanner running"));
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
	if (!thumbStatusLabel) {
		return;
	}
	if (size > 0) {
		thumbStatusLabel->setText(tr("Thumbnails: %1 pending").arg(size));
	} else {
		thumbStatusLabel->setText(tr("Thumbnails: done"));
	}
}

void MainWindow::updateScanState(bool running) {
	if (scanProgress) {
		scanProgress->setVisible(running);
	}
	if (running) {
		if (thumbStatusLabel) {
			thumbStatusLabel->setText(tr("Thumbnails: idle"));
		}
		ui->statusbar->showMessage(tr("Scanning…"));
	} else {
		ui->statusbar->showMessage(tr("Ready"));
	}
}

void MainWindow::showScanError(QString message) {
	QMessageBox::warning(this, tr("Scan"), message);
	updateScanState(false);
}

QIcon MainWindow::getCachedFileIcon(const QString &full_path) {
	const QString key = QFileInfo(full_path).suffix().toLower();
	auto cached_icon = fileIconCache.constFind(key);
	if (cached_icon != fileIconCache.constEnd()) {
		return cached_icon.value();
	}

	QIcon icon = iconProvider.icon(QFileInfo(full_path));
	fileIconCache.insert(key, icon);
	return icon;
}

void MainWindow::executeSearch(const QString &text, bool and_join) {
	if (text.isEmpty()) {
		ClearSearch();
		return;
	}

	current_search_text = text;
	current_search_and_join = and_join;
	if (searchInput->text() != text) {
		QSignalBlocker blocker(searchInput);
		searchInput->setText(text);
	}
	if (searchModeBox->currentIndex() != (and_join ? 0 : 1)) {
		QSignalBlocker blocker(searchModeBox);
		searchModeBox->setCurrentIndex(and_join ? 0 : 1);
	}
	if (browseAction) {
		browseAction->setEnabled(true);
	}
	toolbarHintLabel->setText(tr("Search"));
	clearPreview();

	QApplication::setOverrideCursor(Qt::WaitCursor);
	if (scanProgress) {
		scanProgress->setVisible(true);
	}
	ui->statusbar->showMessage(tr("Searching: %1").arg(text));
	QApplication::processEvents(QEventLoop::ExcludeUserInputEvents);

	QSqlQuery files = db->searchFiles(text, and_join, selected_catalog >= 0 ? selected_catalog : -1);
	in_search_mode = true;
	ShowFiles(files, true);

	if (scanProgress) {
		scanProgress->setVisible(false);
	}
	QApplication::restoreOverrideCursor();
}

void MainWindow::updateBrowseContext() {
	QTreeWidgetItem *folder_item = ui->directoryTree->currentItem();
	if (folder_item != NULL) {
		ui->foldersSubtitleLabel->setText(tr("Browsing %1").arg(folder_item->text(0)));
	} else if (catalogList->currentIndex() >= 0) {
		ui->foldersSubtitleLabel->setText(tr("Catalog: %1").arg(catalogList->currentText()));
	} else {
		ui->foldersSubtitleLabel->setText(tr("Choose a catalog to browse"));
	}
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

void MainWindow::updateEmptyState(int row_count) {
	if (!emptyStateLabel) {
		return;
	}
	const bool empty = row_count == 0;
	if (empty) {
		if (in_search_mode) {
			emptyStateLabel->setText(tr("No files match your search."));
		} else if (catalogList->count() == 0) {
			emptyStateLabel->setText(tr("Add a catalog to start browsing."));
		} else {
			emptyStateLabel->setText(tr("No files to show.\nSelect a folder or run a search."));
		}
	}
	emptyStateLabel->setVisible(empty);
}

void MainWindow::fileListContextMenuRequested(QPoint pos) {
	if (ui->fileList->rowCount() == 0) {
		return;
	}
	QTableWidgetItem *item = ui->fileList->itemAt(pos);
	if (!item) {
		return;
	}
	ui->fileList->setCurrentItem(item);

	QMenu menu(this);
	QAction *open = menu.addAction(Theme::icon("folder-open"), tr("Open containing folder"));
	QAction *copy_path = menu.addAction(tr("Copy full path"));
	QAction *copy_name = menu.addAction(tr("Copy file name"));
	QAction *chosen = menu.exec(ui->fileList->viewport()->mapToGlobal(pos));
	if (!chosen) {
		return;
	}
	if (chosen == open) {
		openContainingFolder();
	} else if (chosen == copy_path) {
		copyFullPath();
	} else if (chosen == copy_name) {
		copyFileName();
	}
}

void MainWindow::openContainingFolder() {
	int row = ui->fileList->currentRow();
	if (row < 0 || !ui->fileList->item(row, 0)) {
		return;
	}
	int id = ui->fileList->item(row, 0)->data(EntryIdRole).toInt();
	DirEntry d = db->getDirentry(id);
	QFileInfo info(d.full_path);
	QDesktopServices::openUrl(QUrl::fromLocalFile(info.absolutePath()));
}

void MainWindow::copyFullPath() {
	int row = ui->fileList->currentRow();
	if (row < 0 || !ui->fileList->item(row, 0)) {
		return;
	}
	int id = ui->fileList->item(row, 0)->data(EntryIdRole).toInt();
	DirEntry d = db->getDirentry(id);
	QApplication::clipboard()->setText(QDir::toNativeSeparators(d.full_path));
	ui->statusbar->showMessage(tr("Copied path to clipboard"));
}

void MainWindow::copyFileName() {
	int row = ui->fileList->currentRow();
	if (row < 0 || !ui->fileList->item(row, 0)) {
		return;
	}
	QString name = ui->fileList->item(row, 0)->text();
	QApplication::clipboard()->setText(name);
	ui->statusbar->showMessage(tr("Copied name to clipboard"));
}
