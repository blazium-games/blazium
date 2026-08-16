/**************************************************************************/
/*  justamcp_asset_tags_tools.cpp                                         */
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
#include "justamcp_asset_tags_tools.h"

#include "modules/modules_enabled.gen.h"

#ifdef MODULE_ASSETTAGS_ENABLED
#include "modules/assettags/asset_tag_coordinator.h"
#include "modules/assettags/asset_tag_manager.h"
#include "modules/assettags/asset_tag_registry.h"
#include "modules/assettags/asset_tag_storage.h"
#include "resources/justamcp_tags_resource_provider.h"
#endif

#include "../justamcp_mcp_spec.h"
#include "../justamcp_pagination.h"
#include "../justamcp_server.h"

#include "justamcp_tool_schema_builder.h"

#include "core/config/project_settings.h"
#include "core/os/os.h"
#include "editor/settings/editor_settings.h"

#ifdef MODULE_ASSETTAGS_ENABLED
static void _invalidate_tags_dictionary_cache() {
	JustAMCPTagsResourceProvider::invalidate_dictionary_cache();
}
#endif

Dictionary JustAMCPAssetTagsTools::_make_error(int p_code, const String &p_message) const {
	Dictionary result;
	result["ok"] = false;
	result["error_code"] = p_code;
	result["error"] = p_message;
	return result;
}

static String _tags_arg_path(const Dictionary &p_args) {
	String path = p_args.get("path", "");
	if (path.is_empty()) {
		path = p_args.get("asset_path", "");
	}
#ifdef MODULE_ASSETTAGS_ENABLED
	return AssetTagStorage::normalize_asset_path(path);
#else
	return path;
#endif
}

static String _tags_arg_tag_name(const Dictionary &p_args) {
	String tag = p_args.get("tag_name", "");
	if (tag.is_empty()) {
		tag = p_args.get("tag", "");
	}
	return tag;
}

String JustAMCPAssetTagsTools::_normalize_tool_name(const String &p_tool_name) const {
	if (p_tool_name.begins_with("blazium_")) {
		return p_tool_name.substr(8);
	}
	return p_tool_name;
}

String JustAMCPAssetTagsTools::_error_message_for_code(Error p_error) const {
	switch (p_error) {
		case ERR_INVALID_PARAMETER:
			return "Invalid parameter";
		case ERR_ALREADY_EXISTS:
			return "Tag already exists";
		case ERR_DOES_NOT_EXIST:
			return "Tag does not exist";
		case ERR_UNCONFIGURED:
			return "Asset tag services unavailable";
		default:
			return "Operation failed";
	}
}

Dictionary JustAMCPAssetTagsTools::_require_elicitation(const String &p_action, const Dictionary &p_args) const {
	if (p_args.get("confirmed", false)) {
		return Dictionary();
	}
	const Dictionary schema = justamcp_confirm_enum_schema();
	const String request_id = p_args.get("request_id", String::num_uint64(OS::get_singleton()->get_ticks_usec()));
	if (JustAMCPServer *server = JustAMCPServer::get_singleton()) {
		String demo_url;
		if (ProjectSettings::get_singleton() && ProjectSettings::get_singleton()->has_setting("blazium/justamcp/url_elicitation_demo_url")) {
			demo_url = String(GLOBAL_GET("blazium/justamcp/url_elicitation_demo_url"));
		}
		if (!demo_url.is_empty() && bool(p_args.get("url_elicit", false)) && justamcp_protocol_supports(server->get_negotiated_protocol_version(), JUSTAMCP_FEATURE_ELICITATION_URL)) {
			server->send_url_elicitation_error(request_id, "elicitation_" + request_id, demo_url, "URL elicitation required for " + p_action);
		} else {
			server->send_elicitation_request(request_id, "form", "Confirm asset tag mutation: " + p_action, schema);
		}
	}
	Dictionary err;
	err["ok"] = false;
	err["elicitation_required"] = true;
	err["elicitation_schema"] = schema;
	err["elicitation_mode"] = "form";
	err["error"] = "Explicit user confirmation required for " + p_action + ". Retry with confirmed=true after approval.";
	return err;
}

