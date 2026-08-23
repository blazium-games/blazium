/**************************************************************************/
/*  justamcp_mcp_apps.h                                                   */
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

#pragma once

#ifdef TOOLS_ENABLED

#include "core/object/class_db.h"
#include "core/object/object.h"
#include "core/os/mutex.h"
#include "core/templates/hash_map.h"
#include "core/templates/hash_set.h"

class JustAMCPMCPAppsHost : public Object {
	GDCLASS(JustAMCPMCPAppsHost, Object);

	static JustAMCPMCPAppsHost *singleton;
	mutable Mutex apps_mutex;
	HashMap<String, Dictionary> open_apps;
	HashSet<String> consented_tools;

protected:
	static void _bind_methods();

public:
	static JustAMCPMCPAppsHost *get_singleton();

	static Dictionary detect_ui_meta(const Dictionary &p_tool_or_result);
	static bool permissions_granted(const Dictionary &p_requested, const Dictionary &p_user_grants);
	static String csp_header(const Dictionary &p_csp);
	static String host_page_html(const String &p_app_html, const String &p_csp);
	static String embedded_host_template();
	static String ui_apps_prefix();
	static String bundled_host_uri();
	static bool sanitize_ui_uri(const String &p_uri, String &r_path, String &r_error);
	static Dictionary apps_extension_capability();

	Dictionary open_app(const String &p_bridge_name, const Dictionary &p_ui_meta, const String &p_html);
	Dictionary get_open_app(const String &p_uri) const;
	Array list_open_apps() const;
	String host_url() const;
	Dictionary proxy_tools_call(const String &p_bridge_name, const String &p_tool_name, const Dictionary &p_arguments, bool p_user_consented);
	void grant_tool_consent(const String &p_tool_key);
	bool has_tool_consent(const String &p_tool_key) const;

	JustAMCPMCPAppsHost();
	~JustAMCPMCPAppsHost();
};

#endif
