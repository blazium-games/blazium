/**************************************************************************/
/*  justamcp_editor_plugin.cpp                                            */
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

#include "justamcp_editor_plugin.h"
#include "justamcp_server.h"
#include "tools/justamcp_prompt_executor.h"
#include "tools/justamcp_resource_executor.h"
#include "tools/justamcp_tool_executor.h"

#include "core/config/project_settings.h"
#include "editor/editor_node.h"
#include "editor/editor_settings.h"
#include "scene/gui/box_container.h"
#include "scene/gui/dialogs.h"
#include "scene/gui/grid_container.h"
#include "scene/gui/text_edit.h"
#include "servers/display_server.h"

static bool _justamcp_tool_error_is_cancelled(const Dictionary &p_result) {
	const Variant err = p_result.get("error", Variant());
	if (err.get_type() == Variant::STRING) {
		return String(err) == "cancelled";
	}
	if (err.get_type() == Variant::DICTIONARY) {
		return String(Dictionary(err).get("message", "")) == "cancelled";
	}
	return false;
}

static String _justamcp_tool_error_message(const Dictionary &p_result) {
	const Variant err = p_result.get("error", Variant());
	if (err.get_type() == Variant::DICTIONARY) {
		const Dictionary err_dict = err;
		if (err_dict.has("message")) {
			return String(err_dict["message"]);
		}
	}
	if (err.get_type() == Variant::STRING) {
		return err;
	}
	return "Unknown error";
}

void JustAMCPConfigUI::_bind_methods() {
	ClassDB::bind_method(D_METHOD("_copy_pressed"), &JustAMCPConfigUI::_copy_pressed);
	ClassDB::bind_method(D_METHOD("_on_settings_changed"), &JustAMCPConfigUI::_on_settings_changed);
}

void JustAMCPConfigUI::_update_config() {
	text_edit_antigravity->set_text(JustAMCPEditorPlugin::get_mcp_config_json(false));
	text_edit_cursor->set_text(JustAMCPEditorPlugin::get_mcp_config_json(true));

	JustAMCPToolExecutor exec;
	int total_tools = exec.get_tool_schemas(false, true).size();
	int active_tools = exec.get_tool_schemas(false, false).size();

	JustAMCPResourceExecutor res_exec;
	int total_res = res_exec.list_resources().get("resources", Array()).operator Array().size();

	JustAMCPPromptExecutor prmpt_exec;
	int total_prompts = prmpt_exec.list_prompts().get("prompts", Array()).operator Array().size();

	if (stats_tools_label) {
		stats_tools_label->set_text("Tools (Active): " + itos(active_tools) + " / " + itos(total_tools));
	}
	if (stats_resources_label) {
		stats_resources_label->set_text("Resources: " + itos(total_res));
	}
	if (stats_prompts_label) {
		stats_prompts_label->set_text("Prompts: " + itos(total_prompts));
	}
}

void JustAMCPConfigUI::_copy_pressed() {
	int current_tab = tab_container->get_current_tab();
	if (current_tab == 0) {
		DisplayServer::get_singleton()->clipboard_set(text_edit_antigravity->get_text());
	} else {
		DisplayServer::get_singleton()->clipboard_set(text_edit_cursor->get_text());
	}
}

void JustAMCPConfigUI::_on_settings_changed() {
	_update_config();
}

void JustAMCPConfigUI::_notification(int p_what) {
	if (p_what == NOTIFICATION_ENTER_TREE) {
		if (ProjectSettings::get_singleton()) {
			ProjectSettings::get_singleton()->connect("settings_changed", callable_mp(this, &JustAMCPConfigUI::_on_settings_changed));
		}
		if (EditorSettings::get_singleton()) {
			EditorSettings::get_singleton()->connect("settings_changed", callable_mp(this, &JustAMCPConfigUI::_on_settings_changed));
		}
		_update_config();
	} else if (p_what == NOTIFICATION_EXIT_TREE) {
		if (ProjectSettings::get_singleton()) {
			ProjectSettings::get_singleton()->disconnect("settings_changed", callable_mp(this, &JustAMCPConfigUI::_on_settings_changed));
		}
		if (EditorSettings::get_singleton()) {
			EditorSettings::get_singleton()->disconnect("settings_changed", callable_mp(this, &JustAMCPConfigUI::_on_settings_changed));
		}
	}
}