void JustAMCPAssetTagsTools::_bind_methods() {
	ClassDB::bind_method(D_METHOD("provide_tool_schemas", "register_only", "ignore_settings", "include_disabled_tools"), &JustAMCPAssetTagsTools::provide_tool_schemas, DEFVAL(false), DEFVAL(false), DEFVAL(false));
	ClassDB::bind_method(D_METHOD("execute_tool", "tool_name", "arguments"), &JustAMCPAssetTagsTools::execute_tool);
}

Array JustAMCPAssetTagsTools::provide_tool_schemas(bool p_register_only, bool p_ignore_settings, bool p_include_disabled_tools) {
	return get_tool_schemas(p_register_only, p_ignore_settings, p_include_disabled_tools);
}

Array JustAMCPAssetTagsTools::get_tool_schemas(bool p_register_only, bool p_ignore_settings, bool p_include_disabled_tools) {
	Array tools;
	String current_category = "asset_tags_tools";
	bool is_core = true;

	auto add_schema = [&](const String &p_name, const String &p_desc, const Vector<String> &p_props, const Vector<String> &p_req) {
		const String full_name = "blazium_" + p_name;
		if (p_register_only) {
			JustAMCPToolSchemaBuilder::register_tool_settings(current_category, full_name, is_core);
			return;
		}
		bool cat_enabled = true;
		bool tool_enabled = true;
		if (!JustAMCPToolSchemaBuilder::resolve_tool_enabled(current_category, full_name, p_ignore_settings, p_include_disabled_tools, cat_enabled, tool_enabled)) {
			return;
		}
		tools.push_back(JustAMCPToolSchemaBuilder::build_tool_schema(full_name, p_desc, current_category, cat_enabled && tool_enabled, p_props, p_req));
	};

	add_schema("tags_list", "Lists project asset tags, optionally filtered to immediate children of a parent tag.",
			Vector<String>{ "parent_tag", "string", "cursor", "string" }, Vector<String>{});
	add_schema("tags_get_info", "Returns comment, source, and immediate child tags for a tag name.",
			Vector<String>{ "tag_name", "string" }, Vector<String>{ "tag_name" });
	add_schema("tags_add", "Adds a tag to the project dictionary. Only call after explicit user permission.",
			Vector<String>{ "tag_name", "string", "comment", "string" }, Vector<String>{ "tag_name" });
	add_schema("tags_remove", "Removes a tag from the project dictionary. Only call after explicit user permission.",
			Vector<String>{ "tag_name", "string" }, Vector<String>{ "tag_name" });
	add_schema("tags_rename", "Renames a tag and updates asset references. Only call after explicit user permission.",
			Vector<String>{ "old_name", "string", "new_name", "string" }, Vector<String>{ "old_name", "new_name" });
	add_schema("tags_update_comment", "Updates the comment on a dictionary tag.",
			Vector<String>{ "tag_name", "string", "comment", "string" }, Vector<String>{ "tag_name", "comment" });
	add_schema("tags_find_assets", "Finds res:// asset paths referencing a tag.",
			Vector<String>{ "tag_name", "string", "match_parent", "boolean", "cursor", "string" }, Vector<String>{ "tag_name" });
	add_schema("tags_set_on_asset", "Assigns tags to a project asset path.",
			Vector<String>{ "path", "string", "tags", "array" }, Vector<String>{ "path", "tags" });
	add_schema("tags_get_on_asset", "Reads tags assigned to a project asset path.",
			Vector<String>{ "path", "string" }, Vector<String>{ "path" });
	add_schema("tags_add_to_asset", "Adds tags to a project asset without replacing existing tags.",
			Vector<String>{ "path", "string", "tags", "array" }, Vector<String>{ "path", "tags" });
	add_schema("tags_remove_from_asset", "Removes tags from a project asset. Tag names match exactly or by prefix (e.g. removing Environment also removes Environment.Nature).",
			Vector<String>{ "path", "string", "tags", "array" }, Vector<String>{ "path", "tags" });
	add_schema("tags_batch_set_on_assets", "Assigns tags to multiple assets in one batched registry transaction.",
			Vector<String>{ "assignments", "array" }, Vector<String>{ "assignments" });
	add_schema("tags_search_assets", "Searches tagged assets by tag list, optional type filter, path glob, or path regex.",
			Vector<String>{ "tags", "array", "type_filter", "string", "path_glob", "string", "path_regex", "string", "require_all", "boolean", "cursor", "string" }, Vector<String>{});
	add_schema("tags_get_unused", "Lists dictionary tags that are not referenced by any asset.",
			Vector<String>{}, Vector<String>{});
	add_schema("tags_rescan", "Reloads the asset tag index from disk.",
			Vector<String>{}, Vector<String>{});
	add_schema("tags_can_undo", "Returns whether the last asset tag transaction can be undone.",
			Vector<String>{}, Vector<String>{});
	add_schema("tags_undo_last_change", "Restores the asset tag dictionary and index from the last transaction snapshot.",
			Vector<String>{}, Vector<String>{});

	return tools;
}

