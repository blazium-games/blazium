/**************************************************************************/
/*  windows_screensaver_export_platform.cpp                               */
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

#include "windows_screensaver_export_platform.h"

#include "core/io/file_access.h"
#include "editor/editor_string_names.h"

EditorExportPlatformWindowsScreensaver::EditorExportPlatformWindowsScreensaver() {
	set_name("Windows Screensaver");
	set_os_name("Windows");
}

String EditorExportPlatformWindowsScreensaver::get_template_file_name(const String &p_target, const String &p_arch) const {
	return "windows_" + p_target + "_" + p_arch + ".screensaver.exe";
}

List<String> EditorExportPlatformWindowsScreensaver::get_binary_extensions(const Ref<EditorExportPreset> &p_preset) const {
	List<String> list;
	list.push_back("scr");
	list.push_back("zip");
	return list;
}

void EditorExportPlatformWindowsScreensaver::get_export_options(List<ExportOption> *r_options) const {
	List<ExportOption> parent;
	EditorExportPlatformWindows::get_export_options(&parent);
	for (const ExportOption &opt : parent) {
		if (opt.option.name == "binary_format/embed_pck") {
			r_options->push_back(ExportOption(opt.option, true, opt.update_visibility, opt.required));
		} else if (opt.option.name == "custom_template/debug" || opt.option.name == "custom_template/release") {
			r_options->push_back(ExportOption(PropertyInfo(Variant::STRING, opt.option.name, PROPERTY_HINT_GLOBAL_FILE, "*.screensaver.exe,*.exe"), opt.default_value, opt.update_visibility, opt.required));
		} else if (opt.option.name == "debug/export_console_wrapper") {
			r_options->push_back(ExportOption(opt.option, 0, opt.update_visibility, opt.required));
		} else {
			r_options->push_back(opt);
		}
	}

	r_options->push_back(ExportOption(PropertyInfo(Variant::STRING, "screensaver/configure_scene", PROPERTY_HINT_FILE, "*.tscn,*.scn"), "res://configure.tscn"));
	r_options->push_back(ExportOption(PropertyInfo(Variant::STRING, "screensaver/unlock_scene", PROPERTY_HINT_FILE, "*.tscn,*.scn"), "res://unlock.tscn"));
	r_options->push_back(ExportOption(PropertyInfo(Variant::STRING, "screensaver/change_password_scene", PROPERTY_HINT_FILE, "*.tscn,*.scn"), "res://change_password.tscn"));
	r_options->push_back(ExportOption(PropertyInfo(Variant::BOOL, "screensaver/password_enabled"), false));
	r_options->push_back(ExportOption(PropertyInfo(Variant::BOOL, "screensaver/lock_workstation_on_exit"), false));
	r_options->push_back(ExportOption(PropertyInfo(Variant::BOOL, "screensaver/quit_on_input"), true));
	r_options->push_back(ExportOption(PropertyInfo(Variant::INT, "screensaver/quit_on_mouse_move_threshold", PROPERTY_HINT_RANGE, "0,64,1"), 5));
	r_options->push_back(ExportOption(PropertyInfo(Variant::BOOL, "screensaver/cover_all_screens"), true));
	r_options->push_back(ExportOption(PropertyInfo(Variant::STRING, "screensaver/cover_mode", PROPERTY_HINT_ENUM, "single,virtual,clone"), "virtual"));
	r_options->push_back(ExportOption(PropertyInfo(Variant::INT, "screensaver/screen", PROPERTY_HINT_RANGE, "-1,16,1"), -1));
	r_options->push_back(ExportOption(PropertyInfo(Variant::STRING, "screensaver/description"), ""));
}

