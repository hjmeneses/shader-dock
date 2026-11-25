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

#include "pads.hpp"

#include <obs-module.h>
#include <graphics/vec4.h>

#include <QHBoxLayout>
#include <QKeyEvent>
#include <QLabel>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QToolButton>
#include <QVBoxLayout>
#include <QWheelEvent>

#include <algorithm>
#include <cmath>

namespace {

const int FOOT = 30;           // alto del pie (texto y botones)
const double DEG_PER_PX = 0.5; // giro por px arrastrado
const double MOVE_PX_PER_PX = 4.0;
const double WHEEL_Z = 40.0;

QString T(const char *key)
{
	return QString::fromUtf8(obs_module_text(key));
}

double wrap180(double a)
{
	return std::fmod(std::fmod(a + 180.0, 360.0) + 360.0, 360.0) - 180.0;
}

double r2(double v)
{
	return std::round(v * 100.0) / 100.0;
}

// Ícono "reiniciar" del HTML (flecha circular), dibujado a mano
QIcon reset_icon(const QColor &color)
{
	QIcon icon;
	for (int size : {16, 32}) {
		QPixmap pm(size, size);
		pm.fill(Qt::transparent);
		QPainter p(&pm);
		p.setRenderHint(QPainter::Antialiasing);
		p.scale(size / 24.0, size / 24.0);
		QPen pen(color, 2.0);
		pen.setCapStyle(Qt::FlatCap);
		p.setPen(pen);
		p.drawArc(QRectF(5, 5, 14, 14), 100 * 16, 290 * 16);
		p.setPen(Qt::NoPen);
		p.setBrush(color);
		p.drawPolygon(QPolygonF({{12, 2}, {7, 6}, {12, 10}}));
		p.end();
		icon.addPixmap(pm);
	}
	return icon;
}

} // namespace

/* ------------------------------------------------------------------ captura */

QImage grab_source(obs_source_t *source, int max_width)
{
	if (!source)
		return {};
	uint32_t w = obs_source_get_width(source), h = obs_source_get_height(source);
	if (!w || !h)
		return {};
	uint32_t cw = w, ch = h;
	if (max_width > 0 && cw > (uint32_t)max_width) {
		cw = (uint32_t)max_width;
		ch = std::max<uint32_t>(1, (uint32_t)std::lround((double)h * cw / w));
	}

	QImage img;
	obs_enter_graphics();
	gs_texrender_t *tr = gs_texrender_create(GS_RGBA, GS_ZS_NONE);
	gs_stagesurf_t *stage = gs_stagesurface_create(cw, ch, GS_RGBA);
	if (tr && stage && gs_texrender_begin(tr, cw, ch)) {
		struct vec4 clear;
		vec4_zero(&clear);
		gs_clear(GS_CLEAR_COLOR, &clear, 0.0f, 0);
		gs_ortho(0.0f, (float)w, 0.0f, (float)h, -100.0f, 100.0f);
		gs_blend_state_push();
		gs_blend_function_separate(GS_BLEND_SRCALPHA, GS_BLEND_INVSRCALPHA, GS_BLEND_ONE, GS_BLEND_INVSRCALPHA);
		obs_source_video_render(source);
		gs_blend_state_pop();
		gs_texrender_end(tr);

		gs_stage_texture(stage, gs_texrender_get_texture(tr));
		uint8_t *data;
		uint32_t linesize;
		if (gs_stagesurface_map(stage, &data, &linesize)) {
			// El resultado queda premultiplicado (color ya multiplicado por alfa)
			img = QImage((int)cw, (int)ch, QImage::Format_RGBA8888_Premultiplied);
			for (uint32_t y = 0; y < ch; y++)
				memcpy(img.scanLine((int)y), data + (size_t)y * linesize, (size_t)cw * 4);
			gs_stagesurface_unmap(stage);
		}
	}
	gs_stagesurface_destroy(stage);
	gs_texrender_destroy(tr);
	obs_leave_graphics();
	return img;
}

/* ------------------------------------------------------------------ base */