Dictionary JustAMCPAssetTagsTools::execute_tool(const String &p_tool_name, const Dictionary &p_args) {
	const String tool_name = _normalize_tool_name(p_tool_name);
#ifdef MODULE_ASSETTAGS_ENABLED
	if (tool_name == "tags_list") {
		return tags_list(p_args);
	}
	if (tool_name == "tags_get_info") {
		return tags_get_info(p_args);
	}
	if (tool_name == "tags_add") {
		return tags_add(p_args);
	}
	if (tool_name == "tags_remove") {
		return tags_remove(p_args);
	}
	if (tool_name == "tags_rename") {
		return tags_rename(p_args);
	}
	if (tool_name == "tags_update_comment") {
		return tags_update_comment(p_args);
	}
	if (tool_name == "tags_find_assets") {
		return tags_find_assets(p_args);
	}
	if (tool_name == "tags_set_on_asset") {
		return tags_set_on_asset(p_args);
	}
	if (tool_name == "tags_get_on_asset") {
		return tags_get_on_asset(p_args);
	}
	if (tool_name == "tags_add_to_asset") {
		return tags_add_to_asset(p_args);
	}
	if (tool_name == "tags_remove_from_asset") {
		return tags_remove_from_asset(p_args);
	}
	if (tool_name == "tags_batch_set_on_assets") {
		return tags_batch_set_on_assets(p_args);
	}
	if (tool_name == "tags_search_assets") {
		return tags_search_assets(p_args);
	}
	if (tool_name == "tags_get_unused") {
		return tags_get_unused(p_args);
	}
	if (tool_name == "tags_rescan") {
		return tags_rescan(p_args);
	}
	if (tool_name == "tags_can_undo") {
		return tags_can_undo(p_args);
	}
	if (tool_name == "tags_undo_last_change") {
		return tags_undo_last_change(p_args);
	}
#endif
	return Dictionary();
}

Dictionary JustAMCPAssetTagsTools::tags_list(const Dictionary &p_args) {
#ifdef MODULE_ASSETTAGS_ENABLED
	AssetTagManager *manager = AssetTagManager::get_singleton();
	if (!manager) {
		return _make_error(-32603, "AssetTagManager unavailable");
	}
	const String cursor = p_args.get("cursor", "");
	Dictionary page = justamcp_pagination_slice_strings(manager->list_tags(p_args.get("parent_tag", "")), cursor, "tags");
	if (page.has("ok") && !bool(page.get("ok", true))) {
		return _make_error(int(page.get("error_code", -32602)), String(page.get("error", "Invalid pagination cursor.")));
	}
	Dictionary result;
	result["ok"] = true;
	result["tags"] = page.get("tags", Array());
	if (page.has("nextCursor")) {
		result["nextCursor"] = page["nextCursor"];
	}
	return result;
#else
	return _make_error(-32603, "Asset tags module is not enabled");
#endif
}

