/**************************************************************************/
/*  justamcp_category_schemas.cpp                                         */
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

#include "justamcp_category_schemas.h"

#include "justamcp_tool_schema_builder.h"

void JustAMCPCategorySchemas::register_category_schemas(const JustAMCPCategorySchemaContext &p_ctx) {
	auto add_schema = [&](const String &p_name, const String &p_desc, const Vector<String> &p_props, const Vector<String> &p_req, const String &p_task_support = "forbidden", const String &p_thread_affinity = "") {
		p_ctx.add_schema(p_name, p_desc, p_props, p_req, p_task_support, p_thread_affinity);
	};
	String &current_category = *p_ctx.current_category;
	bool &is_core = *p_ctx.is_core;

	current_category = "editor_tools";
	is_core = false;
	add_schema("editor_play_scene", "Runs the currently active or specified scene. With duration_ms, plays, delivers timed inputs (kind/type action|key|mouse|target|motion, at_ms, hold_ms), captures a screenshot, and stops.",
			Vector<String>{ "scene_path", "string", "duration_ms", "number", "inputs", "array", "prompt", "string" }, Vector<String>{}, "optional");
	add_schema("editor_run_scene", "Alias of editor_play_scene for timed playtest runs with duration_ms, inputs (kind/type, at_ms, hold_ms, action/key/x/y/target), screenshot, and delivery report.",
			Vector<String>{ "scene_path", "string", "duration_ms", "number", "inputs", "array", "prompt", "string" }, Vector<String>{}, "optional");
	add_schema("editor_play_main", "Runs the project's main scene.",
			Vector<String>{}, Vector<String>{});
	add_schema("editor_stop_play", "Terminates an active play session.",
			Vector<String>{}, Vector<String>{});
	add_schema("editor_is_playing", "Checks if a play session is currently active natively.",
			Vector<String>{}, Vector<String>{});
	add_schema("editor_select_node", "Changes the user's active selection in the Scene Tree dock.",
			Vector<String>{ "node_paths", "array" }, Vector<String>{ "node_paths" });
	add_schema("editor_get_selected", "Retrieves the currently selected nodes.",
			Vector<String>{}, Vector<String>{});
	add_schema("editor_undo", "Triggers Editor global undo action.",
			Vector<String>{}, Vector<String>{});
	add_schema("editor_redo", "Triggers Editor global redo action.",
			Vector<String>{}, Vector<String>{});
	add_schema("editor_take_screenshot", "Captures the editor or runtime viewport. Optional view (2D, 3D, Script) and scale.",
			Vector<String>{ "view", "string", "scale", "number", "prompt", "string", "path", "string" }, Vector<String>{});
	add_schema("editor_set_main_screen", "Switches between 2D, 3D, Script, and AssetLib views.",
			Vector<String>{ "screen_name", "string" }, Vector<String>{ "screen_name" });
	add_schema("editor_open_scene", "Invokes the editor natively to swap active tabs opening a given `.tscn` file onto the viewport.",
			Vector<String>{ "path", "string" }, Vector<String>{ "path" });
	add_schema("editor_get_settings", "Inspects current user-level settings configurations applied in EditorSettings directly.",
			Vector<String>{ "setting", "string" }, Vector<String>{ "setting" });
	add_schema("editor_set_settings", "Manipulates current user-level Editor configurations dynamically applying layout changes instantly.",
			Vector<String>{ "setting", "string", "value", "any" }, Vector<String>{ "setting", "value" });
	add_schema("editor_clear_output", "Clears the editor output panel through the native command palette.",
			Vector<String>{}, Vector<String>{});
	add_schema("editor_screenshot_game", "Captures the visible game display to a PNG file.",
			Vector<String>{}, Vector<String>{});
	add_schema("editor_get_output_log", "Returns recent output lines captured by the JustAMCP engine log hook.",
			Vector<String>{ "limit", "number", "cursor", "string" }, Vector<String>{}, "forbidden", "worker");
	add_schema("editor_get_errors", "Returns recent error and warning lines captured by the JustAMCP engine log hook.",
			Vector<String>{ "limit", "number" }, Vector<String>{}, "forbidden", "worker");
	add_schema("logs_read", "Reads recent JustAMCP/editor log lines with optional filtering and MCP notification replay.",
			Vector<String>{ "limit", "number", "since_index", "number", "source", "string", "cursor", "string" }, Vector<String>{}, "forbidden", "worker");
	add_schema("editor_reload_project", "Requests an editor restart to reload the project.",
			Vector<String>{ "save", "boolean" }, Vector<String>{}, "optional");
	add_schema("editor_save_all_scenes", "Saves all open editor scenes.",
			Vector<String>{}, Vector<String>{});
	add_schema("editor_get_signals", "Lists signals for a class or node in the edited scene.",
			Vector<String>{ "class_name", "string", "node_path", "string" }, Vector<String>{});
	add_schema("qa_start", "Starts a play session then pauses the SceneTree through JustAMCPRuntime. Fails if the runtime singleton is not live.",
			Vector<String>{ "scene", "string", "scene_path", "string", "boot_ms", "number" }, Vector<String>{});
	add_schema("qa_act", "Unpauses, delivers inputs, advances frames or until a probe is true, then pauses again. Requires a live JustAMCPRuntime.",
			Vector<String>{ "inputs", "array", "probes", "array", "advance_frames", "number", "until", "string" }, Vector<String>{});
	add_schema("qa_observe", "Evaluates probe expressions on the running tree while paused. Requires a live JustAMCPRuntime.",
			Vector<String>{ "probes", "array" }, Vector<String>{});
	add_schema("qa_watch", "Connects signal taps through JustAMCPRuntime. Later qa replies can include those events.",
			Vector<String>{ "signals", "array" }, Vector<String>{ "signals" });
	add_schema("qa_drive", "Evaluates a short sandboxed Expression against the running game through JustAMCPRuntime.",
			Vector<String>{ "expr", "string", "code", "string" }, Vector<String>{});
	add_schema("qa_stop", "Unpauses and stops the QA play session.",
			Vector<String>{}, Vector<String>{});
	add_schema("open_in_blazium", "Opens a script or scene path in the Blazium editor.",
			Vector<String>{ "path", "string", "line", "number" }, Vector<String>{ "path" });
	add_schema("rescan_filesystem", "Triggers the editor filesystem to rescan project files.",
			Vector<String>{}, Vector<String>{}, "required");
	add_schema("scene_tree_dump", "Dumps the edited or file-based scene tree with optional filters, pagination, and XML format.",
			Vector<String>{ "max_nodes", "number", "file_path", "string", "scene_path", "string", "root_node_path", "string", "max_depth", "number", "offset", "number", "limit", "number", "mode", "string", "format", "string", "type_filter", "string", "type_filter_inherit", "boolean", "name_filter", "string", "group_filter", "string", "group_filter_mode", "string", "script_filter", "string" }, Vector<String>{});

	current_category = "documentation_tools";
	is_core = true;
	add_schema("docs_list_classes", "Lists internally registered Blazium class documentation summaries from the editor documentation database.",
			Vector<String>{ "query", "string", "limit", "number" }, Vector<String>{}, "forbidden", "worker");
	add_schema("docs_search", "Searches internal Blazium documentation across classes, methods, properties, signals, and constants.",
			Vector<String>{ "query", "string", "member_type", "string", "include_members", "boolean", "limit", "number" }, Vector<String>{ "query" }, "forbidden", "worker");
	add_schema("docs_get_class", "Reads full internal documentation for a Blazium class, optionally including all members.",
			Vector<String>{ "class_name", "string", "include_members", "boolean" }, Vector<String>{ "class_name" }, "forbidden", "worker");
	add_schema("docs_get_member", "Reads internal documentation for a specific class member such as a method, property, signal, constant, enum, or annotation.",
			Vector<String>{ "class_name", "string", "member_type", "string", "member_name", "string" }, Vector<String>{ "class_name", "member_type", "member_name" }, "forbidden", "worker");
	add_schema("classdb_query", "Queries ClassDB for properties, methods, signals, constants, and inheritance for a class.",
			Vector<String>{ "class_name", "string", "query", "string", "include_virtual", "boolean" }, Vector<String>{ "class_name" }, "forbidden", "worker");

	current_category = "networking_tools";
	is_core = false;
	add_schema("networking_create_http_request", "Spawns and configures an HTTPRequest node.",
			Vector<String>{ "parent_path", "string", "name", "string", "timeout", "number" }, Vector<String>{ "parent_path" });
	add_schema("networking_setup_websocket", "Configures WebSocket connection boilerplate.",
			Vector<String>{ "parent_path", "string", "mode", "string", "name", "string" }, Vector<String>{ "parent_path" });
	add_schema("networking_setup_multiplayer", "Instantiates a MultiplayerManager structure.",
			Vector<String>{ "parent_path", "string", "transport", "string", "mode", "string", "address", "string", "port", "number", "max_clients", "number" }, Vector<String>{ "parent_path" });
	add_schema("networking_setup_rpc", "Configures RPC (Remote Procedure Call) capabilities onto nodes appending execution mappings natively.",
			Vector<String>{ "node_path", "string", "method_name", "string", "rpc_mode", "string", "transfer_mode", "string", "call_local", "boolean", "channel", "number" }, Vector<String>{ "node_path", "method_name" });
	add_schema("networking_setup_sync", "Instantiates a MultiplayerSynchronizer evaluating a properties array to replicate values efficiently.",
			Vector<String>{ "parent_path", "string", "name", "string", "properties", "array" }, Vector<String>{ "parent_path", "properties" });
	add_schema("networking_get_info", "Returns current multiplayer peer and connection information.",
			Vector<String>{}, Vector<String>{});

#ifdef MODULE_MULTIUSER_EDITOR_ENABLED

	current_category = "multiuser_tools";
	is_core = false;
	add_schema("multiuser_get_status", "Returns the active collaborative session status, including the local peer ID and connection state natively out of the Multiplayer network node.",
			Vector<String>{}, Vector<String>{});
	add_schema("multiuser_send_chat", "Dispatches a global text string natively to all uniquely connected remote peer instances across the Editor environment.",
			Vector<String>{ "message", "string" }, Vector<String>{ "message" });
	add_schema("multiuser_kick_peer", "Removes a target user peer explicitly from the host environment by ID (Action is silently ignored if the AI host agent lacks active networking privileges).",
			Vector<String>{ "peer_id", "string" }, Vector<String>{ "peer_id" });
	add_schema("multiuser_trigger_autowork", "Issues a distributed RPC signaling all securely connected peers to automatically boot Godot's Autowork pipeline evaluating unit tests concurrently traversing the network.",
			Vector<String>{}, Vector<String>{});
#endif

	current_category = "spatial_tools";
	is_core = false;
	add_schema("spatial_analyze_layout", "Analyzes node density and spacing in a target area.",
			Vector<String>{ "node_path", "string", "include_2d", "boolean", "include_3d", "boolean" }, Vector<String>{});
	add_schema("spatial_suggest_placement", "Calculates empty coordinates suitable for placing nodes.",
			Vector<String>{ "node_type", "string", "context", "string", "parent_path", "string" }, Vector<String>{ "parent_path" });
	add_schema("spatial_detect_overlaps", "Identifies intersecting 3d nodes based on a distance threshold.",
			Vector<String>{ "node_path", "string", "threshold", "number" }, Vector<String>{});
	add_schema("spatial_measure_distance", "Calculates precise pathing distances between nodes.",
			Vector<String>{ "from_path", "string", "to_path", "string" }, Vector<String>{ "from_path", "to_path" });
	add_schema("spatial_bake_navigation", "Asynchronously forces an Editor NavRegion to bake its internal navigation polygons or meshes.",
			Vector<String>{ "node_path", "string", "on_thread", "boolean" }, Vector<String>{ "node_path" });
	add_schema("navigation_set_layers", "Reconfigures navigational layers routing specific bitmask mapping directly over active NavRegions accurately.",
			Vector<String>{ "node_path", "string", "layers", "number" }, Vector<String>{ "node_path", "layers" });
	add_schema("navigation_get_info", "Lists navigation regions and agents below a scene node.",
			Vector<String>{ "node_path", "string" }, Vector<String>{});

	current_category = "runtime_tools";
	is_core = false;
	add_schema("execute_gdscript_snippet", "Evaluates a short in-editor Expression. This is Expression-only and not a headless subprocess runner.",
			Vector<String>{ "code", "string", "target_node", "string" }, Vector<String>{ "code" });
	add_schema("signal_emit", "Fires Godot Event bindings internally passing variable parameter objects mapping signal arguments natively across local instances.",
			Vector<String>{ "node_path", "string", "signal_name", "string", "args", "array" }, Vector<String>{ "node_path", "signal_name" });
	add_schema("runtime_capture_output", "Returns recent native output lines captured during active execution.",
			Vector<String>{ "lines", "number", "clear", "boolean" }, Vector<String>{});
	add_schema("runtime_compare_screenshots", "Compares two sandboxed PNG paths and reports pixel differences.",
			Vector<String>{ "image_a", "string", "image_b", "string", "threshold", "number" }, Vector<String>{ "image_a", "image_b" });
	add_schema("runtime_record_video", "Records PNG frame sequences to res://.video_recordings/. action is start or stop.",
			Vector<String>{ "action", "string" }, Vector<String>{ "action" });
	add_schema("take_game_screenshot", "Captures the running game's viewport as a PNG base64 blob natively from the active viewport.",
			Vector<String>{}, Vector<String>{});
	add_schema("runtime_info", "Snapshots active engine state including FPS, current scene, and frame counters.",
			Vector<String>{}, Vector<String>{});
	add_schema("runtime_get_errors", "Polls the engine's internal error and warning logs incrementally.",
			Vector<String>{ "since_index", "number" }, Vector<String>{});
	add_schema("runtime_capabilities", "Lists all blazium_ commands currently exposed by this MCP session.",
			Vector<String>{}, Vector<String>{});
	add_schema("eval_expression", "Evaluates a sandboxed GDScript expression for rapid diagnostic querying.",
			Vector<String>{ "expr", "string" }, Vector<String>{ "expr" });
	add_schema("runtime_quit", "Forcibly terminates the running game process synchronously.",
			Vector<String>{}, Vector<String>{});
	add_schema("get_network_info", "Retrieves multiplayer state including peer IDs and server status natively.",
			Vector<String>{}, Vector<String>{});
	add_schema("get_audio_info", "Snapshots the current AudioServer bus layout including volumes and mute states.",
			Vector<String>{}, Vector<String>{});
	add_schema("run_custom_command", "Invokes a user-registered custom command on the JustAMCPRuntime singleton.",
			Vector<String>{ "name", "string", "args", "array" }, Vector<String>{ "name" });
	add_schema("runtime_get_tree", "Returns a serialized runtime scene tree from the JustAMCP runtime singleton.",
			Vector<String>{ "root", "string", "max_depth", "number", "include_properties", "boolean" }, Vector<String>{});
	add_schema("runtime_inspect_node", "Returns runtime node metadata and properties for a specific node path.",
			Vector<String>{ "node", "string", "include_properties", "boolean" }, Vector<String>{ "node" });
	add_schema("runtime_run_autowork_tests", "Runs Autowork tests in-process. path may be a res:// or uid:// test script or directory; filter matches method names; timeout is seconds (default 30). Optional include_subdirs, prefix, suffix, select, and sandboxed user:// junit_xml.",
			Vector<String>{ "path", "string", "filter", "string", "timeout", "number", "test_script", "string", "directory_path", "string", "test_name", "string", "include_subdirs", "boolean", "prefix", "string", "suffix", "string", "select", "string", "junit_xml", "string" }, Vector<String>{});
	add_schema("runtime_get_test_results", "Returns the latest Autowork test result snapshot from user://autowork_results.json.",
			Vector<String>{}, Vector<String>{});
	add_schema("wait", "Sleeps server-side for a bounded interval to let runtime/editor state settle.",
			Vector<String>{ "ms", "number", "seconds", "number" }, Vector<String>{});
	add_schema("get_runtime_status", "Returns editor play state and runtime bridge availability.",
			Vector<String>{}, Vector<String>{});
	add_schema("get_runtime_log", "Returns recent JustAMCP runtime/editor log lines.",
			Vector<String>{ "limit", "number" }, Vector<String>{});
	add_schema("query_runtime_node", "Reads a live runtime node through JustAMCPRuntime when the bridge is active.",
			Vector<String>{ "node_path", "string", "properties", "array", "include_children", "boolean" }, Vector<String>{ "node_path" });
	add_schema("runtime_get_autoload", "Reads runtime autoload configuration and loaded singleton node state.",
			Vector<String>{ "name", "string" }, Vector<String>{ "name" });
	add_schema("runtime_find_nodes_by_script", "Finds live runtime nodes whose script path or global class matches.",
			Vector<String>{ "script_path", "string", "class_name", "string", "limit", "number" }, Vector<String>{});
	add_schema("runtime_batch_get_properties", "Reads multiple properties from multiple live runtime nodes.",
			Vector<String>{ "node_paths", "array", "properties", "array" }, Vector<String>{ "node_paths", "properties" });
	add_schema("runtime_find_ui_elements", "Finds visible Control nodes by text and/or class.",
			Vector<String>{ "text", "string", "ui_type", "string", "visible_only", "boolean", "limit", "number" }, Vector<String>{});
	add_schema("runtime_click_button_by_text", "Triggers a live BaseButton whose text matches the query.",
			Vector<String>{ "text", "string" }, Vector<String>{ "text" });
	add_schema("runtime_move_node", "Sets a live runtime node's position-like property.",
			Vector<String>{ "node", "string", "position", "object", "property", "string" }, Vector<String>{ "node", "position" });
	add_schema("runtime_monitor_properties", "Snapshots selected live runtime node properties.",
			Vector<String>{ "node", "string", "properties", "array" }, Vector<String>{ "node", "properties" });

	current_category = "scene_tools";
	is_core = true;
	add_schema("create_scene", "Creates a new hierarchy branch root instantiating complex node definitions mapped onto empty templates efficiently initializing standard root dependencies safely.",
			Vector<String>{ "projectPath", "string", "scenePath", "string", "rootNodeType", "string", "properties", "object" }, Vector<String>{ "scenePath" });
	add_schema("scene_create_inherited", "Constructs an explicitly derived scene cloning base structural settings automatically generating inheritance graphs locally bypassing standard empty node creations.",
			Vector<String>{ "projectPath", "string", "baseScenePath", "string", "newScenePath", "string" }, Vector<String>{ "baseScenePath", "newScenePath" });
	add_schema("list_scene_nodes", "Lists the hierarchical node structure of a given scene.",
			Vector<String>{ "scene_path", "string", "depth", "number" }, Vector<String>{ "scene_path" });
	add_schema("get_node_warnings", "Returns configuration warnings for nodes in the open editor scene, or a PackedScene oracle via file_path.",
			Vector<String>{ "file_path", "string", "scene_path", "string" }, Vector<String>{});
	add_schema("get_scene_file_content", "Reads the raw scene file text for inspection.",
			Vector<String>{ "scenePath", "string", "path", "string" }, Vector<String>{});
	add_schema("delete_scene", "Deletes a scene file from the project.",
			Vector<String>{ "scenePath", "string", "path", "string" }, Vector<String>{});
	add_schema("get_scene_exports", "Lists exported/editor-visible properties on nodes in a scene.",
			Vector<String>{ "scenePath", "string", "path", "string" }, Vector<String>{});
	add_schema("scene_get_current", "Returns the currently edited scene root and path.",
			Vector<String>{}, Vector<String>{});
	add_schema("scene_list_open", "Lists open editor scene tabs and the active scene.",
			Vector<String>{}, Vector<String>{});
	add_schema("scene_set_current", "Switches the active editor scene tab by opening a scene path.",
			Vector<String>{ "path", "string", "scenePath", "string" }, Vector<String>{});
	add_schema("scene_reload", "Reloads a scene path or the current edited scene.",
			Vector<String>{ "path", "string", "scenePath", "string" }, Vector<String>{});
	add_schema("scene_duplicate_file", "Copies a scene file to a new project path.",
			Vector<String>{ "source_path", "string", "dest_path", "string" }, Vector<String>{ "source_path", "dest_path" });
	add_schema("scene_close", "Closes the current edited scene tab, or a specified open scene by path.",
			Vector<String>{ "path", "string", "scenePath", "string", "save", "boolean" }, Vector<String>{});
	add_schema("add_node", "Adds a new node to a scene tree structure.",
			Vector<String>{ "scene_path", "string", "parent_path", "string", "node_type", "string", "node_name", "string", "properties", "object" },
			Vector<String>{ "scene_path", "parent_path", "node_type" });
	add_schema("find_nodes", "Deep searches the scene tree by name, type, and/or group simultaneously.",
			Vector<String>{ "name", "string", "type", "string", "group", "string", "limit", "number" }, Vector<String>{});
	add_schema("get_node_property", "Reads a targeted property from a specific node.",
			Vector<String>{ "node", "string", "property", "string" }, Vector<String>{ "node", "property" });
	add_schema("call_node_method", "Safely invokes a method on a node with arguments.",
			Vector<String>{ "node", "string", "method", "string", "args", "array" }, Vector<String>{ "node", "method" });
	add_schema("wait_for_property", "Polls until a node property matches an expected value or confirms the current state.",
			Vector<String>{ "node", "string", "property", "string", "value", "any" }, Vector<String>{ "node", "property", "value" });
	add_schema("press_button", "Finds a BaseButton-derived node by name and forcibly triggers its pressed signal.",
			Vector<String>{ "name", "string" }, Vector<String>{ "name" });
	add_schema("delete_node", "Removes a specific node from a scene hierarchy.",
			Vector<String>{ "scene_path", "string", "node_path", "string" }, Vector<String>{ "scene_path", "node_path" });
	add_schema("duplicate_node", "Duplicates an existing node within a scene.",
			Vector<String>{ "scene_path", "string", "node_path", "string", "new_name", "string" }, Vector<String>{ "scene_path", "node_path" });
	add_schema("reparent_node", "Changes the parent of a specific node within a scene.",
			Vector<String>{ "scene_path", "string", "node_path", "string", "new_parent_path", "string" }, Vector<String>{ "scene_path", "node_path", "new_parent_path" });
	add_schema("set_node_properties", "Modifies the internal properties of a specified node.",
			Vector<String>{ "scene_path", "string", "node_path", "string", "properties", "object" }, Vector<String>{ "scene_path", "node_path", "properties" });
	add_schema("modify_node_property", "Compatibility wrapper for setting one property on a scene node.",
			Vector<String>{ "scene_path", "string", "node_path", "string", "property", "string", "value", "any" }, Vector<String>{ "node_path", "property", "value" });
	add_schema("get_node_properties", "Reads local serialized properties excluding Godot defaults directly resolving node attributes.",
			Vector<String>{ "node_path", "string", "include_defaults", "boolean" }, Vector<String>{ "node_path" });
	add_schema("create_area_2d", "Creates an Area2D wrapper explicitly configured with standard scene routing.",
			Vector<String>{ "node_name", "string", "parent_node_path", "string", "properties", "object" }, Vector<String>{ "node_name" });
	add_schema("create_line_2d", "Instantiates a Line2D vector series translating simple JS coordinate arrays smoothly.",
			Vector<String>{ "node_name", "string", "parent_node_path", "string", "points", "array", "properties", "object" }, Vector<String>{ "node_name" });
	add_schema("create_polygon_2d", "Shapes a geometric Polygon2D boundary instantiating properly across the parent region context.",
			Vector<String>{ "node_name", "string", "parent_node_path", "string", "points", "array", "properties", "object" }, Vector<String>{ "node_name" });
	add_schema("create_csg_shape", "Generates CSG primitives (CSGBox3D, CSGSphere3D) translating explicit dimensional inputs into 3D vectors intelligently.",
			Vector<String>{ "node_name", "string", "parent_node_path", "string", "shape_type", "string", "width", "number", "height", "number", "depth", "number", "radius", "number", "properties", "object" }, Vector<String>{ "node_name" });
	add_schema("instance_scene", "Spawns an active nested Hierarchy loading an external `.tscn` packed resource.",
			Vector<String>{ "projectPath", "string", "scenePath", "string", "instanceScenePath", "string", "parentNodePath", "string", "nodeName", "string", "properties", "object" }, Vector<String>{ "instanceScenePath" });
	add_schema("setup_camera_2d", "Generates Camera2D structure setting zoom ratios or smoothing filters directly.",
			Vector<String>{ "projectPath", "string", "scenePath", "string", "parentNodePath", "string", "nodeName", "string", "zoom", "number", "smoothing", "boolean", "properties", "object" }, Vector<String>{});
	add_schema("setup_parallax_2d", "Initializes empty ParallaxBackground arrays managing nested tracking.",
			Vector<String>{ "projectPath", "string", "scenePath", "string", "parentNodePath", "string", "nodeName", "string", "properties", "object" }, Vector<String>{});
	add_schema("create_multimesh", "Binds a MultiMeshInstance3D array enabling batch optimizations natively.",
			Vector<String>{ "projectPath", "string", "scenePath", "string", "parentNodePath", "string", "nodeName", "string", "properties", "object" }, Vector<String>{});
	add_schema("setup_skeleton", "Rigs a unified Skeleton3D core attaching animated skeletal bones systematically.",
			Vector<String>{ "projectPath", "string", "scenePath", "string", "parentNodePath", "string", "nodeName", "string", "properties", "object" }, Vector<String>{});
	add_schema("setup_occlusion", "Builds OccluderInstance3D buffers mitigating excess renders strictly mapping defined properties.",
			Vector<String>{ "projectPath", "string", "scenePath", "string", "parentNodePath", "string", "nodeName", "string", "properties", "object" }, Vector<String>{});
	add_schema("load_sprite", "Loads and sets a texture onto a Sprite node.",
			Vector<String>{ "scene_path", "string", "node_path", "string", "texture_path", "string" }, Vector<String>{ "scene_path", "node_path", "texture_path" });
	add_schema("save_scene", "Saves the current state of a scene to disk.",
			Vector<String>{ "scene_path", "string" }, Vector<String>{ "scene_path" });
	add_schema("connect_signal", "Connects a Godot signal dynamically.",
			Vector<String>{ "scene_path", "string", "node_path", "string", "signal_name", "string", "target_path", "string", "method_name", "string" },
			Vector<String>{ "scene_path", "node_path", "signal_name", "target_path", "method_name" });
	add_schema("disconnect_signal", "Disconnects an existing signal binding.",
			Vector<String>{ "scene_path", "string", "node_path", "string", "signal_name", "string", "target_path", "string", "method_name", "string" },
			Vector<String>{ "scene_path", "node_path", "signal_name", "target_path", "method_name" });
	add_schema("list_connections", "Lists active connections of a specified node.",
			Vector<String>{ "scene_path", "string", "node_path", "string" }, Vector<String>{ "scene_path", "node_path" });
	add_schema("list_node_signals", "Lists signals declared on a node in a scene.",
			Vector<String>{ "scenePath", "string", "nodePath", "string" }, Vector<String>{ "scenePath" });
	add_schema("has_signal_connection", "Checks if a specific signal connection exists in a scene.",
			Vector<String>{ "scenePath", "string", "sourceNodePath", "string", "signalName", "string", "targetNodePath", "string", "methodName", "string" }, Vector<String>{ "scenePath", "sourceNodePath", "signalName", "targetNodePath", "methodName" });
	add_schema("set_mesh", "Loads a Mesh resource and assigns it to a node mesh property.",
			Vector<String>{ "node_path", "string", "mesh_path", "string", "property", "string" }, Vector<String>{ "node_path", "mesh_path" });
	add_schema("set_material", "Loads a Material resource and assigns it to a node material property.",
			Vector<String>{ "node_path", "string", "material_path", "string", "property", "string" }, Vector<String>{ "node_path", "material_path" });
	add_schema("set_sprite_texture", "Loads a Texture2D resource and assigns it to a sprite/control texture property.",
			Vector<String>{ "node_path", "string", "texture_path", "string", "property", "string" }, Vector<String>{ "node_path", "texture_path" });
	add_schema("set_collision_shape", "Loads a Shape resource and assigns it to a CollisionShape node shape property.",
			Vector<String>{ "node_path", "string", "shape_path", "string", "property", "string" }, Vector<String>{ "node_path", "shape_path" });
	add_schema("set_resource_property", "Sets a resource or raw value on a node property.",
			Vector<String>{ "node_path", "string", "property", "string", "resource_path", "string", "value", "any" }, Vector<String>{ "node_path", "property" });
	add_schema("save_resource_to_file", "Saves a node resource property to a .tres/.res file.",
			Vector<String>{ "node_path", "string", "property", "string", "save_path", "string" }, Vector<String>{ "node_path", "property", "save_path" });

	current_category = "resource_tools";
	is_core = false;
	add_schema("create_resource", "Creates a generic Godot resource.",
			Vector<String>{ "resource_path", "string", "resource_type", "string" }, Vector<String>{ "resource_path", "resource_type" });
	add_schema("modify_resource", "Modifies an existing resource.",
			Vector<String>{ "resource_path", "string", "properties", "object" }, Vector<String>{ "resource_path", "properties" });
	add_schema("read_resource_file", "Reads a resource file as text or returns basic resource metadata.",
			Vector<String>{ "path", "string", "resource_path", "string" }, Vector<String>{});
	add_schema("edit_resource_file", "Edits a resource file by replacing text content or setting resource properties.",
			Vector<String>{ "path", "string", "resource_path", "string", "content", "string", "properties", "object" }, Vector<String>{});
	add_schema("get_resource_preview", "Returns lightweight preview metadata for a resource.",
			Vector<String>{ "path", "string", "resource_path", "string" }, Vector<String>{});
	add_schema("list_resource_files", "Recursively lists resource files with optional class filtering.",
			Vector<String>{ "path", "string", "type_filter", "string" }, Vector<String>{});
	add_schema("save_resource_as", "Saves an existing resource to a new project path.",
			Vector<String>{ "resource_path", "string", "source_path", "string", "dest_path", "string", "save_path", "string" }, Vector<String>{});
	add_schema("get_resource_dependencies", "Lists ResourceLoader dependencies for a resource path.",
			Vector<String>{ "path", "string" }, Vector<String>{ "path" });
	add_schema("import_asset_copy", "Copies an external asset into the project and refreshes the filesystem.",
			Vector<String>{ "source_path", "string", "dest_path", "string" }, Vector<String>{ "source_path", "dest_path" });
	add_schema("manage_resource_autoloads", "Lists, adds, or removes project autoload entries through resource-style arguments.",
			Vector<String>{ "action", "string", "operation", "string", "name", "string", "path", "string" }, Vector<String>{});
	add_schema("create_material", "Creates a material resource natively.",
			Vector<String>{ "resource_path", "string", "material_type", "string", "properties", "object" }, Vector<String>{ "resource_path", "material_type" });
	add_schema("create_shader_template", "Creates a shader file from code or one of the resource shader templates.",
			Vector<String>{ "shaderPath", "string", "shaderType", "string", "code", "string", "template", "string" }, Vector<String>{ "shaderPath" });
	add_schema("create_tileset", "Creates a standard tileset natively.",
			Vector<String>{ "resource_path", "string" }, Vector<String>{ "resource_path" });
	add_schema("set_tilemap_cells", "Modifies bulk tilemap data.",
			Vector<String>{ "scene_path", "string", "node_path", "string", "cells", "array" }, Vector<String>{ "scene_path", "node_path", "cells" });
	add_schema("set_theme_resource_color", "Configures theme colors natively.",
			Vector<String>{ "resource_path", "string", "theme_type", "string", "color_name", "string", "color", "object" }, Vector<String>{ "resource_path", "color_name" });
	add_schema("set_theme_resource_font_size", "Configures theme font sizes natively.",
			Vector<String>{ "resource_path", "string", "theme_type", "string", "font_size_name", "string", "size", "number" }, Vector<String>{ "resource_path", "font_size_name" });
	add_schema("apply_theme_shader", "Applies shaders to UI theme elements natively.",
			Vector<String>{ "resource_path", "string", "shader_path", "string" }, Vector<String>{ "resource_path", "shader_path" });
	add_schema("resource_import_asset", "Triggers the EditorFileSystem to reimport a resource asynchronously.",
			Vector<String>{ "path", "string" }, Vector<String>{ "path" });
	add_schema("get_resource_info", "Returns basic information about a resource class or resource file.",
			Vector<String>{ "type", "string", "path", "string", "resource_path", "string" }, Vector<String>{});

	current_category = "animation_tools";
	is_core = false;
	add_schema("create_animation", "Creates an animation data resource natively.",
			Vector<String>{ "resource_path", "string", "animation_name", "string", "length", "number" }, Vector<String>{ "resource_path", "animation_name" });
	add_schema("set_animation_keyframe", "Sets or creates a value-track keyframe on an AnimationPlayer animation.",
			Vector<String>{ "scenePath", "string", "playerNodePath", "string", "animationName", "string", "nodePath", "string", "property", "string", "time", "number", "value", "any" }, Vector<String>{ "scenePath", "animationName", "nodePath", "property" });
	add_schema("get_animation_info", "Lists AnimationPlayer animations and track metadata.",
			Vector<String>{ "scenePath", "string", "playerNodePath", "string", "animationName", "string" }, Vector<String>{ "scenePath" });
	add_schema("remove_animation", "Removes an animation from an AnimationPlayer's default library.",
			Vector<String>{ "scenePath", "string", "playerNodePath", "string", "animationName", "string" }, Vector<String>{ "scenePath", "animationName" });
	add_schema("list_animations", "Lists animation names and metadata from an AnimationPlayer.",
			Vector<String>{ "scenePath", "string", "playerNodePath", "string" }, Vector<String>{ "scenePath" });
	add_schema("add_animation_track", "Injects animation tracks into an existing track layout.",
			Vector<String>{ "resource_path", "string", "animation_name", "string", "track_type", "string", "node_path", "string" }, Vector<String>{ "resource_path", "animation_name", "track_type", "node_path" });
	add_schema("create_animation_tree", "Creates an animation tree container.",
			Vector<String>{ "scene_path", "string", "parent_path", "string", "tree_name", "string" }, Vector<String>{ "scene_path", "parent_path" });
	add_schema("get_animation_tree_structure", "Reads an AnimationTree state machine or blend tree structure.",
			Vector<String>{ "scenePath", "string", "animTreePath", "string" }, Vector<String>{ "scenePath", "animTreePath" });
	add_schema("add_animation_state", "Injects structural states to node graphs natively.",
			Vector<String>{ "resource_path", "string", "state_name", "string", "animation_name", "string" }, Vector<String>{ "resource_path", "state_name" });
	add_schema("remove_animation_state", "Removes a state from an AnimationTree state machine.",
			Vector<String>{ "scenePath", "string", "animTreePath", "string", "stateName", "string", "stateMachinePath", "string" }, Vector<String>{ "scenePath", "animTreePath", "stateName" });
	add_schema("connect_animation_states", "Binds state machine states together via transitions.",
			Vector<String>{ "resource_path", "string", "from_state", "string", "to_state", "string" }, Vector<String>{ "resource_path", "from_state", "to_state" });
	add_schema("remove_animation_transition", "Removes an AnimationTree state machine transition by index or endpoint states.",
			Vector<String>{ "scenePath", "string", "animTreePath", "string", "fromState", "string", "toState", "string", "transitionIndex", "number" }, Vector<String>{ "scenePath", "animTreePath" });
	add_schema("set_animation_tree_parameter", "Sets an AnimationTree runtime parameter value in a scene resource.",
			Vector<String>{ "scenePath", "string", "animTreePath", "string", "parameter", "string", "value", "any" }, Vector<String>{ "scenePath", "animTreePath", "parameter" });
	add_schema("set_blend_tree_node", "Adds or replaces an AnimationNodeAnimation inside an AnimationNodeBlendTree.",
			Vector<String>{ "scenePath", "string", "animTreePath", "string", "blendTreePath", "string", "nodeName", "string", "animationName", "string", "position", "object" }, Vector<String>{ "scenePath", "animTreePath", "nodeName" });
	add_schema("create_navigation_region", "Injects a navigation region structurally natively.",
			Vector<String>{ "scene_path", "string", "parent_path", "string", "region_name", "string" }, Vector<String>{ "scene_path", "parent_path" });
	add_schema("create_navigation_agent", "Injects a navigation agent node natively.",
			Vector<String>{ "scene_path", "string", "parent_path", "string", "agent_name", "string" }, Vector<String>{ "scene_path", "parent_path" });
	add_schema("configure_sprite_frames", "Creates or replaces SpriteFrames on an AnimatedSprite2D in a scene file. Each animations[] item needs mode frames or generated_atlas.",
			Vector<String>{ "file_path", "string", "node_path", "string", "animations", "array" }, Vector<String>{ "file_path", "node_path", "animations" });

	current_category = "project_tools";
	is_core = true;
	add_schema("get_project_info", "Returns metadata about the project, renderer, viewport, and autoloads.", Vector<String>{}, Vector<String>{});
	add_schema("get_filesystem_tree", "Returns a recursive file tree with an option to filter by extension.",
			Vector<String>{ "path", "string", "filter", "string", "max_depth", "number" }, Vector<String>{}, "forbidden", "worker");
	add_schema("search_files", "Searches the file system with a glob or fuzzy query.",
			Vector<String>{ "query", "string", "path", "string", "file_type", "string", "max_results", "number" }, Vector<String>{ "query" }, "forbidden", "worker");
	add_schema("search_in_files", "Searches file contents using string matching or regex.",
			Vector<String>{ "query", "string", "path", "string", "max_results", "number", "regex", "boolean", "file_type", "string" }, Vector<String>{ "query" }, "forbidden", "worker");
	add_schema("set_project_setting", "Sets a specific global setting in the project settings file (project.blazium or project.godot).",
			Vector<String>{ "key", "string", "value", "string" }, Vector<String>{ "key", "value" });
	add_schema("uid_to_project_path", "Resolves a resource UID (uid://) to a sandboxed res:// or user:// path.",
			Vector<String>{ "uid", "string" }, Vector<String>{ "uid" }, "forbidden", "worker");
	add_schema("project_path_to_uid", "Resolves a sandboxed res:// or user:// asset path to its resource UID.",
			Vector<String>{ "path", "string" }, Vector<String>{ "path" }, "forbidden", "worker");
	add_schema("asset_assign_uid", "Binds a sandboxed asset file to a new or provided resource UID and persists the UID cache.",
			Vector<String>{ "path", "string", "uid", "string", "overwrite", "boolean" }, Vector<String>{ "path" });
	add_schema("asset_update_uid", "Repoints an existing resource UID to a new sandboxed asset path and persists the UID cache.",
			Vector<String>{ "uid", "string", "path", "string" }, Vector<String>{ "uid", "path" });
	add_schema("asset_remove_uid", "Removes a resource UID mapping without deleting the asset file.",
			Vector<String>{ "uid", "string", "path", "string" }, Vector<String>{});
	add_schema("add_autoload", "Injects a singleton scene or script as an autoload in project scope.",
			Vector<String>{ "name", "string", "path", "string" }, Vector<String>{ "name", "path" });
	add_schema("remove_autoload", "Detaches a singleton autoload from project scope.",
			Vector<String>{ "name", "string" }, Vector<String>{ "name" });
	add_schema("project_get_input_actions", "Retrieves the engine InputMap event bindings natively.",
			Vector<String>{}, Vector<String>{});
	add_schema("project_set_input_action", "Binds a custom InputMap action to an event mapping. events: array of InputEvent dictionaries (type, button_index, keycode, ...) or JSON string fallback.",
			Vector<String>{ "action", "string", "events", "array", "deadzone", "number", "replace_events", "boolean" }, Vector<String>{ "action", "events" });
	add_schema("project_remove_input_action", "Erases an InputMap action definition from project map.",
			Vector<String>{ "action", "string" }, Vector<String>{ "action" });
	add_schema("project_run", "Runs the project or a scene with optional save-all before launch.",
			Vector<String>{ "scene_path", "string", "main", "boolean", "autosave", "boolean" }, Vector<String>{});
	add_schema("inject_drag", "Executes a rapid mouse drag-and-drop event from start to end coordinates.",
			Vector<String>{ "from", "array", "to", "array" }, Vector<String>{ "from", "to" });
	add_schema("inject_scroll", "Triggers a viewport scroll event at specified coordinates with a given delta.",
			Vector<String>{ "x", "number", "y", "number", "delta", "number" }, Vector<String>{ "x", "y" });
	add_schema("inject_gesture", "Injects high-level touch gestures like pinch or swipe into the engine.",
			Vector<String>{ "type", "string", "params", "object" }, Vector<String>{ "type" });
	add_schema("inject_gamepad", "Simulates joypad events including button presses and axis motions.",
			Vector<String>{ "device", "number", "type", "string", "index", "number", "value", "number", "pressed", "boolean" }, Vector<String>{ "type" });

	current_category = "profiling_tools";
	is_core = false;
	add_schema("get_performance_monitors", "Retrieves all performance monitors related to memory, FPS, navigation, rendering.",
			Vector<String>{ "category", "string" }, Vector<String>{});
	add_schema("get_editor_performance", "Retrieves a compact structural overview of the performance footprint of the godot editor process.",
			Vector<String>{}, Vector<String>{});
	add_schema("profiling_detect_bottlenecks", "Analyzes current Performance counters for common bottlenecks.",
			Vector<String>{}, Vector<String>{});
	add_schema("profiling_monitor", "Compares current Performance counters against caller-provided thresholds.",
			Vector<String>{ "fps_min", "number", "frame_time_max_ms", "number", "memory_max_mb", "number", "draw_calls_max", "number" }, Vector<String>{});

	current_category = "export_tools";
	is_core = false;
	add_schema("list_export_presets", "Reads and returns all export presets from export_presets.cfg.",
			Vector<String>{}, Vector<String>{});
	add_schema("export_project", "Triggers a headless Godot export operation.",
			Vector<String>{ "preset_index", "number", "preset_name", "string", "debug", "boolean" }, Vector<String>{}, "required");
	add_schema("export_release", "Exports the project using the release preset.",
			Vector<String>{ "preset_name", "string", "preset_index", "number" }, Vector<String>{}, "required");
	add_schema("export_debug", "Exports the project using the debug preset.",
			Vector<String>{ "preset_name", "string", "preset_index", "number" }, Vector<String>{}, "required");
	add_schema("export_custom", "Exports the project with explicit debug/release selection.",
			Vector<String>{ "preset_name", "string", "preset_index", "number", "debug", "boolean" }, Vector<String>{}, "required");
	add_schema("get_export_info", "Returns metadata regarding absolute template directions and binary configurations.",
			Vector<String>{}, Vector<String>{});
	add_schema("list_android_devices", "Lists Android devices visible to adb.",
			Vector<String>{}, Vector<String>{}, "required");
	add_schema("get_android_preset_info", "Reads Android export preset metadata.",
			Vector<String>{ "preset_name", "string", "preset_index", "number" }, Vector<String>{});
	add_schema("deploy_to_android", "Exports, installs, and optionally launches an Android build through adb.",
			Vector<String>{ "preset_name", "string", "preset_index", "number", "device_serial", "string", "debug", "boolean", "skip_export", "boolean", "launch", "boolean" }, Vector<String>{}, "required");

	current_category = "batch_tools";
	is_core = false;
	add_schema("find_nodes_by_type", "Recursively scans a scene hierarchy looking for class type matches.",
			Vector<String>{ "type", "string", "recursive", "boolean" }, Vector<String>{ "type" });
	add_schema("find_signal_connections", "Collects connections and signal maps spanning a particular node criteria.",
			Vector<String>{ "signal_name", "string", "node_path", "string" }, Vector<String>{});
	add_schema("batch_set_property", "Finds nodes of a class and assigns a batch property mutation dynamically.",
			Vector<String>{ "type", "string", "property", "string", "value", "any" }, Vector<String>{ "type", "property", "value" });
	add_schema("batch_add_nodes", "Adds multiple nodes to the currently edited scene in one call.",
			Vector<String>{ "nodes", "array" }, Vector<String>{ "nodes" });
	add_schema("batch_execute", "Executes multiple JustAMCP tools sequentially with optional undo on failure.",
			Vector<String>{ "steps", "array", "stop_on_error", "boolean", "undo_on_error", "boolean" }, Vector<String>{ "steps" }, "optional");
	add_schema("find_node_references", "Searches file system recursively for reference nodes by string pattern.",
			Vector<String>{ "pattern", "string" }, Vector<String>{ "pattern" });
	add_schema("cross_scene_set_property", "Modifies properties identically across scene files matching parameters saving changes into file system.",
			Vector<String>{ "type", "string", "property", "string", "value", "any", "path_filter", "string", "exclude_addons", "boolean", "max_results", "number" }, Vector<String>{ "type", "property", "value" });
	add_schema("get_scene_dependencies", "Interrogates internal Godot dependencies structure string paths via ResourceLoader.",
			Vector<String>{ "path", "string" }, Vector<String>{ "path" });

	current_category = "script_tools";
	is_core = false;
	add_schema("list_scripts", "Locates .gd, .cs, .gdshader scripts within the project hierarchy.",
			Vector<String>{ "path", "string", "recursive", "boolean", "max_results", "number" }, Vector<String>{});
	add_schema("read_script", "Fetches source text directly from a godot script file.",
			Vector<String>{ "path", "string" }, Vector<String>{ "path" });
	add_schema("create_script", "Writes a script file providing a default extending template out of the box.",
			Vector<String>{ "path", "string", "content", "string", "extends", "string", "class_name", "string" }, Vector<String>{ "path" });
	add_schema("edit_script", "Modifies an existing script intelligently mapping regex replacements or direct injection.",
			Vector<String>{ "path", "string", "content", "string", "insert_at_line", "number", "text", "string", "replacements", "array" }, Vector<String>{ "path" });
	add_schema("attach_script", "Binds a target Godot Resource Script onto a Scene Node dynamically.",
			Vector<String>{ "node_path", "string", "script_path", "string" }, Vector<String>{ "node_path", "script_path" });
	add_schema("delete_script", "Deletes a script file from the project.",
			Vector<String>{ "path", "string" }, Vector<String>{ "path" });
	add_schema("detach_script", "Removes a script from a node in the currently edited scene.",
			Vector<String>{ "node_path", "string" }, Vector<String>{ "node_path" });
	add_schema("get_open_scripts", "Maps what files are actively opened within Godot's script editor GUI.",
			Vector<String>{}, Vector<String>{});
	add_schema("open_script_in_editor", "Opens a script in the editor script workspace.",
			Vector<String>{ "path", "string", "line", "number" }, Vector<String>{ "path" });
	add_schema("get_script_errors", "Returns lightweight script validation error information.",
			Vector<String>{ "path", "string" }, Vector<String>{ "path" });
	add_schema("search_in_scripts", "Searches project script files for a literal string.",
			Vector<String>{ "path", "string", "pattern", "string" }, Vector<String>{ "pattern" });
	add_schema("find_script_symbols", "Extracts classes, functions, variables, signals, constants, and enum symbols from scripts.",
			Vector<String>{ "path", "string" }, Vector<String>{});
	add_schema("patch_script", "Patches a script by replacing or inserting around an anchor.",
			Vector<String>{ "path", "string", "anchor", "string", "search", "string", "replacement", "string", "replace", "string", "insert_before", "string", "insert_after", "string" }, Vector<String>{ "path" });
	add_schema("validate_script", "Compiles a GDScript implicitly returning if valid or syntax errors mapped out.",
			Vector<String>{ "path", "string" }, Vector<String>{ "path" });

	current_category = "node_tools";
	is_core = false;
	add_schema("node_add", "Spawns a godot node natively onto a target structural anchor.",
			Vector<String>{ "type", "string", "parent_path", "string", "name", "string", "properties", "object" }, Vector<String>{ "type" });
	add_schema("node_delete", "Removes a godot node structural anchor.",
			Vector<String>{ "node_path", "string" }, Vector<String>{ "node_path" });
	add_schema("node_duplicate", "Clones a node struct into the scene graph dynamically.",
			Vector<String>{ "node_path", "string", "name", "string" }, Vector<String>{ "node_path" });
	add_schema("node_move", "Alters structural ownership hierarchy within the Scene Tree.",
			Vector<String>{ "node_path", "string", "new_parent_path", "string" }, Vector<String>{ "node_path", "new_parent_path" });
	add_schema("node_update_property", "Explicitly manages individual Variant properties assigned onto a node.",
			Vector<String>{ "node_path", "string", "property", "string", "value", "any" }, Vector<String>{ "node_path", "property", "value" });
	add_schema("node_get_properties", "Interrogates internal parameters within Node bindings structurally.",
			Vector<String>{ "node_path", "string", "category", "string" }, Vector<String>{ "node_path" });
	add_schema("node_add_resource", "Assigns a Resource onto a live node or a scene file node. resource_type may be a class name or load:res://path.",
			Vector<String>{ "node_path", "string", "property", "string", "property_path", "string", "resource_type", "string", "resource_properties", "object", "properties", "object", "file_path", "string" }, Vector<String>{ "node_path", "property", "resource_type" });
	add_schema("node_set_anchor_preset", "Enables UI layout modifications bound towards native godot PRESET flags.",
			Vector<String>{ "node_path", "string", "preset", "string", "keep_offsets", "boolean" }, Vector<String>{ "node_path", "preset" });
	add_schema("node_rename", "Edits node semantic names directly within godot structures.",
			Vector<String>{ "node_path", "string", "new_name", "string" }, Vector<String>{ "node_path", "new_name" });
	add_schema("node_connect_signal", "Binds callable events between source nodes bounding into target instances natively.",
			Vector<String>{ "source_path", "string", "signal_name", "string", "target_path", "string", "method_name", "string" }, Vector<String>{ "source_path", "signal_name", "target_path", "method_name" });
	add_schema("node_disconnect_signal", "Snaps off existing bounding callable events recursively mapped over nodes.",
			Vector<String>{ "source_path", "string", "signal_name", "string", "target_path", "string", "method_name", "string" }, Vector<String>{ "source_path", "signal_name", "target_path", "method_name" });
	add_schema("node_get_groups", "Aggregates internal Godot Groups strings assigned over standard instances.",
			Vector<String>{ "node_path", "string" }, Vector<String>{ "node_path" });
	add_schema("node_set_groups", "Mutates overlapping assignment instances mapped per godot string node groups.",
			Vector<String>{ "node_path", "string", "groups", "array" }, Vector<String>{ "node_path", "groups" });
	add_schema("node_find_in_group", "Provides lookup access globally towards Godot instance sets natively spanning specific groups.",
			Vector<String>{ "group", "string" }, Vector<String>{ "group" });

	current_category = "audio_tools";
	is_core = false;
	add_schema("get_audio_bus_layout", "Returns native godot AudioServer state containing all available buses and their effects.",
			Vector<String>{}, Vector<String>{});
	add_schema("add_audio_bus", "Binds a new structural audio bus dynamically into the godot runtime.",
			Vector<String>{ "name", "string", "at_position", "number", "volume_db", "number", "send", "string", "solo", "boolean", "mute", "boolean" }, Vector<String>{ "name" });
	add_schema("set_audio_bus", "Modifies specific index Godot audio buses layout.",
			Vector<String>{ "name", "string", "volume_db", "number", "solo", "boolean", "mute", "boolean", "bypass_effects", "boolean", "send", "string", "rename", "string" }, Vector<String>{ "name" });
	add_schema("add_audio_bus_effect", "Maps Godot AudioEffects via reflection bridging directly onto buses.",
			Vector<String>{ "bus", "string", "effect_type", "string", "params", "object", "at_position", "number" }, Vector<String>{ "bus", "effect_type" });
	add_schema("add_audio_player", "Injects structural AudioStreamPlayer instances directly mapped inside Godot trees.",
			Vector<String>{ "node_path", "string", "name", "string", "type", "string", "stream", "string", "volume_db", "number", "bus", "string", "autoplay", "boolean", "max_distance", "number", "attenuation", "number", "attenuation_model", "number", "unit_size", "number" }, Vector<String>{ "node_path", "name" });
	add_schema("audio_get_players_info", "Returns hierarchical node maps wrapping natively instantiated stream wrappers.",
			Vector<String>{ "node_path", "string" }, Vector<String>{ "node_path" });

	current_category = "input_tools";
	is_core = false;
	add_schema("simulate_key", "Mocks native godot InputEventKey structs sent across IPC towards godot debug runtimes.",
			Vector<String>{ "keycode", "string", "pressed", "boolean", "shift", "boolean", "ctrl", "boolean", "alt", "boolean" }, Vector<String>{ "keycode" });
	add_schema("simulate_mouse_click", "Mocks native godot InputEventMouseButton bounding structurally across OS IPC streams.",
			Vector<String>{ "button", "number", "pressed", "boolean", "double_click", "boolean", "auto_release", "boolean", "x", "number", "y", "number" }, Vector<String>{});
	add_schema("simulate_mouse_move", "Mocks native godot InputEventMouseMotion translating structural frames over OS IPC streams.",
			Vector<String>{ "x", "number", "y", "number", "relative_x", "number", "relative_y", "number", "button_mask", "number", "unhandled", "boolean" }, Vector<String>{});
	add_schema("simulate_action", "Triggers artificial input action payloads universally mimicking defined map shortcuts across states natively.",
			Vector<String>{ "action", "string", "pressed", "boolean", "strength", "number" }, Vector<String>{ "action" });
	add_schema("simulate_touch", "Mocks screen touch interactions passing accurate index, position, and tap evaluation vectors cleanly bypassing OS screens.",
			Vector<String>{ "action", "string", "position", "object", "x", "number", "y", "number", "pressed", "boolean", "double_tap", "boolean", "index", "number" }, Vector<String>{});
	add_schema("simulate_gamepad", "Fakes joypad signals pushing button index/pressure or axis overrides for active game controller states.",
			Vector<String>{ "device", "number", "button_index", "number", "button", "number", "axis", "number", "axis_value", "number", "pressed", "boolean", "pressure", "number" }, Vector<String>{});
	add_schema("simulate_sequence", "Chains an array of defined inputs mapping deterministic simulation timings dynamically.",
			Vector<String>{ "events", "array", "frame_delay", "number" }, Vector<String>{ "events" });
	add_schema("input_record", "Start listening over input sequence frame loops saving complex vectors natively capturing user simulations optimally.",
			Vector<String>{ "state", "boolean" }, Vector<String>{ "state" });
	add_schema("input_replay", "Streams input buffer traces driving Godot inputs mapped deterministically accurately replaying test routines safely.",
			Vector<String>{ "sequence_buffer_id", "string" }, Vector<String>{ "sequence_buffer_id" });

	current_category = "particle_tools";
	is_core = false;
	add_schema("create_particles", "Instantiates a new particle emitter structure onto a godot node.",
			Vector<String>{ "parent_path", "string", "name", "string", "is_3d", "boolean", "amount", "number", "lifetime", "number", "one_shot", "boolean", "explosiveness", "number", "randomness", "number", "emitting", "boolean" }, Vector<String>{ "parent_path" });
	add_schema("set_particle_material", "Modifies inner Godot struct material values bounding Godot Variant types for particles.",
			Vector<String>{ "node_path", "string", "direction", "any", "spread", "number", "initial_velocity_min", "number", "initial_velocity_max", "number", "gravity", "any", "scale_min", "number", "scale_max", "number", "color", "string", "emission_shape", "string", "emission_sphere_radius", "number", "emission_box_extents", "any", "emission_ring_radius", "number", "emission_ring_inner_radius", "number", "emission_ring_height", "number" }, Vector<String>{ "node_path" });
	add_schema("set_particle_color_gradient", "Maps linear stop structs against color configurations into a proper GradientTexture1D resource natively.",
			Vector<String>{ "node_path", "string", "stops", "array" }, Vector<String>{ "node_path", "stops" });
	add_schema("apply_particle_preset", "Wraps multiple set calls interpolating variables representing default high quality VFX presets.",
			Vector<String>{ "node_path", "string", "preset", "string" }, Vector<String>{ "node_path", "preset" });
	add_schema("get_particle_info", "Interrogates godot nodes returning hierarchical configuration state back to the MCP stream.",
			Vector<String>{ "node_path", "string" }, Vector<String>{ "node_path" });

	current_category = "physics_tools";
	is_core = false;
	add_schema("setup_collision", "Spawns and attaches native 2D/3D collision boundaries onto physical rigid bodies dynamically.",
			Vector<String>{ "node_path", "string", "shape", "string", "dimension", "string", "width", "number", "height", "number", "depth", "number", "radius", "number", "disabled", "boolean", "one_way_collision", "boolean" }, Vector<String>{ "node_path", "shape" });
	add_schema("set_physics_layers", "Controls implicit bitmask configurations assigning node interaction overlap flags natively.",
			Vector<String>{ "node_path", "string", "layer", "number", "mask", "number" }, Vector<String>{ "node_path" });
	add_schema("get_physics_layers", "Reads intrinsic properties parsing bitmask configurations exposing layers visually.",
			Vector<String>{ "node_path", "string" }, Vector<String>{ "node_path" });
	add_schema("add_raycast", "Instantiates native Godot RayCast queries pointing into logical coordinate targets asynchronously bounds.",
			Vector<String>{ "parent_path", "string", "name", "string", "dimension", "string", "target_position", "object", "enabled", "boolean", "collision_mask", "number" }, Vector<String>{ "parent_path" });
	add_schema("setup_physics_body", "Allocates pure native Object implementations defining Physics bodies bounds (Area, Character, Rigid).",
			Vector<String>{ "parent_path", "string", "body_type", "string", "name", "string", "dimension", "string", "collision_layer", "number", "collision_mask", "number" }, Vector<String>{ "parent_path", "body_type" });
	add_schema("get_collision_info", "Walks a sub-hierarchy scraping Godot shape configurations for runtime representations.",
			Vector<String>{ "node_path", "string" }, Vector<String>{ "node_path" });
	add_schema("validate_physics_setup", "Validates character and platform collision masks, shapes, and TileSet physics layers in a scene file.",
			Vector<String>{ "file_path", "string", "character_nodes", "array", "platform_nodes", "array" }, Vector<String>{ "file_path" });

	current_category = "scene3d_tools";
	is_core = false;
	add_schema("add_mesh_instance", "Mints and injects a MeshInstance3D primitive directly into Godot Scene tree natively.",
			Vector<String>{ "parent_path", "string", "name", "string", "mesh_type", "string", "mesh_file", "string", "mesh_properties", "object", "position", "any", "rotation", "any", "scale", "any" }, Vector<String>{ "parent_path" });
	add_schema("setup_lighting", "Configures high performance Godot lights (SpotLight3D, OmniLight3D, DirectionalLight3D) dynamically into the tree.",
			Vector<String>{ "parent_path", "string", "name", "string", "light_type", "string", "preset", "string", "color", "any", "energy", "number", "shadows", "boolean", "range", "number", "attenuation", "number", "spot_angle", "number", "spot_angle_attenuation", "number", "position", "any", "rotation", "any" }, Vector<String>{ "parent_path" });
	add_schema("set_material_3d", "Assigns and computes real-time StandardMaterial3D configurations over Godot primitive and loaded meshes.",
			Vector<String>{ "node_path", "string", "surface_index", "number", "albedo_color", "any", "albedo_texture", "string", "metallic", "number", "roughness", "number", "metallic_texture", "string", "roughness_texture", "string", "normal_texture", "string", "emission", "any", "emission_color", "any", "emission_energy", "number", "emission_texture", "string", "transparency", "any", "cull_mode", "any" }, Vector<String>{ "node_path" });
	add_schema("setup_environment", "Allocates rendering environments over Godot bindings (SSAO, SSR, SDFGI, Glow, Fog).",
			Vector<String>{ "parent_path", "string", "name", "string", "node_path", "string", "background_mode", "string", "background_color", "any", "sky", "object", "ambient_light_color", "any", "ambient_light_energy", "number", "ambient_light_source", "any", "tonemap_mode", "any", "tonemap_exposure", "number", "tonemap_white", "number", "fog_enabled", "boolean", "fog_light_color", "any", "fog_density", "number", "fog_light_energy", "number", "glow_enabled", "boolean", "glow_intensity", "number", "glow_strength", "number", "glow_bloom", "number", "ssao_enabled", "boolean", "ssao_radius", "number", "ssao_intensity", "number", "ssr_enabled", "boolean", "ssr_max_steps", "number", "ssr_fade_in", "number", "ssr_fade_out", "number", "sdfgi_enabled", "boolean" }, Vector<String>{ "parent_path" });
	add_schema("setup_camera_3d", "Allocates viewport Camera3D projection mapping native structural properties inside godot runtime.",
			Vector<String>{ "parent_path", "string", "name", "string", "node_path", "string", "projection", "string", "fov", "number", "size", "number", "near", "number", "far", "number", "cull_mask", "number", "current", "boolean", "position", "any", "rotation", "any", "look_at", "any", "environment_path", "string" }, Vector<String>{ "parent_path" });
	add_schema("add_gridmap", "Mints and instantiates Godot high performance GridMap bounding memory grids locally.",
			Vector<String>{ "parent_path", "string", "name", "string", "node_path", "string", "mesh_library_path", "string", "cell_size", "any", "position", "any", "cells", "array" }, Vector<String>{ "parent_path" });

	current_category = "shader_tools";
	is_core = false;
	add_schema("create_shader", "Mints blank Godot Shader files injecting standard structure for canvas_item, script, and spatial types.",
			Vector<String>{ "path", "string", "content", "string", "shader_type", "string" }, Vector<String>{ "path" });
	add_schema("read_shader", "Loads string source buffers straight out from .gdshader files dynamically mapped in file system.",
			Vector<String>{ "path", "string" }, Vector<String>{ "path" });
	add_schema("edit_shader", "Provides file patching replacing code strings directly inside the Shader Resource loading path.",
			Vector<String>{ "path", "string", "content", "string", "replacements", "array" }, Vector<String>{ "path" });
	add_schema("assign_shader_material", "Mints native generic ShaderMaterial instances binding active .gdshader onto target nodes.",
			Vector<String>{ "node_path", "string", "shader_path", "string" }, Vector<String>{ "node_path", "shader_path" });
	add_schema("set_shader_param", "Reflective native evaluation bounding GDScript variable evaluation parsing shader uniforms natively.",
			Vector<String>{ "node_path", "string", "param", "string", "value", "any" }, Vector<String>{ "node_path", "param" });
	add_schema("get_shader_params", "Extracts Godot shader variables directly from Inspector metadata representations natively.",
			Vector<String>{ "node_path", "string" }, Vector<String>{ "node_path" });

	current_category = "theme_tools";
	is_core = false;
	add_schema("create_theme", "Generates a new Theme resource struct caching visual configuration layouts natively.",
			Vector<String>{ "path", "string", "default_font_size", "number" }, Vector<String>{ "path" });
	add_schema("set_control_theme_color", "Injects property mappings explicitly setting color maps inside control nodes.",
			Vector<String>{ "node_path", "string", "name", "string", "color", "string", "theme_type", "string" }, Vector<String>{ "node_path", "name", "color" });
	add_schema("set_theme_constant", "Maps godot uniform parameter spacings assigning values inside active nodes.",
			Vector<String>{ "node_path", "string", "name", "string", "value", "number" }, Vector<String>{ "node_path", "name" });
	add_schema("set_control_theme_font_size", "Overrides localized godot font sizes overriding global default assignments.",
			Vector<String>{ "node_path", "string", "name", "string", "size", "number" }, Vector<String>{ "node_path", "name" });
	add_schema("set_theme_stylebox", "Constructs standard StyleBoxFlat representations drawing boundaries over Control structures natively.",
			Vector<String>{ "node_path", "string", "name", "string", "bg_color", "string", "border_color", "string", "border_width", "number", "corner_radius", "number", "padding", "number" }, Vector<String>{ "node_path", "name" });
	add_schema("setup_control", "Automates standard Godot UI control flag assignments applying layout layouts.",
			Vector<String>{ "node_path", "string", "anchor_preset", "string", "min_size", "string", "size_flags_h", "string", "size_flags_v", "string", "margins", "object", "separation", "number", "grow_h", "string", "grow_v", "string" }, Vector<String>{ "node_path" });
	add_schema("get_theme_info", "Interrogates godot Inspector bindings wrapping metadata and dynamic lists natively.",
			Vector<String>{ "node_path", "string" }, Vector<String>{ "node_path" });

	current_category = "tilemap_tools";
	is_core = false;
	add_schema("tilemap_set_cell", "Sets a cell on a live or file-based TileMap or TileMapLayer.",
			Vector<String>{ "node_path", "string", "file_path", "string", "layer", "number", "x", "number", "y", "number", "source_id", "number", "atlas_x", "number", "atlas_y", "number", "alternative", "number" }, Vector<String>{ "node_path" });
	add_schema("tilemap_fill_rect", "Fills a rectangle on a TileMap or TileMapLayer. Accepts x/y/width/height or x1/y1/x2/y2.",
			Vector<String>{ "node_path", "string", "file_path", "string", "layer", "number", "x", "number", "y", "number", "width", "number", "height", "number", "x1", "number", "y1", "number", "x2", "number", "y2", "number", "source_id", "number", "atlas_x", "number", "atlas_y", "number", "alternative", "number" }, Vector<String>{ "node_path" });
	add_schema("tilemap_get_cell", "Reads one cell from a live or file-based TileMap or TileMapLayer.",
			Vector<String>{ "node_path", "string", "file_path", "string", "layer", "number", "x", "number", "y", "number" }, Vector<String>{ "node_path" });
	add_schema("tilemap_clear", "Clears cells on a live or file-based TileMap layer or TileMapLayer.",
			Vector<String>{ "node_path", "string", "file_path", "string", "layer", "number" }, Vector<String>{ "node_path" });
	add_schema("tilemap_get_info", "Reads TileSet sources, used_rect, pixel_bounds, physics layers, and optional cells/ASCII from a TileMap or TileMapLayer.",
			Vector<String>{ "node_path", "string", "tilemap_node", "string", "file_path", "string", "layer", "number", "include_cells", "boolean", "ascii", "boolean", "region", "object", "tileset_only", "boolean" }, Vector<String>{ "node_path" });
	add_schema("tilemap_get_used_cells", "Lists used cells from a live or file-based TileMap or TileMapLayer.",
			Vector<String>{ "node_path", "string", "file_path", "string", "layer", "number", "max_count", "number" }, Vector<String>{ "node_path" });
	add_schema("tilemap_draw_h_line", "Draws a horizontal tile line on a TileMap or TileMapLayer.",
			Vector<String>{ "node_path", "string", "file_path", "string", "layer", "number", "x", "number", "y", "number", "length", "number", "source_id", "number", "atlas_x", "number", "atlas_y", "number", "alternative", "number" }, Vector<String>{ "node_path", "length" });
	add_schema("tilemap_draw_v_line", "Draws a vertical tile line on a TileMap or TileMapLayer.",
			Vector<String>{ "node_path", "string", "file_path", "string", "layer", "number", "x", "number", "y", "number", "length", "number", "source_id", "number", "atlas_x", "number", "atlas_y", "number", "alternative", "number" }, Vector<String>{ "node_path", "length" });
	add_schema("tilemap_draw_stairs", "Draws a stair-step tile path on a TileMap or TileMapLayer. direction is up or down.",
			Vector<String>{ "node_path", "string", "file_path", "string", "layer", "number", "x", "number", "y", "number", "length", "number", "direction", "string", "source_id", "number", "atlas_x", "number", "atlas_y", "number", "alternative", "number" }, Vector<String>{ "node_path", "length" });
	add_schema("tilemap_erase_rect", "Erases a rectangle of tiles on a TileMap or TileMapLayer.",
			Vector<String>{ "node_path", "string", "file_path", "string", "layer", "number", "x", "number", "y", "number", "width", "number", "height", "number", "x1", "number", "y1", "number", "x2", "number", "y2", "number" }, Vector<String>{ "node_path" });
	add_schema("tilemap_configure_atlas", "Creates or updates a TileSetAtlasSource on a TileMap or TileMapLayer and creates every atlas cell.",
			Vector<String>{ "node_path", "string", "file_path", "string", "texture_path", "string", "tile_size_x", "number", "tile_size_y", "number", "separation_x", "number", "separation_y", "number", "source_id", "number", "physics_collision_layer", "number", "physics_collision_mask", "number", "add_collision_shapes", "boolean" }, Vector<String>{ "node_path", "texture_path" });
	add_schema("validate_tilemap_structure", "Validates TileMap or TileMapLayer cell counts, continuity, and bounds.",
			Vector<String>{ "node_path", "string", "file_path", "string", "layer", "number", "checks", "object", "tile_count", "number" }, Vector<String>{ "node_path" });

	current_category = "project_tools";
	is_core = false;
	add_schema("project_map_project", "Crawls the physical file system mapping GdScript inheritance, signal connectors, and resource preloads structural graphs natively.",
			Vector<String>{ "root", "string", "include_addons", "boolean", "lod", "number", "max_results", "number" }, Vector<String>{});
	add_schema("project_map_scenes", "Parses .tscn scene tree representations identifying node types and resource dependencies across scene graphs natively.",
			Vector<String>{ "root", "string", "include_addons", "boolean", "max_results", "number" }, Vector<String>{});
	add_schema("project_list_settings", "Queries active Godot ProjectSettings dumping categorized key-value pairs with serialization logic.",
			Vector<String>{ "category", "string", "max_results", "number", "cursor", "string" }, Vector<String>{});
	add_schema("project_update_settings", "Persists localized Godot ProjectSettings mapping dynamic dictionaries onto global configuration buffers recursively.",
			Vector<String>{ "settings", "object" }, Vector<String>{ "settings" });
	add_schema("project_manage_autoloads", "Automates Godot Autoload registrations minting or deleting global project singletons dynamically.",
			Vector<String>{ "operation", "string", "name", "string", "path", "string" }, Vector<String>{ "operation" });
	add_schema("project_get_collision_layers", "Dumps named Godot physics layers extracting user-defined metadata from ProjectSettings configuration natively.",
			Vector<String>{}, Vector<String>{});
	add_schema("read_file", "Reads a res:// or user:// text file with numbered lines. Directories are listed the same way as read_directory.",
			Vector<String>{ "file_path", "string", "path", "string", "start_line", "number", "max_lines", "number", "offset", "number", "limit", "number" }, Vector<String>{ "file_path" }, "forbidden", "worker");
	add_schema("read_directory", "Lists a sandboxed res:// or user:// directory with offset/limit pagination.",
			Vector<String>{ "file_path", "string", "path", "string", "offset", "number", "limit", "number" }, Vector<String>{ "file_path" }, "forbidden", "worker");
	add_schema("create_file", "Creates a sandboxed res:// or user:// file.",
			Vector<String>{ "file_path", "string", "path", "string", "content", "string", "overwrite", "boolean" }, Vector<String>{ "file_path", "content" });
	add_schema("edit_file", "Replaces a unique search_text occurrence in a sandboxed project file.",
			Vector<String>{ "file_path", "string", "path", "string", "search_text", "string", "replace_text", "string", "search", "string", "replace", "string" }, Vector<String>{ "file_path", "search_text" });
	add_schema("move_file", "Moves a sandboxed res:// or user:// file.",
			Vector<String>{ "from", "string", "to", "string", "file_path", "string", "destination", "string" }, Vector<String>{ "from", "to" });
	add_schema("copy_file", "Copies a sandboxed res:// or user:// file. Refuses overwrite unless overwrite=true.",
			Vector<String>{ "from", "string", "to", "string", "file_path", "string", "destination", "string", "overwrite", "boolean" }, Vector<String>{ "from", "to" });
	add_schema("delete_file", "Deletes a sandboxed res:// or user:// file.",
			Vector<String>{ "file_path", "string", "path", "string" }, Vector<String>{ "file_path" });

	current_category = "asset_tools";
	is_core = false;
	add_schema("asset_generate_2d_asset", "Renders raw SVG code directly into Godot Image instances saving PNG assets to disk and scanning filesystem repositories natively.",
			Vector<String>{ "svg_code", "string", "filename", "string", "save_path", "string", "scale", "number" }, Vector<String>{ "svg_code", "filename" });
	add_schema("save_pixel_art", "Writes a sandboxed PNG (path or base64) to res://assets/generated/ plus optional metadata JSON. Does not call hosted image generation.",
			Vector<String>{ "path", "string", "source_path", "string", "filename", "string", "png_base64", "string", "metadata", "object" }, Vector<String>{});

	current_category = "blueprint_tools";
	is_core = false;
	add_schema("blueprint_create_particle_preset", "Applies high-level ParticleProcessMaterial blueprints (fire, smoke, explosion, etc.) including automated Gradient and QuadMesh setup.",
			Vector<String>{ "path", "string", "preset", "string", "is_3d", "boolean" }, Vector<String>{ "path", "preset" });
	add_schema("blueprint_create_material_preset", "Applies curated StandardMaterial3D blueprints (metal, glass, emissive, etc.) to target MeshInstance or Material nodes.",
			Vector<String>{ "path", "string", "preset", "string" }, Vector<String>{ "path", "preset" });
	add_schema("blueprint_setup_camera_preset", "Configures Camera2D or Camera3D nodes for specific gameplay paradigms (top-down, platformer, cinematic, action).",
			Vector<String>{ "path", "string", "preset", "string" }, Vector<String>{ "path", "preset" });

	current_category = "draw_tools";
	is_core = false;
	add_schema("control_draw_recipe", "Stores and executes ordered CanvasItem draw operations on target Control nodes utilizing an embedded dynamic script natively.",
			Vector<String>{ "path", "string", "ops", "array", "clear_existing", "boolean" }, Vector<String>{ "path", "ops" });

	current_category = "environment_tools";
	is_core = false;
	add_schema("environment_create", "Creates a complex Environment (+ optional Sky + ProceduralSkyMaterial) chain and assigns it to a WorldEnvironment node via presets.",
			Vector<String>{ "path", "string", "preset", "string", "sky", "boolean" }, Vector<String>{ "path" });

	current_category = "analysis_tools";
	is_core = false;
	add_schema("find_unused_resources", "Traverses physical file system mapping orphaned godot assets unused by .tscn scenes automatically.",
			Vector<String>{ "path", "string", "include_addons", "boolean" }, Vector<String>{});
	add_schema("analyze_signal_flow", "Dumps all complex recursive callable/signal graphs connecting dynamically natively.",
			Vector<String>{ "max_nodes", "number" }, Vector<String>{});
	add_schema("analyze_scene_complexity", "Counts bounds estimating tree node/logic allocation and depths.",
			Vector<String>{ "path", "string", "max_nodes", "number" }, Vector<String>{});
	add_schema("find_script_references", "Executes strict text boundary parses across source representations verifying usage paths.",
			Vector<String>{ "query", "string", "path", "string", "include_addons", "boolean" }, Vector<String>{ "query" });
	add_schema("detect_circular_dependencies", "Detects DFS recursion cycles detecting deep graph violations dynamically.",
			Vector<String>{ "path", "string", "include_addons", "boolean" }, Vector<String>{});
	add_schema("get_project_statistics", "Calculates physical byte limits checking file distribution and global dependencies.",
			Vector<String>{ "path", "string", "include_addons", "boolean", "max_files", "number" }, Vector<String>{});
	add_schema("project_state", "Aggregates project settings, filesystem statistics, and current scene status.",
			Vector<String>{ "path", "string", "include_addons", "boolean" }, Vector<String>{});
	add_schema("project_advise", "Returns lightweight native project guidance based on current settings and scene state.",
			Vector<String>{}, Vector<String>{});
	add_schema("runtime_diagnose", "Aggregates runtime status, recent errors, and performance logs for diagnosis.",
			Vector<String>{ "limit", "number" }, Vector<String>{});
	add_schema("scene_validate", "Validates the current scene for common missing scripts, owners, and empty root issues.",
			Vector<String>{}, Vector<String>{});
	add_schema("scene_analyze", "Aggregates scene complexity and scene tree dump information.",
			Vector<String>{}, Vector<String>{});
	add_schema("script_analyze", "Searches scripts and returns script-level diagnostics for a query.",
			Vector<String>{ "query", "string", "path", "string", "include_addons", "boolean" }, Vector<String>{});
	add_schema("project_symbol_search", "Searches project scripts for classes, functions, variables, and signal symbols.",
			Vector<String>{ "query", "string", "path", "string", "include_addons", "boolean" }, Vector<String>{ "query" });
	add_schema("project_index", "Builds a lightweight native project index from mapped scripts, scenes, and statistics.",
			Vector<String>{ "path", "string", "include_addons", "boolean", "lod", "number" }, Vector<String>{});
	add_schema("scene_dependency_graph", "Returns scene dependency information using the existing batch scene dependency analyzer.",
			Vector<String>{ "scene_path", "string", "path", "string" }, Vector<String>{});
}

#endif
