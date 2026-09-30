#include "about.h"
#include "ui_about.h"
#include <QApplication>
#include <QIcon>

About::About(QWidget *parent) : QDialog(parent), ui(new Ui::About) {
	ui->setupUi(this);
	ui->iconLabel->setPixmap(QIcon(":/additional/icon.png").pixmap(72, 72));
	ui->versionLabel->setText(tr("Version %1").arg(QApplication::applicationVersion()));
	ui->linkLabel->setText(
	    tr("<a href=\"https://github.com/sinanislekdemir/poorman\">github.com/sinanislekdemir/poorman</a>"
	       " &nbsp;·&nbsp; "
	       "<a href=\"mailto:sinan@islekdemir.com\">sinan@islekdemir.com</a>"));
}