Dictionary JustAMCPAssetTagsTools::tags_get_info(const Dictionary &p_args) {
#ifdef MODULE_ASSETTAGS_ENABLED
	AssetTagManager *manager = AssetTagManager::get_singleton();
	if (!manager) {
		return _make_error(-32603, "AssetTagManager unavailable");
	}
	return manager->get_tag_info(_tags_arg_tag_name(p_args));
#else
	return _make_error(-32603, "Asset tags module is not enabled");
#endif
}

Dictionary JustAMCPAssetTagsTools::tags_add(const Dictionary &p_args) {
#ifdef MODULE_ASSETTAGS_ENABLED
	Dictionary elicit = _require_elicitation("tags_add", p_args);
	if (!elicit.is_empty()) {
		return elicit;
	}
	AssetTagManager *manager = AssetTagManager::get_singleton();
	if (!manager) {
		return _make_error(-32603, "AssetTagManager unavailable");
	}
	const String tag_name = _tags_arg_tag_name(p_args);
	if (tag_name.is_empty()) {
		return _make_error(-32602, "tag_name is required");
	}
	Error err = manager->add_tag(tag_name, p_args.get("comment", ""));
	if (err != OK) {
		return _make_error(-32602, _error_message_for_code(err));
	}
	Dictionary result;
	result["ok"] = true;
	_invalidate_tags_dictionary_cache();
	return result;
#else
	return _make_error(-32603, "Asset tags module is not enabled");
#endif
}

Dictionary JustAMCPAssetTagsTools::tags_remove(const Dictionary &p_args) {
#ifdef MODULE_ASSETTAGS_ENABLED
	Dictionary elicit = _require_elicitation("tags_remove", p_args);
	if (!elicit.is_empty()) {
		return elicit;
	}
	AssetTagCoordinator *coordinator = AssetTagCoordinator::get_singleton();
	if (!coordinator) {
		return _make_error(-32603, "AssetTagCoordinator unavailable");
	}
	const String tag = _tags_arg_tag_name(p_args);
	if (tag.is_empty()) {
		return _make_error(-32602, "tag_name is required");
	}
	if (coordinator->begin_transaction() != OK) {
		return _make_error(-32603, "Failed to begin tag transaction");
	}
	Dictionary result = coordinator->remove_tag_result(tag);
	if (result.get("ok", false)) {
		if (coordinator->commit_transaction() != OK) {
			coordinator->abort_transaction();
			return _make_error(-32603, "Failed to commit tag transaction");
		}
		_invalidate_tags_dictionary_cache();
	} else {
		coordinator->abort_transaction();
	}
	return result;
#else
	return _make_error(-32603, "Asset tags module is not enabled");
#endif
}

Dictionary JustAMCPAssetTagsTools::tags_rename(const Dictionary &p_args) {
#ifdef MODULE_ASSETTAGS_ENABLED
	Dictionary elicit = _require_elicitation("tags_rename", p_args);
	if (!elicit.is_empty()) {
		return elicit;
	}
	AssetTagCoordinator *coordinator = AssetTagCoordinator::get_singleton();
	if (!coordinator) {
		return _make_error(-32603, "AssetTagCoordinator unavailable");
	}
	const String old_name = [&]() -> String {
		String n = p_args.get("old_name", "");
		if (n.is_empty()) {
			n = _tags_arg_tag_name(p_args);
		}
		return n;
	}();
	String new_name = p_args.get("new_name", "");
	if (new_name.is_empty()) {
		new_name = p_args.get("new_tag_name", p_args.get("new_tag", ""));
	}
	if (old_name.is_empty() || new_name.is_empty()) {
		return _make_error(-32602, "old_name and new_name are required");
	}
	if (coordinator->begin_transaction() != OK) {
		return _make_error(-32603, "Failed to begin tag transaction");
	}
	Dictionary result = coordinator->rename_tag_result(old_name, new_name);
	if (result.get("ok", false)) {
		if (coordinator->commit_transaction() != OK) {
			coordinator->abort_transaction();
			return _make_error(-32603, "Failed to commit tag transaction");
		}
		_invalidate_tags_dictionary_cache();
	} else {
		coordinator->abort_transaction();
	}
	return result;
#else
	return _make_error(-32603, "Asset tags module is not enabled");
#endif
}

