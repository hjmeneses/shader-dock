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

/*
 * El dock: lista los filtros de shader propios de todas las escenas y fuentes, agrupados por
 * fuente (la escena al aire primero), con controles para cada uno. Habla directo con libobs:
 * lee y cambia los ajustes del filtro, y escucha las señales de OBS para estar al día.
 *
 * Las señales llegan desde cualquier hilo; todo lo que toca la interfaz se pasa al hilo de Qt
 * con QMetaObject::invokeMethod (en cola).
 */

#include "dock.hpp"

#include <obs-module.h>
#include <util/platform.h>
#include <plugin-support.h>

#include <QCheckBox>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QScrollArea>
#include <QScrollBar>
#include <QSignalBlocker>
#include <QSlider>
#include <QStyle>
#include <QToolButton>
#include <QVBoxLayout>

#include <algorithm>

namespace {

const int TICK_MS = 16;     // ~60 cambios de Progreso por segundo mientras corre una secuencia
const int REFRESH_MS = 250; // se juntan los cambios de estructura que llegan seguidos

QString T(const char *key)
{
	return QString::fromUtf8(obs_module_text(key));
}

QString fmt(double n)
{
	return QString::number(std::round(n * 10.0) / 10.0);
}

QLabel *named(const QString &text, const char *name)
{
	auto *l = new QLabel(text);
	l->setObjectName(name);
	return l;
}

// Vuelve a aplicar la hoja de estilos tras cambiar una propiedad dinámica
void repolish(QWidget *w)
{
	w->style()->unpolish(w);
	w->style()->polish(w);
	for (QWidget *c : w->findChildren<QWidget *>()) {
		c->style()->unpolish(c);
		c->style()->polish(c);
	}
}

const char *const STRUCTURE_SIGNALS[] = {"source_create", "source_remove", "source_rename", "source_filter_add",
					 "source_filter_remove"};

} // namespace

/* ------------------------------------------------------------------ armado */

ShaderDock::ShaderDock(QWidget *parent) : QWidget(parent)
{
	auto *root = new QVBoxLayout(this);
	root->setContentsMargins(0, 0, 0, 0);
	root->setSpacing(0);

	// Arriba: pestañas y opciones, separadas de la lista por una línea
	auto *top = new QFrame(this);
	top->setObjectName("sdTop");
	auto *tl = new QVBoxLayout(top);
	tl->setContentsMargins(8, 4, 8, 4);
	tl->setSpacing(2);
	auto *tabRow = new QHBoxLayout();
	tabRow->setSpacing(2);
	tabFx = new TabButton(T("TabFx"), top);
	tabGeo = new TabButton(T("TabGeo"), top);
	tabFx->setChecked(true);
	tabRow->addWidget(tabFx);
	tabRow->addWidget(tabGeo);
	tl->addLayout(tabRow);
	auto *opts = new QHBoxLayout();
	showOthers = new QCheckBox(T("ShowOthers"), top);
	liveOnly = new QCheckBox(T("LiveOnly"), top);
	showOthers->setObjectName("sdOption");
	liveOnly->setObjectName("sdOption");
	liveOnly->setToolTip(T("LiveOnlyTip"));
	opts->addWidget(showOthers);
	opts->addWidget(liveOnly);
	opts->addStretch(1);
	tl->addLayout(opts);
	root->addWidget(top);

	scroll = new QScrollArea(this);
	scroll->setObjectName("sdScroll");
	scroll->setWidgetResizable(true);
	scroll->setFrameShape(QFrame::NoFrame);
	scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
	root->addWidget(scroll, 1);

	applyStyle();
	loadConfig();
	for (TabButton *t : {tabFx, tabGeo})
		connect(t, &TabButton::toggled, this, [this](bool on) {
			if (!on)
				return;
			saveConfig();
			render();
		});
	connect(showOthers, &QCheckBox::toggled, this, [this] {
		saveConfig();
		render();
	});
	connect(liveOnly, &QCheckBox::toggled, this, [this] {
		saveConfig();
		render();
	});

	animTimer.setInterval(TICK_MS);
	animTimer.setTimerType(Qt::PreciseTimer);
	connect(&animTimer, &QTimer::timeout, this, &ShaderDock::animate);
	refreshTimer.setSingleShot(true);
	refreshTimer.setInterval(REFRESH_MS);
	connect(&refreshTimer, &QTimer::timeout, this, &ShaderDock::refresh);

	signal_handler_t *sh = obs_get_signal_handler();
	for (const char *s : STRUCTURE_SIGNALS)
		signal_handler_connect(sh, s, onStructure, this);
	signal_handler_connect(sh, "source_update", onSourceUpdate, this);
	obs_frontend_add_event_callback(frontendEvent, this);

	render();
	scheduleRefresh();
}

