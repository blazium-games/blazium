/**************************************************************************/
/*  device_autorun_export_plugin.cpp                                      */
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

#ifdef TOOLS_ENABLED

#include "device_autorun_export_plugin.h"

#include "editor/export/editor_export.h"
#include "modules/device_autorun/autorun_inf.h"

#include "core/io/dir_access.h"
#include "core/io/file_access.h"

bool EditorExportDeviceAutorun::supports_platform(const Ref<EditorExportPlatform> &p_export_platform) const {
	return p_export_platform.is_valid() && p_export_platform->get_os_name() == "Windows";
}

void EditorExportDeviceAutorun::_get_export_options(const Ref<EditorExportPlatform> &p_export_platform, List<EditorExportPlatform::ExportOption> *r_options) const {
	if (!supports_platform(p_export_platform)) {
		return;
	}
	r_options->push_back(EditorExportPlatform::ExportOption(PropertyInfo(Variant::BOOL, "autorun/enable"), false));
	r_options->push_back(EditorExportPlatform::ExportOption(PropertyInfo(Variant::STRING, "autorun/label"), ""));
	r_options->push_back(EditorExportPlatform::ExportOption(PropertyInfo(Variant::STRING, "autorun/icon"), ""));
	r_options->push_back(EditorExportPlatform::ExportOption(PropertyInfo(Variant::STRING, "autorun/open"), ""));
	r_options->push_back(EditorExportPlatform::ExportOption(PropertyInfo(Variant::STRING, "autorun/action"), ""));
	r_options->push_back(EditorExportPlatform::ExportOption(PropertyInfo(Variant::STRING, "autorun/shell"), ""));
	r_options->push_back(EditorExportPlatform::ExportOption(PropertyInfo(Variant::DICTIONARY, "autorun/shell_verbs"), Dictionary()));
	r_options->push_back(EditorExportPlatform::ExportOption(PropertyInfo(Variant::BOOL, "autorun/write_usb_note"), false));
}

bool EditorExportDeviceAutorun::_get_export_option_visibility(const Ref<EditorExportPlatform> &p_export_platform, const String &p_option_name) const {
	if (!p_option_name.begins_with("autorun/")) {
		return true;
	}
	if (!supports_platform(p_export_platform)) {
		return false;
	}
	if (p_option_name == "autorun/enable") {
		return true;
	}
	return bool(get_option("autorun/enable"));
}

void EditorExportDeviceAutorun::_export_begin(const HashSet<String> &p_features, bool p_debug, const String &p_path, int p_flags) {
	(void)p_features;
	(void)p_debug;
	(void)p_flags;
	export_path = p_path;
}

void EditorExportDeviceAutorun::_export_end() {
	if (!bool(get_option("autorun/enable"))) {
		export_path = String();
		return;
	}
	if (export_path.is_empty()) {
		return;
	}

	String dir = export_path;
	if (FileAccess::exists(export_path) || export_path.get_extension() != "") {
		dir = export_path.get_base_dir();
	}

	Ref<AutorunInf> inf;
	inf.instantiate();
	inf->set_label(get_option("autorun/label"));
	inf->set_icon(get_option("autorun/icon"));
	String open = get_option("autorun/open");
	if (open.is_empty()) {
		String dest = export_path;
		const Ref<EditorExportPreset> preset = get_export_preset();
		String fallback_ext = "exe";
		if (preset.is_valid()) {
			const String preset_path = preset->get_export_path();
			const String preset_ext = preset_path.get_extension().to_lower();
			if (preset->get_platform().is_valid() && preset->get_platform()->get_name() == "Windows Screensaver") {
				fallback_ext = "scr";
			}
			if (preset_ext == "exe" || preset_ext == "scr") {
				dest = preset_path;
			}
		}
		if (dest.get_extension().to_lower() == "tmp") {
			dest = dest.get_basename() + "." + fallback_ext;
		}
		open = dest.get_file();
	}
	inf->set_open(open);
	inf->set_action(get_option("autorun/action"));
	inf->set_shell(get_option("autorun/shell"));
	inf->set_shell_verbs(get_option("autorun/shell_verbs"));

	Error err = OK;
	Ref<FileAccess> f = FileAccess::open(dir.path_join("autorun.inf"), FileAccess::WRITE, &err);
	if (err == OK && f.is_valid()) {
		f->store_string(inf->build());
	}

	if (bool(get_option("autorun/write_usb_note"))) {
		Ref<FileAccess> note = FileAccess::open(dir.path_join("START.txt"), FileAccess::WRITE, &err);
		if (err == OK && note.is_valid()) {
			note->store_string(AutorunInf::usb_note_text());
		}
	}

	export_path = String();
}

void DeviceAutorunEditorPlugin::_notification(int p_what) {
	if (p_what == NOTIFICATION_ENTER_TREE) {
		if (EditorExport::get_singleton()) {
			export_plugin.instantiate();
			EditorExport::get_singleton()->add_export_plugin(export_plugin);
		}
	} else if (p_what == NOTIFICATION_EXIT_TREE) {
		if (export_plugin.is_valid() && EditorExport::get_singleton()) {
			EditorExport::get_singleton()->remove_export_plugin(export_plugin);
			export_plugin.unref();
		}
	}
}

#endif
