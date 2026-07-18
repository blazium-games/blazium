/**************************************************************************/
/*  justamcp_semantic_search_tools.cpp                                    */
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

#include "justamcp_semantic_search_tools.h"

#include "../justamcp_tool_context.h"
#include "justamcp_readonly_tools.h"
#include "justamcp_tool_schema_builder.h"

#include "core/config/project_settings.h"
#include "editor/editor_settings.h"
#ifdef TOOLS_ENABLED
#include "editor/editor_file_system.h"
#endif

#include "modules/modules_enabled.gen.h"

#ifdef MODULE_SEMANTICSEARCH_ENABLED
#include "modules/semanticsearch/semantic_asset_index.h"
#include "modules/semanticsearch/semantic_async_search_worker.h"
#include "modules/semanticsearch/semantic_search_backend.h"
#include "modules/semanticsearch/semantic_search_backend_factory.h"
#endif

void JustAMCPSemanticSearchTools::_bind_methods() {
	ClassDB::bind_method(D_METHOD("provide_tool_schemas", "register_only", "ignore_settings", "include_disabled_tools"), &JustAMCPSemanticSearchTools::provide_tool_schemas, DEFVAL(false), DEFVAL(false), DEFVAL(false));
	ClassDB::bind_method(D_METHOD("execute_tool", "tool_name", "arguments"), &JustAMCPSemanticSearchTools::execute_tool);
}

String JustAMCPSemanticSearchTools::_normalize_res_path(const String &p_path) const {
	if (p_path.is_empty()) {
		return p_path;
	}
	if (p_path.begins_with("res://")) {
		return p_path;
	}
	return "res://" + p_path;
}

static Dictionary _semantic_structured_ok(const Dictionary &p_result) {
	Dictionary structured;
	structured["ok"] = true;
	for (const Variant &key : p_result.keys()) {
		const String key_name = key;
		if (key_name == "ok" || key_name == "structuredContent") {
			continue;
		}
		structured[key_name] = p_result[key_name];
	}
	return structured;
}

static Dictionary _semantic_tool_cancelled() {
	Dictionary err;
	err["ok"] = false;
	err["error"] = "cancelled";
	return err;
}

Array JustAMCPSemanticSearchTools::provide_tool_schemas(bool p_register_only, bool p_ignore_settings, bool p_include_disabled_tools) {
	return get_tool_schemas(p_register_only, p_ignore_settings, p_include_disabled_tools);
}

Array JustAMCPSemanticSearchTools::get_tool_schemas(bool p_register_only, bool p_ignore_settings, bool p_include_disabled_tools) {
	Array tools;
	const String current_category = "semantic_search_tools";
	const bool is_core = false;

	auto add_schema = [&](const String &p_name, const String &p_desc, const Vector<String> &p_props, const Vector<String> &p_req, const String &p_task_support = String()) {
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
		tools.push_back(JustAMCPToolSchemaBuilder::build_tool_schema(full_name, p_desc, current_category, cat_enabled && tool_enabled, p_props, p_req, p_task_support));
	};

	add_schema("semantic_search", "Semantic search across indexed project assets (lexical, embedding, or hybrid per project settings).",
			Vector<String>{ "query", "string", "limit", "number", "tags", "array", "require_all", "boolean", "path_regex", "string", "class_filter", "string" }, Vector<String>{ "query" }, "required");
	add_schema("semantic_find_similar", "Find assets similar to a given res:// path.",
			Vector<String>{ "path", "string", "limit", "number", "path_regex", "string", "class_filter", "string" }, Vector<String>{ "path" }, "required");
	add_schema("semantic_index_stats", "Return semantic index statistics for the active backend.",
			Vector<String>{}, Vector<String>{});
	add_schema("semantic_rebuild_index", "Rebuild the semantic asset index from tags and asset metadata.",
			Vector<String>{}, Vector<String>{}, "required");
	add_schema("semantic_search_enqueue", "Enqueue an asynchronous semantic search job.",
			Vector<String>{ "query", "string", "limit", "number", "tags", "array", "require_all", "boolean", "path_regex", "string", "class_filter", "string" }, Vector<String>{ "query" });
	add_schema("semantic_search_poll", "Poll the status or results of an asynchronous semantic search job.",
			Vector<String>{ "job_id", "string" }, Vector<String>{ "job_id" });
	add_schema("semantic_search_cancel", "Cancel an asynchronous semantic search job.",
			Vector<String>{ "job_id", "string" }, Vector<String>{ "job_id" });

	if (p_register_only) {
		JustAMCPReadonlyTools::register_readonly_tool("semantic_search");
		JustAMCPReadonlyTools::register_readonly_tool("semantic_find_similar");
		JustAMCPReadonlyTools::register_readonly_tool("semantic_index_stats");
		JustAMCPReadonlyTools::register_readonly_tool("semantic_search_enqueue");
		JustAMCPReadonlyTools::register_readonly_tool("semantic_search_poll");
		JustAMCPReadonlyTools::register_readonly_tool("semantic_search_cancel");
	}

	return tools;
}

