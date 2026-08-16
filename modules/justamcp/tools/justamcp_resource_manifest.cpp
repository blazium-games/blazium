/**************************************************************************/
/*  justamcp_resource_manifest.cpp                                        */
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

#include "justamcp_resource_manifest.h"

#include "../justamcp_mcp_spec.h"

#include "core/variant/array.h"
#include "core/variant/dictionary.h"
#include "core/variant/variant.h"

#include "modules/modules_enabled.gen.h"

static Dictionary _manifest_resource_schema(const String &p_uri, const String &p_name, const String &p_description, const String &p_mime_type = "application/json") {
	Dictionary resource;
	resource["uri"] = p_uri;
	resource["name"] = p_name;
	resource["description"] = p_description;
	resource["mimeType"] = p_mime_type;
	justamcp_attach_icons(resource);
	return resource;
}

static Dictionary _manifest_resource_template_schema(const String &p_uri_template, const String &p_name, const String &p_description, const String &p_mime_type = "application/json") {
	Dictionary resource;
	resource["uriTemplate"] = p_uri_template;
	resource["name"] = p_name;
	resource["description"] = p_description;
	resource["mimeType"] = p_mime_type;
	justamcp_attach_icons(resource);
	return resource;
}

Array JustAMCPResourceManifest::get_static_resource_schemas() {
	Array resources;
	resources.push_back(_manifest_resource_schema("blazium://sessions", "MCP Sessions", "Active JustAMCP editor session metadata."));
	resources.push_back(_manifest_resource_schema("blazium://scene/current", "Current Scene", "Current scene path and play state."));
	resources.push_back(_manifest_resource_schema("blazium://scene/hierarchy", "Scene Hierarchy", "Flattened hierarchy for the active edited scene."));
	resources.push_back(_manifest_resource_schema("blazium://selection/current", "Current Selection", "Currently selected editor nodes."));
	resources.push_back(_manifest_resource_schema("blazium://project/info", "Project Info", "Project name, engine version, paths, active scene, and play state."));
	resources.push_back(_manifest_resource_schema("blazium://project/settings", "Project Settings", "Common project settings subset."));
	resources.push_back(_manifest_resource_schema("blazium://logs/recent", "Recent Logs", "Recent JustAMCP engine log lines."));
	resources.push_back(_manifest_resource_schema("blazium://materials", "Materials", "Material resources found under res://."));
	resources.push_back(_manifest_resource_schema("blazium://input_map", "Input Map", "Project input actions and their configured events."));
	resources.push_back(_manifest_resource_schema("blazium://performance", "Performance", "Performance singleton snapshot."));
	resources.push_back(_manifest_resource_schema("blazium://docs/classes", "Blazium Documentation Classes", "Internal class documentation summaries from the editor documentation database."));
	resources.push_back(_manifest_resource_schema("blazium://guide/testing-loop", "Testing Loop Guide", "How to run, drive, inspect, screenshot, and stop a running game through JustAMCP.", "text/markdown"));
	resources.push_back(_manifest_resource_schema("blazium://guide/scene-editing", "Scene Editing Guide", "How to choose scene, node, resource, signal, and group editing tools.", "text/markdown"));
	resources.push_back(_manifest_resource_schema("blazium://guide/asset-generation", "Asset Generation Guide", "How SVG-to-PNG asset generation works and how sizing options are applied.", "text/markdown"));
	resources.push_back(_manifest_resource_schema("blazium://guide/troubleshooting", "Troubleshooting Guide", "Common failures and recovery steps for runtime, project, and tool workflows.", "text/markdown"));
	resources.push_back(_manifest_resource_schema("blazium://guide/tool-index", "Tool Index Guide", "Goal-oriented guide to the main JustAMCP tool families.", "text/markdown"));
	resources.push_back(_manifest_resource_schema("blazium://guide/asset-tagging", "Asset Tagging Guide", "How to list, assign, verify, and search project asset tags through JustAMCP.", "text/markdown"));
	resources.push_back(_manifest_resource_schema("blazium://editor/state", "Editor State", "Current editor play mode, active scene, and selection summary."));
	resources.push_back(_manifest_resource_schema("blazium://test/results", "Test Results", "Latest Autowork test results when available."));
#ifdef MODULE_ASSETTAGS_ENABLED
	resources.push_back(_manifest_resource_schema("blazium://tags/dictionary", "Asset Tag Dictionary", "Project asset tag dictionary tree."));
#endif
	return resources;
}