Dictionary JustAMCPAssetTagsTools::tags_update_comment(const Dictionary &p_args) {
#ifdef MODULE_ASSETTAGS_ENABLED
	Dictionary elicit = _require_elicitation("tags_update_comment", p_args);
	if (!elicit.is_empty()) {
		return elicit;
	}
	AssetTagManager *manager = AssetTagManager::get_singleton();
	if (!manager) {
		return _make_error(-32603, "AssetTagManager unavailable");
	}
	const String tag_name = _tags_arg_tag_name(p_args);
	const String comment = p_args.get("comment", "");
	if (tag_name.is_empty()) {
		return _make_error(-32602, "tag_name is required");
	}
	Error err = manager->update_tag_comment(tag_name, comment);
	if (err != OK) {
		return _make_error(-32602, _error_message_for_code(err));
	}
	Dictionary result;
	result["ok"] = true;
	result["tag_name"] = tag_name;
	result["comment"] = comment;
	_invalidate_tags_dictionary_cache();
	return result;
#else
	return _make_error(-32603, "Asset tags module is not enabled");
#endif
}

Dictionary JustAMCPAssetTagsTools::tags_find_assets(const Dictionary &p_args) {
#ifdef MODULE_ASSETTAGS_ENABLED
	AssetTagRegistry *registry = AssetTagRegistry::get_singleton();
	if (!registry) {
		return _make_error(-32603, "AssetTagRegistry unavailable");
	}
	const String cursor = p_args.get("cursor", "");
	Dictionary page = justamcp_pagination_slice_strings(registry->find_assets_by_tag(_tags_arg_tag_name(p_args), p_args.get("match_parent", true)), cursor, "paths");
	if (page.has("ok") && !bool(page.get("ok", true))) {
		return _make_error(int(page.get("error_code", -32602)), String(page.get("error", "Invalid pagination cursor.")));
	}
	Dictionary result;
	result["ok"] = true;
	result["paths"] = page.get("paths", Array());
	if (page.has("nextCursor")) {
		result["nextCursor"] = page["nextCursor"];
	}
	return result;
#else
	return _make_error(-32603, "Asset tags module is not enabled");
#endif
}

static PackedStringArray _tags_from_args(const Dictionary &p_args) {
	PackedStringArray tags;
	if (p_args.has("tags")) {
		Array raw = p_args["tags"];
		for (int i = 0; i < raw.size(); i++) {
			tags.push_back(String(raw[i]));
		}
	}
	return tags;
}

#ifdef MODULE_ASSETTAGS_ENABLED
static Error _tags_commit_transaction(AssetTagCoordinator *p_coordinator, AssetTagRegistry *p_registry) {
	if (p_coordinator) {
		return p_coordinator->commit_transaction();
	}
	if (p_registry) {
		return p_registry->commit_batch();
	}
	return OK;
}

static void _tags_abort_transaction(AssetTagCoordinator *p_coordinator, AssetTagRegistry *p_registry) {
	if (p_coordinator) {
		p_coordinator->abort_transaction();
	} else if (p_registry) {
		p_registry->abort_batch();
	}
}

static Error _tags_begin_transaction(AssetTagCoordinator *p_coordinator, AssetTagRegistry *p_registry) {
	if (p_coordinator) {
		return p_coordinator->begin_transaction();
	}
	if (p_registry) {
		p_registry->begin_batch();
	}
	return OK;
}
#endif

