/**************************************************************************/
/*  windows_livewallpaper_export_platform.cpp                             */
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

#include "windows_livewallpaper_export_platform.h"

#include "editor/editor_string_names.h"

EditorExportPlatformWindowsLiveWallpaper::EditorExportPlatformWindowsLiveWallpaper() {
	set_name("Windows Live Wallpaper");
	set_os_name("Windows");
}

String EditorExportPlatformWindowsLiveWallpaper::get_template_file_name(const String &p_target, const String &p_arch) const {
	return "windows_" + p_target + "_" + p_arch + ".livewallpaper.exe";
}

List<String> EditorExportPlatformWindowsLiveWallpaper::get_binary_extensions(const Ref<EditorExportPreset> &p_preset) const {
	List<String> list;
	list.push_back("exe");
	list.push_back("zip");
	return list;
}

void EditorExportPlatformWindowsLiveWallpaper::get_export_options(List<ExportOption> *r_options) const {
	List<ExportOption> parent;
	EditorExportPlatformWindows::get_export_options(&parent);
	for (const ExportOption &opt : parent) {
		if (opt.option.name == "binary_format/embed_pck") {
			r_options->push_back(ExportOption(opt.option, true, opt.update_visibility, opt.required));
		} else if (opt.option.name == "custom_template/debug" || opt.option.name == "custom_template/release") {
			r_options->push_back(ExportOption(PropertyInfo(Variant::STRING, opt.option.name, PROPERTY_HINT_GLOBAL_FILE, "*.livewallpaper.exe,*.exe"), opt.default_value, opt.update_visibility, opt.required));
		} else if (opt.option.name == "debug/export_console_wrapper") {
			r_options->push_back(ExportOption(opt.option, 0, opt.update_visibility, opt.required));
		} else {
			r_options->push_back(opt);
		}
	}

	r_options->push_back(ExportOption(PropertyInfo(Variant::BOOL, "livewallpaper/cover_all_screens"), true));
	r_options->push_back(ExportOption(PropertyInfo(Variant::BOOL, "livewallpaper/pause_on_lock"), true));
	r_options->push_back(ExportOption(PropertyInfo(Variant::STRING, "livewallpaper/description"), ""));
}

bool EditorExportPlatformWindowsLiveWallpaper::get_export_option_visibility(const EditorExportPreset *p_preset, const String &p_option) const {
	if (p_option == "debug/export_console_wrapper" || p_option == "application/console_wrapper_icon") {
		return false;
	}
	return EditorExportPlatformWindows::get_export_option_visibility(p_preset, p_option);
}

void EditorExportPlatformWindowsLiveWallpaper::get_platform_features(List<String> *r_features) const {
	EditorExportPlatformWindows::get_platform_features(r_features);
	r_features->push_back("livewallpaper");
}

void EditorExportPlatformWindowsLiveWallpaper::get_preset_features(const Ref<EditorExportPreset> &p_preset, List<String> *r_features) const {
	EditorExportPlatformWindows::get_preset_features(p_preset, r_features);
	r_features->push_back("livewallpaper");
}

bool EditorExportPlatformWindowsLiveWallpaper::has_valid_export_configuration(const Ref<EditorExportPreset> &p_preset, String &r_error, bool &r_missing_templates, bool p_debug) const {
	String err;
	const bool valid = EditorExportPlatformWindows::has_valid_export_configuration(p_preset, err, r_missing_templates, p_debug);
	if (r_missing_templates) {
		err += TTR("Live Wallpaper exports require windows_{debug|release}_{arch}.livewallpaper.exe templates (build with livewallpaper_template=yes). The unflavored Windows Desktop template cannot be used.") + "\n";
	}
	if (!err.is_empty()) {
		r_error = err;
	}
	return valid;
}

HashMap<String, Variant> EditorExportPlatformWindowsLiveWallpaper::get_custom_project_settings(const Ref<EditorExportPreset> &p_preset) const {
	HashMap<String, Variant> settings;
	settings["blazium/livewallpaper/enabled"] = true;
	settings["blazium/livewallpaper/cover_all_screens"] = p_preset->get("livewallpaper/cover_all_screens");
	settings["blazium/livewallpaper/pause_on_lock"] = p_preset->get("livewallpaper/pause_on_lock");
	const String description = p_preset->get("livewallpaper/description");
	if (!description.is_empty()) {
		settings["application/config/description"] = description;
	}
	return settings;
}

Error EditorExportPlatformWindowsLiveWallpaper::export_project(const Ref<EditorExportPreset> &p_preset, bool p_debug, const String &p_path, BitField<EditorExportPlatform::DebugFlags> p_flags) {
	String path = p_path;
	if (!path.get_extension().to_lower().ends_with("zip") && path.get_extension().to_lower() != "exe") {
		path = path.get_basename() + ".exe";
	}
	return EditorExportPlatformWindows::export_project(p_preset, p_debug, path, p_flags);
}

#endif