ShaderDock::~ShaderDock()
{
	alive = false;
	signal_handler_t *sh = obs_get_signal_handler();
	for (const char *s : STRUCTURE_SIGNALS)
		signal_handler_disconnect(sh, s, onStructure, this);
	signal_handler_disconnect(sh, "source_update", onSourceUpdate, this);
	obs_frontend_remove_event_callback(frontendEvent, this);
	clearCatalog();
}

/* ------------------------------------------------------------------ señales de OBS */

void ShaderDock::onStructure(void *data, calldata_t *)
{
	QMetaObject::invokeMethod(static_cast<ShaderDock *>(data), "scheduleRefresh", Qt::QueuedConnection);
}

void ShaderDock::onFilterSignal(void *data, calldata_t *cd)
{
	auto src = (quintptr)calldata_ptr(cd, "source");
	QMetaObject::invokeMethod(static_cast<ShaderDock *>(data), "filterChanged", Qt::QueuedConnection,
				  Q_ARG(quintptr, src));
}

void ShaderDock::onSourceUpdate(void *data, calldata_t *cd)
{
	obs_source_t *src = static_cast<obs_source_t *>(calldata_ptr(cd, "source"));
	if (!src || obs_source_get_type(src) != OBS_SOURCE_TYPE_FILTER)
		return;
	QMetaObject::invokeMethod(static_cast<ShaderDock *>(data), "filterChanged", Qt::QueuedConnection,
				  Q_ARG(quintptr, (quintptr)src));
}

void ShaderDock::frontendEvent(enum obs_frontend_event event, void *data)
{
	auto *d = static_cast<ShaderDock *>(data);
	switch (event) {
	case OBS_FRONTEND_EVENT_FINISHED_LOADING:
	case OBS_FRONTEND_EVENT_SCENE_LIST_CHANGED:
	case OBS_FRONTEND_EVENT_SCENE_COLLECTION_CHANGED:
		d->scheduleRefresh();
		break;
	case OBS_FRONTEND_EVENT_SCENE_CHANGED:
	case OBS_FRONTEND_EVENT_PREVIEW_SCENE_CHANGED:
	case OBS_FRONTEND_EVENT_STUDIO_MODE_ENABLED:
	case OBS_FRONTEND_EVENT_STUDIO_MODE_DISABLED:
		d->updateLive();
		d->render();
		break;
	case OBS_FRONTEND_EVENT_THEME_CHANGED:
		d->applyStyle();
		d->render();
		break;
	case OBS_FRONTEND_EVENT_SCENE_COLLECTION_CLEANUP:
	case OBS_FRONTEND_EVENT_EXIT:
		// Hay que soltar las referencias antes de que OBS libere la colección
		d->refreshTimer.stop();
		d->stopAll();
		d->clearCatalog();
		d->render();
		break;
	default:
		break;
	}
}

/* ------------------------------------------------------------------ catálogo */

void ShaderDock::clearCatalog()
{
	sourceSignals.clear(); // antes que las referencias: desconectar necesita la fuente viva
	cat = Catalog();
	rows.clear();
}

void ShaderDock::scheduleRefresh()
{
	if (alive)
		refreshTimer.start();
}

void ShaderDock::refresh()
{
	sourceSignals.clear();
	cat = build_catalog();

	for (const auto &c : cat.containers) {
		signal_handler_t *sh = obs_source_get_signal_handler(c);
		sourceSignals.emplace_back(sh, "item_add", onStructure, this);
		sourceSignals.emplace_back(sh, "item_remove", onStructure, this);
	}
	for (const auto &p : cat.parents)
		sourceSignals.emplace_back(obs_source_get_signal_handler(p), "reorder_filters", onStructure, this);
	for (const auto &it : cat.items)
		sourceSignals.emplace_back(obs_source_get_signal_handler(it.filter), "enable", onFilterSignal, this);

	// Animaciones de filtros que ya no existen
	for (auto a = anims.begin(); a != anims.end();)
		a = find(a->first) ? std::next(a) : anims.erase(a);
	if (anims.empty())
		animTimer.stop();
	// Las que siguen: el filtro sigue en Progreso animado, no en lo que se leyó
	for (auto &a : anims)
		if (FilterItem *it = find(a.first))
			it->auto_play = false;

	updateLive();
	render();
}