Dictionary JustAMCPAssetTagsTools::tags_set_on_asset(const Dictionary &p_args) {
#ifdef MODULE_ASSETTAGS_ENABLED
	Dictionary elicit = _require_elicitation("tags_set_on_asset", p_args);
	if (!elicit.is_empty()) {
		return elicit;
	}
	AssetTagRegistry *registry = AssetTagRegistry::get_singleton();
	AssetTagCoordinator *coordinator = AssetTagCoordinator::get_singleton();
	if (!registry) {
		return _make_error(-32603, "AssetTagRegistry unavailable");
	}
	const String path = _tags_arg_path(p_args);
	if (path.is_empty()) {
		return _make_error(-32602, "path is required");
	}
	const Error begin_err = _tags_begin_transaction(coordinator, registry);
	if (begin_err != OK) {
		return _make_error(-32603, "Failed to begin asset tag transaction");
	}
	const Error err = registry->set_tags_for_asset(path, _tags_from_args(p_args));
	if (err != OK) {
		_tags_abort_transaction(coordinator, registry);
		return _make_error(-32602, _error_message_for_code(err));
	}
	const Error commit_err = _tags_commit_transaction(coordinator, registry);
	if (commit_err != OK) {
		return _make_error(-32602, _error_message_for_code(commit_err));
	}
	Dictionary result;
	result["ok"] = true;
	_invalidate_tags_dictionary_cache();
	return result;
#else
	return _make_error(-32603, "Asset tags module is not enabled");
#endif
}

Dictionary JustAMCPAssetTagsTools::tags_get_on_asset(const Dictionary &p_args) {
#ifdef MODULE_ASSETTAGS_ENABLED
	AssetTagRegistry *registry = AssetTagRegistry::get_singleton();
	if (!registry) {
		return _make_error(-32603, "AssetTagRegistry unavailable");
	}
	const String path = _tags_arg_path(p_args);
	if (path.is_empty()) {
		return _make_error(-32602, "path is required");
	}
	Dictionary result;
	result["ok"] = true;
	result["path"] = path;
	result["tags"] = registry->get_tags_for_asset(path);
	return result;
#else
	return _make_error(-32603, "Asset tags module is not enabled");
#endif
}

Dictionary JustAMCPAssetTagsTools::tags_add_to_asset(const Dictionary &p_args) {
#ifdef MODULE_ASSETTAGS_ENABLED
	Dictionary elicit = _require_elicitation("tags_add_to_asset", p_args);
	if (!elicit.is_empty()) {
		return elicit;
	}
	AssetTagRegistry *registry = AssetTagRegistry::get_singleton();
	AssetTagCoordinator *coordinator = AssetTagCoordinator::get_singleton();
	if (!registry) {
		return _make_error(-32603, "AssetTagRegistry unavailable");
	}
	const String path = _tags_arg_path(p_args);
	if (path.is_empty()) {
		return _make_error(-32602, "path is required");
	}
	const Error begin_err = _tags_begin_transaction(coordinator, registry);
	if (begin_err != OK) {
		return _make_error(-32603, "Failed to begin asset tag transaction");
	}
	const Error err = registry->add_tags_to_asset(path, _tags_from_args(p_args));
	if (err != OK) {
		_tags_abort_transaction(coordinator, registry);
		return _make_error(-32602, _error_message_for_code(err));
	}
	const Error commit_err = _tags_commit_transaction(coordinator, registry);
	if (commit_err != OK) {
		return _make_error(-32602, _error_message_for_code(commit_err));
	}
	Dictionary result;
	result["ok"] = true;
	_invalidate_tags_dictionary_cache();
	return result;
#else
	return _make_error(-32603, "Asset tags module is not enabled");
#endif
}

