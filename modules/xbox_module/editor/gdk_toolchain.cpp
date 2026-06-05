/**************************************************************************/
/*  gdk_toolchain.cpp                                                     */
/**************************************************************************/
/*                         This file is part of:                          */
/*                             BLAZIUM ENGINE                             */
/*                          https://blazium.app                           */
/**************************************************************************/
/* Copyright (c) 2024-present Blazium Engine contributors.                */
/*                                                                        */
/* Permission is hereby granted, free of charge, to any person obtaining  */
/* a copy of this software and associated documentation files (the        */
/* "Software"), to deal in the Software without restriction, including    */
/* without limitation the rights to use, copy, modify, merge, publish,    */
/* distribute, sublicense, and/or sell copies of the Software, and to     */
/* permit persons to whom the Software is furnished to do so, subject to  */
/* the following conditions:                                              */
/*                                                                        */
/* The above copyright notice and this permission notice shall be         */
/* included in all copies or substantial portions of the Software.        */
/*                                                                        */
/* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,        */
/* EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF     */
/* MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. */
/* IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY   */
/* CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,   */
/* TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE      */
/* SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.                 */
/**************************************************************************/

#include "gdk_toolchain.h"

#include "core/io/dir_access.h"
#include "core/io/file_access.h"
#include "core/os/os.h"

namespace {

String _trim_trailing_slash(const String &p_path) {
	String path = p_path;
	while (path.ends_with("/") || path.ends_with("\\")) {
		path = path.substr(0, path.length() - 1);
	}
	return path;
}

bool _dir_has_xgame_runtime(const String &p_path) {
	return FileAccess::exists(p_path.path_join("include").path_join("XGameRuntime.h"));
}

String _normalize_windows_layout(const String &p_root) {
	if (p_root.is_empty()) {
		return String();
	}
	String root = _trim_trailing_slash(p_root);
	if (_dir_has_xgame_runtime(root)) {
		return root;
	}
	String windows = root.path_join("windows");
	if (_dir_has_xgame_runtime(windows)) {
		return windows;
	}
	return String();
}

String _extract_version_from_path(const String &p_path) {
	PackedStringArray parts = p_path.replace("\\", "/").split("/");
	for (int i = parts.size() - 1; i >= 0; --i) {
		const String &part = parts[i];
		if (part.length() == 6 && part.is_valid_int()) {
			return part;
		}
	}
	return String();
}

} //namespace

GDKToolchain::GDKToolchain() {
	_detect_gdk();
}

void GDKToolchain::_detect_gdk() {
	Vector<String> edition_roots;

	for (const String &env_name : { "GameDKCoreLatest", "GameDKLatest", "GRDKLatest" }) {
		String env_value = OS::get_singleton()->get_environment(env_name);
		if (env_value.is_empty()) {
			continue;
		}
		String normalized = _normalize_windows_layout(env_value);
		if (!normalized.is_empty() && !edition_roots.has(normalized)) {
			edition_roots.push_back(normalized);
		}
	}

	const String default_root = "C:/Program Files (x86)/Microsoft GDK";
	if (DirAccess::exists(default_root)) {
		Ref<DirAccess> da = DirAccess::open(default_root);
		if (da.is_valid()) {
			da->list_dir_begin();
			String entry = da->get_next();
			int best_num = -1;
			String best_root;
			while (!entry.is_empty()) {
				if (da->current_is_dir() && entry.is_valid_int()) {
					int version_num = entry.to_int();
					String candidate = _normalize_windows_layout(default_root.path_join(entry));
					if (!candidate.is_empty() && version_num > best_num) {
						best_num = version_num;
						best_root = candidate;
					}
				}
				entry = da->get_next();
			}
			da->list_dir_end();
			if (!best_root.is_empty() && !edition_roots.has(best_root)) {
				edition_roots.push_back(best_root);
			}
		}
	}

	String env_bin = OS::get_singleton()->get_environment("GDK_BIN");
	if (!env_bin.is_empty() && DirAccess::exists(env_bin)) {
		bin_dir = env_bin;
	}

	for (const String &root : edition_roots) {
		gdk_windows_root = root;
		edition_root = _trim_trailing_slash(root.get_base_dir());
		gdk_version = _extract_version_from_path(edition_root);

		if (bin_dir.is_empty()) {
			const String candidate = edition_root.path_join("bin");
			if (DirAccess::exists(candidate)) {
				bin_dir = candidate;
			}
		}

		if (!bin_dir.is_empty()) {
			makepkg_path = bin_dir.path_join("makepkg.exe");
			wdapp_path = bin_dir.path_join("wdapp.exe");
			sandbox_path = bin_dir.path_join("XblPCSandbox.exe");
			game_config_editor_path = bin_dir.path_join("GameConfigEditor.exe");
		}

		if (FileAccess::exists(makepkg_path) && FileAccess::exists(wdapp_path)) {
			available = true;
			return;
		}
	}

	available = false;
	gdk_windows_root = String();
	edition_root = String();
}

bool GDKToolchain::is_gdk_available() const {
	return available;
}

String GDKToolchain::get_gdk_version() const {
	return gdk_version;
}

String GDKToolchain::get_gdk_windows_root() const {
	return gdk_windows_root;
}

String GDKToolchain::get_edition_root() const {
	return edition_root;
}

String GDKToolchain::get_bin_dir() const {
	return bin_dir;
}

String GDKToolchain::get_makepkg_path() const {
	return makepkg_path;
}

String GDKToolchain::get_wdapp_path() const {
	return wdapp_path;
}

String GDKToolchain::get_sandbox_path() const {
	return sandbox_path;
}

String GDKToolchain::get_game_config_editor_path() const {
	return game_config_editor_path;
}

Vector<String> GDKToolchain::get_runtime_dll_names(bool p_debug) const {
	Vector<String> names;
	names.push_back("libHttpClient.dll");
	names.push_back("XCurl.dll");
	if (p_debug) {
		names.push_back("Microsoft.Xbox.Services.C.Thunks.Debug.dll");
	} else {
		names.push_back("Microsoft.Xbox.Services.C.Thunks.dll");
	}
	return names;
}

Error GDKToolchain::copy_runtime_dlls(const String &p_staging_dir, bool p_debug) const {
	if (gdk_windows_root.is_empty()) {
		return ERR_UNAVAILABLE;
	}

	Vector<String> search_dirs;
	search_dirs.push_back(gdk_windows_root.path_join("bin").path_join("x64"));
	if (!bin_dir.is_empty()) {
		search_dirs.push_back(bin_dir);
	}
	search_dirs.push_back(gdk_windows_root.path_join("bin"));

	for (const String &dll_name : get_runtime_dll_names(p_debug)) {
		bool copied = false;
		for (const String &dir : search_dirs) {
			const String src = dir.path_join(dll_name);
			if (!FileAccess::exists(src)) {
				continue;
			}
			const String dst = p_staging_dir.path_join(dll_name);
			Error err = DirAccess::copy_absolute(src, dst);
			if (err != OK) {
				return err;
			}
			copied = true;
			break;
		}
		if (!copied) {
			WARN_PRINT(vformat("xbox_module: GDK runtime DLL not found: %s", dll_name));
		}
	}

	return OK;
}

Ref<GDKToolchain> GDKToolchain::create() {
	Ref<GDKToolchain> toolchain;
	toolchain.instantiate();
	return toolchain;
}
