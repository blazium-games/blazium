/**************************************************************************/
/*  justamcp_editor_plugin.h                                              */
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

#pragma once

#ifdef TOOLS_ENABLED

#include "justamcp_server.h"
#include "tools/justamcp_tool_executor.h"

#include "editor/inspector/editor_inspector.h"
#include "editor/plugins/editor_plugin.h"
#include "scene/gui/button.h"
#include "scene/gui/label.h"
#include "scene/gui/margin_container.h"
#include "scene/gui/tab_container.h"
#include "scene/gui/text_edit.h"

class JustAMCPConfigUI : public MarginContainer {
	GDCLASS(JustAMCPConfigUI, MarginContainer);

	TabContainer *tab_container = nullptr;
	TextEdit *text_edit_antigravity = nullptr;
	TextEdit *text_edit_cursor = nullptr;
	TextEdit *text_edit_opencode = nullptr;
	Button *copy_button = nullptr;

	Label *stats_tools_label = nullptr;
	Label *stats_resources_label = nullptr;
	Label *stats_prompts_label = nullptr;

	void _update_config();
	void _copy_pressed();
	void _on_settings_changed();

protected:
	static void _bind_methods();
	void _notification(int p_what);

public:
	JustAMCPConfigUI();
};

class JustAMCPConfigInspectorPlugin : public EditorInspectorPlugin {
	GDCLASS(JustAMCPConfigInspectorPlugin, EditorInspectorPlugin);

public:
	virtual bool can_handle(Object *p_object) override;
	virtual bool parse_property(Object *p_object, const Variant::Type p_type, const String &p_path, const PropertyHint p_hint, const String &p_hint_text, const BitField<PropertyUsageFlags> p_usage, const bool p_wide) override;
};

class JustAMCPEditorPlugin : public EditorPlugin {
	GDCLASS(JustAMCPEditorPlugin, EditorPlugin);

private:
	JustAMCPServer *mcp_server = nullptr;
	JustAMCPToolExecutor *tool_executor = nullptr;
	Label *status_label = nullptr;
	Ref<JustAMCPConfigInspectorPlugin> inspector_plugin;

	void _setup_status_indicator();
	void _show_configuration_dialog();
	void _on_server_status_changed(bool p_started);

	void _on_tool_requested(const Variant &p_request_id, const String &p_tool_name, const Dictionary &p_args);
	void _invalidate_subscribed_editor_resources();
	void _on_filesystem_changed_for_subscriptions();

protected:
	static void _bind_methods();
	void _notification(int p_what);

public:
	virtual String get_plugin_name() const override { return "JustAMCP"; }
	bool has_main_screen() const override { return false; }
	enum MCPConfigClient {
		MCP_CONFIG_ANTIGRAVITY = 0,
		MCP_CONFIG_CURSOR = 1,
		MCP_CONFIG_OPENCODE = 2,
	};
	static String get_mcp_config_json(MCPConfigClient p_client = MCP_CONFIG_ANTIGRAVITY);

	JustAMCPEditorPlugin();
	~JustAMCPEditorPlugin();
};

#endif