String EditorExportPlatformWindowsScreensaver::get_export_option_warning(const EditorExportPreset *p_preset, const StringName &p_name) const {
	if (p_preset && (p_name == "screensaver/configure_scene" || p_name == "screensaver/unlock_scene" || p_name == "screensaver/change_password_scene")) {
		const String path = p_preset->get(p_name);
		if (!path.is_empty() && !FileAccess::exists(path)) {
			return vformat(TTR("Screensaver scene does not exist: %s"), path);
		}
		return String();
	}
	return EditorExportPlatformWindows::get_export_option_warning(p_preset, p_name);
}

bool EditorExportPlatformWindowsScreensaver::get_export_option_visibility(const EditorExportPreset *p_preset, const String &p_option) const {
	if (p_option == "debug/export_console_wrapper" || p_option == "application/console_wrapper_icon") {
		return false;
	}
	return EditorExportPlatformWindows::get_export_option_visibility(p_preset, p_option);
}

void EditorExportPlatformWindowsScreensaver::get_platform_features(List<String> *r_features) const {
	EditorExportPlatformWindows::get_platform_features(r_features);
	r_features->push_back("screensaver");
}

void EditorExportPlatformWindowsScreensaver::get_preset_features(const Ref<EditorExportPreset> &p_preset, List<String> *r_features) const {
	EditorExportPlatformWindows::get_preset_features(p_preset, r_features);
	r_features->push_back("screensaver");
}

bool EditorExportPlatformWindowsScreensaver::has_valid_export_configuration(const Ref<EditorExportPreset> &p_preset, String &r_error, bool &r_missing_templates, bool p_debug) const {
	String err;
	const bool valid = EditorExportPlatformWindows::has_valid_export_configuration(p_preset, err, r_missing_templates, p_debug);
	if (r_missing_templates) {
		err += TTR("Screensaver exports require windows_{debug|release}_{arch}.screensaver.exe templates (build with screensaver_template=yes). The unflavored Windows Desktop template cannot be used.") + "\n";
	}
	if (!err.is_empty()) {
		r_error = err;
	}
	return valid;
}

HashMap<String, Variant> EditorExportPlatformWindowsScreensaver::get_custom_project_settings(const Ref<EditorExportPreset> &p_preset) const {
	HashMap<String, Variant> settings;
	settings["blazium/screensaver/enabled"] = true;
	settings["blazium/screensaver/configure_scene"] = p_preset->get("screensaver/configure_scene");
	settings["blazium/screensaver/unlock_scene"] = p_preset->get("screensaver/unlock_scene");
	settings["blazium/screensaver/change_password_scene"] = p_preset->get("screensaver/change_password_scene");
	settings["blazium/screensaver/password_enabled"] = p_preset->get("screensaver/password_enabled");
	settings["blazium/screensaver/lock_workstation_on_exit"] = p_preset->get("screensaver/lock_workstation_on_exit");
	settings["blazium/screensaver/quit_on_input"] = p_preset->get("screensaver/quit_on_input");
	settings["blazium/screensaver/quit_on_mouse_move_threshold"] = p_preset->get("screensaver/quit_on_mouse_move_threshold");
	settings["blazium/screensaver/cover_all_screens"] = p_preset->get("screensaver/cover_all_screens");
	settings["blazium/screensaver/cover_mode"] = p_preset->get("screensaver/cover_mode");
	settings["blazium/screensaver/screen"] = p_preset->get("screensaver/screen");

	settings["display/window/size/mode"] = 0;
	const String description = p_preset->get("screensaver/description");
	if (!description.is_empty()) {
		settings["application/config/description"] = description;
	}
	return settings;
}

Error EditorExportPlatformWindowsScreensaver::export_project(const Ref<EditorExportPreset> &p_preset, bool p_debug, const String &p_path, BitField<EditorExportPlatform::DebugFlags> p_flags) {
	String path = p_path;
	if (!path.get_extension().to_lower().ends_with("zip") && path.get_extension().to_lower() != "scr") {
		path = path.get_basename() + ".scr";
	}
	return EditorExportPlatformWindows::export_project(p_preset, p_debug, path, p_flags);
}

#endif