// Colores del tema de OBS + el estilo del dock HTML
void ShaderDock::applyStyle()
{
	colors = DockColors::from(palette());
	setStyleSheet(dock_stylesheet(colors));
}

void ShaderDock::updateLive()
{
	OBSSourceAutoRelease p = obs_frontend_get_current_scene();
	program = p ? obs_source_get_name(p) : "";
	preview.clear();
	if (obs_frontend_preview_program_mode_active()) {
		OBSSourceAutoRelease v = obs_frontend_get_current_preview_scene();
		preview = v ? obs_source_get_name(v) : "";
	}
}

FilterItem *ShaderDock::find(const std::string &key)
{
	for (auto &it : cat.items)
		if (it.key == key)
			return &it;
	return nullptr;
}

void ShaderDock::filterChanged(quintptr filter)
{
	for (auto &it : cat.items) {
		if ((quintptr)it.filter.Get() != filter)
			continue;
		double progress = it.progress;
		read_state(it);
		if (anims.count(it.key)) {
			it.progress = progress; // manda la animación
			it.auto_play = false;
		}
		showRow(it);
		// Si los sliders de OBS cambian la geometría, el pad se pone al día (salvo mientras se arrastra)
		auto r = rows.find(it.key);
		if (r != rows.end() && r->second.pad && !r->second.pad->dragging())
			r->second.pad->reload();
	}
}

/* ------------------------------------------------------------------ lista */

void ShaderDock::render()
{
	if (!alive)
		return;
	int scrollPos = scroll->verticalScrollBar()->value();
	rows.clear();

	std::set<std::string> onAir = reach(cat, program);
	std::set<std::string> inPreview = reach(cat, preview);
	for (const auto &n : onAir)
		inPreview.erase(n);
	auto rank = [&](const FilterItem &i) {
		return onAir.count(i.source_name) ? 0 : inPreview.count(i.source_name) ? 1 : 2;
	};

	// Con "Solo escena en vivo", cada pestaña muestra lo de la escena al aire (y la de vista previa).
	// Si esa escena no tiene nada en la pestaña, se muestra todo en vez de dejarla vacía.
	auto forTab = [&](bool geo) {
		std::vector<FilterItem *> all;
		for (auto &i : cat.items)
			if ((i.own || showOthers->isChecked()) && i.geometry() == geo)
				all.push_back(&i);
		if (liveOnly->isChecked()) {
			std::vector<FilterItem *> mine;
			for (auto *i : all)
				if (rank(*i) < 2)
					mine.push_back(i);
			if (!mine.empty())
				return mine;
		}
		return all;
	};
	auto fx = forTab(false);
	auto geoList = forTab(true);
	tabFx->setCount((int)fx.size());
	tabGeo->setCount((int)geoList.size());

	const bool geo = geoTab();
	std::vector<FilterItem *> visible = geo ? geoList : fx;
	std::stable_sort(visible.begin(), visible.end(),
			 [&](FilterItem *a, FilterItem *b) { return rank(*a) < rank(*b); });
	// La marca solo sirve si distingue algo: si todo lo visible está al aire, no se pone
	const bool mark = !std::all_of(visible.begin(), visible.end(), [&](FilterItem *i) { return rank(*i) == 0; });

	auto *host = new QWidget();
	host->setObjectName("sdList");
	auto *list = new QVBoxLayout(host);
	list->setContentsMargins(8, 8, 8, 8);
	list->setSpacing(8);

	if (visible.empty()) {
		QString title, text;
		if (geo) {
			title = T("NoGeometry");
			text = T("AddGeometry");
		} else {
			size_t total = 0;
			for (const auto &i : cat.items)
				total += !i.geometry();
			size_t hidden = total - visible.size();
			title = T("NoShaders");
			text = hidden ? T("HiddenOthers").arg(hidden) : T("AddOne");
		}
		auto *msg = named(QString("<b style=\"color:%3\">%1</b><br>%2")
					  .arg(title.toHtmlEscaped(), text.toHtmlEscaped(), colors.text.name()),
				  "sdEmpty");
		msg->setWordWrap(true);
		msg->setAlignment(Qt::AlignCenter);
		msg->setContentsMargins(12, 24, 12, 24);
		list->addWidget(msg);
	}

	// Agrupados por fuente, en el orden en que aparecen
	std::vector<std::pair<std::string, std::vector<FilterItem *>>> groups;
	for (auto *it : visible) {
		auto g =
			std::find_if(groups.begin(), groups.end(), [&](auto &p) { return p.first == it->source_name; });
		if (g == groups.end()) {
			groups.push_back({it->source_name, {}});
			g = groups.end() - 1;
		}
		g->second.push_back(it);
	}

	for (auto &[sourceName, group] : groups) {
		// Tarjeta: banda con el nombre de la fuente y un renglón por filtro
		int r = rank(*group.front());
		const char *live = mark && r == 0 ? "air" : mark && r == 1 ? "preview" : "";
		auto *card = new QFrame();
		card->setObjectName("sdCard");
		card->setProperty("live", live);
		auto *cl = new QVBoxLayout(card);
		cl->setContentsMargins(1, 1, 1, 1);
		cl->setSpacing(0);

		auto *head = new QFrame();
		head->setObjectName("sdCardHead");
		auto *hl = new QHBoxLayout(head);
		hl->setContentsMargins(10, 5, 8, 5);
		hl->setSpacing(6);
		QString name = QString::fromStdString(sourceName);
		auto *title = named(name.toUpper(), "sdCardTitle");
		QFont tf = title->font();
		tf.setLetterSpacing(QFont::PercentageSpacing, 106);
		title->setFont(tf);
		title->setToolTip(name);
		title->setMinimumWidth(10);
		title->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
		hl->addWidget(title, 1);
		if (*live) {
			auto *b = named(T(r == 0 ? "OnAir" : "InPreview"), "sdBadge");
			b->setProperty("live", live);
			hl->addWidget(b);
		}
		cl->addWidget(head);

		bool first = true;
		for (auto *it : group) {
			QFrame *f = buildFilter(*it);
			f->setProperty("first", first);
			first = false;
			cl->addWidget(f);
		}
		list->addWidget(card);
	}
	list->addStretch(1);

	QWidget *old = scroll->takeWidget();
	scroll->setWidget(host);
	if (old)
		old->deleteLater();
	scroll->verticalScrollBar()->setValue(scrollPos);
}

