/**************************************************************************/
/*  justamcp_project_settings.cpp                                         */
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

#include "justamcp_project_settings.h"

#include "tools/justamcp_prompt_executor.h"
#include "tools/justamcp_resource_executor.h"
#include "tools/justamcp_tool_executor.h"

#include "core/config/project_settings.h"
#include "editor/editor_settings.h"

void JustAMCPProjectSettings::register_project_settings() {
	GLOBAL_DEF_BASIC("blazium/justamcp/override_editor_settings", false);
	GLOBAL_DEF_BASIC("blazium/justamcp/server_enabled", false);
	GLOBAL_DEF_BASIC("blazium/justamcp/server_port", 6506);
	GLOBAL_DEF_BASIC("blazium/justamcp/protocol_version", "2026-07-28");
	if (ProjectSettings::get_singleton()) {
		ProjectSettings::get_singleton()->set_custom_property_info(PropertyInfo(Variant::STRING, "blazium/justamcp/protocol_version", PROPERTY_HINT_ENUM, "2026-07-28,2025-11-25,2025-06-18,2025-03-26,2024-11-05"));
	}
	GLOBAL_DEF_BASIC("blazium/justamcp/accepted_protocol_versions", String());
	GLOBAL_DEF_BASIC("blazium/justamcp/oauth_enabled", false);
	GLOBAL_DEF_BASIC("blazium/justamcp/client_id", String());
	GLOBAL_DEF_BASIC("blazium/justamcp/client_secret", String());
	GLOBAL_DEF_BASIC("blazium/justamcp/oauth_authorization_servers", String());
	GLOBAL_DEF_BASIC("blazium/justamcp/oauth_scopes_supported", "mcp");
	GLOBAL_DEF_BASIC("blazium/justamcp/oauth_resource", String());
	GLOBAL_DEF_BASIC("blazium/justamcp/oauth_cimd_json", String());
	GLOBAL_DEF_BASIC("blazium/justamcp/url_elicitation_demo_url", String());
	GLOBAL_DEF_BASIC("blazium/justamcp/z_mcp_config", String());
	GLOBAL_DEF_BASIC("blazium/justamcp/enable_debug_logging", false);
	GLOBAL_DEF_BASIC("blazium/justamcp/forward_engine_logs", true);
	GLOBAL_DEF_BASIC("blazium/justamcp/list_page_size", 50);
	GLOBAL_DEF_BASIC("blazium/justamcp/mcp_log_buffer_size", 500);
	GLOBAL_DEF_BASIC("blazium/justamcp/task_default_ttl_ms", 600000);
	GLOBAL_DEF_BASIC("blazium/justamcp/task_poll_interval_ms", 1000);
	GLOBAL_DEF_BASIC("blazium/justamcp/task_max_concurrent", 16);
	GLOBAL_DEF_BASIC("blazium/justamcp/bind_to_localhost_only", true);
	GLOBAL_DEF_BASIC("blazium/justamcp/session_ttl_seconds", 3600);
	GLOBAL_DEF_BASIC("blazium/justamcp/session_allow_client_delete", true);
	GLOBAL_DEF_BASIC("blazium/justamcp/streamable_http_strict_origin", true);
	GLOBAL_DEF_BASIC("blazium/justamcp/max_enqueue_per_sec_per_session", 10);
	GLOBAL_DEF_BASIC("blazium/justamcp/streamable_http_allowed_origin", String());
	GLOBAL_DEF_BASIC("blazium/justamcp/enable_toolset_discovery", true);
	GLOBAL_DEF_BASIC("blazium/justamcp/allow_execute_tool_bypass", false);
	GLOBAL_DEF_BASIC("blazium/justamcp/stateless_tool_timeout_ms", 120000);
	GLOBAL_DEF_BASIC("blazium/justamcp/task_result_max_wait_ms", 120000);
	GLOBAL_DEF_BASIC("blazium/justamcp/request_route_ttl_sec", 3600);
	GLOBAL_DEF_BASIC("blazium/justamcp/pending_post_sse_timeout_sec", 120);
	GLOBAL_DEF_BASIC("blazium/justamcp/max_pending_per_session", 8);
	GLOBAL_DEF_BASIC("blazium/justamcp/stateless_tool_blocking", false);
	GLOBAL_DEF_BASIC("blazium/justamcp/parallel_readonly_lane", false);
	GLOBAL_DEF_BASIC("blazium/justamcp/readonly_worker_concurrency", 2);
	GLOBAL_DEF_BASIC("blazium/justamcp/mcp_clients", Array());
	GLOBAL_DEF_BASIC("blazium/justamcp/bridge_url_allow_hosts", Array());
	GLOBAL_DEF_BASIC("blazium/justamcp/in_flight_cancel_deadline_ms", 5000);

	JustAMCPToolExecutor::register_tool_settings();
	JustAMCPPromptExecutor::register_settings();
	JustAMCPResourceExecutor::register_settings();
}