PadBase::PadBase(obs_source_t *src, obs_source_t *flt, QWidget *parent)
	: QWidget(parent),
	  source(obs_source_get_weak_source(src)),
	  filter(obs_source_get_weak_source(flt))
{
	setFocusPolicy(Qt::StrongFocus);
	setMouseTracking(true);
	// Una fuente que no está en la escena al aire no se carga ni se dibuja (las imágenes se
	// descargan, por ejemplo). Mientras el pad exista, cuenta como "mostrándose", igual que en
	// un proyector o en la vista previa.
	if (src)
		obs_source_inc_showing(src);
	shotTimer.setSingleShot(true);
	connect(&shotTimer, &QTimer::timeout, this, &PadBase::grabNow);

	auto *v = new QVBoxLayout(this);
	v->setContentsMargins(0, 0, 0, 0);
	v->addStretch(1);
	auto *foot = new QHBoxLayout();
	foot->setSpacing(4);
	readout = new QLabel();
	readout->setObjectName("sdPct");
	readout->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
	foot->addWidget(readout, 1);
	v->addLayout(foot);
}

PadBase::~PadBase()
{
	OBSSourceAutoRelease s = obs_weak_source_get_source(source);
	if (s)
		obs_source_dec_showing(s);
}

QToolButton *PadBase::footButton(const QString &text, const QString &tip)
{
	auto *b = new QToolButton();
	b->setObjectName("sdCtl");
	b->setText(text);
	b->setToolTip(tip);
	b->setFixedSize(24, 24);
	b->setCursor(Qt::PointingHandCursor);
	static_cast<QHBoxLayout *>(layout()->itemAt(1)->layout())->addWidget(b);
	return b;
}

QToolButton *PadBase::resetButton(const QString &tip)
{
	QToolButton *b = footButton(QString(), tip);
	b->setIcon(reset_icon(colors().text));
	b->setIconSize(QSize(14, 14));
	return b;
}

void PadBase::apply(obs_data_t *s)
{
	OBSSourceAutoRelease f = obs_weak_source_get_source(filter);
	if (f)
		obs_source_update(f, s);
}

OBSDataAutoRelease PadBase::settings() const
{
	OBSSourceAutoRelease f = obs_weak_source_get_source(filter);
	return f ? obs_source_get_settings(f) : obs_data_create();
}

void PadBase::resizeEvent(QResizeEvent *e)
{
	QWidget::resizeEvent(e);
	fitHeight();
}

void PadBase::fitHeight()
{
	int h = heightForWidth(width() > 0 ? width() : 280);
	if (minimumHeight() != h)
		setMinimumHeight(h);
}

void PadBase::refreshShot(int delay)
{
	shotTimer.start(delay);
}

void PadBase::grabNow()
{
	if (shotBusy)
		return;
	shotBusy = true;
	OBSSourceAutoRelease s = obs_weak_source_get_source(source);
	QImage img = grab_source(s, shotWidth);
	shotBusy = false;
	// Recién mostrada, la fuente puede tardar unos cuadros en cargar: se reintenta
	bool empty = img.isNull();
	if (!empty) {
		empty = true;
		for (int y = 0; y < img.height() && empty; y += 4) {
			const QRgb *row = reinterpret_cast<const QRgb *>(img.constScanLine(y));
			for (int x = 0; x < img.width(); x += 4)
				if (qAlpha(row[x])) {
					empty = false;
					break;
				}
		}
	}
	if (empty && emptyShots < 10) {
		emptyShots++;
		refreshShot(300);
		if (img.isNull())
			return;
	} else if (!empty) {
		emptyShots = 0;
	}
	shot = img;
	shotTaken();
	update();
}

/* ------------------------------------------------------------------ giro 3D */

RotationPad::RotationPad(Kind k, obs_source_t *src, obs_source_t *flt, QWidget *parent)
	: PadBase(src, flt, parent),
	  kind(k)
{
	setToolTip(kind == Kind::Bfx ? T("BfxHelp") : T("Fx3dHelp"));
	shotWidth = 360;
	for (char a : {'X', 'Y', 'Z'}) {
		if (kind == Kind::Bfx && a == 'Z')
			break;
		QToolButton *b = footButton(QString(a), T("Fx3dResetAxis").arg(QChar(a)));
		connect(b, &QToolButton::clicked, this, [this, a] { resetAxis(a); });
	}
	connect(resetButton(T("Fx3dReset")), &QToolButton::clicked, this, &RotationPad::zero);
	reload();
	fitHeight();
	refreshShot(0);
}

