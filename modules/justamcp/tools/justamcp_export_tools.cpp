/**************************************************************************/
/*  justamcp_export_tools.cpp                                             */
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

#include "justamcp_export_tools.h"

#include "../justamcp_server.h"
#include "../justamcp_tool_context.h"

#include "core/config/project_settings.h"
#include "core/io/config_file.h"
#include "core/io/dir_access.h"
#include "core/io/file_access.h"
#include "core/object/worker_thread_pool.h"
#include "core/os/os.h"
#include "core/os/thread.h"

#ifdef TOOLS_ENABLED
#include "editor/settings/editor_settings.h"
#endif

#include "../justamcp_mcp_tool_macros.h"

void JustAMCPExportTools::_bind_methods() {}

JustAMCPExportTools::JustAMCPExportTools() {}

JustAMCPExportTools::~JustAMCPExportTools() {}

Dictionary JustAMCPExportTools::_execute_blocking_tool_sync(const String &p_tool_name, const Dictionary &p_args) {
	if (p_tool_name == "list_android_devices") {
		return _list_android_devices(p_args);
	}
	if (p_tool_name == "deploy_to_android") {
		return _deploy_to_android(p_args);
	}
	Dictionary err;
	err["ok"] = false;
	err["error"] = "Unknown blocking export tool: " + p_tool_name;
	return err;
}

struct JustAMCPExportBlockingJob {
	JustAMCPExportTools *tools = nullptr;
	String tool_name;
	Dictionary args;
	Variant request_id;
};

static void _justamcp_export_blocking_worker(void *p_userdata) {
	JustAMCPExportBlockingJob *job = static_cast<JustAMCPExportBlockingJob *>(p_userdata);
	Dictionary result;
	if (job && job->tools) {
		result = job->tools->_execute_blocking_tool_sync(job->tool_name, job->args);
	} else {
		result["ok"] = false;
		result["error"] = "Export blocking job missing tools instance";
	}
	const Variant request_id = job ? job->request_id : Variant();
	if (job) {
		memdelete(job);
	}
	JustAMCPServer *server = JustAMCPServer::get_singleton();
	if (server && request_id.get_type() != Variant::NIL) {
		server->call_deferred(SNAME("_deferred_complete_tool_dict"), request_id, result);
	}
}

bool JustAMCPExportTools::_try_schedule_blocking_tool(const String &p_tool_name, const Dictionary &p_args, Dictionary &r_pending) const {
	if (!Thread::is_main_thread()) {
		return false;
	}
	const Variant request_id = justamcp_get_active_tool_request_id();
	if (request_id.get_type() == Variant::NIL) {
		return false;
	}
	WorkerThreadPool *pool = WorkerThreadPool::get_singleton();
	if (!pool) {
		return false;
	}
	JustAMCPExportBlockingJob *job = memnew(JustAMCPExportBlockingJob);
	job->tools = const_cast<JustAMCPExportTools *>(this);
	job->tool_name = p_tool_name;
	job->args = p_args;
	job->request_id = request_id;
	pool->add_native_task(&_justamcp_export_blocking_worker, job, true, "JustAMCPExportADB");
	r_pending["ok"] = true;
	r_pending["_justamcp_async_pending"] = true;
	return true;
}

Dictionary JustAMCPExportTools::execute_tool(const String &p_tool_name, const Dictionary &p_args) {
	if (p_tool_name == "list_export_presets") {
		return _list_export_presets(p_args);
	} else if (p_tool_name == "export_project") {
		return _export_project(p_args);
	} else if (p_tool_name == "get_export_info") {
		return _get_export_info(p_args);
	} else if (p_tool_name == "list_android_devices" || p_tool_name == "deploy_to_android") {
		Dictionary pending;
		if (_try_schedule_blocking_tool(p_tool_name, p_args, pending)) {
			return pending;
		}
		if (Thread::is_main_thread()) {
			Dictionary err;
			err["ok"] = false;
			err["error"] = "Export/ADB tools require async WorkerThreadPool scheduling on the main thread";
			err["_justamcp_async_required"] = true;
			return err;
		}
		return _execute_blocking_tool_sync(p_tool_name, p_args);
	} else if (p_tool_name == "get_android_preset_info") {
		return _get_android_preset_info(p_args);
	}

	Dictionary err;
	err["code"] = -32601;
	err["message"] = "Method not found: " + p_tool_name;
	Dictionary res;
	res["error"] = err;
	return Dictionary();
}

