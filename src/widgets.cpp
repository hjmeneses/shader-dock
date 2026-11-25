/*
Shader Dock
Copyright (C) 2026 hjmeneses

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License along
with this program. If not, see <https://www.gnu.org/licenses/>
*/

#include "widgets.hpp"

#include <QPainter>
#include <QPainterPath>
#include <QPixmap>
#include <QSvgRenderer>

#include <obs-module.h>

#include <cmath>

/* ------------------------------------------------------------------ colores */

namespace {
QColor mix(const QColor &a, const QColor &b, double t)
{
	return QColor::fromRgbF((float)(a.redF() + (b.redF() - a.redF()) * t),
				(float)(a.greenF() + (b.greenF() - a.greenF()) * t),
				(float)(a.blueF() + (b.blueF() - a.blueF()) * t));
}

QString css(const QColor &c)
{
	return c.name(QColor::HexRgb);
}
} // namespace

DockColors DockColors::from(const QPalette &p)
{
	DockColors c;
	c.bg = p.color(QPalette::Window);
	c.text = p.color(QPalette::WindowText);
	c.accent = p.color(QPalette::Highlight);
	// Paneles: un paso hacia el texto desde el fondo, como en el HTML (#1d1f26 → #262933 → #2f3340)
	c.panel = mix(c.bg, c.text, 0.04);
	c.panel2 = mix(c.bg, c.text, 0.08);
	c.line = mix(c.bg, c.text, 0.16);
	c.muted = mix(c.bg, c.text, 0.62);
	c.live = QColor(0xf0, 0x52, 0x52); // al aire: rojo, como el indicador de transmisión
	return c;
}

QString dock_stylesheet(const DockColors &c)
{
	QString qss(R"(
QScrollArea#sdScroll, QWidget#sdList { background: transparent; }
QFrame#sdTop { border-bottom: 1px solid %4; }
QCheckBox#sdOption { color: %6; font-size: 11px; }

QFrame#sdCard { background: %2; border: 1px solid %4; border-radius: 8px; }
QFrame#sdCard[live="air"] { border-color: %8; }
QFrame#sdCard[live="preview"] { border-color: %7; }
QFrame#sdCardHead { background: %3; border: none; border-top-left-radius: 7px; border-top-right-radius: 7px; }
QLabel#sdCardTitle { color: %6; font-size: 11px; font-weight: 600; }
QLabel#sdBadge { color: white; border-radius: 7px; padding: 0 6px; font-size: 10px; font-weight: 600; }
QLabel#sdBadge[live="air"] { background: %8; }
QLabel#sdBadge[live="preview"] { background: %7; }

QFrame#sdFilter { border: none; border-top: 1px solid %4; background: transparent; }
QFrame#sdFilter[first="true"] { border-top: none; }
QLabel#sdName { color: %5; font-weight: 600; }
QLabel#sdDetail { color: %6; font-size: 11px; }
QFrame#sdFilter[off="true"] QLabel#sdName { color: %6; }
QFrame#sdFilter[off="true"] QLabel#sdDetail { color: %6; }

QToolButton#sdCtl { background: %3; border: 1px solid %4; border-radius: 6px; padding: 0; }
QToolButton#sdCtl:hover { border-color: %7; }
QToolButton#sdCtl[playing="true"] { background: %7; border-color: %7; }
QLabel#sdPct { color: %6; font-size: 11px; }
QCheckBox#sdLoop { color: %6; font-size: 11px; }

QSlider#sdProgress::groove:horizontal { height: 4px; background: %4; border-radius: 2px; }
QSlider#sdProgress::sub-page:horizontal { background: %7; border-radius: 2px; }
QSlider#sdProgress::handle:horizontal { background: %7; width: 12px; height: 12px; margin: -4px 0; border-radius: 6px; }
QSlider#sdProgress::handle:horizontal:hover { background: %5; }

QLabel#sdEmpty { color: %6; font-size: 11px; }
QLabel#sdEmptyTitle { color: %5; font-weight: 600; font-size: 13px; }
QPushButton#sdAdd { background: %7; color: white; border: none; border-radius: 6px; padding: 6px 14px; font-weight: 600; }
QPushButton#sdAdd:hover { background: %9; }
)");
	// Sustitución explícita: QString::arg con varios argumentos numera solo los marcadores que
	// aparecen, y como %1 no se usa, recorría todos los colores un lugar
	const QString values[] = {css(c.bg),     css(c.panel), css(c.panel2),
				  css(c.line),   css(c.text),  css(c.muted),
				  css(c.accent), css(c.live),  css(c.accent.lighter(115))};
	for (int n = 9; n >= 1; n--)
		qss.replace(QString("%%1").arg(n), values[n - 1]);
	return qss;
}

/* ------------------------------------------------------------------ íconos */