QFrame *ShaderDock::buildFilter(FilterItem &it)
{
	const std::string key = it.key;
	auto *box = new QFrame();
	box->setObjectName("sdFilter");
	auto *v = new QVBoxLayout(box);
	v->setContentsMargins(10, 8, 10, 8);
	v->setSpacing(6);

	auto *top = new QHBoxLayout();
	top->setSpacing(8);
	auto *enable = new Switch();
	enable->setChecked(it.enabled);
	top->addWidget(enable, 0, Qt::AlignVCenter);

	QString label = it.type == FilterType::Effect3D      ? T(it.second ? "Effect3D2" : "Effect3D")
			: it.type == FilterType::BfxRotation ? T("BfxRotation")
			: it.entry                           ? QString::fromStdString(it.entry->display_label())
							     : QString::fromStdString(it.filter_name);
	QString filterName = QString::fromStdString(it.filter_name);
	QString detail = it.detail.empty() ? T("InlineShader") : QString::fromStdString(it.detail);
	auto *names = new QVBoxLayout();
	names->setSpacing(0);
	auto *title = named(label, "sdName");
	title->setToolTip(filterName);
	auto *sub = named(filterName != label ? filterName + " · " + detail : detail, "sdDetail");
	for (QLabel *l : {title, sub}) {
		l->setMinimumWidth(10);
		l->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
	}
	names->addWidget(title);
	names->addWidget(sub);
	top->addLayout(names, 1);
	v->addLayout(top);

	Row row;
	row.box = box;
	row.enable = enable;
	connect(enable, &Switch::toggled, this, [this, key](bool on) {
		if (FilterItem *i = find(key))
			setEnabled(*i, on);
	});

	if (it.type == FilterType::Sequence) {
		auto *controls = new QHBoxLayout();
		controls->setSpacing(4);
		auto button = [&](Glyph g, const QString &tip) {
			auto *b = new QToolButton();
			b->setObjectName("sdCtl");
			b->setIcon(glyph_icon(g, colors.text));
			b->setIconSize(QSize(16, 16));
			b->setFixedSize(28, 28);
			b->setToolTip(tip);
			b->setCursor(Qt::PointingHandCursor);
			controls->addWidget(b);
			return b;
		};
		auto *bStart = button(Glyph::Start, T("ToStart"));
		auto *bPlay = button(Glyph::Play, T("Play").arg(fmt(it.duration)));
		auto *bEnd = button(Glyph::End, T("ToEnd"));
		auto *slider = new QSlider(Qt::Horizontal);
		slider->setObjectName("sdProgress");
		slider->setRange(0, 1000);
		slider->setValue((int)std::lround(it.progress * 10.0));
		slider->setToolTip(T("ProgressOf").arg(filterName));
		slider->setMinimumWidth(40);
		controls->addSpacing(4);
		controls->addWidget(slider, 1);
		auto *pct = named(QString("%1%").arg(std::lround(it.progress)), "sdPct");
		pct->setMinimumWidth(pct->fontMetrics().horizontalAdvance("100%") + 4);
		pct->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
		controls->addWidget(pct);
		auto *loop = new QCheckBox(T("Loop"));
		loop->setObjectName("sdLoop");
		loop->setToolTip(T("LoopTip"));
		loop->setChecked(it.auto_play);
		controls->addWidget(loop);
		v->addLayout(controls);

		row.play = bPlay;
		row.slider = slider;
		row.pct = pct;
		row.loop = loop;

		connect(bStart, &QToolButton::clicked, this, [this, key] {
			if (FilterItem *i = find(key))
				jump(*i, 0.0);
		});
		connect(bEnd, &QToolButton::clicked, this, [this, key] {
			if (FilterItem *i = find(key))
				jump(*i, 100.0);
		});
		connect(bPlay, &QToolButton::clicked, this, [this, key] {
			if (anims.count(key))
				stop(key);
			else if (FilterItem *i = find(key))
				play(*i);
		});
		// Solo los cambios del usuario: los del dock se hacen con las señales bloqueadas
		connect(slider, &QSlider::valueChanged, this, [this, key](int value) {
			if (FilterItem *i = find(key))
				jump(*i, value / 10.0);
		});
		connect(loop, &QCheckBox::toggled, this, [this, key](bool on) {
			if (FilterItem *i = find(key))
				setLoop(*i, on);
		});
	}

	if (it.type == FilterType::Effect3D || it.type == FilterType::BfxRotation) {
		auto kind = it.type == FilterType::Effect3D ? RotationPad::Kind::Effect3D : RotationPad::Kind::Bfx;
		row.pad = new RotationPad(kind, it.source, it.filter);
		v->addWidget(row.pad);
	} else if (it.type == FilterType::Corners) {
		row.pad = new CornerPad(it.source, it.filter);
		v->addWidget(row.pad);
	}

	rows[key] = row;
	showRow(it);
	if (anims.count(key))
		setPlaying(key, true);
	return box;
}

