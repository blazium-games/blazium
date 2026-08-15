/**************************************************************************/
/*  justamcp_resource_executor.cpp                                        */
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

#include "core/object/class_db.h"
#include "core/object/callable_mp.h"
#include "justamcp_resource_executor.h"
#include "../justamcp_pagination.h"
#include "../justamcp_read_limits.h"
#include "core/config/project_settings.h"
#include "core/io/file_access.h"
#include "core/io/json.h"
#include "core/os/os.h"
#include "core/os/thread.h"
#include "editor/file_system/editor_file_system.h"
#include "editor/settings/editor_settings.h"
#include "justamcp_resource_manifest.h"
#include "resources/justamcp_blazium_resource_registry.h"
#include "resources/justamcp_materials_resource_provider.h"
#include "resources/justamcp_resource_autowork_results.h"
#include "resources/justamcp_resource_project_file.h"
#include "resources/justamcp_resource_video_recordings.h"

void JustAMCPResourceExecutor::_bind_methods() {
	ClassDB::bind_method(D_METHOD("list_resources", "cursor"), &JustAMCPResourceExecutor::list_resources, DEFVAL(""));
	ClassDB::bind_method(D_METHOD("list_resource_templates", "cursor"), &JustAMCPResourceExecutor::list_resource_templates, DEFVAL(""));
	ClassDB::bind_method(D_METHOD("read_resource", "uri"), &JustAMCPResourceExecutor::read_resource);
	ClassDB::bind_method(D_METHOD("add_resource", "resource"), &JustAMCPResourceExecutor::add_resource);
}

void JustAMCPResourceExecutor::register_settings() {
	JustAMCPResourceExecutor exec;

	Dictionary resources_dict = exec.list_resources();
	if (resources_dict.has("resources")) {
		Array resources = resources_dict["resources"];
		for (int i = 0; i < resources.size(); i++) {
			Dictionary res = resources[i];
			String name = res["name"];
			String desc = res["description"];
			String path = "blazium/justamcp/resources/" + name;

			GLOBAL_DEF_NOVAL_BASIC(PropertyInfo(Variant::STRING, path, PROPERTY_HINT_MULTILINE_TEXT, desc, PROPERTY_USAGE_DEFAULT | PROPERTY_USAGE_READ_ONLY), String());
			if (EditorSettings::get_singleton()) {
				EDITOR_DEF_BASIC(path, String());
				EditorSettings::get_singleton()->add_property_hint(PropertyInfo(Variant::STRING, path, PROPERTY_HINT_MULTILINE_TEXT, desc, PROPERTY_USAGE_DEFAULT | PROPERTY_USAGE_READ_ONLY));
			}
		}
	}

	Dictionary templates_dict = exec.list_resource_templates();
	if (templates_dict.has("resourceTemplates")) {
		Array templates = templates_dict["resourceTemplates"];
		for (int i = 0; i < templates.size(); i++) {
			Dictionary templ = templates[i];
			String name = templ["name"];
			String desc = templ["description"];
			String path = "blazium/justamcp/resources/" + name;

			GLOBAL_DEF_NOVAL_BASIC(PropertyInfo(Variant::STRING, path, PROPERTY_HINT_MULTILINE_TEXT, desc, PROPERTY_USAGE_DEFAULT | PROPERTY_USAGE_READ_ONLY), String());
			if (EditorSettings::get_singleton()) {
				EDITOR_DEF_BASIC(path, String());
				EditorSettings::get_singleton()->add_property_hint(PropertyInfo(Variant::STRING, path, PROPERTY_HINT_MULTILINE_TEXT, desc, PROPERTY_USAGE_DEFAULT | PROPERTY_USAGE_READ_ONLY));
			}
		}
	}
}

JustAMCPResourceExecutor::JustAMCPResourceExecutor() {
	add_resource(memnew(JustAMCPResourceProjectFile));
	add_resource(memnew(JustAMCPResourceVideoRecordings));
	add_resource(memnew(JustAMCPResourceAutoworkResults));

	if (Thread::is_main_thread() && EditorFileSystem::get_singleton()) {
		const Callable cb = callable_mp_static(&JustAMCPResourceExecutor::_on_filesystem_changed);
		if (!EditorFileSystem::get_singleton()->is_connected("filesystem_changed", cb)) {
			EditorFileSystem::get_singleton()->connect("filesystem_changed", cb);
		}
	}
}

void JustAMCPResourceExecutor::_on_filesystem_changed() {
	static uint64_t last_invalidate_usec = 0;
	const uint64_t now = OS::get_singleton()->get_ticks_usec();
	if (now - last_invalidate_usec < 250000ULL) {
		return;
	}
	last_invalidate_usec = now;
	JustAMCPMaterialsResourceProvider::invalidate_cache();
}

JustAMCPResourceExecutor::~JustAMCPResourceExecutor() {
	if (Thread::is_main_thread() && EditorFileSystem::get_singleton()) {
		const Callable cb = callable_mp_static(&JustAMCPResourceExecutor::_on_filesystem_changed);
		if (EditorFileSystem::get_singleton()->is_connected("filesystem_changed", cb)) {
			EditorFileSystem::get_singleton()->disconnect("filesystem_changed", cb);
		}
	}
}

void JustAMCPResourceExecutor::add_resource(const Ref<JustAMCPResource> &p_resource) {
	if (p_resource.is_valid()) {
		registered_resources.push_back(p_resource);
	}
}

Dictionary JustAMCPResourceExecutor::_make_resource_schema(const String &p_uri, const String &p_name, const String &p_description, const String &p_mime_type) const {
	Dictionary resource;
	resource["uri"] = p_uri;
	resource["name"] = p_name;
	resource["description"] = p_description;
	resource["mimeType"] = p_mime_type;
	return resource;
}

