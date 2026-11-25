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

// Piezas de la interfaz con el estilo del dock HTML, pero con los colores del tema de OBS:
// todo sale de la paleta (fondo, texto, acento = "highlight"), así que cambia con el tema.

#pragma once

#include <QAbstractButton>
#include <QColor>
#include <QIcon>
#include <QPalette>
#include <QString>

// Colores derivados de la paleta del tema
struct DockColors {
	QColor bg, panel, panel2, line, text, muted, accent, live;
	static DockColors from(const QPalette &p);
};

QString dock_stylesheet(const DockColors &c);

enum class Glyph { Start, Play, Stop, End };
QIcon glyph_icon(Glyph g, const QColor &color);

// Interruptor tipo "switch" (como el del HTML)
class Switch : public QAbstractButton {
	Q_OBJECT
public:
	explicit Switch(QWidget *parent = nullptr);
	QSize sizeHint() const override;

protected:
	void paintEvent(QPaintEvent *) override;
};

// Pestaña con subrayado de acento y el conteo en una burbuja
class TabButton : public QAbstractButton {
	Q_OBJECT
public:
	explicit TabButton(const QString &text, QWidget *parent = nullptr);
	void setCount(int n);
	QSize sizeHint() const override;

protected:
	void paintEvent(QPaintEvent *) override;
	void enterEvent(QEnterEvent *) override;
	void leaveEvent(QEvent *) override;

private:
	int count = 0;
	bool hover = false;
};