Dictionary JustAMCPSemanticSearchTools::execute_tool(const String &p_tool_name, const Dictionary &p_args) {
	String tool_name = p_tool_name;
	if (tool_name.begins_with("blazium_")) {
		tool_name = tool_name.substr(8);
	}
	if (tool_name == "semantic_search") {
		return semantic_search(p_args);
	}
	if (tool_name == "semantic_find_similar") {
		return semantic_find_similar(p_args);
	}
	if (tool_name == "semantic_index_stats") {
		return semantic_index_stats(p_args);
	}
	if (tool_name == "semantic_rebuild_index") {
		return semantic_rebuild_index(p_args);
	}
	if (tool_name == "semantic_search_enqueue") {
		return semantic_search_enqueue(p_args);
	}
	if (tool_name == "semantic_search_poll") {
		return semantic_search_poll(p_args);
	}
	if (tool_name == "semantic_search_cancel") {
		return semantic_search_cancel(p_args);
	}
	return Dictionary();
}

Dictionary JustAMCPSemanticSearchTools::semantic_search(const Dictionary &p_args) {
#ifdef MODULE_SEMANTICSEARCH_ENABLED
	if (justamcp_is_cancel_requested()) {
		return _semantic_tool_cancelled();
	}
	const String query = p_args.get("query", "");
	if (query.strip_edges().is_empty()) {
		Dictionary err;
		err["ok"] = false;
		err["error"] = "query is required and must be non-empty";
		return err;
	}
	Ref<SemanticSearchBackend> backend = SemanticSearchBackendFactory::create_active_backend();
	if (backend.is_null()) {
		Dictionary err;
		err["ok"] = false;
		err["error"] = "SemanticSearchBackend unavailable";
		return err;
	}
	Dictionary result;
	result["ok"] = true;
	result["backend"] = SemanticSearchBackendFactory::get_effective_backend_name();
	Array tags;
	if (p_args.has("tags")) {
		const Variant tags_var = p_args.get("tags", Array());
		if (tags_var.get_type() == Variant::ARRAY) {
			tags = tags_var;
		}
	}
	PackedStringArray tag_filters;
	for (int i = 0; i < tags.size(); i++) {
		tag_filters.push_back(String(tags[i]));
	}
	const bool require_all = p_args.get("require_all", true);
	const String path_regex = p_args.get("path_regex", "");
	const String class_filter = p_args.get("class_filter", "");
	if (justamcp_is_cancel_requested()) {
		return _semantic_tool_cancelled();
	}
	Array results = backend->search_with_filters(query, int(p_args.get("limit", 20)), tag_filters, require_all, path_regex, class_filter);
	if (justamcp_is_cancel_requested()) {
		return _semantic_tool_cancelled();
	}
	if (SemanticAssetIndex *index = SemanticAssetIndex::get_singleton()) {
		const String filter_error = index->get_last_filter_error();
		if (!filter_error.is_empty()) {
			Dictionary err;
			err["ok"] = false;
			err["error"] = filter_error;
			err["error_code"] = -32602;
			return err;
		}
	}
	result["results"] = results;
	result["structuredContent"] = _semantic_structured_ok(result);
	return result;
#else
	Dictionary err;
	err["ok"] = false;
	err["error"] = "semanticsearch module is not enabled";
	return err;
#endif
}

