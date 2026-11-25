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

// Manifiesto de shaders propios (data/shaders.json, generado desde shaders.js del dock HTML
// con tools/manifest.mjs): cómo reconocer cada filtro y qué controles darle.

#pragma once

#include <string>
#include <vector>

struct ManifestEntry {
	std::string repo;
	std::string label;
	std::string label_en;
	std::string type; // sequence, continuous, stinger, corners
	double duration = 0.0;
	std::vector<std::string> files; // nombres de archivo .shader, en minúsculas
	std::vector<std::string> kinds; // tipos de filtro nativos (plugins)

	const std::string &display_label() const;
};

namespace manifest {
void load();
const ManifestEntry *by_file(const std::string &file_lower);
const ManifestEntry *by_kind(const std::string &kind);
} // namespace manifest
