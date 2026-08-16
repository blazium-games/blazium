/**************************************************************************/
/*  justamcp_profiling_tools.cpp                                          */
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

#include "justamcp_profiling_tools.h"

#include "../justamcp_mcp_tool_macros.h"

#include "main/performance.h"

void JustAMCPProfilingTools::_bind_methods() {}

JustAMCPProfilingTools::JustAMCPProfilingTools() {}

JustAMCPProfilingTools::~JustAMCPProfilingTools() {}

Dictionary JustAMCPProfilingTools::execute_tool(const String &p_tool_name, const Dictionary &p_args) {
	if (p_tool_name == "get_performance_monitors") {
		return _get_performance_monitors(p_args);
	} else if (p_tool_name == "get_editor_performance") {
		return _get_editor_performance(p_args);
	} else if (p_tool_name == "profiling_detect_bottlenecks") {
		return _detect_bottlenecks(p_args);
	} else if (p_tool_name == "profiling_monitor") {
		return _monitor_thresholds(p_args);
	}

	Dictionary err;
	err["code"] = -32601;
	err["message"] = "Method not found: " + p_tool_name;
	Dictionary res;
	res["error"] = err;
	return Dictionary();
}

Dictionary JustAMCPProfilingTools::_get_performance_monitors(const Dictionary &p_params) {
	Dictionary monitors;
	Performance *perf = Performance::get_singleton();
	if (!perf) {
		return MCP_ERROR(-32603, "Performance singleton is not available");
	}

	monitors["fps"] = perf->get_monitor(Performance::TIME_FPS);
	monitors["frame_time_msec"] = perf->get_monitor(Performance::TIME_PROCESS) * 1000.0;
	monitors["physics_frame_time_msec"] = perf->get_monitor(Performance::TIME_PHYSICS_PROCESS) * 1000.0;
	monitors["navigation_process_msec"] = perf->get_monitor(Performance::TIME_NAVIGATION_PROCESS) * 1000.0;

	monitors["memory_static"] = perf->get_monitor(Performance::MEMORY_STATIC);
	monitors["memory_static_max"] = perf->get_monitor(Performance::MEMORY_STATIC_MAX);

	monitors["object_count"] = perf->get_monitor(Performance::OBJECT_COUNT);
	monitors["object_resource_count"] = perf->get_monitor(Performance::OBJECT_RESOURCE_COUNT);
	monitors["object_node_count"] = perf->get_monitor(Performance::OBJECT_NODE_COUNT);
	monitors["object_orphan_node_count"] = perf->get_monitor(Performance::OBJECT_ORPHAN_NODE_COUNT);

	monitors["render_total_objects_in_frame"] = perf->get_monitor(Performance::RENDER_TOTAL_OBJECTS_IN_FRAME);
	monitors["render_total_primitives_in_frame"] = perf->get_monitor(Performance::RENDER_TOTAL_PRIMITIVES_IN_FRAME);
	monitors["render_total_draw_calls_in_frame"] = perf->get_monitor(Performance::RENDER_TOTAL_DRAW_CALLS_IN_FRAME);
	monitors["render_video_mem_used"] = perf->get_monitor(Performance::RENDER_VIDEO_MEM_USED);

	monitors["physics_2d_active_objects"] = perf->get_monitor(Performance::PHYSICS_2D_ACTIVE_OBJECTS);
	monitors["physics_2d_collision_pairs"] = perf->get_monitor(Performance::PHYSICS_2D_COLLISION_PAIRS);
	monitors["physics_2d_island_count"] = perf->get_monitor(Performance::PHYSICS_2D_ISLAND_COUNT);

	monitors["physics_3d_active_objects"] = perf->get_monitor(Performance::PHYSICS_3D_ACTIVE_OBJECTS);
	monitors["physics_3d_collision_pairs"] = perf->get_monitor(Performance::PHYSICS_3D_COLLISION_PAIRS);
	monitors["physics_3d_island_count"] = perf->get_monitor(Performance::PHYSICS_3D_ISLAND_COUNT);

	monitors["navigation_active_maps"] = perf->get_monitor(Performance::NAVIGATION_ACTIVE_MAPS);
	monitors["navigation_region_count"] = perf->get_monitor(Performance::NAVIGATION_REGION_COUNT);
	monitors["navigation_agent_count"] = perf->get_monitor(Performance::NAVIGATION_AGENT_COUNT);

	String category = p_params.has("category") ? String(p_params["category"]) : "";
	if (!category.is_empty()) {
		Dictionary filtered;
		Array keys = monitors.keys();
		for (int i = 0; i < keys.size(); i++) {
			String key = keys[i];
			if (key.begins_with(category)) {
				filtered[key] = monitors[key];
			}
		}
		Dictionary res;
		res["monitors"] = filtered;
		res["category"] = category;
		return MCP_SUCCESS(res);
	}

	Dictionary res;
	res["monitors"] = monitors;
	return MCP_SUCCESS(res);
}

