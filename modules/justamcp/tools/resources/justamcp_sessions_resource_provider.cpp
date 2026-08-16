/**************************************************************************/
/*  justamcp_sessions_resource_provider.cpp                               */
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

#include "justamcp_sessions_resource_provider.h"

#include "../../justamcp_server.h"

#include "core/config/engine.h"
#include "core/config/project_settings.h"
#include "core/io/json.h"

static Dictionary _sessions_json_contents(const String &p_uri, const Dictionary &p_payload) {
	Dictionary result;
	result["ok"] = true;
	Array contents;
	Dictionary content;
	content["uri"] = p_uri;
	content["mimeType"] = "application/json";
	content["text"] = JSON::stringify(p_payload, "\t");
	contents.push_back(content);
	result["contents"] = contents;
	return result;
}

bool JustAMCPSessionsResourceProvider::can_read(const String &p_canonical_uri) {
	return p_canonical_uri == "blazium://sessions";
}

Dictionary JustAMCPSessionsResourceProvider::read(const String &p_uri, const String &p_canonical_uri) {
	(void)p_canonical_uri;
	Dictionary session;
	session["session_id"] = "justamcp-editor";
	session["godot_version"] = Engine::get_singleton()->get_version_info().get("string", "unknown");
	session["project_path"] = ProjectSettings::get_singleton()->get_resource_path();
	session["project_name"] = ProjectSettings::get_singleton()->get_setting("application/config/name", "");
	session["is_active"] = JustAMCPServer::get_singleton() ? JustAMCPServer::get_singleton()->is_server_started() : true;

	Array sessions;
	sessions.push_back(session);
	Dictionary payload;
	payload["count"] = sessions.size();
	payload["sessions"] = sessions;
	return _sessions_json_contents(p_uri, payload);
}

#endif
