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

#include "manifest.hpp"

#include <obs-module.h>
#include <util/platform.h>
#include <plugin-support.h>

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

#include <deque>

namespace {
std::deque<ManifestEntry> entries; // deque: los punteros no cambian al agregar

std::string lower(std::string s)
{
	for (auto &c : s)
		if (c >= 'A' && c <= 'Z')
			c = (char)(c - 'A' + 'a');
	return s;
}

bool spanish()
{
	const char *loc = obs_get_locale();
	return loc && loc[0] == 'e' && loc[1] == 's';
}
} // namespace

const std::string &ManifestEntry::display_label() const
{
	return spanish() || label_en.empty() ? label : label_en;
}

namespace {
// Lo que identifica una entrada al mezclar dos manifiestos: su repo y su primer archivo o tipo
std::string key(const ManifestEntry &e)
{
	const std::string &id = !e.files.empty() ? e.files[0] : !e.kinds.empty() ? e.kinds[0] : e.label;
	return e.repo + "|" + id;
}

// Lee un shaders.json; false si no existe o no es una lista válida
bool read(const char *path, std::vector<ManifestEntry> &out)
{
	char *text = path ? os_quick_read_utf8_file(path) : nullptr;
	if (!text)
		return false;
	QJsonParseError err;
	QJsonDocument doc = QJsonDocument::fromJson(QByteArray(text), &err);
	bfree(text);
	if (!doc.isArray()) {
		obs_log(LOG_WARNING, "%s: %s", path, err.errorString().toUtf8().constData());
		return false;
	}
	for (const QJsonValue v : doc.array()) {
		QJsonObject o = v.toObject();
		ManifestEntry e;
		e.repo = o["repo"].toString().toStdString();
		e.label = o["label"].toString().toStdString();
		e.label_en = o["label_en"].toString().toStdString();
		e.type = o["type"].toString().toStdString();
		e.duration = o["duration"].toDouble(0.0);
		for (const QJsonValue f : o["files"].toArray())
			e.files.push_back(lower(f.toString().toStdString()));
		for (const QJsonValue k : o["kinds"].toArray())
			e.kinds.push_back(k.toString().toStdString());
		out.push_back(std::move(e));
	}
	return true;
}
} // namespace

void manifest::load()
{
	entries.clear();

	// 1. El que viene con el plugin
	std::vector<ManifestEntry> list;
	char *path = obs_module_file("shaders.json");
	if (!read(path, list))
		obs_log(LOG_WARNING, "could not read the bundled shaders.json");
	bfree(path);

	// 2. El actualizado (tools/update-manifest), en la carpeta de configuración del usuario: así un
	//    shader nuevo no obliga a reinstalar el plugin. Sus entradas reemplazan a las del plugin con
	//    la misma clave y las nuevas se agregan; si luego se instala un plugin más nuevo, lo que
	//    traiga de más sigue apareciendo.
	std::vector<ManifestEntry> user;
	path = obs_module_config_path("shaders.json");
	if (read(path, user)) {
		for (auto &u : user) {
			bool replaced = false;
			for (auto &e : list)
				if (key(e) == key(u)) {
					e = u;
					replaced = true;
					break;
				}
			if (!replaced)
				list.push_back(u);
		}
		obs_log(LOG_INFO, "manifest: %zu entries from the updated shaders.json", user.size());
	}
	bfree(path);

	for (auto &e : list)
		entries.push_back(std::move(e));
	obs_log(LOG_INFO, "manifest: %zu shaders", entries.size());
}

const ManifestEntry *manifest::by_file(const std::string &file_lower)
{
	for (const auto &e : entries)
		for (const auto &f : e.files)
			if (f == file_lower)
				return &e;
	return nullptr;
}

const ManifestEntry *manifest::by_kind(const std::string &kind)
{
	for (const auto &e : entries)
		for (const auto &k : e.kinds)
			if (k == kind)
				return &e;
	return nullptr;
}