Dictionary JustAMCPAssetTagsTools::tags_remove_from_asset(const Dictionary &p_args) {
#ifdef MODULE_ASSETTAGS_ENABLED
	Dictionary elicit = _require_elicitation("tags_remove_from_asset", p_args);
	if (!elicit.is_empty()) {
		return elicit;
	}
	AssetTagRegistry *registry = AssetTagRegistry::get_singleton();
	AssetTagCoordinator *coordinator = AssetTagCoordinator::get_singleton();
	if (!registry) {
		return _make_error(-32603, "AssetTagRegistry unavailable");
	}
	const String path = _tags_arg_path(p_args);
	if (path.is_empty()) {
		return _make_error(-32602, "path is required");
	}
	const Error begin_err = _tags_begin_transaction(coordinator, registry);
	if (begin_err != OK) {
		return _make_error(-32603, "Failed to begin asset tag transaction");
	}
	const Error err = registry->remove_tags_from_asset(path, _tags_from_args(p_args));
	if (err != OK) {
		_tags_abort_transaction(coordinator, registry);
		return _make_error(-32602, _error_message_for_code(err));
	}
	const Error commit_err = _tags_commit_transaction(coordinator, registry);
	if (commit_err != OK) {
		return _make_error(-32602, _error_message_for_code(commit_err));
	}
	Dictionary result;
	result["ok"] = true;
	_invalidate_tags_dictionary_cache();
	return result;
#else
	return _make_error(-32603, "Asset tags module is not enabled");
#endif
}

Dictionary JustAMCPAssetTagsTools::tags_batch_set_on_assets(const Dictionary &p_args) {
#ifdef MODULE_ASSETTAGS_ENABLED
	Dictionary elicit = _require_elicitation("tags_batch_set_on_assets", p_args);
	if (!elicit.is_empty()) {
		return elicit;
	}
	AssetTagRegistry *registry = AssetTagRegistry::get_singleton();
	AssetTagCoordinator *coordinator = AssetTagCoordinator::get_singleton();
	if (!registry) {
		return _make_error(-32603, "AssetTagRegistry unavailable");
	}
	const Array assignments = p_args.get("assignments", Array());
	if (assignments.is_empty()) {
		return _make_error(-32602, "assignments array is required");
	}
	if (coordinator) {
		const Error begin_err = coordinator->begin_transaction();
		if (begin_err != OK) {
			return _make_error(-32603, "Failed to begin asset tag transaction");
		}
	} else {
		registry->begin_batch();
	}
	int updated = 0;
	Array errors;
	for (int i = 0; i < assignments.size(); i++) {
		const Dictionary item = assignments[i];
		const String path = item.get("path", "");
		const Array tags_array = item.get("tags", Array());
		if (path.is_empty()) {
			Dictionary entry_error;
			entry_error["index"] = i;
			entry_error["error"] = "path is required";
			errors.push_back(entry_error);
			continue;
		}
		PackedStringArray tags;
		for (int j = 0; j < tags_array.size(); j++) {
			tags.push_back(String(tags_array[j]));
		}
		const Error set_err = registry->set_tags_for_asset(path, tags);
		if (set_err == OK) {
			updated++;
		} else {
			Dictionary entry_error;
			entry_error["index"] = i;
			entry_error["path"] = path;
			entry_error["error"] = _error_message_for_code(set_err);
			entry_error["error_code"] = set_err;
			errors.push_back(entry_error);
		}
	}
	if (!errors.is_empty()) {
		if (coordinator) {
			coordinator->abort_transaction();
		} else {
			registry->abort_batch();
		}
	} else {
		if (coordinator) {
			coordinator->commit_transaction();
		} else {
			registry->commit_batch();
		}
		_invalidate_tags_dictionary_cache();
	}
	Dictionary result;
	result["ok"] = errors.is_empty();
	result["updated"] = updated;
	result["failed"] = errors.size();
	if (!errors.is_empty()) {
		result["errors"] = errors;
	}
	return result;
#else
	return _make_error(-32603, "Asset tags module is not enabled");
#endif
}

