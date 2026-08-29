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
#include "core/templates/vector.h"
#include "servers/display_server.h"

#ifdef TESTS_ENABLED
static int _test_mcp_port = -1;
static int _test_mcp_game_port = -1;
#endif

static bool _justamcp_has_arg(const String &p_arg) {
	for (const String &arg : OS::get_singleton()->get_cmdline_args()) {
		if (arg == p_arg) {
			return true;
		}
	}
	return false;
}

bool JustAMCPCliArgs::is_valid_port(int p_port) {
	return p_port >= 1 && p_port <= 65535;
}

int JustAMCPCliArgs::mcp_port() {
#ifdef TESTS_ENABLED
	if (is_valid_port(_test_mcp_port)) {
		return _test_mcp_port;
	}
#endif
	const List<String> &args = OS::get_singleton()->get_cmdline_args();
	for (const List<String>::Element *E = args.front(); E; E = E->next()) {
		if (E->get() == "--mcp-port" && E->next()) {
			const int port = E->next()->get().to_int();
			if (is_valid_port(port)) {
				return port;
			}
		}
	}
	return -1;
}

int JustAMCPCliArgs::mcp_game_port() {
#ifdef TESTS_ENABLED
	if (is_valid_port(_test_mcp_game_port)) {
		return _test_mcp_game_port;
	}
#endif
	const List<String> &args = OS::get_singleton()->get_cmdline_args();
	for (const List<String>::Element *E = args.front(); E; E = E->next()) {
		if (E->get() == "--mcp-game-port" && E->next()) {
			const int port = E->next()->get().to_int();
			if (is_valid_port(port)) {
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

bool JustAMCPCliArgs::skip_mcp_server_for_args(const Vector<String> &p_args) {
	bool skip = false;
	bool enable = false;
	for (int i = 0; i < p_args.size(); i++) {
		const String &arg = p_args[i];
		if (arg == "--test" || arg == "--tests" || arg.begins_with("--aw-") ||
				arg == "--help" || arg == "-h" || arg == "/?" || arg == "--version" ||
				arg == "--check-only" || arg.begins_with("--export")) {
			skip = true;
		}
		if (arg == "--enable-mcp" || arg == "--enable-mcp-game-control") {
			enable = true;
		}
	}
	return skip && !enable;
}

bool JustAMCPCliArgs::skip_mcp_server() {
	Vector<String> args;
	for (const String &arg : OS::get_singleton()->get_cmdline_args()) {
		args.push_back(arg);
	}
	return skip_mcp_server_for_args(args);
}

bool JustAMCPCliArgs::should_instantiate_editor_server_for(bool p_skip, bool p_server_enabled) {
	return !p_skip && p_server_enabled;
}

bool JustAMCPCliArgs::should_instantiate_runtime_for(bool p_skip, bool p_game_requested) {
	return !p_skip && p_game_requested;
}

#ifdef TESTS_ENABLED
void JustAMCPCliArgs::set_test_mcp_port(int p_port) {
	_test_mcp_port = p_port;
}

void JustAMCPCliArgs::set_test_mcp_game_port(int p_port) {
	_test_mcp_game_port = p_port;
}

void JustAMCPCliArgs::clear_test_overrides() {
	_test_mcp_port = -1;
	_test_mcp_game_port = -1;
}
#endif
