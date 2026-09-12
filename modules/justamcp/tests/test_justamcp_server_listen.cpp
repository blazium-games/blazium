/**************************************************************************/
/*  test_justamcp_server_listen.cpp                                       */
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

#include "test_justamcp_server_listen.h"

#ifdef TESTS_ENABLED

#include "../justamcp_cli_args.h"
#include "../justamcp_server.h"
#include "core/config/project_settings.h"
#include "modules/modules_enabled.gen.h"
#include "tests/test_macros.h"

#if defined(MODULE_HTTPSERVER_ENABLED)
#include "modules/httpserver/http_server.h"
#endif

namespace {

void _stop_httpserver_if_listening() {
#if defined(MODULE_HTTPSERVER_ENABLED)
	if (HTTPServer::get_singleton() && HTTPServer::get_singleton()->is_listening()) {
		HTTPServer::get_singleton()->stop();
	}
#endif
}

struct JustAMCPListenSettingsGuard {
	Variant prev_override;
	Variant prev_enabled;
	Variant prev_port;
	Variant prev_bind;

	JustAMCPListenSettingsGuard() {
		ProjectSettings *ps = ProjectSettings::get_singleton();
		ERR_FAIL_NULL(ps);
		prev_override = ps->get_setting("blazium/justamcp/override_editor_settings");
		prev_enabled = ps->get_setting("blazium/justamcp/server_enabled");
		prev_port = ps->get_setting("blazium/justamcp/server_port");
		prev_bind = ps->get_setting("blazium/justamcp/bind_to_localhost_only");
	}

	~JustAMCPListenSettingsGuard() {
		ProjectSettings *ps = ProjectSettings::get_singleton();
		if (!ps) {
			return;
		}
		ps->set_setting("blazium/justamcp/override_editor_settings", prev_override);
		ps->set_setting("blazium/justamcp/server_enabled", prev_enabled);
		ps->set_setting("blazium/justamcp/server_port", prev_port);
		ps->set_setting("blazium/justamcp/bind_to_localhost_only", prev_bind);
	}

	void apply(bool p_enabled, int p_port) {
		ProjectSettings *ps = ProjectSettings::get_singleton();
		ERR_FAIL_NULL(ps);
		ps->set_setting("blazium/justamcp/override_editor_settings", true);
		ps->set_setting("blazium/justamcp/server_enabled", p_enabled);
		ps->set_setting("blazium/justamcp/server_port", p_port);
		ps->set_setting("blazium/justamcp/bind_to_localhost_only", true);
	}
};

} // namespace

void test_justamcp_server_start_listens() {
#if !defined(MODULE_HTTPSERVER_ENABLED)
	return;
#else
	REQUIRE(HTTPServer::get_singleton());
	_stop_httpserver_if_listening();

	JustAMCPListenSettingsGuard settings;
	const int port = 16506;
	settings.apply(true, port);

	JustAMCPServer server;
	server.test_start_server();
	CHECK(server.is_server_started());
	CHECK(server.get_listening_port() == port);
	CHECK(HTTPServer::get_singleton()->is_listening());

	server.test_stop_server();
	CHECK(!server.is_server_started());
	_stop_httpserver_if_listening();
#endif
}

void test_justamcp_server_failed_listen_does_not_activate() {
#if !defined(MODULE_HTTPSERVER_ENABLED)
	return;
#else
	REQUIRE(HTTPServer::get_singleton());
	_stop_httpserver_if_listening();

	JustAMCPListenSettingsGuard settings;
	const int port = 16507;
	settings.apply(true, port);

	JustAMCPServer server;
	server.test_set_forced_listen_error(ERR_CANT_CREATE);
	server.test_start_server();
	CHECK(!server.is_server_started());
	CHECK(server.get_listening_port() == -1);
	CHECK(!HTTPServer::get_singleton()->is_listening());
#endif
}

void test_justamcp_server_cli_port_wins_over_settings() {
#if !defined(MODULE_HTTPSERVER_ENABLED)
	return;
#else
	REQUIRE(HTTPServer::get_singleton());
	_stop_httpserver_if_listening();

	JustAMCPListenSettingsGuard settings;
	settings.apply(true, 16506);
	JustAMCPCliArgs::set_test_mcp_port(16516);

	JustAMCPServer server;
	server.test_start_server();
	CHECK(server.is_server_started());
	CHECK(server.get_listening_port() == 16516);
	CHECK(HTTPServer::get_singleton()->is_listening());

	server.test_stop_server();
	JustAMCPCliArgs::clear_test_overrides();
	_stop_httpserver_if_listening();
#endif
}

#endif
