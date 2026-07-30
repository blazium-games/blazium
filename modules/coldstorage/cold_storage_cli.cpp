/**************************************************************************/
/*  cold_storage_cli.cpp                                                  */
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

#include "cold_storage_cli.h"

#include "core/os/os.h"

static bool _arg_starts(const String &p_arg, const String &p_prefix, String &r_value) {
	if (!p_arg.begins_with(p_prefix)) {
		return false;
	}
	r_value = p_arg.substr(p_prefix.length());
	return true;
}

ColdStorageCliOverrides cold_storage_parse_cli() {
	ColdStorageCliOverrides o;
	const List<String> &args = OS::get_singleton()->get_cmdline_args();
	for (const String &arg : args) {
		if (arg == "--enable-coldstorage") {
			o.enable = true;
			o.has_enable = true;
		} else if (arg == "--coldstorage-auto-pull") {
			o.auto_pull = true;
			o.has_auto_pull = true;
		} else if (arg == "--coldstorage-no-validate") {
			o.no_validate = true;
		} else if (arg == "--coldstorage-tls") {
			o.tls = true;
			o.has_tls = true;
		} else if (arg == "--coldstorage-tls-insecure") {
			o.tls_insecure = true;
			o.has_tls_insecure = true;
		} else {
			String val;
			if (_arg_starts(arg, "--coldstorage-host=", val)) {
				o.host = val;
				o.has_host = true;
			} else if (_arg_starts(arg, "--coldstorage-port=", val)) {
				o.port = val.to_int();
				o.has_port = true;
			} else if (_arg_starts(arg, "--coldstorage-user=", val)) {
				o.user = val;
				o.has_user = true;
			} else if (_arg_starts(arg, "--coldstorage-password=", val)) {
				o.password = val;
				o.has_password = true;
			} else if (_arg_starts(arg, "--coldstorage-ticket=", val)) {
				o.ticket = val;
				o.has_ticket = true;
			} else if (_arg_starts(arg, "--coldstorage-jwt=", val)) {
				o.jwt = val;
				o.has_jwt = true;
			} else if (_arg_starts(arg, "--coldstorage-workspace=", val)) {
				o.workspace = val;
				o.has_workspace = true;
			} else if (_arg_starts(arg, "--coldstorage-repo=", val)) {
				o.repo = val;
				o.has_repo = true;
			}
		}
	}
	return o;
}

void cold_storage_apply_cli_to_config(ColdStorageConnectionConfig &r_cfg, const ColdStorageCliOverrides &p_cli) {
	if (p_cli.has_enable) {
		r_cfg.enabled = p_cli.enable;
		r_cfg.autoload_on_startup = p_cli.enable;
	}
	if (p_cli.has_auto_pull) {
		r_cfg.auto_pull = p_cli.auto_pull;
	}
	if (p_cli.no_validate) {
		r_cfg.validate_on_startup = false;
	}
	if (p_cli.has_host) {
		r_cfg.host = p_cli.host;
	}
	if (p_cli.has_port) {
		r_cfg.port = p_cli.port;
	}
	if (p_cli.has_user) {
		r_cfg.user = p_cli.user;
	}
	if (p_cli.has_password) {
		r_cfg.password = p_cli.password;
	}
	if (p_cli.has_ticket) {
		r_cfg.ticket = p_cli.ticket;
	}
	if (p_cli.has_jwt) {
		r_cfg.jwt = p_cli.jwt;
	}
	if (p_cli.has_workspace) {
		r_cfg.workspace = p_cli.workspace;
	}
	if (p_cli.has_repo) {
		r_cfg.repo = p_cli.repo;
	}
	if (p_cli.has_tls) {
		r_cfg.use_tls = p_cli.tls;
	}
	if (p_cli.has_tls_insecure) {
		r_cfg.tls_insecure = p_cli.tls_insecure;
	}
}

bool cold_storage_should_autoconnect(const ColdStorageConnectionConfig &p_cfg, const ColdStorageCliOverrides &p_cli) {
	if (p_cli.has_enable && p_cli.enable) {
		return true;
	}
	return p_cfg.enabled && p_cfg.autoload_on_startup;
}

#endif