String JustAMCPExportTools::_resolve_adb_path() const {
#ifdef TOOLS_ENABLED
	if (EditorSettings::get_singleton() && EditorSettings::get_singleton()->has_setting("export/android/adb")) {
		String configured = EditorSettings::get_singleton()->get_setting("export/android/adb");
		if (!configured.is_empty() && FileAccess::exists(configured)) {
			return configured;
		}
	}
#endif
	return "adb";
}

Dictionary JustAMCPExportTools::_run_blocking(const String &p_command, const List<String> &p_args) const {
	Dictionary ret;
	if (Thread::is_main_thread()) {
		ret["ok"] = false;
		ret["error_code"] = FAILED;
		ret["exit_code"] = -1;
		ret["stdout"] = "";
		ret["error"] = "OS::execute refused on main thread";
		return ret;
	}
	String output;
	int exit_code = 0;
	Error err = OS::get_singleton()->execute(p_command, p_args, &output, &exit_code, true);
	ret["ok"] = err == OK;
	ret["error_code"] = err;
	ret["exit_code"] = exit_code;
	ret["stdout"] = output;
	return ret;
}

Dictionary JustAMCPExportTools::_find_android_preset(const String &p_preset_name, int p_preset_index) const {
	Dictionary preset;
	String presets_path = "res://export_presets.cfg";
	if (!FileAccess::exists(presets_path)) {
		return preset;
	}

	Ref<ConfigFile> cfg;
	cfg.instantiate();
	if (cfg->load(presets_path) != OK) {
		return preset;
	}

	int idx = 0;
	while (cfg->has_section("preset." + itos(idx))) {
		String section = "preset." + itos(idx);
		String platform = cfg->get_value(section, "platform", "");
		String name = cfg->get_value(section, "name", "");
		bool matches = false;
		if (!p_preset_name.is_empty()) {
			matches = name == p_preset_name;
		} else if (p_preset_index >= 0) {
			matches = idx == p_preset_index;
		} else {
			matches = platform == "Android";
		}

		if (matches) {
			preset["index"] = idx;
			preset["name"] = name;
			preset["platform"] = platform;
			preset["runnable"] = cfg->get_value(section, "runnable", false);
			preset["export_path"] = cfg->get_value(section, "export_path", "");
			String options_section = "preset." + itos(idx) + ".options";
			String package_name;
			if (cfg->has_section(options_section)) {
				package_name = cfg->get_value(options_section, "package/unique_name", "");
			}
			preset["package_name"] = package_name;
			return preset;
		}
		idx++;
	}
	return preset;
}

Dictionary JustAMCPExportTools::_list_export_presets(const Dictionary &p_params) {
	String presets_path = "res://export_presets.cfg";
	if (!FileAccess::exists(presets_path)) {
		Dictionary res;
		res["presets"] = Array();
		res["count"] = 0;
		res["message"] = "No export_presets.cfg found";
		return MCP_SUCCESS(res);
	}

	Ref<ConfigFile> cfg;
	cfg.instantiate();
	Error err = cfg->load(presets_path);
	if (err != OK) {
		return MCP_INTERNAL("Failed to read export_presets.cfg");
	}

	Array presets;
	int idx = 0;
	while (cfg->has_section("preset." + itos(idx))) {
		String section = "preset." + itos(idx);
		Dictionary preset;
		preset["index"] = idx;
		preset["name"] = cfg->get_value(section, "name", "");
		preset["platform"] = cfg->get_value(section, "platform", "");
		preset["runnable"] = cfg->get_value(section, "runnable", false);
		preset["export_path"] = cfg->get_value(section, "export_path", "");
		presets.push_back(preset);
		idx++;
	}

	Dictionary res;
	res["presets"] = presets;
	res["count"] = presets.size();
	return MCP_SUCCESS(res);
}