JustAMCPConfigUI::JustAMCPConfigUI() {
	VBoxContainer *vbox = memnew(VBoxContainer);
	add_child(vbox);

	Label *info_label = memnew(Label);
	info_label->set_text("Default MCP Config (Add to your IDE mcp_config.json):");
	vbox->add_child(info_label);

	GridContainer *stats_grid = memnew(GridContainer);
	stats_grid->set_columns(3);
	stats_grid->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	vbox->add_child(stats_grid);

	stats_tools_label = memnew(Label);
	stats_tools_label->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	stats_grid->add_child(stats_tools_label);

	stats_resources_label = memnew(Label);
	stats_resources_label->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	stats_grid->add_child(stats_resources_label);

	stats_prompts_label = memnew(Label);
	stats_prompts_label->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	stats_grid->add_child(stats_prompts_label);

	tab_container = memnew(TabContainer);
	tab_container->set_v_size_flags(Control::SIZE_EXPAND_FILL);
	vbox->add_child(tab_container);

	text_edit_antigravity = memnew(TextEdit);
	text_edit_antigravity->set_name("AntiGravity");
	text_edit_antigravity->set_custom_minimum_size(Size2(0, 200));
	text_edit_antigravity->set_editable(false);
	tab_container->add_child(text_edit_antigravity);

	text_edit_cursor = memnew(TextEdit);
	text_edit_cursor->set_name("Cursor");
	text_edit_cursor->set_custom_minimum_size(Size2(0, 200));
	text_edit_cursor->set_editable(false);
	tab_container->add_child(text_edit_cursor);

	copy_button = memnew(Button);
	copy_button->set_text("Copy Config to Clipboard");
	copy_button->connect("pressed", callable_mp(this, &JustAMCPConfigUI::_copy_pressed));
	vbox->add_child(copy_button);
}

bool JustAMCPConfigInspectorPlugin::can_handle(Object *p_object) {
	String cname = p_object->get_class();
	if (cname == "ProjectSettings" || cname == "EditorSettings" || cname == "SectionedInspectorFilter") {
		return true;
	}
	return false;
}

bool JustAMCPConfigInspectorPlugin::parse_property(Object *p_object, const Variant::Type p_type, const String &p_path, const PropertyHint p_hint, const String &p_hint_text, const BitField<PropertyUsageFlags> p_usage, const bool p_wide) {
	if (p_path == "blazium/justamcp/z_mcp_config" || p_path == "z_mcp_config") {
		JustAMCPConfigUI *ui = memnew(JustAMCPConfigUI);
		add_custom_control(ui);
		return true; // Stop default property editor rendering mapping
	}

	String found_path = "";
	String prop_value = "";

	JustAMCPPromptExecutor p_exec;
	Array prompts = p_exec.list_prompts().get("prompts", Array());
	for (int i = 0; i < prompts.size(); i++) {
		Dictionary p = prompts[i];
		String n = p["name"];
		String d = p["description"];
		if (p_path == n || p_path.ends_with("prompts/" + n)) {
			found_path = "blazium/justamcp/prompts/" + n;
			prop_value = d;
			break;
		}
	}

	if (found_path.is_empty()) {
		JustAMCPResourceExecutor r_exec;
		Array res = r_exec.list_resources().get("resources", Array());
		for (int i = 0; i < res.size(); i++) {
			Dictionary r = res[i];
			String n = r["name"];
			String d = r["description"];
			if (p_path == n || p_path.ends_with("resources/" + n)) {
				found_path = "blazium/justamcp/resources/" + n;
				prop_value = d;
				break;
			}
		}

		if (found_path.is_empty()) {
			Array tmpl = r_exec.list_resource_templates().get("resourceTemplates", Array());
			for (int i = 0; i < tmpl.size(); i++) {
				Dictionary r = tmpl[i];
				String n = r["name"];
				String d = r["description"];
				if (p_path == n || p_path.ends_with("resources/" + n)) {
					found_path = "blazium/justamcp/resources/" + n;
					prop_value = d;
					break;
				}
			}
		}
	}

	if (!found_path.is_empty()) {
		VBoxContainer *vbox = memnew(VBoxContainer);
		vbox->set_h_size_flags(Control::SIZE_EXPAND_FILL);

		Label *lbl = memnew(Label);
		lbl->set_text(found_path.get_file());
		vbox->add_child(lbl);

		TextEdit *text_edit = memnew(TextEdit);
		text_edit->set_editable(false);
		text_edit->set_text(prop_value);
		text_edit->set_custom_minimum_size(Size2(0, 60));
		text_edit->set_v_size_flags(Control::SIZE_EXPAND_FILL);
		text_edit->set_h_size_flags(Control::SIZE_EXPAND_FILL);
		text_edit->add_theme_color_override("font_readonly_color", Color(0.8, 0.8, 0.8));
		vbox->add_child(text_edit);

		add_custom_control(vbox);
		return true;
	}

	return false;
}