Dictionary JustAMCPSemanticSearchTools::semantic_find_similar(const Dictionary &p_args) {
#ifdef MODULE_SEMANTICSEARCH_ENABLED
	if (justamcp_is_cancel_requested()) {
		return _semantic_tool_cancelled();
	}
	const String path = _normalize_res_path(p_args.get("path", ""));
	if (path.is_empty()) {
		Dictionary err;
		err["ok"] = false;
		err["error"] = "path is required";
		return err;
	}
	Ref<SemanticSearchBackend> backend = SemanticSearchBackendFactory::create_active_backend();
	if (backend.is_null()) {
		Dictionary err;
		err["ok"] = false;
		err["error"] = "SemanticSearchBackend unavailable";
		return err;
	}
	Dictionary result;
	result["ok"] = true;
	result["backend"] = SemanticSearchBackendFactory::get_effective_backend_name();
	const String path_regex = p_args.get("path_regex", "");
	const String class_filter = p_args.get("class_filter", "");
	if (justamcp_is_cancel_requested()) {
		return _semantic_tool_cancelled();
	}
	Array results = backend->find_similar_with_filters(path, int(p_args.get("limit", 10)), path_regex, class_filter);
	if (justamcp_is_cancel_requested()) {
		return _semantic_tool_cancelled();
	}
	if (SemanticAssetIndex *index = SemanticAssetIndex::get_singleton()) {
		const String filter_error = index->get_last_filter_error();
		if (!filter_error.is_empty()) {
			Dictionary err;
			err["ok"] = false;
			err["error"] = filter_error;
			err["error_code"] = -32602;
			return err;
		}
	}
	result["results"] = results;
	result["structuredContent"] = _semantic_structured_ok(result);
	return result;
#else
	Dictionary err;
	err["ok"] = false;
	err["error"] = "semanticsearch module is not enabled";
	return err;
#endif
}

Dictionary JustAMCPSemanticSearchTools::semantic_index_stats(const Dictionary &p_args) {
	(void)p_args;
#ifdef MODULE_SEMANTICSEARCH_ENABLED
	Ref<SemanticSearchBackend> backend = SemanticSearchBackendFactory::create_active_backend();
	if (backend.is_null()) {
		Dictionary err;
		err["ok"] = false;
		err["error"] = "SemanticSearchBackend unavailable";
		return err;
	}
	Dictionary result;
	result["ok"] = true;
	result["backend"] = SemanticSearchBackendFactory::get_effective_backend_name();
	result["stats"] = backend->get_stats();
	result["structuredContent"] = _semantic_structured_ok(result);
	return result;
#else
	Dictionary err;
	err["ok"] = false;
	err["error"] = "semanticsearch module is not enabled";
	return err;
#endif
}

Dictionary JustAMCPSemanticSearchTools::semantic_rebuild_index(const Dictionary &p_args) {
#ifdef MODULE_SEMANTICSEARCH_ENABLED
	Ref<SemanticSearchBackend> backend = SemanticSearchBackendFactory::create_active_backend();
	if (backend.is_null()) {
		Dictionary err;
		err["ok"] = false;
		err["error"] = "SemanticSearchBackend unavailable";
		return err;
	}
	const Error err = backend->rebuild();
	Dictionary result;
	result["ok"] = err == OK;
	result["backend"] = SemanticSearchBackendFactory::get_effective_backend_name();
	if (err != OK) {
		result["error"] = "Failed to rebuild semantic index";
		result["error_code"] = err;
	}
	result["stats"] = backend->get_stats();
	Dictionary structured;
	structured["ok"] = result["ok"];
	structured["backend"] = result["backend"];
	structured["stats"] = result["stats"];
	if (result.has("error")) {
		structured["error"] = result["error"];
	}
	result["structuredContent"] = structured;
	return result;
#else
	Dictionary err;
	err["ok"] = false;
	err["error"] = "semanticsearch module is not enabled";
	return err;
#endif
}

