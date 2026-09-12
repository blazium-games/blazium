/**************************************************************************/
/*  test_justamcp_runtime_export.h                                        */
/**************************************************************************/
/*                         This file is part of:                          */
/*                             BLAZIUM ENGINE                             */
/*                          https://blazium.app                           */
/**************************************************************************/

#pragma once

#include "tests/test_macros.h"

void test_justamcp_skip_autowork_unless_enable_mcp();
void test_justamcp_instantiate_gates();
void test_justamcp_runtime_port_is_editor_plus_one();
void test_justamcp_register_tool_in_tools_list();
void test_justamcp_runtime_host_list_is_project_only();
void test_justamcp_runtime_host_refuses_editor_port();
void test_justamcp_project_mcp_name_and_dir_validation();
void test_justamcp_runtime_host_call_and_get();
void test_justamcp_invalid_ports_are_rejected();

TEST_CASE("[Modules][JustAMCP] Autowork skips MCP unless --enable-mcp") {
	test_justamcp_skip_autowork_unless_enable_mcp();
}

TEST_CASE("[Modules][JustAMCP] instantiate gates") {
	test_justamcp_instantiate_gates();
}

TEST_CASE("[Modules][JustAMCP] game port is editor plus one") {
	test_justamcp_runtime_port_is_editor_plus_one();
}

TEST_CASE("[Modules][JustAMCP] register_tool appears in tools/list") {
	test_justamcp_register_tool_in_tools_list();
}

TEST_CASE("[Modules][JustAMCP] runtime-host list is project tools only") {
	test_justamcp_runtime_host_list_is_project_only();
}

TEST_CASE("[Modules][JustAMCP] runtime host refuses editor port") {
	test_justamcp_runtime_host_refuses_editor_port();
}

TEST_CASE("[Modules][JustAMCP] project MCP name and dir validation") {
	test_justamcp_project_mcp_name_and_dir_validation();
}

TEST_CASE("[Modules][JustAMCP] runtime-host tools/call and prompts/get") {
	test_justamcp_runtime_host_call_and_get();
}

TEST_CASE("[Modules][JustAMCP] invalid MCP ports are rejected") {
	test_justamcp_invalid_ports_are_rejected();
}
