/**************************************************************************/
/*  justamcp_cli_args.h                                                   */
/**************************************************************************/
/*                         This file is part of:                          */
/*                             BLAZIUM ENGINE                             */
/*                          https://blazium.app                           */
/**************************************************************************/

#pragma once

class JustAMCPCliArgs {
public:
	static int mcp_port();
	static bool enable_mcp();
	static bool enable_mcp_game_control();
	static bool disable_game_mcp();
	static bool is_headless();
	static bool is_unit_test();
	static bool skip_mcp_server();

#ifdef TESTS_ENABLED
	static void set_test_mcp_port(int p_port);
	static void clear_test_overrides();
#endif
};