Dictionary JustAMCPExportTools::_export_project(const Dictionary &p_params) {
	if (justamcp_is_cancel_requested()) {
		Dictionary err;
		err["ok"] = false;
		err["error"] = "cancelled";
		return err;
	}
	justamcp_report_progress(0, 2, "Preparing export");
	int preset_index = p_params.has("preset_index") ? int(p_params["preset_index"]) : -1;
	String preset_name = p_params.has("preset_name") ? String(p_params["preset_name"]) : "";
	bool debug = p_params.has("debug") ? bool(p_params["debug"]) : true;

	String presets_path = "res://export_presets.cfg";
	if (!FileAccess::exists(presets_path)) {
		return MCP_ERROR(-32000, "No export_presets.cfg found. Configure exports in Project > Export first.");
	}

	Ref<ConfigFile> cfg;
	cfg.instantiate();
	Error err = cfg->load(presets_path);
	if (err != OK) {
		return MCP_INTERNAL("Failed to read export_presets.cfg");
	}

	String target_section = "";
	String target_name = "";
	String target_path = "";

	if (!preset_name.is_empty()) {
		int idx = 0;
		while (cfg->has_section("preset." + itos(idx))) {
			String section = "preset." + itos(idx);
			if (String(cfg->get_value(section, "name", "")) == preset_name) {
				target_section = section;
				target_name = preset_name;
				target_path = cfg->get_value(section, "export_path", "");
				break;
			}
			idx++;
		}
	} else if (preset_index >= 0) {
		String section = "preset." + itos(preset_index);
		if (cfg->has_section(section)) {
			target_section = section;
			target_name = cfg->get_value(section, "name", "");
			target_path = cfg->get_value(section, "export_path", "");
		}
	}

	if (target_section.is_empty()) {
		return MCP_NOT_FOUND("Export preset");
	}

	if (target_path.is_empty()) {
		return MCP_ERROR(-32000, "Export path not configured for preset '" + target_name + "'");
	}

	String godot_path = OS::get_singleton()->get_executable_path();
	String project_path = ProjectSettings::get_singleton()->globalize_path("res://");
	String export_path = target_path;
	if (target_path.begins_with("res://")) {
		export_path = ProjectSettings::get_singleton()->globalize_path(target_path);
	}

	String flag = debug ? "--export-debug" : "--export-release";
	String command = vformat("\"%s\" --headless --path \"%s\" %s \"%s\"", godot_path, project_path, flag, target_name);

	Dictionary res;
	res["preset"] = target_name;
	res["export_path"] = export_path;
	res["debug"] = debug;
	res["command"] = command;
	res["message"] = "Run the command above to export. Direct export from editor plugin is not supported in Godot 4 via simple MCP calls yet.";
	justamcp_report_progress(2, 2, "Export command prepared");
	return MCP_SUCCESS(res);
}

Dictionary JustAMCPExportTools::_get_export_info(const Dictionary &p_params) {
	Dictionary info;
	info["has_export_presets"] = FileAccess::exists("res://export_presets.cfg");
	info["godot_executable"] = OS::get_singleton()->get_executable_path();
	info["project_path"] = ProjectSettings::get_singleton()->globalize_path("res://");

	String templates_path = OS::get_singleton()->get_data_path().path_join("export_templates");
	info["templates_dir"] = templates_path;
	info["templates_installed"] = DirAccess::dir_exists_absolute(templates_path);

	return MCP_SUCCESS(info);
}

Dictionary JustAMCPExportTools::_list_android_devices(const Dictionary &p_params) {
	String adb = _resolve_adb_path();
	List<String> args;
	args.push_back("devices");
	args.push_back("-l");
	Dictionary run = _run_blocking(adb, args);
	if (!bool(run.get("ok", false)) || int(run.get("exit_code", 0)) != 0) {
		Dictionary data;
		data["adb_path"] = adb;
		data["output"] = run.get("stdout", "");
		return MCP_ERROR_DATA(-32000, "adb failed. Install Android platform-tools or configure export/android/adb.", data);
	}

	Array devices;
	Vector<String> lines = String(run.get("stdout", "")).split("\n");
	for (int i = 0; i < lines.size(); i++) {
		String line = lines[i].strip_edges();
		if (line.is_empty() || line.begins_with("List of devices") || line.begins_with("* daemon")) {
			continue;
		}
		Vector<String> parts = line.split(" ", false);
		if (parts.size() < 2) {
			continue;
		}
		Dictionary device;
		device["serial"] = parts[0];
		device["state"] = parts[1];
		for (int j = 2; j < parts.size(); j++) {
			int sep = parts[j].find(":");
			if (sep > 0) {
				device[parts[j].substr(0, sep)] = parts[j].substr(sep + 1);
			}
		}
		devices.push_back(device);
	}

	Dictionary res;
	res["devices"] = devices;
	res["count"] = devices.size();
	res["adb_path"] = adb;
	return MCP_SUCCESS(res);
}