Dictionary JustAMCPSemanticSearchTools::semantic_search_enqueue(const Dictionary &p_args) {
#ifdef MODULE_SEMANTICSEARCH_ENABLED
	const String query = p_args.get("query", "");
	if (query.strip_edges().is_empty()) {
		Dictionary err;
		err["ok"] = false;
		err["error"] = "query is required and must be non-empty";
		return err;
	}
	SemanticAsyncSearchWorker *worker = SemanticAsyncSearchWorker::get_singleton();
	if (!worker) {
		Dictionary err;
		err["ok"] = false;
		err["error"] = "SemanticAsyncSearchWorker unavailable";
		return err;
	}
	Array tags;
	if (p_args.has("tags")) {
		const Variant tags_var = p_args.get("tags", Array());
		if (tags_var.get_type() == Variant::ARRAY) {
			tags = tags_var;
		}
	}
	PackedStringArray tag_filters;
	for (int i = 0; i < tags.size(); i++) {
		tag_filters.push_back(String(tags[i]));
	}
	const String job_id = worker->enqueue_search_with_filters(
			query,
			int(p_args.get("limit", 20)),
			tag_filters,
			bool(p_args.get("require_all", true)),
			p_args.get("path_regex", ""),
			p_args.get("class_filter", ""));
	Dictionary result;
	result["ok"] = true;
	result["job_id"] = job_id;
	result["finished"] = false;
	Dictionary structured;
	structured["ok"] = true;
	structured["job_id"] = job_id;
	structured["finished"] = false;
	result["structuredContent"] = structured;
	return result;
#else
	Dictionary err;
	err["ok"] = false;
	err["error"] = "semanticsearch module is not enabled";
	return err;
#endif
}

Dictionary JustAMCPSemanticSearchTools::semantic_search_poll(const Dictionary &p_args) {
#ifdef MODULE_SEMANTICSEARCH_ENABLED
	const String job_id = p_args.get("job_id", "");
	if (job_id.is_empty()) {
		Dictionary err;
		err["ok"] = false;
		err["error"] = "job_id is required";
		return err;
	}
	SemanticAsyncSearchWorker *worker = SemanticAsyncSearchWorker::get_singleton();
	if (!worker) {
		Dictionary err;
		err["ok"] = false;
		err["error"] = "SemanticAsyncSearchWorker unavailable";
		return err;
	}
	Dictionary result = worker->poll_search(job_id);
	Dictionary structured = result.duplicate(true);
	result["structuredContent"] = structured;
	return result;
#else
	Dictionary err;
	err["ok"] = false;
	err["error"] = "semanticsearch module is not enabled";
	return err;
#endif
}

Dictionary JustAMCPSemanticSearchTools::semantic_search_cancel(const Dictionary &p_args) {
#ifdef MODULE_SEMANTICSEARCH_ENABLED
	const String job_id = p_args.get("job_id", "");
	if (job_id.is_empty()) {
		Dictionary err;
		err["ok"] = false;
		err["error"] = "job_id is required";
		return err;
	}
	SemanticAsyncSearchWorker *worker = SemanticAsyncSearchWorker::get_singleton();
	if (!worker) {
		Dictionary err;
		err["ok"] = false;
		err["error"] = "SemanticAsyncSearchWorker unavailable";
		return err;
	}
	const bool cancelled = worker->cancel_search(job_id);
	Dictionary result;
	result["ok"] = cancelled;
	result["job_id"] = job_id;
	if (!cancelled) {
		result["error"] = "Unknown search job.";
	}
	Dictionary structured = result.duplicate(true);
	result["structuredContent"] = structured;
	return result;
#else
	Dictionary err;
	err["ok"] = false;
	err["error"] = "semanticsearch module is not enabled";
	return err;
#endif
}

JustAMCPSemanticSearchTools::JustAMCPSemanticSearchTools() {}
JustAMCPSemanticSearchTools::~JustAMCPSemanticSearchTools() {}

#endif
