#include "theme.h"

#include <QApplication>
#include <QFile>
#include <QFont>
#include <QPainter>
#include <QPainterPath>
#include <QPalette>
#include <QPixmap>
#include <QSettings>

namespace {

Theme::Mode g_mode = Theme::Mode::Dark;

const Theme::Colors &darkColors() {
	static const Theme::Colors c = []() {
		Theme::Colors x;
		x.window = QColor("#272822");
		x.panel = QColor("#2D2E27");
		x.base = QColor("#272822");
		x.altBase = QColor("#2D2E27");
		x.border = QColor("#49483E");
		x.text = QColor("#F8F8F2");
		x.muted = QColor("#A6A28C");
		x.accent = QColor("#F92672");
		x.selectionText = QColor("#F8F8F2");
		x.positive = QColor("#A6E22E");
		x.negative = QColor("#75715E");
		return x;
	}();
	return c;
}

const Theme::Colors &lightColors() {
	static const Theme::Colors c = []() {
		Theme::Colors x;
		x.window = QColor("#F4F4F0");
		x.panel = QColor("#FFFFFF");
		x.base = QColor("#FFFFFF");
		x.altBase = QColor("#F6F6F1");
		x.border = QColor("#DAD8CE");
		x.text = QColor("#2B2B27");
		x.muted = QColor("#6E6D63");
		x.accent = QColor("#D81E6E");
		x.selectionText = QColor("#FFFFFF");
		x.positive = QColor("#2E7D32");
		x.negative = QColor("#8A8A80");
		return x;
	}();
	return c;
}

QString loadStyleSheet(const QString &resource) {
	QFile file(resource);
	if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
		return QString();
	}
	return QString::fromUtf8(file.readAll());
}

void drawGlyph(QPainter &p, const QString &name, const QColor &color) {
	p.setRenderHint(QPainter::Antialiasing, true);
	QPen pen(color);
	pen.setWidthF(1.9);
	pen.setCapStyle(Qt::RoundCap);
	pen.setJoinStyle(Qt::RoundJoin);
	p.setPen(pen);
	p.setBrush(Qt::NoBrush);

	if (name == "document-open" || name == "folder") {
		QPainterPath path;
		path.moveTo(3, 7);
		path.lineTo(9, 7);
		path.lineTo(11, 9);
		path.lineTo(21, 9);
		path.lineTo(21, 19);
		path.lineTo(3, 19);
		path.closeSubpath();
		p.drawPath(path);
		if (name == "document-open") {
			p.drawLine(3, 12, 21, 12);
		}
	} else if (name == "document-save" || name == "drive-harddisk") {
		p.drawRoundedRect(QRectF(4, 4, 16, 16), 2, 2);
		p.drawRect(QRectF(8, 4, 8, 5));
		p.drawRoundedRect(QRectF(7, 13, 10, 7), 1, 1);
	} else if (name == "document-new") {
		QPainterPath page;
		page.moveTo(6, 3);
		page.lineTo(14, 3);
		page.lineTo(19, 8);
		page.lineTo(19, 21);
		page.lineTo(6, 21);
		page.closeSubpath();
		p.drawPath(page);
		p.drawLine(14, 3, 14, 8);
		p.drawLine(14, 8, 19, 8);
	} else if (name == "list-add") {
		p.drawEllipse(QPointF(12, 12), 8.5, 8.5);
		p.drawLine(8, 12, 16, 12);
		p.drawLine(12, 8, 12, 16);
	} else if (name == "edit-find") {
		p.drawEllipse(QPointF(10.5, 10.5), 6.5, 6.5);
		p.drawLine(15.2, 15.2, 20, 20);
	} else if (name == "edit-clear") {
		p.drawEllipse(QPointF(12, 12), 8.5, 8.5);
		p.drawLine(8.5, 8.5, 15.5, 15.5);
		p.drawLine(15.5, 8.5, 8.5, 15.5);
	} else if (name == "help-about" || name == "help-browser") {
		p.drawEllipse(QPointF(12, 12), 9, 9);
		QFont font = p.font();
		font.setBold(true);
		font.setPixelSize(13);
		p.setFont(font);
		p.drawText(QRectF(0, 0, 24, 24), Qt::AlignCenter, "?");
	} else if (name == "application-exit") {
		p.drawArc(QRectF(4, 6, 16, 15), 60 * 16, 300 * 16);
		p.drawLine(12, 3, 12, 12);
	} else if (name == "view-refresh") {
		p.drawArc(QRectF(5, 5, 14, 14), 60 * 16, 260 * 16);
		p.drawLine(16.5, 4.5, 16.5, 9.5);
		p.drawLine(16.5, 4.5, 11.5, 4.5);
	} else if (name == "preferences-system" || name == "theme") {
		QPainterPath moon;
		moon.addEllipse(QPointF(12, 12), 8, 8);
		QPainterPath cut;
		cut.addEllipse(QPointF(16.5, 8.5), 7.5, 7.5);
		p.setPen(Qt::NoPen);
		p.setBrush(color);
		p.drawPath(moon.subtracted(cut));
	} else if (name == "dialog-information" || name == "about") {
		p.drawEllipse(QPointF(12, 12), 9, 9);
		QFont font = p.font();
		font.setBold(true);
		font.setPixelSize(13);
		p.setFont(font);
		p.drawText(QRectF(0, 0, 24, 24), Qt::AlignCenter, "i");
	} else if (name == "view-list") {
		p.drawLine(8, 7, 20, 7);
		p.drawLine(8, 12, 20, 12);
		p.drawLine(8, 17, 20, 17);
		p.drawLine(4, 7, 4.5, 7);
		p.drawLine(4, 12, 4.5, 12);
		p.drawLine(4, 17, 4.5, 17);
	} else if (name == "folder-add") {
		QPainterPath path;
		path.moveTo(3, 7);
		path.lineTo(9, 7);
		path.lineTo(11, 9);
		path.lineTo(21, 9);
		path.lineTo(21, 19);
		path.lineTo(3, 19);
		path.closeSubpath();
		p.drawPath(path);
		p.drawLine(12, 14, 18, 14);
		p.drawLine(15, 11, 15, 17);
	} else if (name == "folder-open") {
		QPainterPath path;
		path.moveTo(3, 18);
		path.lineTo(3, 6);
		path.lineTo(9, 6);
		path.lineTo(11, 8);
		path.lineTo(19, 8);
		path.lineTo(19, 11);
		p.drawPath(path);
		p.drawLine(3, 18, 8, 11);
		p.drawLine(8, 11, 22, 11);
		p.drawLine(22, 11, 17, 18);
		p.drawLine(17, 18, 3, 18);
	} else if (name == "flash") {
		QPainterPath bolt;
		bolt.moveTo(13, 3);
		bolt.lineTo(6, 13);
		bolt.lineTo(11, 13);
		bolt.lineTo(10, 21);
		bolt.lineTo(17, 11);
		bolt.lineTo(12, 11);
		bolt.closeSubpath();
		p.setPen(Qt::NoPen);
		p.setBrush(color);
		p.drawPath(bolt);
	} else {
		p.drawRoundedRect(QRectF(4, 4, 16, 16), 3, 3);
	}
}

