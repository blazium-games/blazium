/**************************************************************************/
/*  justamcp_cli_args.cpp                                                 */
/**************************************************************************/
/*                         This file is part of:                          */
/*                             BLAZIUM ENGINE                             */
/*                          https://blazium.app                           */
/**************************************************************************/

#include "justamcp_cli_args.h"

#include "core/os/os.h"
#include "core/string/ustring.h"
#include "core/templates/list.h"
#include "servers/display_server.h"

#ifdef TESTS_ENABLED
static int _test_mcp_port = -1;
#endif

static bool _justamcp_has_arg(const String &p_arg) {
	for (const String &arg : OS::get_singleton()->get_cmdline_args()) {
		if (arg == p_arg) {
			return true;
		}
	}
	return false;
}

int JustAMCPCliArgs::mcp_port() {
#ifdef TESTS_ENABLED
	if (_test_mcp_port > 0) {
		return _test_mcp_port;
	}
#endif
	const List<String> &args = OS::get_singleton()->get_cmdline_args();
	for (const List<String>::Element *E = args.front(); E; E = E->next()) {
		if (E->get() == "--mcp-port" && E->next()) {
			const int port = E->next()->get().to_int();
			if (port > 0) {
				return port;
			}
		}
	}
	return -1;
}

bool JustAMCPCliArgs::enable_mcp() {
	return _justamcp_has_arg("--enable-mcp");
}

bool JustAMCPCliArgs::enable_mcp_game_control() {
	return _justamcp_has_arg("--enable-mcp-game-control");
}

bool JustAMCPCliArgs::disable_game_mcp() {
	return _justamcp_has_arg("--disable-game-mcp");
}

bool JustAMCPCliArgs::is_headless() {
	if (DisplayServer::get_singleton() != nullptr) {
		return DisplayServer::get_singleton()->get_name() == "headless";
	}
	return _justamcp_has_arg("--headless");
}

bool JustAMCPCliArgs::is_unit_test() {
	return _justamcp_has_arg("--test") || _justamcp_has_arg("--tests");
}

bool JustAMCPCliArgs::skip_mcp_server() {
	for (const String &arg : OS::get_singleton()->get_cmdline_args()) {
		if (arg == "--test" || arg == "--tests" || arg.begins_with("--aw-") ||
				arg == "--help" || arg == "-h" || arg == "/?" || arg == "--version" ||
				arg == "--check-only" || arg.begins_with("--export")) {
			return true;
		}
	}
	return false;
}

#ifdef TESTS_ENABLED
void JustAMCPCliArgs::set_test_mcp_port(int p_port) {
	_test_mcp_port = p_port;
}

void JustAMCPCliArgs::clear_test_overrides() {
	_test_mcp_port = -1;
}
#endif
