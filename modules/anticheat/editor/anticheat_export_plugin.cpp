/**************************************************************************/
/*  anticheat_export_plugin.cpp                                           */
/**************************************************************************/
/*                         This file is part of:                          */
/*                             BLAZIUM ENGINE                             */
/*                          https://blazium.app                           */
/**************************************************************************/

#ifdef TOOLS_ENABLED

#include "anticheat_export_plugin.h"

#include "core/config/project_settings.h"
#include "core/io/dir_access.h"
#include "core/io/file_access.h"
#include "editor/export/editor_export_platform.h"

void AnticheatExportPlugin::_export_begin(const HashSet<String> &p_features, bool p_debug, const String &p_path, int p_flags) {
	(void)p_debug;
	(void)p_flags;
	export_path = p_path;
	export_features = p_features;
}

static bool export_wants_windows(const HashSet<String> &features) {
	if (features.has("windows")) {
		return true;
	}
	if (features.has("linux") || features.has("linuxbsd") || features.has("x11") || features.has("macos") || features.has("osx")) {
		return false;
	}
#ifdef WINDOWS_ENABLED
	return true;
#else
	return false;
#endif
}

void AnticheatExportPlugin::_export_end() {
	if (export_path.is_empty()) {
		return;
	}
	const String dest_dir = export_path.get_base_dir();
	String src_dir = ProjectSettings::get_singleton()->get("anticheat/export/bin_dir");
	if (src_dir.is_empty()) {
		src_dir = "res://anticheat/bin";
	}
	if (src_dir.begins_with("res://")) {
		src_dir = ProjectSettings::get_singleton()->globalize_path(src_dir);
	}
	Ref<DirAccess> da = DirAccess::create(DirAccess::ACCESS_FILESYSTEM);
	const bool verify = ProjectSettings::get_singleton()->get("anticheat/verify_runtime_signature");
	const bool require_agent = ProjectSettings::get_singleton()->get("anticheat/require_agent");
	const bool require = verify || require_agent;
	if (da.is_null() || !DirAccess::exists(src_dir)) {
		if (require) {
			Ref<EditorExportPlatform> plat = get_export_platform();
			if (plat.is_valid()) {
				plat->add_message(EditorExportPlatform::EXPORT_MESSAGE_ERROR, "Anticheat", "Runtime bin dir missing while verify_runtime_signature or require_agent is enabled.");
			} else {
				ERR_PRINT("Anticheat export failed: runtime bin dir missing while verify_runtime_signature or require_agent is enabled.");
			}
		}
		export_path = String();
		export_features.clear();
		return;
	}

	Vector<String> names;
	if (export_wants_windows(export_features)) {
		names.push_back("bzcl.dll");
		names.push_back("bzcl64.dll");
		names.push_back("bzsv.dll");
		names.push_back("bzsv64.dll");
		names.push_back("bzgb.dll");
		names.push_back("bzgb64.dll");
		names.push_back("bzag.dll");
		names.push_back("bzag64.dll");
	} else {
		names.push_back("libbzcl.so");
		names.push_back("libbzsv.so");
		names.push_back("libbzgb.so");
		names.push_back("libbzag.so");
	}
	bool copied_client = false;
	bool copied_client_sig = false;
	for (int i = 0; i < names.size(); i++) {
		const String src = src_dir.path_join(names[i]);
		if (!FileAccess::exists(src)) {
			continue;
		}
		da->copy(src, dest_dir.path_join(names[i]));
		const bool is_client = names[i].begins_with("bzcl") || names[i].begins_with("libbzcl");
		if (is_client) {
			copied_client = true;
		}
		const String sig = src + ".sig";
		if (FileAccess::exists(sig)) {
			da->copy(sig, dest_dir.path_join(names[i] + ".sig"));
			if (is_client) {
				copied_client_sig = true;
			}
		}
	}
	if (require && (!copied_client || !copied_client_sig)) {
		Ref<EditorExportPlatform> plat = get_export_platform();
		if (plat.is_valid()) {
			plat->add_message(EditorExportPlatform::EXPORT_MESSAGE_ERROR, "Anticheat", "Runtime client binary or .sig missing while verify_runtime_signature or require_agent is enabled.");
		} else {
			ERR_PRINT("Anticheat export failed: runtime client binary or .sig missing while verify_runtime_signature or require_agent is enabled.");
		}
	}
	export_path = String();
	export_features.clear();
}

#endif
