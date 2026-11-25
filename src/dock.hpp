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

#pragma once

#include <obs.hpp>
#include <obs-frontend-api.h>

#include <QElapsedTimer>
#include <QPointer>
#include <QTimer>
#include <QWidget>

#include <map>
#include <string>
#include <vector>

#include "catalog.hpp"
#include "widgets.hpp"
#include "pads.hpp"

class QCheckBox;
class QFrame;
class QLabel;
class QScrollArea;
class QSlider;

class QToolButton;
class QVBoxLayout;

class ShaderDock : public QWidget {
	Q_OBJECT

public:
	explicit ShaderDock(QWidget *parent = nullptr);
	~ShaderDock() override;

public slots:
	void scheduleRefresh();
	void refresh();
	void render();
	void filterChanged(quintptr filter);

private:
	struct Row {
		QPointer<QFrame> box;
		QPointer<Switch> enable;
		QPointer<QToolButton> play;
		QPointer<QSlider> slider;
		QPointer<QLabel> pct;
		QPointer<QCheckBox> loop;
		QPointer<PadBase> pad;
	};
	struct Anim {
		QElapsedTimer clock;
		double duration;
	};

	// Interfaz
	TabButton *tabFx;
	TabButton *tabGeo;
	QCheckBox *showOthers;
	QCheckBox *liveOnly;
	QScrollArea *scroll;
	QWidget *listHost = nullptr;
	DockColors colors;

	// Estado
	Catalog cat;
	std::map<std::string, Row> rows;
	std::map<std::string, Anim> anims;
	QTimer animTimer;
	QTimer refreshTimer;
	std::vector<OBSSignal> sourceSignals; // de las fuentes del catálogo (se rehacen con él)
	std::string program, preview;

	QFrame *buildFilter(FilterItem &it);
	FilterItem *find(const std::string &key);
	void showRow(const FilterItem &it);
	void setPlaying(const std::string &key, bool playing);

	// Acciones sobre el filtro
	void apply(FilterItem &it, double progress, int autoPlay);
	void setEnabled(FilterItem &it, bool on);
	void play(FilterItem &it);
	void stop(const std::string &key);
	void stopAll();
	void jump(FilterItem &it, double p);
	void setLoop(FilterItem &it, bool on);
	void animate();

	void updateLive();
	void applyStyle();
	bool geoTab() const { return tabGeo->isChecked(); }
	void clearCatalog();
	void loadConfig();
	void saveConfig();

	static void frontendEvent(enum obs_frontend_event event, void *data);
	static void onStructure(void *data, calldata_t *cd);
	static void onFilterSignal(void *data, calldata_t *cd);
	static void onSourceUpdate(void *data, calldata_t *cd);
	bool alive = true;
};