Dictionary JustAMCPProfilingTools::_get_editor_performance(const Dictionary &p_params) {
	Performance *perf = Performance::get_singleton();
	if (!perf) {
		return MCP_ERROR(-32603, "Performance singleton is not available");
	}

	Dictionary summary;
	summary["fps"] = perf->get_monitor(Performance::TIME_FPS);
	summary["frame_time_msec"] = perf->get_monitor(Performance::TIME_PROCESS) * 1000.0;
	summary["draw_calls"] = perf->get_monitor(Performance::RENDER_TOTAL_DRAW_CALLS_IN_FRAME);
	summary["objects_in_frame"] = perf->get_monitor(Performance::RENDER_TOTAL_OBJECTS_IN_FRAME);
	summary["node_count"] = perf->get_monitor(Performance::OBJECT_NODE_COUNT);
	summary["orphan_nodes"] = perf->get_monitor(Performance::OBJECT_ORPHAN_NODE_COUNT);
	summary["memory_static_mb"] = double(perf->get_monitor(Performance::MEMORY_STATIC)) / (1024.0 * 1024.0);
	summary["video_mem_mb"] = double(perf->get_monitor(Performance::RENDER_VIDEO_MEM_USED)) / (1024.0 * 1024.0);

	return MCP_SUCCESS(summary);
}

Dictionary JustAMCPProfilingTools::_detect_bottlenecks(const Dictionary &p_params) {
	Performance *perf = Performance::get_singleton();
	if (!perf) {
		return MCP_ERROR(-32603, "Performance singleton is not available");
	}

	double fps = perf->get_monitor(Performance::TIME_FPS);
	double frame_time = perf->get_monitor(Performance::TIME_PROCESS) * 1000.0;
	double physics_time = perf->get_monitor(Performance::TIME_PHYSICS_PROCESS) * 1000.0;
	int draw_calls = int(perf->get_monitor(Performance::RENDER_TOTAL_DRAW_CALLS_IN_FRAME));
	int primitives = int(perf->get_monitor(Performance::RENDER_TOTAL_PRIMITIVES_IN_FRAME));
	int orphan_nodes = int(perf->get_monitor(Performance::OBJECT_ORPHAN_NODE_COUNT));
	int node_count = int(perf->get_monitor(Performance::OBJECT_NODE_COUNT));
	double static_mem_mb = double(perf->get_monitor(Performance::MEMORY_STATIC)) / (1024.0 * 1024.0);
	double video_mem_mb = double(perf->get_monitor(Performance::RENDER_VIDEO_MEM_USED)) / (1024.0 * 1024.0);

	Array issues;
	auto add_issue = [&](const String &p_severity, const String &p_category, const String &p_message) {
		Dictionary issue;
		issue["severity"] = p_severity;
		issue["category"] = p_category;
		issue["message"] = p_message;
		issues.push_back(issue);
	};

	if (fps < 30.0) {
		add_issue("critical", "fps", vformat("FPS is %.1f (below 30)", fps));
	} else if (fps < 60.0) {
		add_issue("warning", "fps", vformat("FPS is %.1f (below 60)", fps));
	}
	if (frame_time > 33.0) {
		add_issue("critical", "frame_time", vformat("Frame time %.1fms exceeds 33ms budget", frame_time));
	} else if (frame_time > 16.6) {
		add_issue("warning", "frame_time", vformat("Frame time %.1fms exceeds 16.6ms budget", frame_time));
	}
	if (physics_time > 16.0) {
		add_issue("warning", "physics", vformat("Physics process time %.1fms is high", physics_time));
	}
	if (draw_calls > 2000) {
		add_issue("critical", "draw_calls", vformat("%d draw calls; consider batching, instancing, or reducing objects", draw_calls));
	} else if (draw_calls > 500) {
		add_issue("warning", "draw_calls", vformat("%d draw calls; monitor for performance impact", draw_calls));
	}
	if (primitives > 1000000) {
		add_issue("warning", "primitives", vformat("%d primitives in frame; consider LOD or mesh simplification", primitives));
	}
	if (orphan_nodes > 0) {
		add_issue("warning", "memory_leak", vformat("%d orphan nodes detected; possible memory leak", orphan_nodes));
	}
	if (node_count > 10000) {
		add_issue("warning", "node_count", vformat("%d nodes in tree; consider object pooling or LOD", node_count));
	}
	if (static_mem_mb > 512.0) {
		add_issue("warning", "memory", vformat("Static memory %.0fMB; high usage", static_mem_mb));
	}
	if (video_mem_mb > 1024.0) {
		add_issue("warning", "video_memory", vformat("Video memory %.0fMB; consider texture compression or resolution reduction", video_mem_mb));
	}

	bool critical = false;
	for (int i = 0; i < issues.size(); i++) {
		Dictionary issue = issues[i];
		if (String(issue["severity"]) == "critical") {
			critical = true;
			break;
		}
	}

	Dictionary summary;
	summary["fps"] = fps;
	summary["frame_time_ms"] = frame_time;
	summary["draw_calls"] = draw_calls;
	summary["node_count"] = node_count;
	summary["orphan_nodes"] = orphan_nodes;
	summary["static_memory_mb"] = static_mem_mb;
	summary["video_memory_mb"] = video_mem_mb;

	Dictionary res;
	res["overall_status"] = critical ? "critical" : (issues.is_empty() ? "healthy" : "needs_attention");
	res["issue_count"] = issues.size();
	res["issues"] = issues;
	res["summary"] = summary;
	return MCP_SUCCESS(res);
}