void RotationPad::reload()
{
	OBSDataAutoRelease s = settings();
	if (kind == Kind::Effect3D) {
		rx = obs_data_get_double(s, "rot_x");
		ry = obs_data_get_double(s, "rot_y");
		rz = obs_data_get_double(s, "rot_z");
		px = obs_data_get_double(s, "pos_x");
		py = obs_data_get_double(s, "pos_y");
		pz = obs_data_get_double(s, "pos_z");
	} else {
		bfxMode = (int)obs_data_get_int(s, "control_mode");
		rx = obs_data_get_double(s, "rotation_x_angle");
		ry = obs_data_get_double(s, "rotation_y_angle");
		rz = obs_data_get_double(s, "rotation_axis_angle");
		axis = obs_data_get_double(s, "axis_angle");
		scale = obs_data_get_double(s, "scale");
		if (scale <= 0.0)
			scale = 1.0;
	}
	show();
	refreshShot(150);
}

QSize RotationPad::sizeHint() const
{
	return {280, heightForWidth(280)};
}

int RotationPad::heightForWidth(int w) const
{
	int h = 120;
	if (!shot.isNull())
		h = std::clamp((int)std::lround((double)w * shot.height() / shot.width()), 90, 240);
	return h + FOOT;
}

QRect RotationPad::area() const
{
	return QRect(0, 0, width(), height() - FOOT);
}

void RotationPad::shotTaken()
{
	fitHeight();
}

void RotationPad::push()
{
	OBSDataAutoRelease d = obs_data_create();
	if (kind == Kind::Effect3D) {
		obs_data_set_double(d, "rot_x", r2(rx));
		obs_data_set_double(d, "rot_y", r2(ry));
		obs_data_set_double(d, "rot_z", r2(rz));
		obs_data_set_double(d, "pos_x", std::round(px));
		obs_data_set_double(d, "pos_y", std::round(py));
		obs_data_set_double(d, "pos_z", std::round(pz));
	} else {
		obs_data_set_double(d, "rotation_x_angle", r2(rx));
		obs_data_set_double(d, "rotation_y_angle", r2(ry));
		obs_data_set_double(d, "rotation_axis_angle", r2(rz));
		obs_data_set_double(d, "axis_angle", r2(axis));
		obs_data_set_double(d, "scale", std::round(scale * 1000.0) / 1000.0);
	}
	apply(d);
}

void RotationPad::show()
{
	QString t;
	if (kind == Kind::Effect3D) {
		t = QString("X %1°  Y %2°  Z %3°").arg(std::lround(rx)).arg(std::lround(ry)).arg(std::lround(rz));
		if (px != 0.0 || py != 0.0 || pz != 0.0)
			t += QString("   ·   %1, %2, %3 px")
				     .arg(std::lround(px))
				     .arg(std::lround(py))
				     .arg(std::lround(pz));
	} else if (bfxMode == 3) {
		t = QString("%1° · %2 %3°  ×%4")
			    .arg(std::lround(rz))
			    .arg(T("BfxAxis"))
			    .arg(std::lround(axis))
			    .arg(scale, 0, 'f', 2);
	} else {
		t = QString("X %1°  Y %2°  ×%3").arg(std::lround(rx)).arg(std::lround(ry)).arg(scale, 0, 'f', 2);
	}
	readout->setText(t);
	update();
}

void RotationPad::zero()
{
	rx = ry = rz = px = py = pz = 0.0;
	axis = 0.0;
	scale = 1.0;
	show();
	push();
	refreshShot(150);
}

void RotationPad::resetAxis(char a)
{
	if (a == 'X')
		rx = 0.0;
	else if (a == 'Y')
		ry = 0.0;
	else
		rz = 0.0;
	show();
	push();
	refreshShot(150);
}