void ShaderDock::showRow(const FilterItem &it)
{
	auto r = rows.find(it.key);
	if (r == rows.end())
		return;
	Row &row = r->second;
	if (row.enable) {
		QSignalBlocker b(row.enable);
		row.enable->setChecked(it.enabled);
		row.enable->setToolTip(it.enabled ? T("TurnOff") : T("TurnOn"));
	}
	// Apagado: los nombres atenuados (los controles siguen activos para dejarlo preparado)
	if (row.box && row.box->property("off").toBool() == it.enabled) {
		row.box->setProperty("off", !it.enabled);
		repolish(row.box);
	}
	if (row.slider && !row.slider->isSliderDown()) {
		QSignalBlocker b(row.slider);
		row.slider->setValue((int)std::lround(it.progress * 10.0));
	}
	if (row.pct)
		row.pct->setText(QString("%1%").arg(std::lround(it.progress)));
	if (row.loop) {
		QSignalBlocker b(row.loop);
		row.loop->setChecked(it.auto_play);
	}
	if (row.play && !anims.count(it.key))
		row.play->setToolTip(T("Play").arg(fmt(it.duration)));
}

void ShaderDock::setPlaying(const std::string &key, bool playing)
{
	auto r = rows.find(key);
	if (r == rows.end() || !r->second.play)
		return;
	QToolButton *b = r->second.play;
	b->setIcon(playing ? glyph_icon(Glyph::Stop, Qt::white) : glyph_icon(Glyph::Play, colors.text));
	if (b->property("playing").toBool() != playing) {
		b->setProperty("playing", playing);
		repolish(b);
	}
	FilterItem *it = find(key);
	b->setToolTip(playing ? T("Stop") : T("Play").arg(fmt(it ? it->duration : 0.0)));
}

