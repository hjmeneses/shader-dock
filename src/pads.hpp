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

// Pads de geometría: se arrastra con el mouse sobre una captura de la fuente.
//   RotationPad: 3D Effect (Exeldro), 3D Effect 2 y BFX 3D Rotation
//   CornerPad:   Perspectiva (corner pin, perspectiva.shader)

#pragma once

#include <obs.hpp>

#include <QImage>
#include <QPointF>
#include <QTimer>
#include <QWidget>

#include <array>

#include "widgets.hpp"

class QLabel;
class QToolButton;

// Captura de la fuente con sus filtros, como la ve la escena (null si no se pudo)
QImage grab_source(obs_source_t *source, int max_width);

// Área de dibujo con la captura de fondo; la comparten los dos pads
class PadBase : public QWidget {
	Q_OBJECT
public:
	PadBase(obs_source_t *source, obs_source_t *filter, QWidget *parent);
	bool dragging() const { return drag; }
	virtual void reload() = 0; // vuelve a leer los ajustes del filtro

protected:
	OBSWeakSourceAutoRelease source;
	OBSWeakSourceAutoRelease filter;
	QImage shot;
	QTimer shotTimer;
	bool drag = false;
	bool shotBusy = false;
	int shotWidth = 480;

	void apply(obs_data_t *settings);    // cambia los ajustes del filtro (los demás se conservan)
	OBSDataAutoRelease settings() const; // ajustes actuales del filtro
	void refreshShot(int delay);         // captura nueva dentro de delay ms
	void grabNow();                      // captura ya (al arrastrar)
	virtual void shotTaken() {}
	void resizeEvent(QResizeEvent *) override;
	void fitHeight(); // alto mínimo según el ancho (el área con scroll no respeta heightForWidth)

	QLabel *readout;
	QToolButton *footButton(const QString &text, const QString &tip);
	QToolButton *resetButton(const QString &tip);
	DockColors colors() const { return DockColors::from(palette()); }
};

class RotationPad : public PadBase {
	Q_OBJECT
public:
	enum class Kind { Effect3D, Bfx };
	RotationPad(Kind kind, obs_source_t *source, obs_source_t *filter, QWidget *parent = nullptr);
	void reload() override;
	QSize sizeHint() const override;
	int heightForWidth(int w) const override;

protected:
	void paintEvent(QPaintEvent *) override;
	void mousePressEvent(QMouseEvent *) override;
	void mouseMoveEvent(QMouseEvent *) override;
	void mouseReleaseEvent(QMouseEvent *) override;
	void mouseDoubleClickEvent(QMouseEvent *) override;
	void wheelEvent(QWheelEvent *) override;
	void keyPressEvent(QKeyEvent *) override;
	void shotTaken() override;

private:
	Kind kind;
	// 3D Effect: giro X/Y/Z en grados y posición en px. BFX: giro X/Y (modos 1 y 2) o, en el modo
	// 3, ángulo de giro (rz) alrededor de un eje del plano XY a "axis" grados; escala.
	double rx = 0, ry = 0, rz = 0, px = 0, py = 0, pz = 0;
	double axis = 0, scale = 1;
	int bfxMode = 1;
	QPoint last;

	QRect area() const; // la parte de arriba del widget (el pie va debajo)
	void push();        // manda los valores al filtro
	void show();        // texto de abajo + repintado
	void zero();
	void resetAxis(char a);
};

class CornerPad : public PadBase {
	Q_OBJECT
public:
	CornerPad(obs_source_t *source, obs_source_t *filter, QWidget *parent = nullptr);
	void reload() override;
	QSize sizeHint() const override;
	int heightForWidth(int w) const override;

protected:
	void paintEvent(QPaintEvent *) override;
	void mousePressEvent(QMouseEvent *) override;
	void mouseMoveEvent(QMouseEvent *) override;
	void mouseReleaseEvent(QMouseEvent *) override;
	void mouseDoubleClickEvent(QMouseEvent *) override;
	void leaveEvent(QEvent *) override;
	void shotTaken() override;

private:
	std::array<QPointF, 4> corners; // tl, tr, br, bl en % de la fuente
	std::array<QPointF, 4> dragBase;
	QPointF dragStart;
	int dragIdx = -1; // 0..3 una esquina, 4 = todas
	int partner = -1;
	int hover = -1;
	int expandL = 0, expandR = 0, expandT = 0, expandB = 0;
	QSize base; // tamaño de la fuente en px (captura menos el margen de Expandir)
	double aspect = 9.0 / 16.0;
	QToolButton *grow;

	QRect area() const;
	QPointF toPx(QPointF pct) const;
	QPointF toPct(QPointF px) const;
	int nearest(QPointF px) const;
	bool inside(QPointF pct) const;
	bool expanded() const { return expandL > 0 || expandR > 0 || expandT > 0 || expandB > 0; }
	void push();
	void show();
	void showGrow();
	void zero();
};