void RotationPad::mousePressEvent(QMouseEvent *e)
{
	if (e->button() != Qt::LeftButton || !area().contains(e->position().toPoint()))
		return;
	last = e->position().toPoint();
	drag = true;
	setCursor(Qt::ClosedHandCursor);
	update();
}

void RotationPad::mouseMoveEvent(QMouseEvent *e)
{
	if (!drag) {
		setCursor(area().contains(e->position().toPoint()) ? Qt::OpenHandCursor : Qt::ArrowCursor);
		return;
	}
	QPoint pt = e->position().toPoint();
	double dx = pt.x() - last.x(), dy = pt.y() - last.y();
	last = pt;
	const auto mods = e->modifiers();
	if (kind == Kind::Effect3D) {
		if (mods & Qt::ControlModifier) {
			px += dx * MOVE_PX_PER_PX;
			py += dy * MOVE_PX_PER_PX;
		} else if (mods & Qt::ShiftModifier) {
			rz = wrap180(rz + dx * DEG_PER_PX);
		} else {
			ry = wrap180(ry + dx * DEG_PER_PX);
			rx = wrap180(rx + dy * DEG_PER_PX);
		}
	} else if (bfxMode == 3) {
		// Modo eje: horizontal = giro alrededor del eje, vertical = ángulo del eje (0..360)
		rz = wrap180(rz + dx * DEG_PER_PX);
		axis = std::fmod(std::fmod(axis + dy * DEG_PER_PX, 360.0) + 360.0, 360.0);
	} else {
		ry = wrap180(ry + dx * DEG_PER_PX);
		rx = wrap180(rx + dy * DEG_PER_PX);
	}
	show();
	push();
	grabNow();
}

void RotationPad::mouseReleaseEvent(QMouseEvent *)
{
	if (!drag)
		return;
	drag = false;
	setCursor(Qt::OpenHandCursor);
	update();
	// Una captura más con el valor final
	refreshShot(150);
}

void RotationPad::mouseDoubleClickEvent(QMouseEvent *e)
{
	if (area().contains(e->position().toPoint()))
		zero();
}

void RotationPad::wheelEvent(QWheelEvent *e)
{
	const int dir = e->angleDelta().y() > 0 ? 1 : -1;
	if (kind == Kind::Effect3D)
		pz += dir * WHEEL_Z;
	else
		scale = std::clamp(scale * (dir > 0 ? 1.05 : 1.0 / 1.05), 0.1, 4.0);
	show();
	push();
	refreshShot(120);
	e->accept();
}

void RotationPad::keyPressEvent(QKeyEvent *e)
{
	const double step = 5.0;
	const int k = e->key();
	if (k != Qt::Key_Left && k != Qt::Key_Right && k != Qt::Key_Up && k != Qt::Key_Down) {
		QWidget::keyPressEvent(e);
		return;
	}
	const double s = (k == Qt::Key_Right || k == Qt::Key_Down) ? step : -step;
	if ((e->modifiers() & Qt::ShiftModifier) && (k == Qt::Key_Left || k == Qt::Key_Right))
		rz = wrap180(rz + s);
	else if (k == Qt::Key_Left || k == Qt::Key_Right)
		ry = wrap180(ry + s);
	else
		rx = wrap180(rx + s);
	show();
	push();
	refreshShot(120);
}

