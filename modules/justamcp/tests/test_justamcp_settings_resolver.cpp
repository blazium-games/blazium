/**************************************************************************/
/*  test_justamcp_settings_resolver.cpp                                   */
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

#ifdef TESTS_ENABLED

#include "test_justamcp_settings_resolver.h"
#include "../justamcp_cli_args.h"
#include "../tools/justamcp_json_rpc_helpers.h"
#include "../tools/justamcp_prompt_executor.h"
#include "../tools/justamcp_resource_manifest.h"
#include "../tools/justamcp_settings_resolver.h"
#include "../tools/justamcp_tool_dispatcher.h"
#include "../tools/justamcp_tool_executor.h"
#include "../tools/justamcp_tool_schema_cache.h"
#include "core/config/project_settings.h"
#include "modules/modules_enabled.gen.h"
#include "tests/test_macros.h"

void test_justamcp_settings_resolver() {
	// macOS editor CI runs `--test` without `--headless`; fixtures write Project Settings.
	CHECK(JustAMCPSettingsResolver::uses_project_override());
	bool cat_enabled = false;
	bool tool_enabled = false;
	CHECK(JustAMCPSettingsResolver::resolve_tool_enabled("asset_tags_tools", "blazium_tags_list", true, false, cat_enabled, tool_enabled));
	CHECK(cat_enabled);
	CHECK(tool_enabled);
	CHECK(JustAMCPSettingsResolver::resolve_category_enabled("asset_tags_tools", true));
}

void test_justamcp_settings_resolver_typed_values() {
	ProjectSettings *ps = ProjectSettings::get_singleton();
	CHECK(ps);

	const int prev_port = int(ps->get_setting("blazium/justamcp/server_port", 6506));
	const String prev_origin = String(ps->get_setting("blazium/justamcp/streamable_http_allowed_origin", ""));
	const bool prev_override = bool(ps->get_setting("blazium/justamcp/override_editor_settings", false));
	const bool had_hosts = ps->has_setting("blazium/justamcp/bridge_url_allow_hosts");
	const Variant prev_hosts = had_hosts ? ps->get_setting("blazium/justamcp/bridge_url_allow_hosts") : Variant();

	ps->set_setting("blazium/justamcp/override_editor_settings", true);
	ps->set_setting("blazium/justamcp/server_port", 6511);
	ps->set_setting("blazium/justamcp/streamable_http_allowed_origin", "https://example.test");
	Array hosts;
	hosts.push_back("example.test");
	ps->set_setting("blazium/justamcp/bridge_url_allow_hosts", hosts);

	CHECK(JustAMCPSettingsResolver::resolve_int("blazium/justamcp/server_port", 6506) == 6511);
	CHECK(JustAMCPSettingsResolver::resolve_string("blazium/justamcp/streamable_http_allowed_origin") == "https://example.test");
	const Array resolved = JustAMCPSettingsResolver::resolve_array("blazium/justamcp/bridge_url_allow_hosts");
	CHECK(resolved.size() == 1);
	CHECK(String(resolved[0]) == "example.test");

	JustAMCPCliArgs::set_test_mcp_port(6512);
	CHECK(JustAMCPSettingsResolver::resolve_server_port() == 6512);
	JustAMCPCliArgs::clear_test_overrides();
	CHECK(JustAMCPSettingsResolver::resolve_server_port() == 6511);

	ps->set_setting("blazium/justamcp/server_port", prev_port);
	ps->set_setting("blazium/justamcp/streamable_http_allowed_origin", prev_origin);
	ps->set_setting("blazium/justamcp/override_editor_settings", prev_override);
	if (had_hosts) {
		ps->set_setting("blazium/justamcp/bridge_url_allow_hosts", prev_hosts);
	} else {
		ps->clear("blazium/justamcp/bridge_url_allow_hosts");
	}
}

void test_justamcp_resource_manifest() {
	Array resources = JustAMCPResourceManifest::get_static_resource_schemas();
	CHECK(resources.size() >= 10);
	bool has_dictionary = false;
	for (int i = 0; i < resources.size(); i++) {
		Dictionary item = resources[i];
		if (String(item.get("uri", "")) == "blazium://tags/dictionary") {
			has_dictionary = true;
		}
	}
#ifdef MODULE_ASSETTAGS_ENABLED
	CHECK(has_dictionary);
#else
	(void)has_dictionary;
#endif
	Array templates = JustAMCPResourceManifest::get_static_resource_template_schemas();
	CHECK(templates.size() >= 5);
	CHECK(JustAMCPToolSchemaCache::get_schemas(false, true, true, true).size() > 0);
	JustAMCPToolExecutor counter;
	(void)counter;
	CHECK(JustAMCPToolExecutor::get_active_instance() == nullptr);
}

#ifdef TOOLS_ENABLED
static bool _schema_has_tool(const Array &p_schemas, const String &p_name) {
	for (int i = 0; i < p_schemas.size(); i++) {
		Dictionary tool = p_schemas[i];
		if (String(tool.get("name", "")) == p_name) {
			return true;
		}
	}
	return false;
}

