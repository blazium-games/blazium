/**************************************************************************/
/*  justamcp_guides_resource_provider.cpp                                 */
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

#include "justamcp_guides_resource_provider.h"

#include "justamcp_resource_json.h"

bool JustAMCPGuidesResourceProvider::can_read(const String &p_canonical_uri) {
	return p_canonical_uri.begins_with("blazium://guide/");
}

Dictionary JustAMCPGuidesResourceProvider::read(const String &p_uri, const String &p_canonical_uri) {
	String slug = p_canonical_uri.substr(String("blazium://guide/").length());
	String title;
	String body;

	if (slug == "testing-loop") {
		title = "Testing a Running Blazium Game from MCP";
		body = "1. Start the game with `blazium_editor_play_scene` or `blazium_editor_play_main`.\n"
			   "2. Check `blazium_editor_is_playing`, `blazium_runtime_info`, and `blazium_editor_get_errors` before sending input.\n"
			   "3. Drive input with `blazium_simulate_key`, `blazium_simulate_mouse_click`, `blazium_simulate_action`, or `blazium_send_input` when a runtime bridge is active.\n"
			   "4. Use `blazium_wait` between actions, then inspect state with `blazium_runtime_inspect_node`, `blazium_query_runtime_node`, or `blazium_take_game_screenshot`.\n"
			   "5. Stop with `blazium_editor_stop_play` before editing scripts that run every frame.\n";
	} else if (slug == "scene-editing") {
		title = "Scene Editing Patterns";
		body = "- Create scenes with `blazium_create_scene` and add nodes with `blazium_add_node`.\n"
			   "- Use `blazium_set_node_properties` for multiple node values and `blazium_set_resource_property` for nested resources.\n"
			   "- Replace common node resources with `blazium_set_collision_shape`, `blazium_set_sprite_texture`, `blazium_set_mesh`, and `blazium_set_material`.\n"
			   "- Persist node-attached resources with `blazium_save_resource_to_file`.\n"
			   "- Verify signal wiring with `blazium_list_connections`, `blazium_list_node_signals`, and `blazium_has_signal_connection`.\n";
	} else if (slug == "asset-generation") {
		title = "Generating 2D Assets";
		body = "`blazium_asset_generate_2d_asset` renders SVG directly through the engine image loader and saves a PNG under the project. "
			   "Prefer explicit SVG width, height, and viewBox values for predictable output. Use width, height, or scale arguments to control rasterization size when available.\n";
	} else if (slug == "troubleshooting") {
		title = "Troubleshooting";
		body = "- If runtime inspection fails, confirm the game is running and the JustAMCP runtime bridge is enabled.\n"
			   "- If new files do not appear, call `blazium_rescan_filesystem` or reopen the editor filesystem dock.\n"
			   "- If an API call fails, verify the target class with `blazium_classdb_query` or `blazium_docs_get_class` before editing code.\n"
			   "- If project settings were edited, restart or reload the project when the setting is only read during startup.\n";
	} else if (slug == "tool-index") {
		title = "JustAMCP Quick Tool Index by Goal";
		body = "JustAMCP groups common Blazium editor, runtime, documentation, and asset workflows into these tool families:\n"
			   "- Files and scripts: `blazium_read_file`, `blazium_create_script`, `blazium_edit_script`, `blazium_validate_script`, `blazium_search_in_scripts`.\n"
			   "- Scenes and nodes: `blazium_create_scene`, `blazium_list_scene_nodes`, `blazium_add_node`, `blazium_duplicate_node`, `blazium_scene_tree_dump`.\n"
			   "- Project: `blazium_project_list_settings`, `blazium_project_update_settings`, `blazium_project_get_input_actions`, `blazium_project_set_input_action`.\n"
			   "- Runtime: `blazium_editor_play_scene`, `blazium_runtime_info`, `blazium_wait`, `blazium_take_game_screenshot`, `blazium_editor_stop_play`.\n"
			   "- Docs: `blazium_docs_search`, `blazium_docs_get_class`, `blazium_classdb_query`.\n"
			   "- Asset tags: `blazium_tags_list`, `blazium_tags_set_on_asset`, `blazium_tags_find_assets`, `blazium_tags_search_assets`. Read `blazium://tags/dictionary` and use prompt `blazium_asset_tagging_workflow`.\n";
	} else if (slug == "asset-tagging") {
		title = "Asset Tagging";
		body = "1. Read `blazium://guide/asset-tagging` and `blazium://tags/dictionary`.\n"
			   "2. List tags with `blazium_tags_list` and inspect details with `blazium_tags_get_info`.\n"
			   "3. Assign tags with `blazium_tags_set_on_asset`, `blazium_tags_add_to_asset`, or remove with `blazium_tags_remove_from_asset`.\n"
			   "4. Verify assignments with `blazium_tags_get_on_asset` or `blazium_tags_find_assets`.\n"
			   "5. Search across assets with `blazium_tags_search_assets`. Mutating dictionary tools require explicit user permission.\n";
	} else {
		return JustAMCPResourceJson::make_json_error(p_uri, "Unknown guide: " + slug);
	}

	return JustAMCPResourceJson::make_text_contents(p_uri, "# " + title + "\n\n" + body);
}

#endif