void RotationPad::paintEvent(QPaintEvent *)
{
	DockColors c = colors();
	QPainter p(this);
	p.setRenderHint(QPainter::Antialiasing);
	QRectF a = QRectF(area()).adjusted(0.5, 0.5, -0.5, -0.5);
	QPainterPath clip;
	clip.addRoundedRect(a, 6, 6);
	p.fillPath(clip, c.bg);
	p.setClipPath(clip);

	// Cruz de referencia
	p.setPen(QPen(c.line, 1));
	p.drawLine(QPointF(a.center().x(), a.top()), QPointF(a.center().x(), a.bottom()));
	p.drawLine(QPointF(a.left(), a.center().y()), QPointF(a.right(), a.center().y()));

	if (!shot.isNull()) {
		QSizeF s = QSizeF(shot.size()).scaled(a.size(), Qt::KeepAspectRatio);
		QRectF r(a.center().x() - s.width() / 2, a.center().y() - s.height() / 2, s.width(), s.height());
		p.drawImage(r, shot);
	}

	// Tarjeta de orientación: la misma rotación sobre un rectángulo (perspectiva simple)
	const bool small = !shot.isNull();
	const double cw = small ? 34 : 96, ch = small ? 19 : 54;
	const double persp = small ? 140 : 260;
	QPointF center = small ? QPointF(a.right() - 6 - 28, a.bottom() - 6 - 20) : a.center();
	if (small) {
		p.setPen(Qt::NoPen);
		p.setBrush(QColor(0, 0, 0, 115));
		p.drawRoundedRect(QRectF(center.x() - 28, center.y() - 20, 56, 40), 4, 4);
	}
	auto rad = [](double d) {
		return d * 3.14159265358979 / 180.0;
	};
	auto project = [&](double x, double y) {
		double z = 0.0;
		auto rotX = [&](double t) {
			double y2 = y * std::cos(t) - z * std::sin(t), z2 = y * std::sin(t) + z * std::cos(t);
			y = y2;
			z = z2;
		};
		auto rotY = [&](double t) {
			double x2 = x * std::cos(t) + z * std::sin(t), z2 = -x * std::sin(t) + z * std::cos(t);
			x = x2;
			z = z2;
		};
		auto rotZ = [&](double t) {
			double x2 = x * std::cos(t) - y * std::sin(t), y2 = x * std::sin(t) + y * std::cos(t);
			x = x2;
			y = y2;
		};
		if (kind == Kind::Effect3D) {
			rotZ(rad(rz));
			rotY(rad(ry));
			rotX(rad(rx));
		} else if (bfxMode == 3) {
			// Giro alrededor del eje (cos a, sin a, 0): Rodrigues
			double t = rad(rz), ax = std::cos(rad(axis)), ay = std::sin(rad(axis));
			double d = ax * x + ay * y, ct = std::cos(t), st = std::sin(t);
			double x2 = x * ct + ax * d * (1 - ct);
			double y2 = y * ct + ay * d * (1 - ct);
			double z2 = (ax * y - ay * x) * st;
			x = x2;
			y = y2;
			z = z2;
		} else if (bfxMode == 2) {
			rotX(rad(rx));
			rotY(rad(ry));
		} else {
			rotY(rad(ry));
			rotX(rad(rx));
		}
		double k = persp / (persp - z);
		return QPointF(center.x() + x * k, center.y() + y * k);
	};
	QPolygonF card({project(-cw / 2, -ch / 2), project(cw / 2, -ch / 2), project(cw / 2, ch / 2),
			project(-cw / 2, ch / 2)});
	QColor fill = c.accent;
	fill.setAlphaF(0.4f);
	p.setPen(QPen(c.accent, 1));
	p.setBrush(fill);
	p.drawPolygon(card);
	// Marca en la esquina de arriba a la izquierda, para ver si la tarjeta está de espaldas
	const double m = small ? 5 : 10, o = small ? 2 : 4;
	QPolygonF mark({project(-cw / 2 + o, -ch / 2 + o), project(-cw / 2 + o + m, -ch / 2 + o),
			project(-cw / 2 + o + m, -ch / 2 + o + m), project(-cw / 2 + o, -ch / 2 + o + m)});
	p.setPen(Qt::NoPen);
	p.setBrush(QColor(0xf0, 0xb4, 0x29));
	p.drawPolygon(mark);

	p.setClipping(false);
	p.setPen(QPen(drag || hasFocus() ? c.accent : c.line, 1));
	p.setBrush(Qt::NoBrush);
	p.drawRoundedRect(a, 6, 6);
}

/* ------------------------------------------------------------------ esquinas */