void JustAMCPEditorPlugin::_bind_methods() {
	ClassDB::bind_method(D_METHOD("_on_tool_requested", "request_id", "tool_name", "args"), &JustAMCPEditorPlugin::_on_tool_requested);
	ClassDB::bind_method(D_METHOD("_show_configuration_dialog"), &JustAMCPEditorPlugin::_show_configuration_dialog);
	ClassDB::bind_method(D_METHOD("_on_server_status_changed", "started"), &JustAMCPEditorPlugin::_on_server_status_changed);
}

JustAMCPEditorPlugin::JustAMCPEditorPlugin() {
}

JustAMCPEditorPlugin::~JustAMCPEditorPlugin() {
}

void JustAMCPEditorPlugin::_notification(int p_what) {
	switch (p_what) {
		case NOTIFICATION_ENTER_TREE: {
			EditorNode *editor_node = EditorNode::get_singleton();
			if (editor_node) {
				mcp_server = Object::cast_to<JustAMCPServer>(editor_node->get_node_or_null(NodePath("JustAMCPServer")));
			}
			if (!mcp_server) {
				mcp_server = memnew(JustAMCPServer);
				mcp_server->set_name("JustAMCPServer");
				if (editor_node) {
					editor_node->call_deferred(SNAME("add_child"), mcp_server);
				} else {
					add_child(mcp_server);
				}
			} else if (editor_node && mcp_server->get_parent() != editor_node) {
				editor_node->call_deferred(SNAME("add_child"), mcp_server);
			}

			tool_executor = memnew(JustAMCPToolExecutor);
			tool_executor->set_editor_plugin(this);

			if (!mcp_server->is_connected("tool_requested", callable_mp(this, &JustAMCPEditorPlugin::_on_tool_requested))) {
				mcp_server->connect("tool_requested", callable_mp(this, &JustAMCPEditorPlugin::_on_tool_requested));
			}
			if (!mcp_server->is_connected("request_cancelled", callable_mp(mcp_server, &JustAMCPServer::_on_request_cancelled))) {
				mcp_server->connect("request_cancelled", callable_mp(mcp_server, &JustAMCPServer::_on_request_cancelled));
			}
			if (!mcp_server->is_connected("server_status_changed", callable_mp(this, &JustAMCPEditorPlugin::_on_server_status_changed))) {
				mcp_server->connect("server_status_changed", callable_mp(this, &JustAMCPEditorPlugin::_on_server_status_changed));
			}

			_setup_status_indicator();

			inspector_plugin.instantiate();
			EditorInspector::add_inspector_plugin(inspector_plugin);

			// We only show the configuration dialog under Tools Menu
			add_tool_menu_item("JustAMCP Configuration", callable_mp(this, &JustAMCPEditorPlugin::_show_configuration_dialog));

			// DO NOT call add_autoload_singleton with a native C++ object here, it will crash
			// register_types.cpp's Engine::add_singleton handles native registration natively without SceneTree polling.
		} break;

		case NOTIFICATION_EXIT_TREE: {
			remove_tool_menu_item("JustAMCP Configuration");

			if (inspector_plugin.is_valid()) {
				EditorInspector::remove_inspector_plugin(inspector_plugin);
				inspector_plugin.unref();
			}

			if (mcp_server) {
				if (mcp_server->is_connected("tool_requested", callable_mp(this, &JustAMCPEditorPlugin::_on_tool_requested))) {
					mcp_server->disconnect("tool_requested", callable_mp(this, &JustAMCPEditorPlugin::_on_tool_requested));
				}
				if (mcp_server->is_connected("server_status_changed", callable_mp(this, &JustAMCPEditorPlugin::_on_server_status_changed))) {
					mcp_server->disconnect("server_status_changed", callable_mp(this, &JustAMCPEditorPlugin::_on_server_status_changed));
				}
				// Keep the MCP server alive on EditorNode for headless module test runs.
				mcp_server = nullptr;
			}

			if (tool_executor) {
				memdelete(tool_executor);
				tool_executor = nullptr;
			}

			if (status_label) {
				remove_control_from_container(CONTAINER_TOOLBAR, status_label);
				status_label->queue_free();
				status_label = nullptr;
			}
		} break;
	}
}