Dictionary JustAMCPExportTools::_get_android_preset_info(const Dictionary &p_params) {
	Dictionary preset = _find_android_preset(p_params.get("preset_name", ""), p_params.get("preset_index", -1));
	if (preset.is_empty()) {
		return MCP_NOT_FOUND("Android export preset");
	}
	if (String(preset.get("platform", "")) != "Android") {
		return MCP_ERROR(-32000, "Selected export preset is not an Android preset.");
	}
	return MCP_SUCCESS(preset);
}

Dictionary JustAMCPExportTools::_deploy_to_android(const Dictionary &p_params) {
	Dictionary preset = _find_android_preset(p_params.get("preset_name", ""), p_params.get("preset_index", -1));
	if (preset.is_empty() || String(preset.get("platform", "")) != "Android") {
		return MCP_NOT_FOUND("Android export preset");
	}

	String export_path = preset.get("export_path", "");
	if (export_path.is_empty()) {
		return MCP_ERROR(-32000, "Android preset export_path is not configured.");
	}
	String apk_path = export_path.begins_with("res://") ? ProjectSettings::get_singleton()->globalize_path(export_path) : export_path;
	bool debug = p_params.get("debug", true);
	bool skip_export = p_params.get("skip_export", false);
	bool launch = p_params.get("launch", true);
	String device_serial = p_params.get("device_serial", "");
	Array steps;

	if (!skip_export) {
		List<String> export_args;
		export_args.push_back("--headless");
		export_args.push_back("--path");
		export_args.push_back(ProjectSettings::get_singleton()->globalize_path("res://"));
		export_args.push_back(debug ? "--export-debug" : "--export-release");
		export_args.push_back(preset.get("name", ""));
		export_args.push_back(apk_path);
		Dictionary export_run = _run_blocking(OS::get_singleton()->get_executable_path(), export_args);
		Dictionary step;
		step["step"] = "export";
		step["exit_code"] = export_run.get("exit_code", -1);
		step["stdout"] = export_run.get("stdout", "");
		steps.push_back(step);
		if (!bool(export_run.get("ok", false)) || int(export_run.get("exit_code", 0)) != 0) {
			Dictionary data;
			data["steps"] = steps;
			return MCP_ERROR_DATA(-32000, "Blazium Android export failed.", data);
		}
	}

	if (!FileAccess::exists(apk_path)) {
		Dictionary data;
		data["apk_path"] = apk_path;
		data["steps"] = steps;
		return MCP_ERROR_DATA(-32000, "APK not found after export.", data);
	}

	String adb = _resolve_adb_path();
	List<String> install_args;
	if (!device_serial.is_empty()) {
		install_args.push_back("-s");
		install_args.push_back(device_serial);
	}
	install_args.push_back("install");
	install_args.push_back("-r");
	install_args.push_back(apk_path);
	Dictionary install_run = _run_blocking(adb, install_args);
	Dictionary install_step;
	install_step["step"] = "install";
	install_step["exit_code"] = install_run.get("exit_code", -1);
	install_step["stdout"] = install_run.get("stdout", "");
	steps.push_back(install_step);
	if (!bool(install_run.get("ok", false)) || int(install_run.get("exit_code", 0)) != 0) {
		Dictionary data;
		data["steps"] = steps;
		return MCP_ERROR_DATA(-32000, "adb install failed.", data);
	}

	String package_name = preset.get("package_name", "");
	if (launch && !package_name.is_empty()) {
		List<String> launch_args;
		if (!device_serial.is_empty()) {
			launch_args.push_back("-s");
			launch_args.push_back(device_serial);
		}
		launch_args.push_back("shell");
		launch_args.push_back("monkey");
		launch_args.push_back("-p");
		launch_args.push_back(package_name);
		launch_args.push_back("-c");
		launch_args.push_back("android.intent.category.LAUNCHER");
		launch_args.push_back("1");
		Dictionary launch_run = _run_blocking(adb, launch_args);
		Dictionary launch_step;
		launch_step["step"] = "launch";
		launch_step["exit_code"] = launch_run.get("exit_code", -1);
		launch_step["stdout"] = launch_run.get("stdout", "");
		steps.push_back(launch_step);
	}

	Dictionary res;
	res["preset"] = preset.get("name", "");
	res["apk_path"] = apk_path;
	res["device"] = device_serial;
	res["package_name"] = package_name;
	res["steps"] = steps;
	return MCP_SUCCESS(res);
}