namespace {
const char *const CORNER_KEYS[4][2] = {{"tl_x", "tl_y"}, {"tr_x", "tr_y"}, {"br_x", "br_y"}, {"bl_x", "bl_y"}};
const QPointF CORNER_DEF[4] = {{0, 0}, {100, 0}, {100, 100}, {0, 100}};
const double VIEW_MIN = -30.0; // el pad muestra de -30 % a 130 % para poder sacar esquinas
const double VIEW_SPAN = 160.0;
const double HIT = 12.0;

double clampPct(double v)
{
	return std::clamp(v, -50.0, 150.0);
}

// La misma homografía que el shader (cuadrado unitario → esquinas), para dibujar la rejilla
struct Homography {
	double a, b, d, e, g, h;
	QPointF p0;
	explicit Homography(const std::array<QPointF, 4> &p)
	{
		p0 = p[0];
		double sx = p[0].x() - p[1].x() + p[2].x() - p[3].x(), sy = p[0].y() - p[1].y() + p[2].y() - p[3].y();
		double d1x = p[1].x() - p[2].x(), d1y = p[1].y() - p[2].y(), d2x = p[3].x() - p[2].x(),
		       d2y = p[3].y() - p[2].y();
		double den = d1x * d2y - d2x * d1y;
		g = h = 0.0;
		if (std::abs(den) > 1e-9) {
			g = (sx * d2y - d2x * sy) / den;
			h = (d1x * sy - sx * d1y) / den;
		}
		a = p[1].x() - p[0].x() + g * p[1].x();
		b = p[3].x() - p[0].x() + h * p[3].x();
		d = p[1].y() - p[0].y() + g * p[1].y();
		e = p[3].y() - p[0].y() + h * p[3].y();
	}
	QPointF operator()(double u, double v) const
	{
		double w = g * u + h * v + 1.0;
		return {(a * u + b * v + p0.x()) / w, (d * u + e * v + p0.y()) / w};
	}
};
} // namespace

CornerPad::CornerPad(obs_source_t *src, obs_source_t *flt, QWidget *parent) : PadBase(src, flt, parent)
{
	setToolTip(T("CpHelp"));
	shotWidth = 0; // la primera captura completa, para conocer los px de la fuente
	grow = footButton(QString(), T("CpExpand"));
	grow->setText(QString::fromUtf8("⤢"));
	connect(grow, &QToolButton::clicked, this, [this] {
		bool on = expanded();
		if (!on && base.isEmpty())
			return;
		if (on) {
			expandL = expandR = expandT = expandB = 0;
		} else {
			expandL = expandR = (int)std::lround(base.width() / 2.0);
			expandT = expandB = (int)std::lround(base.height() / 2.0);
		}
		OBSDataAutoRelease d = obs_data_create();
		obs_data_set_int(d, "expand_left", expandL);
		obs_data_set_int(d, "expand_right", expandR);
		obs_data_set_int(d, "expand_top", expandT);
		obs_data_set_int(d, "expand_bottom", expandB);
		apply(d);
		showGrow();
		update();
		refreshShot(300);
	});
	connect(resetButton(T("CpReset")), &QToolButton::clicked, this, &CornerPad::zero);
	reload();
	fitHeight();
	refreshShot(0);
}

void CornerPad::reload()
{
	OBSDataAutoRelease s = settings();
	for (int i = 0; i < 4; i++) {
		const char *kx = CORNER_KEYS[i][0], *ky = CORNER_KEYS[i][1];
		double x = obs_data_has_user_value(s, kx) || obs_data_has_default_value(s, kx)
				   ? obs_data_get_double(s, kx)
				   : CORNER_DEF[i].x();
		double y = obs_data_has_user_value(s, ky) || obs_data_has_default_value(s, ky)
				   ? obs_data_get_double(s, ky)
				   : CORNER_DEF[i].y();
		corners[i] = {x, y};
	}
	expandL = (int)obs_data_get_int(s, "expand_left");
	expandR = (int)obs_data_get_int(s, "expand_right");
	expandT = (int)obs_data_get_int(s, "expand_top");
	expandB = (int)obs_data_get_int(s, "expand_bottom");
	showGrow();
	show();
	refreshShot(250);
}

QSize CornerPad::sizeHint() const
{
	return {280, heightForWidth(280)};
}

int CornerPad::heightForWidth(int w) const
{
	return (int)std::lround(w * aspect) + FOOT;
}

QRect CornerPad::area() const
{
	return QRect(0, 0, width(), height() - FOOT);
}