/* ------------------------------------------------------------------ acciones */

// progress < 0 o autoPlay < 0: no se tocan
void ShaderDock::apply(FilterItem &it, double progress, int autoPlay)
{
	OBSDataAutoRelease d = obs_data_create();
	if (progress >= 0.0)
		obs_data_set_double(d, "progress", progress);
	if (autoPlay >= 0)
		obs_data_set_bool(d, "auto_play", autoPlay != 0);
	obs_source_update(it.filter, d);
}

void ShaderDock::setEnabled(FilterItem &it, bool on)
{
	it.enabled = on;
	obs_source_set_enabled(it.filter, on);
	showRow(it);
}

// Inicia la secuencia: apaga el bucle y mueve Progreso de 0 a 100 en la duración del filtro
void ShaderDock::play(FilterItem &it)
{
	stop(it.key);
	Anim a;
	a.clock.start();
	a.duration = it.duration > 0.0 ? it.duration : 8.0;
	anims[it.key] = a;
	it.auto_play = false;
	it.progress = 0.0;
	apply(it, 0.0, 0);
	showRow(it);
	setPlaying(it.key, true);
	if (!animTimer.isActive())
		animTimer.start();
}

void ShaderDock::animate()
{
	for (auto a = anims.begin(); a != anims.end();) {
		FilterItem *it = find(a->first);
		if (!it) {
			a = anims.erase(a);
			continue;
		}
		double p = std::min(100.0, a->second.clock.elapsed() / (a->second.duration * 10.0));
		it->progress = p;
		apply(*it, p, -1);
		showRow(*it);
		if (p >= 100.0) {
			std::string key = a->first;
			a = anims.erase(a);
			setPlaying(key, false);
		} else {
			++a;
		}
	}
	if (anims.empty())
		animTimer.stop();
}

void ShaderDock::stop(const std::string &key)
{
	if (anims.erase(key))
		setPlaying(key, false);
	if (anims.empty())
		animTimer.stop();
}

void ShaderDock::stopAll()
{
	std::vector<std::string> keys;
	for (auto &a : anims)
		keys.push_back(a.first);
	for (auto &k : keys)
		stop(k);
}

void ShaderDock::jump(FilterItem &it, double p)
{
	stop(it.key);
	it.auto_play = false;
	it.progress = p;
	apply(it, p, 0);
	showRow(it);
}

void ShaderDock::setLoop(FilterItem &it, bool on)
{
	if (on)
		stop(it.key);
	it.auto_play = on;
	apply(it, -1.0, on ? 1 : 0);
	showRow(it);
}

/* ------------------------------------------------------------------ preferencias */

void ShaderDock::loadConfig()
{
	char *path = obs_module_config_path("dock.json");
	OBSDataAutoRelease d = path ? obs_data_create_from_json_file(path) : nullptr;
	bfree(path);
	if (!d)
		return;
	QSignalBlocker b1(tabFx), b2(showOthers), b3(liveOnly), b4(tabGeo);
	(obs_data_get_int(d, "tab") == 1 ? tabGeo : tabFx)->setChecked(true);
	showOthers->setChecked(obs_data_get_bool(d, "show_others"));
	liveOnly->setChecked(obs_data_get_bool(d, "live_only"));
}

void ShaderDock::saveConfig()
{
	char *dir = obs_module_config_path("");
	char *path = obs_module_config_path("dock.json");
	if (dir && path) {
		os_mkdirs(dir);
		OBSDataAutoRelease d = obs_data_create();
		obs_data_set_int(d, "tab", geoTab() ? 1 : 0);
		obs_data_set_bool(d, "show_others", showOthers->isChecked());
		obs_data_set_bool(d, "live_only", liveOnly->isChecked());
		obs_data_save_json_safe(d, path, "tmp", "bak");
	}
	bfree(dir);
	bfree(path);
}
