/**************************************************************************/
/*  cold_storage_settings.cpp                                             */
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

#include "cold_storage_settings.h"

#include "core/config/project_settings.h"
#include "editor/editor_settings.h"

namespace {
const char *NS = "blazium/coldstorage/";

bool _is_secret_key(const String &p_key) {
	return p_key.ends_with("/password") || p_key.ends_with("/ticket") || p_key.ends_with("/jwt");
}

void _clear_legacy_project_secrets() {
	if (!ProjectSettings::get_singleton()) {
		return;
	}
	bool cleared = false;
	const String keys[] = {
		String(NS) + "password",
		String(NS) + "ticket",
		String(NS) + "jwt",
	};
	for (const String &key : keys) {
		if (ProjectSettings::get_singleton()->has_setting(key)) {
			ProjectSettings::get_singleton()->clear(key);
			cleared = true;
		}
	}
	if (cleared) {
		ProjectSettings::get_singleton()->save();
	}
}

Variant _get_secret_setting(const String &p_key, const Variant &p_default) {
	if (EditorSettings::get_singleton() && EditorSettings::get_singleton()->has_setting(p_key)) {
		return EditorSettings::get_singleton()->get(p_key);
	}
	return p_default;
}
} //namespace

void cold_storage_register_project_settings() {
	GLOBAL_DEF_BASIC(String(NS) + "override_editor_settings", false);
	GLOBAL_DEF_BASIC(String(NS) + "enabled", false);
	GLOBAL_DEF_BASIC(String(NS) + "autoload_on_startup", false);
	GLOBAL_DEF_BASIC(String(NS) + "validate_on_startup", true);
	GLOBAL_DEF_BASIC(String(NS) + "auto_pull", false);
	GLOBAL_DEF_BASIC(String(NS) + "host", "127.0.0.1");
	GLOBAL_DEF_BASIC(String(NS) + "port", 1666);
	GLOBAL_DEF_BASIC(String(NS) + "use_tls", false);
	GLOBAL_DEF_BASIC(String(NS) + "tls_insecure", false);
	GLOBAL_DEF_BASIC(String(NS) + "user", String());
	// Secrets (password/ticket/jwt) are EditorSettings-only — never project.godot.
	GLOBAL_DEF_BASIC(String(NS) + "workspace", "default");
	GLOBAL_DEF_BASIC(String(NS) + "repo", "default");
	GLOBAL_DEF_BASIC(String(NS) + "workspace_root", String());
	GLOBAL_DEF_BASIC(String(NS) + "ca_file", String());
	GLOBAL_DEF_BASIC(String(NS) + "z_connection_controls", String());
	_clear_legacy_project_secrets();
}

void cold_storage_register_editor_settings() {
	if (!EditorSettings::get_singleton()) {
		return;
	}
	EDITOR_DEF_BASIC(String(NS) + "enabled", false);
	EDITOR_DEF_BASIC(String(NS) + "autoload_on_startup", false);
	EDITOR_DEF_BASIC(String(NS) + "validate_on_startup", true);
	EDITOR_DEF_BASIC(String(NS) + "auto_pull", false);
	EDITOR_DEF_BASIC(String(NS) + "host", "127.0.0.1");
	EDITOR_DEF_BASIC(String(NS) + "port", 1666);
	EDITOR_DEF_BASIC(String(NS) + "use_tls", false);
	EDITOR_DEF_BASIC(String(NS) + "tls_insecure", false);
	EDITOR_DEF_BASIC(String(NS) + "user", "");
	EDITOR_DEF_BASIC(String(NS) + "password", "");
	EDITOR_DEF_BASIC(String(NS) + "ticket", "");
	EDITOR_DEF_BASIC(String(NS) + "jwt", "");
	EDITOR_DEF_BASIC(String(NS) + "workspace", "default");
	EDITOR_DEF_BASIC(String(NS) + "repo", "default");
	EDITOR_DEF_BASIC(String(NS) + "workspace_root", "");
	EDITOR_DEF_BASIC(String(NS) + "ca_file", "");
	EDITOR_DEF_BASIC(String(NS) + "z_connection_controls", "");
}