void CornerPad::shotTaken()
{
	if (base.isEmpty()) {
		int w = shot.width() - expandL - expandR, h = shot.height() - expandT - expandB;
		if (w > 0 && h > 0) {
			base = QSize(w, h);
			aspect = (double)h / w;
			fitHeight();
		}
		shotWidth = 480; // las siguientes, chicas
		showGrow();
	}
}

QPointF CornerPad::toPx(QPointF pct) const
{
	QRect a = area();
	return {(pct.x() - VIEW_MIN) / VIEW_SPAN * a.width(), (pct.y() - VIEW_MIN) / VIEW_SPAN * a.height()};
}

QPointF CornerPad::toPct(QPointF px) const
{
	QRect a = area();
	return {px.x() / a.width() * VIEW_SPAN + VIEW_MIN, px.y() / a.height() * VIEW_SPAN + VIEW_MIN};
}

int CornerPad::nearest(QPointF px) const
{
	int best = -1;
	double bd = HIT;
	for (int i = 0; i < 4; i++) {
		QPointF q = toPx(corners[i]);
		double d = std::hypot(q.x() - px.x(), q.y() - px.y());
		if (d <= bd) {
			bd = d;
			best = i;
		}
	}
	return best;
}

bool CornerPad::inside(QPointF pt) const
{
	// Punto dentro del cuadrilátero (convexo o no): regla par-impar
	bool ins = false;
	for (int i = 0, j = 3; i < 4; j = i++) {
		const QPointF &a = corners[i], &b = corners[j];
		if ((a.y() > pt.y()) != (b.y() > pt.y()) &&
		    pt.x() < (b.x() - a.x()) * (pt.y() - a.y()) / (b.y() - a.y()) + a.x())
			ins = !ins;
	}
	return ins;
}

void CornerPad::push()
{
	OBSDataAutoRelease d = obs_data_create();
	for (int i = 0; i < 4; i++) {
		obs_data_set_double(d, CORNER_KEYS[i][0], r2(corners[i].x()));
		obs_data_set_double(d, CORNER_KEYS[i][1], r2(corners[i].y()));
	}
	apply(d);
}

void CornerPad::show()
{
	QStringList parts;
	for (const auto &c : corners)
		parts << QString("%1,%2").arg(std::lround(c.x())).arg(std::lround(c.y()));
	readout->setText(parts.join("  "));
	update();
}

void CornerPad::showGrow()
{
	bool on = expanded();
	grow->setEnabled(on || !base.isEmpty());
	grow->setToolTip(on ? T("CpShrink") : T("CpExpand"));
	grow->setStyleSheet(on ? QString("QToolButton { color: %1; border-color: %1; }").arg(colors().accent.name())
			       : QString());
}

void CornerPad::zero()
{
	for (int i = 0; i < 4; i++)
		corners[i] = CORNER_DEF[i];
	show();
	push();
	refreshShot(250);
}

void CornerPad::mousePressEvent(QMouseEvent *e)
{
	QPointF pt = e->position();
	if (e->button() != Qt::LeftButton || !area().contains(pt.toPoint()))
		return;
	int idx = (e->modifiers() & Qt::AltModifier) ? -1 : nearest(pt);
	if (idx < 0) {
		if (!(e->modifiers() & Qt::AltModifier) && !inside(toPct(pt)))
			return;
		idx = 4;
	}
	drag = true;
	dragIdx = idx;
	partner = -1;
	dragStart = toPct(pt);
	dragBase = corners;
	update();
}