Dictionary JustAMCPProfilingTools::_monitor_thresholds(const Dictionary &p_params) {
	Performance *perf = Performance::get_singleton();
	if (!perf) {
		return MCP_ERROR(-32603, "Performance singleton is not available");
	}

	double fps_min = p_params.has("fps_min") ? double(p_params["fps_min"]) : 30.0;
	double frame_time_max_ms = p_params.has("frame_time_max_ms") ? double(p_params["frame_time_max_ms"]) : 33.0;
	double memory_max_mb = p_params.has("memory_max_mb") ? double(p_params["memory_max_mb"]) : 512.0;
	int draw_calls_max = p_params.has("draw_calls_max") ? int(p_params["draw_calls_max"]) : 1000;

	double fps = perf->get_monitor(Performance::TIME_FPS);
	double frame_time = perf->get_monitor(Performance::TIME_PROCESS) * 1000.0;
	double memory_mb = double(perf->get_monitor(Performance::MEMORY_STATIC)) / (1024.0 * 1024.0);
	int draw_calls = int(perf->get_monitor(Performance::RENDER_TOTAL_DRAW_CALLS_IN_FRAME));

	Array alerts;
	if (fps < fps_min) {
		alerts.push_back(vformat("FPS %.1f below threshold %.1f", fps, fps_min));
	}
	if (frame_time > frame_time_max_ms) {
		alerts.push_back(vformat("Frame time %.1fms exceeds threshold %.1fms", frame_time, frame_time_max_ms));
	}
	if (memory_mb > memory_max_mb) {
		alerts.push_back(vformat("Memory %.0fMB exceeds threshold %.0fMB", memory_mb, memory_max_mb));
	}
	if (draw_calls > draw_calls_max) {
		alerts.push_back(vformat("Draw calls %d exceeds threshold %d", draw_calls, draw_calls_max));
	}

	Dictionary thresholds;
	thresholds["fps_min"] = fps_min;
	thresholds["frame_time_max_ms"] = frame_time_max_ms;
	thresholds["memory_max_mb"] = memory_max_mb;
	thresholds["draw_calls_max"] = draw_calls_max;

	Dictionary current;
	current["fps"] = fps;
	current["frame_time_ms"] = frame_time;
	current["memory_mb"] = memory_mb;
	current["draw_calls"] = draw_calls;

	Dictionary res;
	res["thresholds"] = thresholds;
	res["current"] = current;
	res["alerts"] = alerts;
	res["alert_count"] = alerts.size();
	return MCP_SUCCESS(res);
}