void test_justamcp_list_vs_execute_and_active_count() {
	ProjectSettings *ps = ProjectSettings::get_singleton();
	CHECK(ps);

	const bool prev_override = bool(ps->get_setting("blazium/justamcp/override_editor_settings", false));
	const bool had_cat = ps->has_setting("blazium/justamcp/tools/editor_tools");
	const Variant prev_cat = had_cat ? ps->get_setting("blazium/justamcp/tools/editor_tools") : Variant();
	const bool had_tool = ps->has_setting("blazium/justamcp/tools/editor_tools/blazium_editor_play_scene");
	const Variant prev_tool = had_tool ? ps->get_setting("blazium/justamcp/tools/editor_tools/blazium_editor_play_scene") : Variant();
	const bool had_prompts = ps->has_setting("blazium/justamcp/prompts");
	const Variant prev_prompts = had_prompts ? ps->get_setting("blazium/justamcp/prompts") : Variant();

	ps->set_setting("blazium/justamcp/override_editor_settings", true);
	JustAMCPSettingsResolver::set_category_default("editor_tools", false);
	ps->set_setting("blazium/justamcp/tools/editor_tools", false);
	ps->set_setting("blazium/justamcp/tools/editor_tools/blazium_editor_play_scene", true);

	CHECK(!JustAMCPSettingsResolver::is_tool_listed("editor_tools", "blazium_editor_play_scene"));
	CHECK(JustAMCPSettingsResolver::is_tool_executable("editor_tools", "blazium_editor_play_scene"));
	CHECK(JustAMCPToolDispatcher::is_tool_enabled("blazium_editor_play_scene", "editor_tools"));

	JustAMCPToolSchemaCache::invalidate();
	const int total_before = JustAMCPToolSchemaCache::get_schemas(false, true, false, true).size();
	const int active_before = JustAMCPToolSchemaCache::get_schemas(false, false, false, false).size();
	CHECK(_schema_has_tool(JustAMCPToolSchemaCache::get_schemas(false, true, false, true), "blazium_editor_play_scene"));
	CHECK(!_schema_has_tool(JustAMCPToolSchemaCache::get_schemas(false, false, false, false), "blazium_editor_play_scene"));

	ps->set_setting("blazium/justamcp/tools/editor_tools", true);
	JustAMCPJsonRpcHelpers::mark_mcp_tool_settings_dirty();
	const int total_after = JustAMCPToolSchemaCache::get_schemas(false, true, false, true).size();
	const int active_after = JustAMCPToolSchemaCache::get_schemas(false, false, false, false).size();
	CHECK(total_after == total_before);
	CHECK(active_after > active_before);
	CHECK(_schema_has_tool(JustAMCPToolSchemaCache::get_schemas(false, false, false, false), "blazium_editor_play_scene"));

	ps->set_setting("blazium/justamcp/tools/editor_tools/blazium_editor_play_scene", false);
	CHECK(!JustAMCPSettingsResolver::is_tool_executable("editor_tools", "blazium_editor_play_scene"));
	CHECK(!JustAMCPSettingsResolver::is_tool_listed("editor_tools", "blazium_editor_play_scene"));

	JustAMCPToolExecutor::register_tool_settings();
#ifdef MODULE_ASSETTAGS_ENABLED
	CHECK(ps->has_setting("blazium/justamcp/tools/asset_tags_tools"));
#endif

	ps->set_setting("blazium/justamcp/prompts", false);
	JustAMCPPromptExecutor prompt_exec;
	const Array listed_prompts = prompt_exec.list_prompts("", false).get("prompts", Array());
	const Array all_prompts = prompt_exec.list_prompts("", true).get("prompts", Array());
	CHECK(listed_prompts.size() == 0);
	CHECK(all_prompts.size() > 0);
	Dictionary prompt_args;
	Dictionary got = prompt_exec.get_prompt("blazium_context", prompt_args);
	CHECK(got.get("ok", true));

	ps->set_setting("blazium/justamcp/override_editor_settings", prev_override);
	if (had_cat) {
		ps->set_setting("blazium/justamcp/tools/editor_tools", prev_cat);
	} else {
		ps->clear("blazium/justamcp/tools/editor_tools");
	}
	if (had_tool) {
		ps->set_setting("blazium/justamcp/tools/editor_tools/blazium_editor_play_scene", prev_tool);
	} else {
		ps->clear("blazium/justamcp/tools/editor_tools/blazium_editor_play_scene");
	}
	if (had_prompts) {
		ps->set_setting("blazium/justamcp/prompts", prev_prompts);
	} else {
		ps->clear("blazium/justamcp/prompts");
	}
	JustAMCPToolSchemaCache::invalidate();
}
#else
void test_justamcp_list_vs_execute_and_active_count() {
	SUCCEED();
}
#endif

#endif