// Los mismos trazos del HTML, en una caja de 24 × 24
QIcon glyph_icon(Glyph g, const QColor &color)
{
	QPainterPath path;
	switch (g) {
	case Glyph::Start:
		path.addRect(6, 6, 2, 12);
		path.addPolygon(QPolygonF({{9.5, 12}, {18, 18}, {18, 6}, {9.5, 12}}));
		break;
	case Glyph::Play:
		path.addPolygon(QPolygonF({{8, 5}, {8, 19}, {19, 12}, {8, 5}}));
		break;
	case Glyph::Stop:
		path.addRect(6, 6, 12, 12);
		break;
	case Glyph::End:
		path.addPolygon(QPolygonF({{6, 18}, {14.5, 12}, {6, 6}, {6, 18}}));
		path.addRect(16, 6, 2, 12);
		break;
	}
	QIcon icon;
	for (int size : {16, 32, 48}) {
		QPixmap pm(size, size);
		pm.fill(Qt::transparent);
		QPainter p(&pm);
		p.setRenderHint(QPainter::Antialiasing);
		p.scale(size / 24.0, size / 24.0);
		p.fillPath(path, color);
		p.end();
		icon.addPixmap(pm);
	}
	return icon;
}

/* ------------------------------------------------------------------ interruptor */

Switch::Switch(QWidget *parent) : QAbstractButton(parent)
{
	setCheckable(true);
	setCursor(Qt::PointingHandCursor);
	setFocusPolicy(Qt::TabFocus);
}

QSize Switch::sizeHint() const
{
	return {30, 17};
}

void Switch::paintEvent(QPaintEvent *)
{
	DockColors c = DockColors::from(palette());
	QPainter p(this);
	p.setRenderHint(QPainter::Antialiasing);
	QRectF r(0.5, (height() - 17) / 2.0 + 0.5, 29, 16);
	p.setPen(hasFocus() ? QPen(c.accent, 1.5) : Qt::NoPen);
	p.setBrush(isChecked() ? c.accent : c.line);
	p.drawRoundedRect(r, 8, 8);
	p.setPen(Qt::NoPen);
	p.setBrush(Qt::white);
	double x = isChecked() ? r.right() - 14.5 : r.left() + 1.5;
	p.drawEllipse(QRectF(x, r.top() + 1.5, 13, 13));
}

/* ------------------------------------------------------------------ pestañas */

TabButton::TabButton(const QString &text, QWidget *parent) : QAbstractButton(parent)
{
	setText(text);
	setCheckable(true);
	setAutoExclusive(true);
	setCursor(Qt::PointingHandCursor);
	setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
}

void TabButton::setCount(int n)
{
	count = n;
	update();
}

QSize TabButton::sizeHint() const
{
	QFont f = font();
	f.setBold(true);
	QFontMetrics fm(f);
	return {fm.horizontalAdvance(text()) + 40, fm.height() + 14};
}

void TabButton::enterEvent(QEnterEvent *)
{
	hover = true;
	update();
}

void TabButton::leaveEvent(QEvent *)
{
	hover = false;
	update();
}

void TabButton::paintEvent(QPaintEvent *)
{
	DockColors c = DockColors::from(palette());
	QPainter p(this);
	p.setRenderHint(QPainter::Antialiasing);

	QFont f = font();
	f.setBold(true);
	p.setFont(f);
	QFontMetrics fm(f);

	QFont small = font();
	small.setPointSizeF(small.pointSizeF() * 0.8);
	small.setBold(true);
	QFontMetrics sm(small);
	QString num = QString::number(count);
	int bubble = count > 0 ? std::max(18, sm.horizontalAdvance(num) + 10) : 0;
	int total = fm.horizontalAdvance(text()) + (bubble ? 6 + bubble : 0);
	int x = (width() - total) / 2;
	int mid = (height() - 2) / 2;

	p.setPen(isChecked() || hover ? c.text : c.muted);
	p.drawText(QRect(x, 0, total, height() - 2), Qt::AlignLeft | Qt::AlignVCenter, text());

	if (bubble) {
		QRectF b(x + total - bubble, mid - 8, bubble, 16);
		p.setPen(Qt::NoPen);
		p.setBrush(isChecked() ? c.accent : c.panel2);
		p.drawRoundedRect(b, 8, 8);
		p.setFont(small);
		p.setPen(isChecked() ? QColor(Qt::white) : c.muted);
		p.drawText(b, Qt::AlignCenter, num);
	}
	if (isChecked())
		p.fillRect(QRect(0, height() - 2, width(), 2), c.accent);
}

/* ------------------------------------------------------------------ lista vacía */

QPixmap empty_art(const QColor &color, int height, qreal dpr)
{
	char *path = obs_module_file("empty.svg");
	QSvgRenderer svg(QString::fromUtf8(path ? path : ""));
	bfree(path);
	if (!svg.isValid())
		return {};
	QSizeF size = svg.defaultSize();
	const int width = (int)std::lround(size.width() * height / size.height());
	QPixmap pm(QSize(width, height) * dpr);
	pm.setDevicePixelRatio(dpr);
	pm.fill(Qt::transparent);
	QPainter p(&pm);
	p.setRenderHint(QPainter::Antialiasing);
	svg.render(&p, QRectF(0, 0, width, height));
	// La forma queda; el color se cambia por el del texto
	p.setCompositionMode(QPainter::CompositionMode_SourceIn);
	p.fillRect(QRectF(0, 0, width, height), color);
	p.end();
	return pm;
}