Dictionary JustAMCPAssetTagsTools::tags_search_assets(const Dictionary &p_args) {
#ifdef MODULE_ASSETTAGS_ENABLED
	AssetTagRegistry *registry = AssetTagRegistry::get_singleton();
	if (!registry) {
		return _make_error(-32603, "AssetTagRegistry unavailable");
	}
	Dictionary search = registry->search_assets(_tags_from_args(p_args), p_args.get("type_filter", ""), p_args.get("path_glob", ""), p_args.get("path_regex", ""), p_args.get("require_all", true));
	if (!bool(search.get("ok", false))) {
		return search;
	}
	const String cursor = p_args.get("cursor", "");
	Dictionary page = justamcp_pagination_slice_array(search.get("assets", Array()), cursor, "assets");
	if (page.has("ok") && !bool(page.get("ok", true))) {
		return _make_error(int(page.get("error_code", -32602)), String(page.get("error", "Invalid pagination cursor.")));
	}
	Dictionary result;
	result["ok"] = true;
	result["assets"] = page.get("assets", Array());
	result["count"] = Array(result["assets"]).size();
	if (page.has("nextCursor")) {
		result["nextCursor"] = page["nextCursor"];
	}
	return result;
#else
	return _make_error(-32603, "Asset tags module is not enabled");
#endif
}

Dictionary JustAMCPAssetTagsTools::tags_get_unused(const Dictionary &p_args) {
#ifdef MODULE_ASSETTAGS_ENABLED
	AssetTagRegistry *registry = AssetTagRegistry::get_singleton();
	if (!registry) {
		return _make_error(-32603, "AssetTagRegistry unavailable");
	}
	(void)p_args;
	Dictionary result;
	result["ok"] = true;
	result["tags"] = registry->get_unused_tags();
	return result;
#else
	return _make_error(-32603, "Asset tags module is not enabled");
#endif
}

Dictionary JustAMCPAssetTagsTools::tags_rescan(const Dictionary &p_args) {
#ifdef MODULE_ASSETTAGS_ENABLED
	AssetTagRegistry *registry = AssetTagRegistry::get_singleton();
	if (!registry) {
		return _make_error(-32603, "AssetTagRegistry unavailable");
	}
	(void)p_args;
	const Error err = registry->rescan();
	if (err != OK) {
		return _make_error(-32603, "Cannot rescan while asset tag batch or unsaved index changes are pending");
	}
	Dictionary result;
	result["ok"] = true;
	_invalidate_tags_dictionary_cache();
	return result;
#else
	return _make_error(-32603, "Asset tags module is not enabled");
#endif
}

Dictionary JustAMCPAssetTagsTools::tags_can_undo(const Dictionary &p_args) {
#ifdef MODULE_ASSETTAGS_ENABLED
	AssetTagCoordinator *coordinator = AssetTagCoordinator::get_singleton();
	if (!coordinator) {
		return _make_error(-32603, "AssetTagCoordinator unavailable");
	}
	(void)p_args;
	Dictionary result;
	result["ok"] = true;
	result["can_undo"] = coordinator->can_undo();
	return result;
#else
	return _make_error(-32603, "Asset tags module is not enabled");
#endif
}

Dictionary JustAMCPAssetTagsTools::tags_undo_last_change(const Dictionary &p_args) {
#ifdef MODULE_ASSETTAGS_ENABLED
	AssetTagCoordinator *coordinator = AssetTagCoordinator::get_singleton();
	if (!coordinator) {
		return _make_error(-32603, "AssetTagCoordinator unavailable");
	}
	(void)p_args;
	if (!coordinator->can_undo()) {
		return _make_error(-32602, "No undo snapshot available");
	}
	const Error err = coordinator->undo_last_change();
	if (err != OK) {
		return _make_error(-32603, "Failed to restore asset tag undo snapshot");
	}
	Dictionary result;
	result["ok"] = true;
	_invalidate_tags_dictionary_cache();
	return result;
#else
	return _make_error(-32603, "Asset tags module is not enabled");
#endif
}

JustAMCPAssetTagsTools::JustAMCPAssetTagsTools() {}
JustAMCPAssetTagsTools::~JustAMCPAssetTagsTools() {}

#endif
