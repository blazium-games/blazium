/**************************************************************************/
/*  ps3_editor_plugin.cpp                                                 */
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

#include "ps3_editor_plugin.h"

#include "core/config/project_settings.h"
#include "core/io/dir_access.h"
#include "core/io/file_access.h"
#include "core/version.h"
#include "editor/editor_node.h"
#include "editor/editor_paths.h"
#include "editor/export/editor_export.h"

static void _ps3_editor_init_callback() {
	EditorNode::get_singleton()->add_editor_plugin(memnew(PS3EditorPlugin));
}

void PS3EditorPlugin::initialize() {
	EditorNode::add_init_callback(_ps3_editor_init_callback);
}

PS3EditorPlugin::PS3EditorPlugin() {
	add_tool_menu_item("PS3: Validate Toolchain", callable_mp(this, &PS3EditorPlugin::_validate_toolchain));
	add_tool_menu_item("PS3: Install Templates", callable_mp(this, &PS3EditorPlugin::_install_templates));
	add_tool_menu_item("PS3: Deploy to RPCS3", callable_mp(this, &PS3EditorPlugin::_deploy_rpcs3));
	add_tool_menu_item("PS3: Launch in RPCS3", callable_mp(this, &PS3EditorPlugin::_launch_rpcs3));
	add_tool_menu_item("PS3: Open RPCS3 Data Folder", callable_mp(this, &PS3EditorPlugin::_open_rpcs3_data_folder));
}

String PS3EditorPlugin::_get_scripts_root() const {
	const String env_root = OS::get_singleton()->get_environment("BLAZIUM_ROOT");
	if (!env_root.is_empty()) {
		const String candidate = env_root.path_join("platform").path_join("ps3").path_join("scripts");
		if (DirAccess::dir_exists_absolute(candidate)) {
			return candidate;
		}
	}

	const String exe_root = OS::get_singleton()->get_executable_path().get_base_dir().path_join("..");
	const String candidate = exe_root.path_join("platform").path_join("ps3").path_join("scripts");
	if (DirAccess::dir_exists_absolute(candidate)) {
		return candidate;
	}

	return ProjectSettings::get_singleton()->get_resource_path()
			.path_join("..")
			.path_join("..")
			.path_join("blazium")
			.path_join("platform")
			.path_join("ps3")
			.path_join("scripts");
}

String PS3EditorPlugin::_get_ps3_title_id() const {
	for (int i = 0; i < EditorExport::get_singleton()->get_export_preset_count(); i++) {
		Ref<EditorExportPreset> preset = EditorExport::get_singleton()->get_export_preset(i);
		if (preset.is_valid() && preset->get_platform()->get_name() == "PS3") {
			const String title_id = preset->get("application/title_id");
			if (!title_id.is_empty()) {
				return title_id;
			}
		}
	}
	return "BLAZ00001";
}

String PS3EditorPlugin::_get_emulator_root() const {
	for (int i = 0; i < EditorExport::get_singleton()->get_export_preset_count(); i++) {
		Ref<EditorExportPreset> preset = EditorExport::get_singleton()->get_export_preset(i);
		if (preset.is_valid() && preset->get_platform()->get_name() == "PS3") {
			const String emulator_root = preset->get("application/emulator_root");
			if (!emulator_root.is_empty()) {
				return emulator_root;
			}
		}
	}
	return ProjectSettings::get_singleton()->get_resource_path().path_join("..").path_join("..").path_join("ps3_emulator");
}

String PS3EditorPlugin::_get_export_dir() const {
	for (int i = 0; i < EditorExport::get_singleton()->get_export_preset_count(); i++) {
		Ref<EditorExportPreset> preset = EditorExport::get_singleton()->get_export_preset(i);
		if (preset.is_valid() && preset->get_platform()->get_name() == "PS3") {
			const String export_path = preset->get_export_path();
			if (!export_path.is_empty()) {
				return export_path.get_basename();
			}
		}
	}
	return ProjectSettings::get_singleton()->get_resource_path().path_join("export").path_join("ps3");
}