Dictionary JustAMCPResourceExecutor::_make_resource_template_schema(const String &p_uri_template, const String &p_name, const String &p_description, const String &p_mime_type) const {
	Dictionary resource;
	resource["uriTemplate"] = p_uri_template;
	resource["name"] = p_name;
	resource["description"] = p_description;
	resource["mimeType"] = p_mime_type;
	return resource;
}

Dictionary JustAMCPResourceExecutor::_make_json_contents(const String &p_uri, const Dictionary &p_payload) const {
	Dictionary result;
	result["ok"] = true;

	Array contents;
	Dictionary content;
	content["uri"] = p_uri;
	content["mimeType"] = "application/json";
	content["text"] = JSON::stringify(p_payload);
	contents.push_back(content);
	result["contents"] = contents;
	return result;
}

Dictionary JustAMCPResourceExecutor::_make_text_contents(const String &p_uri, const String &p_text, const String &p_mime_type) const {
	Dictionary result;
	result["ok"] = true;

	Array contents;
	Dictionary content;
	content["uri"] = p_uri;
	content["mimeType"] = p_mime_type;
	content["text"] = p_text;
	contents.push_back(content);
	result["contents"] = contents;
	return result;
}

Dictionary JustAMCPResourceExecutor::_make_json_error_payload(const String &p_uri, const String &p_error) const {
	Dictionary payload;
	payload["connected"] = false;
	payload["error"] = p_error;
	return _make_json_contents(p_uri, payload);
}

String JustAMCPResourceExecutor::_canonicalize_resource_uri(const String &p_uri) const {
	if (p_uri.begins_with("godot://")) {
		return "blazium://" + p_uri.substr(String("godot://").length());
	}
	if (p_uri.begins_with("godot-mcp://guide/")) {
		return "blazium://guide/" + p_uri.substr(String("godot-mcp://guide/").length());
	}
	return p_uri;
}

Dictionary JustAMCPResourceExecutor::list_resources(const String &cursor) {
	Array resources;

	for (int i = 0; i < registered_resources.size(); i++) {
		if (registered_resources[i].is_valid() && !registered_resources[i]->is_template()) {
			resources.push_back(registered_resources[i]->get_schema());
		}
	}

	Array manifest_resources = JustAMCPResourceManifest::get_static_resource_schemas();
	for (int i = 0; i < manifest_resources.size(); i++) {
		resources.push_back(manifest_resources[i]);
	}

	return justamcp_pagination_slice_array(resources, cursor, "resources");
}

Dictionary JustAMCPResourceExecutor::list_resource_templates(const String &cursor) {
	Dictionary result;
	Array templates;

	for (int i = 0; i < registered_resources.size(); i++) {
		if (registered_resources[i].is_valid() && registered_resources[i]->is_template()) {
			templates.push_back(registered_resources[i]->get_schema());
		}
	}

	Array manifest_templates = JustAMCPResourceManifest::get_static_resource_template_schemas();
	for (int i = 0; i < manifest_templates.size(); i++) {
		templates.push_back(manifest_templates[i]);
	}

	return justamcp_pagination_slice_array(templates, cursor, "resourceTemplates");
}

Dictionary JustAMCPResourceExecutor::read_resource(const String &p_uri) {
	String canonical_uri = _canonicalize_resource_uri(p_uri);
	for (int i = 0; i < registered_resources.size(); i++) {
		if (registered_resources[i].is_valid()) {
			bool match = false;
			if (registered_resources[i]->is_template()) {
				String tmpl = registered_resources[i]->get_uri();
				if (tmpl.begins_with("res://") && p_uri.begins_with("res://")) {
					match = true;
				}
			} else if (registered_resources[i]->get_uri() == p_uri || registered_resources[i]->get_uri() == canonical_uri) {
				match = true;
			}

			if (match) {
				Dictionary res = registered_resources[i]->read_resource(p_uri);
				if (res.has("ok") && bool(Variant(res["ok"]))) {
					return res;
				}
			}
		}
	}

	if (canonical_uri.begins_with("blazium://")) {
		Dictionary result = _read_blazium_resource(p_uri);
		if (result.get("ok", false)) {
			return result;
		}
	}

	Dictionary result;
	result["ok"] = false;
	result["error_code"] = -32602;
	result["error"] = "Unknown resource URI: " + p_uri;
	return result;
}

Dictionary JustAMCPResourceExecutor::_read_blazium_resource(const String &p_uri) const {
	const String canonical_uri = _canonicalize_resource_uri(p_uri);

	if (JustAMCPBlaziumResourceRegistry::can_read(canonical_uri)) {
		return JustAMCPBlaziumResourceRegistry::read(p_uri, canonical_uri);
	}

	if (canonical_uri == "blazium://test/results") {
		Dictionary payload;
		payload["available"] = false;
		payload["source"] = "autowork";
		payload["content"] = String();
		const String json_path = "user://autowork_results.json";
		const String xml_path = "user://autowork_results.xml";
		String result_path;
		if (FileAccess::exists(json_path)) {
			result_path = json_path;
		} else if (FileAccess::exists(xml_path)) {
			result_path = xml_path;
		}
		if (!result_path.is_empty()) {
			String text;
			int64_t size = 0;
			Dictionary read_err;
			if (!justamcp_read_utf8_within_limit(result_path, JUSTAMCP_MAX_SYNC_READ_BYTES, text, size, read_err)) {
				return read_err;
			}
			payload["available"] = true;
			payload["path"] = result_path;
			payload["content"] = text;
		}
		return _make_json_contents(p_uri, payload);
	}

	Dictionary result;
	result["ok"] = false;
	return result;
}

#endif
