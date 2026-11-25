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

// Qué filtros hay en la colección de escenas: recorre escenas, grupos y fuentes, y reconoce los
// filtros de shader (por archivo), los nativos (por tipo) y el 3D Effect de Exeldro.

#pragma once

#include <obs.hpp>

#include <map>
#include <set>
#include <string>
#include <vector>

#include "manifest.hpp"

enum class FilterType { Sequence, Continuous, Corners, Effect3D, BfxRotation };

struct FilterItem {
	std::string key; // fuente + filtro
	std::string source_name;
	std::string filter_name;
	OBSSource source; // referencias fuertes: se sueltan al reconstruir el catálogo
	OBSSource filter;
	const ManifestEntry *entry = nullptr;
	bool own = false;    // reconocido (manifiesto, 3D Effect o BFX); los demás son "otros shaders"
	bool second = false; // 3D Effect 2 en lugar del original
	std::string detail;
	FilterType type = FilterType::Continuous;

	// Estado leído del filtro
	bool enabled = true;
	double duration = 8.0;
	double progress = 0.0; // 0..100
	bool auto_play = true;

	bool geometry() const
	{
		return type == FilterType::Corners || type == FilterType::Effect3D || type == FilterType::BfxRotation;
	}
};

struct Catalog {
	std::vector<FilterItem> items;
	std::map<std::string, std::vector<std::string>> children; // escena o grupo → fuentes que contiene
	std::vector<OBSSource> containers;                        // escenas y grupos
	std::vector<OBSSource> parents;                           // fuentes con algún filtro de la lista
};

Catalog build_catalog();
void read_state(FilterItem &it);

// Todo lo que se ve desde una escena: ella, sus fuentes, y lo que hay dentro de grupos y escenas
// anidadas, a cualquier profundidad
std::set<std::string> reach(const Catalog &cat, const std::string &root);