void CornerPad::mouseMoveEvent(QMouseEvent *e)
{
	QPointF pt = e->position();
	if (!drag) {
		int h = area().contains(pt.toPoint()) ? nearest(pt) : -1;
		if (h != hover) {
			hover = h;
			update();
		}
		setCursor(h >= 0 ? Qt::CrossCursor : inside(toPct(pt)) ? Qt::SizeAllCursor : Qt::ArrowCursor);
		return;
	}
	QPointF p = toPct(pt);
	double dx = p.x() - dragStart.x(), dy = p.y() - dragStart.y();
	auto c = dragBase;
	partner = -1;
	if (dragIdx == 4) {
		for (auto &q : c)
			q += QPointF(dx, dy);
	} else if (e->modifiers() & Qt::ShiftModifier) {
		// Perspectiva: la esquina se mueve sobre un eje y su vecina del mismo lado, en espejo
		const int i = dragIdx;
		if (std::abs(dx) >= std::abs(dy)) {
			static const int row[4] = {1, 0, 3, 2}; // misma fila: tl↔tr, br↔bl
			const int j = row[i];
			c[i].rx() += dx;
			c[j].rx() -= dx;
			partner = j;
		} else {
			static const int col[4] = {3, 2, 1, 0}; // misma columna: tl↔bl, tr↔br
			const int j = col[i];
			c[i].ry() += dy;
			c[j].ry() -= dy;
			partner = j;
		}
	} else {
		c[dragIdx] += QPointF(dx, dy);
	}
	for (int i = 0; i < 4; i++)
		corners[i] = {clampPct(c[i].x()), clampPct(c[i].y())};
	show();
	push();
}

void CornerPad::mouseReleaseEvent(QMouseEvent *)
{
	if (!drag)
		return;
	drag = false;
	dragIdx = -1;
	partner = -1;
	update();
	refreshShot(250);
}

void CornerPad::mouseDoubleClickEvent(QMouseEvent *e)
{
	if (area().contains(e->position().toPoint()))
		zero();
}

void CornerPad::leaveEvent(QEvent *)
{
	if (!drag && hover != -1) {
		hover = -1;
		update();
	}
}

void CornerPad::paintEvent(QPaintEvent *)
{
	DockColors c = colors();
	QPainter p(this);
	p.setRenderHint(QPainter::Antialiasing);
	QRectF a = QRectF(area());
	QPainterPath clip;
	clip.addRoundedRect(a.adjusted(0.5, 0.5, -0.5, -0.5), 6, 6);
	p.fillPath(clip, c.bg);
	p.setClipPath(clip);

	// La captura ya trae el filtro aplicado y, con Expandir, también el margen alrededor
	if (!shot.isNull()) {
		QRectF m(0, 0, 100, 100);
		if (!base.isEmpty())
			m = QRectF(QPointF(-expandL * 100.0 / base.width(), -expandT * 100.0 / base.height()),
				   QPointF(100.0 + expandR * 100.0 / base.width(),
					   100.0 + expandB * 100.0 / base.height()));
		p.drawImage(QRectF(toPx(m.topLeft()), toPx(m.bottomRight())), shot);
	}
	QPen dash(c.line, 1, Qt::DashLine);
	p.setPen(dash);
	p.setBrush(Qt::NoBrush);
	p.drawRect(QRectF(toPx({0, 0}), toPx({100, 100})).adjusted(0.5, 0.5, -0.5, -0.5));

	// Rejilla en perspectiva 4×4 y contorno
	Homography H(corners);
	QPen grid(QColor(255, 255, 255, 150), 1);
	p.setPen(grid);
	for (int k = 1; k < 4; k++) {
		double u = k / 4.0;
		p.drawLine(toPx(H(u, 0)), toPx(H(u, 1)));
		p.drawLine(toPx(H(0, u)), toPx(H(1, u)));
	}
	p.setPen(QPen(c.accent, 1.5));
	QPolygonF outline;
	for (const auto &q : corners)
		outline << toPx(q);
	p.drawPolygon(outline);

	// Anclas
	for (int i = 0; i < 4; i++) {
		QPointF q = toPx(corners[i]);
		bool on = drag ? (dragIdx == i || partner == i || dragIdx == 4) : hover == i;
		p.setPen(QPen(on ? QColor(Qt::white) : c.accent, 1.5));
		p.setBrush(on ? c.accent : QColor(Qt::white));
		p.drawRect(QRectF(q.x() - 4.5, q.y() - 4.5, 9, 9));
	}

	p.setClipping(false);
	p.setPen(QPen(drag || hasFocus() ? c.accent : c.line, 1));
	p.setBrush(Qt::NoBrush);
	p.drawRoundedRect(a.adjusted(0.5, 0.5, -0.5, -0.5), 6, 6);
}
