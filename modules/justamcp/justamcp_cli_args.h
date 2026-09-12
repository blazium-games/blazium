/**************************************************************************/
/*  justamcp_cli_args.h                                                   */
/**************************************************************************/
/*                         This file is part of:                          */
/*                             BLAZIUM ENGINE                             */
/*                          https://blazium.app                           */
/**************************************************************************/

#pragma once

#include "core/string/ustring.h"
#include "core/templates/vector.h"

class JustAMCPCliArgs {
public:
	static bool is_valid_port(int p_port);
	static int mcp_port();
	static int mcp_game_port();
	static bool enable_mcp();
	static bool enable_mcp_game_control();
	static bool disable_game_mcp();
	static bool is_headless();
	static bool is_unit_test();
	static bool skip_mcp_server();
	static bool skip_mcp_server_for_args(const Vector<String> &p_args);
	static bool should_instantiate_editor_server_for(bool p_skip, bool p_server_enabled);
	static bool should_instantiate_runtime_for(bool p_skip, bool p_game_requested);

#ifdef TESTS_ENABLED
	static void set_test_mcp_port(int p_port);
	static void set_test_mcp_game_port(int p_port);
	static void clear_test_overrides();
#endif
};