Array JustAMCPResourceManifest::get_static_resource_template_schemas() {
	Array templates;
	templates.push_back(_manifest_resource_template_schema("blazium://node/{path}/properties", "Node Properties", "Editor-visible properties for a node path such as blazium://node/Main/Camera3D/properties."));
	templates.push_back(_manifest_resource_template_schema("blazium://node/{path}/children", "Node Children", "Direct children for a node path such as blazium://node/Main/children."));
	templates.push_back(_manifest_resource_template_schema("blazium://node/{path}/groups", "Node Groups", "Groups assigned to a node path such as blazium://node/Main/Enemy/groups."));
	templates.push_back(_manifest_resource_template_schema("blazium://script/{path}", "Script Source", "Read a script by omitting the res:// prefix, such as blazium://script/scripts/player.gd."));
	templates.push_back(_manifest_resource_template_schema("blazium://docs/search/{query}", "Documentation Search", "Search internal Blazium class and member documentation."));
	templates.push_back(_manifest_resource_template_schema("blazium://docs/class/{class_name}", "Class Documentation", "Read internal documentation for one Blazium class."));
	templates.push_back(_manifest_resource_template_schema("blazium://docs/member/{class_name}/{member_type}/{member_name}", "Member Documentation", "Read internal documentation for one method, property, signal, constant, enum, annotation, or theme property."));
	templates.push_back(_manifest_resource_template_schema("blazium://logs/mcp/cursor/{cursor}", "MCP Log Notifications", "Paginated buffered MCP notifications/message history. Use blazium://logs/mcp or blazium://logs/mcp/start for the first page."));
	templates.push_back(_manifest_resource_template_schema("blazium://logs/recent/cursor/{cursor}", "Recent Engine Logs", "Paginated recent engine log lines captured by JustAMCP. Use blazium://logs/recent or blazium://logs/recent/start for the first page."));
	templates.push_back(_manifest_resource_template_schema("blazium://system/logs/cursor/{cursor}", "System Logs (Paginated)", "Paginated engine log lines as JSON. Use blazium://system/logs or blazium://system/logs/start for the first page."));
#ifdef MODULE_ASSETTAGS_ENABLED
	templates.push_back(_manifest_resource_template_schema("blazium://tags/asset/{path}", "Asset Tags", "Tags assigned to one res:// asset path."));
	templates.push_back(_manifest_resource_template_schema("blazium://tags/dictionary/cursor/{cursor}", "Asset Tag Dictionary (Paginated)", "Paginated asset tag dictionary entries."));
	templates.push_back(_manifest_resource_template_schema("blazium://assets/by-tag/{tag}", "Assets By Tag", "Assets referencing a tag name."));
	templates.push_back(_manifest_resource_template_schema("blazium://assets/by-tag/{tag}/cursor/{cursor}", "Assets By Tag (Paginated)", "Paginated assets referencing a tag name."));
#endif
#ifdef MODULE_SEMANTICSEARCH_ENABLED
	templates.push_back(_manifest_resource_template_schema("blazium://semantic/search/{query}", "Semantic Search", "Semantic search results for a query string (backend per project settings)."));
	templates.push_back(_manifest_resource_template_schema("blazium://semantic/search/{query}/limit/{limit}", "Semantic Search (Limited)", "Semantic search results with an explicit result limit."));
	templates.push_back(_manifest_resource_template_schema("blazium://semantic/search/{query}/tags/{tag1,tag2}/require_all/{true|false}", "Semantic Search (Tag Filtered)", "Semantic search filtered by asset tags."));
	templates.push_back(_manifest_resource_template_schema("blazium://semantic/search/{query}/cursor/{cursor}", "Semantic Search (Paginated)", "Paginated semantic search results."));
	templates.push_back(_manifest_resource_template_schema("blazium://semantic/similar/{path}", "Semantic Similar Assets", "Assets similar to a res:// path."));
	templates.push_back(_manifest_resource_template_schema("blazium://semantic/similar/{path}/limit/{limit}", "Semantic Similar Assets (Limited)", "Similar assets with an explicit result limit."));
	templates.push_back(_manifest_resource_template_schema("blazium://semantic/index/stats", "Semantic Index Stats", "Semantic search index statistics."));
	templates.push_back(_manifest_resource_template_schema("blazium://semantic/index/rebuild", "Semantic Index Rebuild", "Rebuild the semantic search index from tagged assets."));
	templates.push_back(_manifest_resource_template_schema("blazium://semantic/asset/{path}", "Semantic Asset Entry", "Semantic index entry for one res:// asset path."));
#endif
	return templates;
}

#endif
