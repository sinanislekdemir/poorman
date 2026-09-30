#include "mainwindow.h"
#include "theme.h"

#include <QApplication>
#include <QIcon>

int main(int argc, char *argv[]) {
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
	QApplication::setAttribute(Qt::AA_EnableHighDpiScaling);
	QApplication::setAttribute(Qt::AA_UseHighDpiPixmaps);
#endif

	QApplication a(argc, argv);
	QApplication::setOrganizationName("PoorMansCatalog");
	QApplication::setApplicationName("PoorMansCatalog");
	QApplication::setApplicationVersion("1.2.2");
	QApplication::setWindowIcon(QIcon(":/additional/icon.png"));
	QApplication::setStyle("Fusion");

	Theme::apply(Theme::savedMode());

	MainWindow w;
	w.show();
	return a.exec();
}