void PS3EditorPlugin::_validate_toolchain() {
	String output;
	int exit_code = 0;
	List<String> args;
	args.push_back("bash");
	args.push_back(_get_scripts_root().path_join("validate_toolchain.sh"));

	Error err = OS::get_singleton()->execute("wsl", args, &output, &exit_code, true);
	if (err != OK || exit_code != 0) {
		EditorNode::get_singleton()->show_warning(vformat(TTR("PS3 toolchain validation failed.\n%s"), output));
		return;
	}

	const String templates_dir = EditorPaths::get_singleton()->get_export_templates_dir().path_join(EXTERNAL_VERSION_FULL_CONFIG);
	const bool release_ok = FileAccess::exists(templates_dir.path_join("ps3_release.ppc64"));
	const bool debug_ok = FileAccess::exists(templates_dir.path_join("ps3_debug.ppc64"));

	String message = output.strip_edges();
	if (!release_ok && !debug_ok) {
		message += "\n" + TTR("Export templates not installed. Use PS3: Install Templates after building in WSL.");
	} else {
		message += "\n" + TTR("Export templates found.");
	}

	EditorNode::get_singleton()->show_warning(message, TTR("PS3 Toolchain"));
}

void PS3EditorPlugin::_install_templates() {
	const String script_path = _get_scripts_root().path_join("install_templates.py");
	if (!FileAccess::exists(script_path)) {
		EditorNode::get_singleton()->show_warning(vformat(TTR("Missing script: %s"), script_path));
		return;
	}

	List<String> args;
	args.push_back(script_path);
	args.push_back("--version");
	args.push_back(EXTERNAL_VERSION_FULL_CONFIG);

	String output;
	int exit_code = 0;
	Error err = OS::get_singleton()->execute("py", args, &output, &exit_code, true);
	if (err != OK || exit_code != 0) {
		EditorNode::get_singleton()->show_warning(vformat(TTR("Template install failed.\n%s"), output));
		return;
	}

	EditorNode::get_singleton()->show_warning(output, TTR("PS3 Templates"));
}

void PS3EditorPlugin::_deploy_rpcs3() {
	const String title_id = _get_ps3_title_id();
	const String export_dir = _get_export_dir();
	const String emulator_root = _get_emulator_root();
	const String script_path = _get_scripts_root().path_join("deploy_rpcs3.py");

	if (!FileAccess::exists(script_path)) {
		EditorNode::get_singleton()->show_warning(vformat(TTR("Missing script: %s"), script_path));
		return;
	}

	List<String> args;
	args.push_back(script_path);
	args.push_back("--export-dir");
	args.push_back(export_dir);
	args.push_back("--title-id");
	args.push_back(title_id);
	args.push_back("--emulator-root");
	args.push_back(emulator_root);

	String output;
	int exit_code = 0;
	Error err = OS::get_singleton()->execute("py", args, &output, &exit_code, true);
	if (err != OK || exit_code != 0) {
		EditorNode::get_singleton()->show_warning(vformat(TTR("RPCS3 deploy failed.\n%s"), output));
		return;
	}

	EditorNode::get_singleton()->show_warning(output, TTR("PS3 Deploy"));
}

void PS3EditorPlugin::_launch_rpcs3() {
	const String title_id = _get_ps3_title_id();
	const String emulator_root = _get_emulator_root();
	const String script_path = _get_scripts_root().path_join("launch_rpcs3.py");

	if (!FileAccess::exists(script_path)) {
		EditorNode::get_singleton()->show_warning(vformat(TTR("Missing script: %s"), script_path));
		return;
	}

	List<String> args;
	args.push_back(script_path);
	args.push_back("--emulator-root");
	args.push_back(emulator_root);
	args.push_back("--title-id");
	args.push_back(title_id);

	String output;
	int exit_code = 0;
	Error err = OS::get_singleton()->execute("py", args, &output, &exit_code, true);
	if (err != OK || exit_code != 0) {
		EditorNode::get_singleton()->show_warning(vformat(TTR("RPCS3 launch failed.\n%s\n\nLaunch %s from the RPCS3 game list (not Boot Game on the export folder)."), output, title_id));
		return;
	}

	EditorNode::get_singleton()->show_warning(output, TTR("PS3 Launch"));
}

void PS3EditorPlugin::_open_rpcs3_data_folder() {
	const String game_dir = _get_emulator_root().path_join("dev_hdd0").path_join("game");
	Ref<DirAccess> da = DirAccess::create(DirAccess::ACCESS_FILESYSTEM);
	if (da->make_dir_recursive(game_dir) != OK) {
		EditorNode::get_singleton()->show_warning(TTR("Could not create RPCS3 game directory."));
		return;
	}
	OS::get_singleton()->shell_open(ProjectSettings::get_singleton()->globalize_path(game_dir));
}