QPixmap renderGlyph(const QString &name, const QColor &color, int size) {
	QPixmap pixmap(size, size);
	pixmap.fill(Qt::transparent);
	QPainter p(&pixmap);
	p.scale(size / 24.0, size / 24.0);
	drawGlyph(p, name, color);
	p.end();
	return pixmap;
}

} // namespace

namespace Theme {

Mode current() { return g_mode; }

void setCurrent(Mode mode) { g_mode = mode; }

Mode savedMode() {
	QSettings settings;
	const QString value = settings.value("appearance/theme", "dark").toString();
	return value == "light" ? Mode::Light : Mode::Dark;
}

void saveMode(Mode mode) {
	QSettings settings;
	settings.setValue("appearance/theme", mode == Mode::Light ? "light" : "dark");
}

const Colors &colors() { return g_mode == Mode::Light ? lightColors() : darkColors(); }

QString styleSheet(Mode mode) {
	return loadStyleSheet(mode == Mode::Light ? ":/themes/light.qss" : ":/themes/dark.qss");
}

void apply(Mode mode) {
	g_mode = mode;
	QApplication *app = qobject_cast<QApplication *>(QCoreApplication::instance());
	if (!app) {
		return;
	}

	const Colors &c = colors();
	QPalette palette;
	palette.setColor(QPalette::Window, c.window);
	palette.setColor(QPalette::WindowText, c.text);
	palette.setColor(QPalette::Base, c.base);
	palette.setColor(QPalette::AlternateBase, c.altBase);
	palette.setColor(QPalette::Text, c.text);
	palette.setColor(QPalette::Button, c.panel);
	palette.setColor(QPalette::ButtonText, c.text);
	palette.setColor(QPalette::BrightText, c.accent);
	palette.setColor(QPalette::Highlight, c.accent);
	palette.setColor(QPalette::HighlightedText, c.selectionText);
	palette.setColor(QPalette::ToolTipBase, c.panel);
	palette.setColor(QPalette::ToolTipText, c.text);
	palette.setColor(QPalette::PlaceholderText, c.muted);
	palette.setColor(QPalette::Light, c.panel);
	palette.setColor(QPalette::Midlight, c.altBase);
	palette.setColor(QPalette::Mid, c.border);
	palette.setColor(QPalette::Dark, c.border);
	palette.setColor(QPalette::Shadow, c.window);
	palette.setColor(QPalette::Disabled, QPalette::Text, c.muted);
	palette.setColor(QPalette::Disabled, QPalette::WindowText, c.muted);
	palette.setColor(QPalette::Disabled, QPalette::ButtonText, c.muted);
	palette.setColor(QPalette::Disabled, QPalette::HighlightedText, c.muted);
	app->setPalette(palette);

	app->setStyleSheet(styleSheet(mode));
}

QIcon icon(const QString &name) {
	const QColor color = colors().muted;
	QIcon result;
	for (int size : {16, 20, 24, 32, 48}) {
		result.addPixmap(renderGlyph(name, color, size));
	}
	return result;
}

} // namespace Theme