Variant cold_storage_get_setting(const String &p_key, const Variant &p_default) {
	if (_is_secret_key(p_key)) {
		return _get_secret_setting(p_key, p_default);
	}
	const bool use_project = ProjectSettings::get_singleton() &&
			(bool)ProjectSettings::get_singleton()->get_setting(String(NS) + "override_editor_settings", false);
	if (use_project || !EditorSettings::get_singleton()) {
		if (ProjectSettings::get_singleton() && ProjectSettings::get_singleton()->has_setting(p_key)) {
			return ProjectSettings::get_singleton()->get_setting(p_key);
		}
		return p_default;
	}
	if (EditorSettings::get_singleton()->has_setting(p_key)) {
		return EditorSettings::get_singleton()->get(p_key);
	}
	return p_default;
}

ColdStorageConnectionConfig cold_storage_load_config() {
	_clear_legacy_project_secrets();
	ColdStorageConnectionConfig c;
	c.enabled = cold_storage_get_setting(String(NS) + "enabled", false);
	c.autoload_on_startup = cold_storage_get_setting(String(NS) + "autoload_on_startup", false);
	c.validate_on_startup = cold_storage_get_setting(String(NS) + "validate_on_startup", true);
	c.auto_pull = cold_storage_get_setting(String(NS) + "auto_pull", false);
	c.host = cold_storage_get_setting(String(NS) + "host", "127.0.0.1");
	c.port = cold_storage_get_setting(String(NS) + "port", 1666);
	c.use_tls = cold_storage_get_setting(String(NS) + "use_tls", false);
	c.tls_insecure = cold_storage_get_setting(String(NS) + "tls_insecure", false);
	c.user = cold_storage_get_setting(String(NS) + "user", String());
	c.password = cold_storage_get_setting(String(NS) + "password", String());
	c.ticket = cold_storage_get_setting(String(NS) + "ticket", String());
	c.jwt = cold_storage_get_setting(String(NS) + "jwt", String());
	c.workspace = cold_storage_get_setting(String(NS) + "workspace", "default");
	c.repo = cold_storage_get_setting(String(NS) + "repo", "default");
	c.workspace_root = cold_storage_get_setting(String(NS) + "workspace_root", String());
	c.ca_file = cold_storage_get_setting(String(NS) + "ca_file", String());
	return c;
}

void cold_storage_save_config(const ColdStorageConnectionConfig &p_cfg) {
	auto set_public = [&](const String &key, const Variant &val) {
		if (ProjectSettings::get_singleton()) {
			ProjectSettings::get_singleton()->set_setting(key, val);
		}
		if (EditorSettings::get_singleton()) {
			EditorSettings::get_singleton()->set(key, val);
		}
	};
	auto set_secret = [&](const String &key, const Variant &val) {
		if (EditorSettings::get_singleton()) {
			EditorSettings::get_singleton()->set(key, val);
		}
	};

	set_public(String(NS) + "enabled", p_cfg.enabled);
	set_public(String(NS) + "autoload_on_startup", p_cfg.autoload_on_startup);
	set_public(String(NS) + "validate_on_startup", p_cfg.validate_on_startup);
	set_public(String(NS) + "auto_pull", p_cfg.auto_pull);
	set_public(String(NS) + "host", p_cfg.host);
	set_public(String(NS) + "port", p_cfg.port);
	set_public(String(NS) + "use_tls", p_cfg.use_tls);
	set_public(String(NS) + "tls_insecure", p_cfg.tls_insecure);
	set_public(String(NS) + "user", p_cfg.user);
	set_secret(String(NS) + "password", p_cfg.password);
	set_secret(String(NS) + "ticket", p_cfg.ticket);
	set_secret(String(NS) + "jwt", p_cfg.jwt);
	set_public(String(NS) + "workspace", p_cfg.workspace);
	set_public(String(NS) + "repo", p_cfg.repo);
	set_public(String(NS) + "workspace_root", p_cfg.workspace_root);
	set_public(String(NS) + "ca_file", p_cfg.ca_file);

	_clear_legacy_project_secrets();

	if (EditorSettings::get_singleton()) {
		EditorSettings::get_singleton()->save();
	}
}

#endif
