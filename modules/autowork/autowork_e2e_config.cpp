/**************************************************************************/
/*  autowork_e2e_config.cpp                                               */
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

#include "autowork_e2e_config.h"

#include "core/config/project_settings.h"
#include "core/os/os.h"

#ifdef TOOLS_ENABLED
#include "editor/editor_settings.h"
#endif

bool AutoworkE2EConfig::parsed = false;
bool AutoworkE2EConfig::enabled = false;
String AutoworkE2EConfig::host = "127.0.0.1";
int AutoworkE2EConfig::port = AutoworkE2EConfig::DEFAULT_PORT;
String AutoworkE2EConfig::token = "";
bool AutoworkE2EConfig::logging = false;
String AutoworkE2EConfig::port_file = "";

void AutoworkE2EConfig::_bind_methods() {
	ClassDB::bind_static_method("AutoworkE2EConfig", D_METHOD("is_enabled"), &AutoworkE2EConfig::is_enabled);
	ClassDB::bind_static_method("AutoworkE2EConfig", D_METHOD("get_host"), &AutoworkE2EConfig::get_host);
	ClassDB::bind_static_method("AutoworkE2EConfig", D_METHOD("get_port"), &AutoworkE2EConfig::get_port);
	ClassDB::bind_static_method("AutoworkE2EConfig", D_METHOD("get_token"), &AutoworkE2EConfig::get_token);
	ClassDB::bind_static_method("AutoworkE2EConfig", D_METHOD("get_port_file"), &AutoworkE2EConfig::get_port_file);
	ClassDB::bind_static_method("AutoworkE2EConfig", D_METHOD("is_logging"), &AutoworkE2EConfig::is_logging);
}

void AutoworkE2EConfig::ensure_parsed() {
	if (parsed) {
		return;
	}
	parsed = true;

	List<String> args = OS::get_singleton()->get_cmdline_user_args();
	for (const String &arg : args) {
		if (arg == "--aw-e2e") {
			enabled = true;
		} else if (arg == "--aw-e2e-log") {
			logging = true;
		} else if (arg.begins_with("--aw-e2e-port=")) {
			String value = arg.substr(String("--aw-e2e-port=").length());
			if (value.is_valid_int()) {
				port = value.to_int();
			} else {
				WARN_PRINT("blazium-e2e: invalid port value '" + value + "', using default " + itos(DEFAULT_PORT));
				port = DEFAULT_PORT;
			}
		} else if (arg.begins_with("--aw-e2e-host=")) {
			host = arg.substr(String("--aw-e2e-host=").length());
		} else if (arg.begins_with("--aw-e2e-token=")) {
			token = arg.substr(String("--aw-e2e-token=").length());
		} else if (arg.begins_with("--aw-e2e-port-file=")) {
			port_file = arg.substr(String("--aw-e2e-port-file=").length());
		}
	}

	if (!enabled) {
		if (ProjectSettings::get_singleton() && ProjectSettings::get_singleton()->has_setting("blazium/autowork/e2e_enabled")) {
			enabled = ProjectSettings::get_singleton()->get("blazium/autowork/e2e_enabled");
		}
#ifdef TOOLS_ENABLED
		if (!enabled && EditorSettings::get_singleton() && EditorSettings::get_singleton()->has_setting("blazium/autowork/e2e_enabled")) {
			enabled = EditorSettings::get_singleton()->get("blazium/autowork/e2e_enabled");
		}
#endif
	}
}

bool AutoworkE2EConfig::is_enabled() {
	ensure_parsed();
	return enabled;
}

String AutoworkE2EConfig::get_host() {
	ensure_parsed();
	return host;
}

int AutoworkE2EConfig::get_port() {
	ensure_parsed();
	return port;
}

String AutoworkE2EConfig::get_token() {
	ensure_parsed();
	return token;
}

String AutoworkE2EConfig::get_port_file() {
	ensure_parsed();
	return port_file;
}

bool AutoworkE2EConfig::is_logging() {
	ensure_parsed();
	return logging;
}