void JustAMCPEditorPlugin::_setup_status_indicator() {
	status_label = memnew(Label);
	status_label->set_text("MCP Server Active");
	status_label->add_theme_color_override("font_color", Color(0, 1, 0)); // GREEN
	status_label->add_theme_font_size_override("font_size", 12);
	status_label->set_visible(mcp_server && mcp_server->is_server_started());
	add_control_to_container(CONTAINER_TOOLBAR, status_label);
}

void JustAMCPEditorPlugin::_on_server_status_changed(bool p_started) {
	if (status_label) {
		status_label->set_visible(p_started);
	}
}

void JustAMCPEditorPlugin::_show_configuration_dialog() {
	AcceptDialog *dialog = memnew(AcceptDialog);
	dialog->set_title("JustAMCP Configuration");
	dialog->set_min_size(Size2(600, 300));

	VBoxContainer *vbox = memnew(VBoxContainer);
	dialog->add_child(vbox);

	Label *info_label = memnew(Label);
	info_label->set_text("Add the following JSON to your IDE mcp_config.json:");
	vbox->add_child(info_label);

	TabContainer *tab_container = memnew(TabContainer);
	tab_container->set_v_size_flags(Control::SIZE_EXPAND_FILL);
	vbox->add_child(tab_container);

	TextEdit *text_edit_ag = memnew(TextEdit);
	text_edit_ag->set_name("AntiGravity");
	text_edit_ag->set_v_size_flags(Control::SIZE_EXPAND_FILL);
	text_edit_ag->set_editable(false);
	text_edit_ag->set_text(JustAMCPEditorPlugin::get_mcp_config_json(false));
	tab_container->add_child(text_edit_ag);

	TextEdit *text_edit_cursor = memnew(TextEdit);
	text_edit_cursor->set_name("Cursor");
	text_edit_cursor->set_v_size_flags(Control::SIZE_EXPAND_FILL);
	text_edit_cursor->set_editable(false);
	text_edit_cursor->set_text(JustAMCPEditorPlugin::get_mcp_config_json(true));
	tab_container->add_child(text_edit_cursor);

	EditorNode::get_singleton()->get_gui_base()->add_child(dialog);
	dialog->popup_centered();
}