void JustAMCPProjectSettings::register_editor_settings() {
	if (!EditorSettings::get_singleton()) {
		return;
	}
	EDITOR_DEF_BASIC("blazium/justamcp/server_enabled", false);
	EditorSettings::get_singleton()->add_property_hint(PropertyInfo(Variant::BOOL, "blazium/justamcp/server_enabled"));

	EDITOR_DEF_BASIC("blazium/justamcp/server_port", 6506);
	EditorSettings::get_singleton()->add_property_hint(PropertyInfo(Variant::INT, "blazium/justamcp/server_port"));

	EDITOR_DEF_BASIC("blazium/justamcp/protocol_version", "2026-07-28");
	EditorSettings::get_singleton()->add_property_hint(PropertyInfo(Variant::STRING, "blazium/justamcp/protocol_version", PROPERTY_HINT_ENUM, "2026-07-28,2025-11-25,2025-06-18,2025-03-26,2024-11-05"));

	EDITOR_DEF_BASIC("blazium/justamcp/accepted_protocol_versions", "");
	EditorSettings::get_singleton()->add_property_hint(PropertyInfo(Variant::STRING, "blazium/justamcp/accepted_protocol_versions"));

	EDITOR_DEF_BASIC("blazium/justamcp/oauth_enabled", false);
	EditorSettings::get_singleton()->add_property_hint(PropertyInfo(Variant::BOOL, "blazium/justamcp/oauth_enabled"));

	EDITOR_DEF_BASIC("blazium/justamcp/client_id", "");
	EditorSettings::get_singleton()->add_property_hint(PropertyInfo(Variant::STRING, "blazium/justamcp/client_id"));

	EDITOR_DEF_BASIC("blazium/justamcp/client_secret", "");
	EditorSettings::get_singleton()->add_property_hint(PropertyInfo(Variant::STRING, "blazium/justamcp/client_secret"));

	EDITOR_DEF_BASIC("blazium/justamcp/oauth_authorization_servers", "");
	EditorSettings::get_singleton()->add_property_hint(PropertyInfo(Variant::STRING, "blazium/justamcp/oauth_authorization_servers"));

	EDITOR_DEF_BASIC("blazium/justamcp/oauth_scopes_supported", "mcp");
	EditorSettings::get_singleton()->add_property_hint(PropertyInfo(Variant::STRING, "blazium/justamcp/oauth_scopes_supported"));

	EDITOR_DEF_BASIC("blazium/justamcp/oauth_resource", "");
	EditorSettings::get_singleton()->add_property_hint(PropertyInfo(Variant::STRING, "blazium/justamcp/oauth_resource"));

	EDITOR_DEF_BASIC("blazium/justamcp/oauth_cimd_json", "");
	EditorSettings::get_singleton()->add_property_hint(PropertyInfo(Variant::STRING, "blazium/justamcp/oauth_cimd_json"));

	EDITOR_DEF_BASIC("blazium/justamcp/url_elicitation_demo_url", "");
	EditorSettings::get_singleton()->add_property_hint(PropertyInfo(Variant::STRING, "blazium/justamcp/url_elicitation_demo_url"));

	EDITOR_DEF_BASIC("blazium/justamcp/z_mcp_config", "");
	EditorSettings::get_singleton()->add_property_hint(PropertyInfo(Variant::STRING, "blazium/justamcp/z_mcp_config"));

	EDITOR_DEF_BASIC("blazium/justamcp/enable_debug_logging", false);
	EditorSettings::get_singleton()->add_property_hint(PropertyInfo(Variant::BOOL, "blazium/justamcp/enable_debug_logging"));

	EDITOR_DEF_BASIC("blazium/justamcp/forward_engine_logs", true);
	EditorSettings::get_singleton()->add_property_hint(PropertyInfo(Variant::BOOL, "blazium/justamcp/forward_engine_logs"));

	EDITOR_DEF_BASIC("blazium/justamcp/list_page_size", 50);
	EditorSettings::get_singleton()->add_property_hint(PropertyInfo(Variant::INT, "blazium/justamcp/list_page_size", PROPERTY_HINT_RANGE, "1,500,1"));

	EDITOR_DEF_BASIC("blazium/justamcp/mcp_log_buffer_size", 500);
	EditorSettings::get_singleton()->add_property_hint(PropertyInfo(Variant::INT, "blazium/justamcp/mcp_log_buffer_size", PROPERTY_HINT_RANGE, "1,5000,1"));

	EDITOR_DEF_BASIC("blazium/justamcp/task_default_ttl_ms", 600000);
	EditorSettings::get_singleton()->add_property_hint(PropertyInfo(Variant::INT, "blazium/justamcp/task_default_ttl_ms", PROPERTY_HINT_RANGE, "1000,3600000,1000"));

	EDITOR_DEF_BASIC("blazium/justamcp/task_poll_interval_ms", 1000);
	EditorSettings::get_singleton()->add_property_hint(PropertyInfo(Variant::INT, "blazium/justamcp/task_poll_interval_ms", PROPERTY_HINT_RANGE, "100,60000,100"));

	EDITOR_DEF_BASIC("blazium/justamcp/task_max_concurrent", 16);
	EditorSettings::get_singleton()->add_property_hint(PropertyInfo(Variant::INT, "blazium/justamcp/task_max_concurrent", PROPERTY_HINT_RANGE, "1,128,1"));

	EDITOR_DEF_BASIC("blazium/justamcp/bind_to_localhost_only", true);
	EditorSettings::get_singleton()->add_property_hint(PropertyInfo(Variant::BOOL, "blazium/justamcp/bind_to_localhost_only"));

	EDITOR_DEF_BASIC("blazium/justamcp/session_ttl_seconds", 3600);
	EditorSettings::get_singleton()->add_property_hint(PropertyInfo(Variant::INT, "blazium/justamcp/session_ttl_seconds", PROPERTY_HINT_RANGE, "0,86400,1"));

	EDITOR_DEF_BASIC("blazium/justamcp/session_allow_client_delete", true);
	EditorSettings::get_singleton()->add_property_hint(PropertyInfo(Variant::BOOL, "blazium/justamcp/session_allow_client_delete"));

	EDITOR_DEF_BASIC("blazium/justamcp/streamable_http_strict_origin", true);
	EditorSettings::get_singleton()->add_property_hint(PropertyInfo(Variant::BOOL, "blazium/justamcp/streamable_http_strict_origin"));

	EDITOR_DEF_BASIC("blazium/justamcp/max_enqueue_per_sec_per_session", 10);
	EditorSettings::get_singleton()->add_property_hint(PropertyInfo(Variant::INT, "blazium/justamcp/max_enqueue_per_sec_per_session", PROPERTY_HINT_RANGE, "0,1000,1"));

	EDITOR_DEF_BASIC("blazium/justamcp/streamable_http_allowed_origin", "");
	EditorSettings::get_singleton()->add_property_hint(PropertyInfo(Variant::STRING, "blazium/justamcp/streamable_http_allowed_origin"));

	EDITOR_DEF_BASIC("blazium/justamcp/enable_toolset_discovery", true);
	EditorSettings::get_singleton()->add_property_hint(PropertyInfo(Variant::BOOL, "blazium/justamcp/enable_toolset_discovery"));

	EDITOR_DEF_BASIC("blazium/justamcp/allow_execute_tool_bypass", false);
	EditorSettings::get_singleton()->add_property_hint(PropertyInfo(Variant::BOOL, "blazium/justamcp/allow_execute_tool_bypass"));

	EDITOR_DEF_BASIC("blazium/justamcp/stateless_tool_timeout_ms", 120000);
	EditorSettings::get_singleton()->add_property_hint(PropertyInfo(Variant::INT, "blazium/justamcp/stateless_tool_timeout_ms", PROPERTY_HINT_RANGE, "1000,600000,1000"));

	EDITOR_DEF_BASIC("blazium/justamcp/task_result_max_wait_ms", 120000);
	EditorSettings::get_singleton()->add_property_hint(PropertyInfo(Variant::INT, "blazium/justamcp/task_result_max_wait_ms", PROPERTY_HINT_RANGE, "1000,600000,1000"));

	EDITOR_DEF_BASIC("blazium/justamcp/request_route_ttl_sec", 3600);
	EditorSettings::get_singleton()->add_property_hint(PropertyInfo(Variant::INT, "blazium/justamcp/request_route_ttl_sec", PROPERTY_HINT_RANGE, "0,86400,1"));

	EDITOR_DEF_BASIC("blazium/justamcp/pending_post_sse_timeout_sec", 120);
	EditorSettings::get_singleton()->add_property_hint(PropertyInfo(Variant::INT, "blazium/justamcp/pending_post_sse_timeout_sec", PROPERTY_HINT_RANGE, "1,3600,1"));

	EDITOR_DEF_BASIC("blazium/justamcp/max_pending_per_session", 8);
	EditorSettings::get_singleton()->add_property_hint(PropertyInfo(Variant::INT, "blazium/justamcp/max_pending_per_session", PROPERTY_HINT_RANGE, "0,256,1"));

	EDITOR_DEF_BASIC("blazium/justamcp/stateless_tool_blocking", false);
	EditorSettings::get_singleton()->add_property_hint(PropertyInfo(Variant::BOOL, "blazium/justamcp/stateless_tool_blocking"));

	EDITOR_DEF_BASIC("blazium/justamcp/parallel_readonly_lane", false);
	EditorSettings::get_singleton()->add_property_hint(PropertyInfo(Variant::BOOL, "blazium/justamcp/parallel_readonly_lane", PROPERTY_HINT_NONE, "Deprecated: use readonly_worker_concurrency."));

	EDITOR_DEF_BASIC("blazium/justamcp/readonly_worker_concurrency", 2);
	EditorSettings::get_singleton()->add_property_hint(PropertyInfo(Variant::INT, "blazium/justamcp/readonly_worker_concurrency", PROPERTY_HINT_RANGE, "0,16,1"));

	EDITOR_DEF_BASIC("blazium/justamcp/mcp_clients", Array());
	EditorSettings::get_singleton()->add_property_hint(PropertyInfo(Variant::ARRAY, "blazium/justamcp/mcp_clients"));
	EDITOR_DEF_BASIC("blazium/justamcp/bridge_url_allow_hosts", Array());
	EditorSettings::get_singleton()->add_property_hint(PropertyInfo(Variant::ARRAY, "blazium/justamcp/bridge_url_allow_hosts"));

	EDITOR_DEF_BASIC("blazium/justamcp/in_flight_cancel_deadline_ms", 5000);
	EditorSettings::get_singleton()->add_property_hint(PropertyInfo(Variant::INT, "blazium/justamcp/in_flight_cancel_deadline_ms", PROPERTY_HINT_RANGE, "0,120000,100"));

	JustAMCPToolExecutor::register_tool_settings();
	JustAMCPPromptExecutor::register_settings();
	JustAMCPResourceExecutor::register_settings();
}

#endif
