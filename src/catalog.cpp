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

#include "catalog.hpp"

#include <obs-frontend-api.h>

#include <cstring>

namespace {

const char *SHADER_FILTER = "shader_filter";
const char *EFFECT_3D = "3d_effect_filter";          // 3D Effect de Exeldro
const char *EFFECT_3D_2 = "3d_effect_2_filter";      // 3D Effect 2 (fork propio, convive con el original)
const char *BFX_ROTATION = "filter-bfx-3d-rotation"; // script Lua que trae OBS (frontend-tools)

std::string base_name_lower(const char *path)
{
	std::string s = path ? path : "";
	size_t cut = s.find_last_of("/\\");
	if (cut != std::string::npos)
		s = s.substr(cut + 1);
	for (auto &c : s)
		if (c >= 'A' && c <= 'Z')
			c = (char)(c - 'A' + 'a');
	return s;
}

bool has_value(obs_data_t *s, const char *name)
{
	return obs_data_has_user_value(s, name) || obs_data_has_default_value(s, name);
}

void add_filters(Catalog &cat, obs_source_t *parent)
{
	struct Ctx {
		Catalog *cat;
		obs_source_t *parent;
		bool any;
	} ctx{&cat, parent, false};

	obs_source_enum_filters(
		parent,
		[](obs_source_t *, obs_source_t *filter, void *param) {
			auto *c = static_cast<Ctx *>(param);
			const char *id = obs_source_get_unversioned_id(filter);
			if (!id)
				return;

			FilterItem it;
			if (strcmp(id, EFFECT_3D) == 0 || strcmp(id, EFFECT_3D_2) == 0) {
				it.type = FilterType::Effect3D;
				it.own = true;
				it.second = strcmp(id, EFFECT_3D_2) == 0;
				it.detail = it.second ? "hjmeneses" : "Exeldro";
			} else if (strcmp(id, BFX_ROTATION) == 0) {
				it.type = FilterType::BfxRotation;
				it.own = true;
				it.detail = "BFX · script";
			} else {
				const ManifestEntry *native = manifest::by_kind(id);
				if (!native && strcmp(id, SHADER_FILTER) != 0)
					return;
				OBSDataAutoRelease s = obs_source_get_settings(filter);
				std::string file;
				if (!native && obs_data_get_bool(s, "from_file"))
					file = base_name_lower(obs_data_get_string(s, "shader_file_name"));
				const ManifestEntry *entry =
					native ? native : (file.empty() ? nullptr : manifest::by_file(file));
				if (entry && entry->type == "stinger")
					return;
				it.entry = entry;
				it.own = entry != nullptr;
				it.detail = entry ? entry->repo : file;
				if (entry)
					it.type = entry->type == "sequence"  ? FilterType::Sequence
						  : entry->type == "corners" ? FilterType::Corners
									     : FilterType::Continuous;
				else
					it.type = has_value(s, "progress") ? FilterType::Sequence
									   : FilterType::Continuous;
			}

			it.source = c->parent;
			it.filter = filter;
			it.source_name = obs_source_get_name(c->parent);
			it.filter_name = obs_source_get_name(filter);
			it.key = it.source_name + '\x1f' + it.filter_name;
			read_state(it);
			c->cat->items.push_back(std::move(it));
			c->any = true;
		},
		&ctx);

	if (ctx.any)
		cat.parents.emplace_back(parent);
}

void add_children(Catalog &cat, obs_source_t *container)
{
	obs_scene_t *scene = obs_group_or_scene_from_source(container);
	if (!scene)
		return;
	auto &list = cat.children[obs_source_get_name(container)];
	obs_scene_enum_items(
		scene,
		[](obs_scene_t *, obs_sceneitem_t *item, void *param) {
			auto *l = static_cast<std::vector<std::string> *>(param);
			l->push_back(obs_source_get_name(obs_sceneitem_get_source(item)));
			return true;
		},
		&list);
	cat.containers.emplace_back(container);
}

} // namespace

void read_state(FilterItem &it)
{
	it.enabled = obs_source_enabled(it.filter);
	if (it.type != FilterType::Sequence)
		return;
	OBSDataAutoRelease s = obs_source_get_settings(it.filter);
	double d = obs_data_get_double(s, "duration");
	it.duration = d > 0.0 ? d : (it.entry && it.entry->duration > 0.0 ? it.entry->duration : 8.0);
	it.progress = obs_data_get_double(s, "progress");
	it.auto_play = has_value(s, "auto_play") ? obs_data_get_bool(s, "auto_play") : true;
}

Catalog build_catalog()
{
	Catalog cat;
	std::set<obs_source_t *> seen;
	std::vector<obs_source_t *> order; // escenas (en el orden de OBS), grupos, fuentes

	struct obs_frontend_source_list scenes = {};
	obs_frontend_get_scenes(&scenes);
	for (size_t i = 0; i < scenes.sources.num; i++)
		if (seen.insert(scenes.sources.array[i]).second)
			order.push_back(scenes.sources.array[i]);
	const size_t from_frontend = order.size();

	// Grupos (y cualquier escena que no esté en la lista de arriba)
	struct EnumCtx {
		std::set<obs_source_t *> *seen;
		std::vector<obs_source_t *> *order;
	} ectx{&seen, &order};
	obs_enum_scenes(
		[](void *param, obs_source_t *src) {
			auto *c = static_cast<EnumCtx *>(param);
			if (c->seen->insert(src).second) {
				obs_source_get_ref(src);
				c->order->push_back(src);
			}
			return true;
		},
		&ectx);
	size_t scenes_and_groups = order.size();

	obs_enum_sources(
		[](void *param, obs_source_t *src) {
			auto *c = static_cast<EnumCtx *>(param);
			if (c->seen->insert(src).second) {
				obs_source_get_ref(src);
				c->order->push_back(src);
			}
			return true;
		},
		&ectx);

	for (size_t i = 0; i < order.size(); i++) {
		if (i < scenes_and_groups)
			add_children(cat, order[i]);
		add_filters(cat, order[i]);
	}

	// Las de la lista del frontend se liberan con la lista; las enumeradas, una por una
	for (size_t i = from_frontend; i < order.size(); i++)
		obs_source_release(order[i]);
	obs_frontend_source_list_free(&scenes);
	return cat;
}

std::set<std::string> reach(const Catalog &cat, const std::string &root)
{
	std::set<std::string> seen;
	if (root.empty())
		return seen;
	std::vector<std::string> stack{root};
	while (!stack.empty()) {
		std::string n = stack.back();
		stack.pop_back();
		if (!seen.insert(n).second)
			continue;
		auto it = cat.children.find(n);
		if (it != cat.children.end())
			for (const auto &c : it->second)
				stack.push_back(c);
	}
	return seen;
}