void JustAMCPEditorPlugin::_on_tool_requested(const Variant &p_request_id, const String &p_tool_name, const Dictionary &p_args) {
	if (!mcp_server) {
		return;
	}

	if (!tool_executor) {
		Dictionary payload;
		payload["code"] = -32000;
		payload["message"] = "JustAMCP tool executor is unavailable.";
		mcp_server->send_tool_result(p_request_id, false, payload, "JustAMCP tool executor is unavailable.");
		return;
	}

	if (mcp_server->is_current_tool_cancel_requested()) {
		Dictionary cancelled;
		cancelled["ok"] = false;
		cancelled["error"] = "cancelled";
		mcp_server->send_tool_result(p_request_id, false, cancelled, "cancelled");
		return;
	}

	Dictionary result = tool_executor->execute_tool(p_tool_name, p_args);

	if (mcp_server->is_current_tool_cancel_requested() && !_justamcp_tool_error_is_cancelled(result)) {
		result["ok"] = false;
		result["error"] = "cancelled";
	}

	bool success = result.get("ok", false);

	if (success) {
		Dictionary payload = result.duplicate();
		payload.erase("ok");
		mcp_server->send_tool_result(p_request_id, true, payload, "");
	} else {
		const String error_msg = _justamcp_tool_error_message(result);
		const Variant payload = result.get("error", Variant());
		mcp_server->send_tool_result(p_request_id, false, payload, error_msg);
	}
}

String JustAMCPEditorPlugin::get_mcp_config_json(bool p_is_cursor) {
	int port = 6506;
	bool oauth_enabled = false;
	String client_id = "";
	String client_secret = "";

	bool use_project_override = false;
	if (ProjectSettings::get_singleton() && ProjectSettings::get_singleton()->has_setting("blazium/justamcp/override_editor_settings")) {
		use_project_override = GLOBAL_GET("blazium/justamcp/override_editor_settings");
	}

	if (use_project_override || !EditorSettings::get_singleton()) {
		if (ProjectSettings::get_singleton()) {
			if (ProjectSettings::get_singleton()->has_setting("blazium/justamcp/server_port")) {
				port = static_cast<int>(GLOBAL_GET("blazium/justamcp/server_port"));
			}
			if (ProjectSettings::get_singleton()->has_setting("blazium/justamcp/oauth_enabled")) {
				oauth_enabled = GLOBAL_GET("blazium/justamcp/oauth_enabled");
			}
			if (ProjectSettings::get_singleton()->has_setting("blazium/justamcp/client_id")) {
				client_id = String(GLOBAL_GET("blazium/justamcp/client_id"));
			}
			if (ProjectSettings::get_singleton()->has_setting("blazium/justamcp/client_secret")) {
				client_secret = String(GLOBAL_GET("blazium/justamcp/client_secret"));
			}
		}
	} else if (EditorSettings::get_singleton()) {
		if (EditorSettings::get_singleton()->has_setting("blazium/justamcp/server_port")) {
			port = EditorSettings::get_singleton()->get_setting("blazium/justamcp/server_port");
		}
		if (EditorSettings::get_singleton()->has_setting("blazium/justamcp/oauth_enabled")) {
			oauth_enabled = EditorSettings::get_singleton()->get_setting("blazium/justamcp/oauth_enabled");
		}
		if (EditorSettings::get_singleton()->has_setting("blazium/justamcp/client_id")) {
			client_id = String(EditorSettings::get_singleton()->get_setting("blazium/justamcp/client_id"));
		}
		if (EditorSettings::get_singleton()->has_setting("blazium/justamcp/client_secret")) {
			client_secret = String(EditorSettings::get_singleton()->get_setting("blazium/justamcp/client_secret"));
		}
	}

	String json_config = "{\n";
	json_config += "  \"mcpServers\": {\n";
	json_config += "    \"blazium-mcp\": {\n";
	if (p_is_cursor) {
		json_config += "      \"url\": \"http://127.0.0.1:" + itos(port) + "/sse\"";
	} else {
		json_config += "      \"serverUrl\": \"http://127.0.0.1:" + itos(port) + "/mcp\"";
	}

	if (oauth_enabled && (!client_id.is_empty() || !client_secret.is_empty())) {
		json_config += ",\n      \"oauth\": {\n";
		json_config += "        \"clientId\": \"" + client_id + "\",\n";
		json_config += "        \"clientSecret\": \"" + client_secret + "\"\n";
		json_config += "      }\n";
	} else {
		json_config += "\n";
	}

	json_config += "    }\n";
	json_config += "  }\n";
	json_config += "}";
	return json_config;
}

#endif // TOOLS_ENABLED
