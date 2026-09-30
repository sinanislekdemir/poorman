#ifndef THEME_H
#define THEME_H

#include <QColor>
#include <QIcon>
#include <QString>

namespace Theme {

enum class Mode { Dark, Light };

struct Colors {
	QColor window;
	QColor panel;
	QColor base;
	QColor altBase;
	QColor border;
	QColor text;
	QColor muted;
	QColor accent;
	QColor selectionText;
	QColor positive;
	QColor negative;
};

Mode current();
void setCurrent(Mode mode);

Mode savedMode();
void saveMode(Mode mode);

const Colors &colors();
QString styleSheet(Mode mode);
void apply(Mode mode);

QIcon icon(const QString &name);

} // namespace Theme

#endif // THEME_H
