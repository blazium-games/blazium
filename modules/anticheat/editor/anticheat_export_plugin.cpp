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
	(void)p_features;
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
	const bool require = ProjectSettings::get_singleton()->get("anticheat/verify_runtime_signature");
	if (da.is_null() || !DirAccess::exists(src_dir)) {
		if (require) {
			ERR_PRINT("Anticheat export failed: runtime bin dir missing while anticheat/verify_runtime_signature is enabled.");
		}
		export_path = String();
		return;
	}

	Vector<String> names;
#ifdef WINDOWS_ENABLED
	names.push_back("bzcl.dll");
	names.push_back("bzcl64.dll");
	names.push_back("bzsv.dll");
	names.push_back("bzsv64.dll");
	names.push_back("bzgb.dll");
	names.push_back("bzgb64.dll");
	names.push_back("bzag.dll");
	names.push_back("bzag64.dll");
#else
	names.push_back("libbzcl.so");
	names.push_back("libbzsv.so");
	names.push_back("libbzgb.so");
	names.push_back("libbzag.so");
#endif
	bool copied_client = false;
	for (int i = 0; i < names.size(); i++) {
		const String src = src_dir.path_join(names[i]);
		if (!FileAccess::exists(src)) {
			continue;
		}
		da->copy(src, dest_dir.path_join(names[i]));
		if (names[i].begins_with("bzcl") || names[i].begins_with("libbzcl")) {
			copied_client = true;
		}
		const String sig = src + ".sig";
		if (FileAccess::exists(sig)) {
			da->copy(sig, dest_dir.path_join(names[i] + ".sig"));
		}
	}
	if (require && !copied_client) {
		ERR_PRINT("Anticheat export failed: runtime binaries missing while anticheat/verify_runtime_signature is enabled.");
	}
	export_path = String();
}

#endif
