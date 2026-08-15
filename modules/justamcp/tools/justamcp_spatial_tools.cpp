/**************************************************************************/
/*  justamcp_spatial_tools.cpp                                            */
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

#include "justamcp_spatial_tools.h"
#include "../justamcp_editor_plugin.h"
#include "../justamcp_editor_scene_access.h"
#include "editor/editor_interface.h"
#include "scene/main/scene_tree.h"
#include "scene/2d/navigation/navigation_agent_2d.h"
#include "scene/2d/navigation/navigation_region_2d.h"
#include "scene/3d/navigation/navigation_agent_3d.h"
#include "scene/3d/navigation/navigation_region_3d.h"

void JustAMCPSpatialTools::_bind_methods() {}

void JustAMCPSpatialTools::set_editor_plugin(JustAMCPEditorPlugin *p_plugin) {
	editor_plugin = p_plugin;
}

Node *JustAMCPSpatialTools::_get_scene_root() {
	if (editor_plugin && editor_plugin->get_editor_interface()) {
		return JustAMCPEditorSceneAccess::get_edited_root();
	}
	return nullptr;
}

Node *JustAMCPSpatialTools::_get_node(const String &p_path) {
	if (p_path.is_empty()) {
		return _get_scene_root();
	}
	Node *scene_root = _get_scene_root();
	if (!scene_root) {
		return nullptr;
	}
	if (p_path.begins_with("/root/")) {
		String rel = p_path.substr(6);
		return scene_root->get_tree()->get_root()->get_node_or_null(NodePath(rel));
	} else if (p_path == "/root") {
		return scene_root;
	} else {
		return scene_root->get_node_or_null(NodePath(p_path));
	}
}

void JustAMCPSpatialTools::_collect_spatial_nodes(Node *p_node, Array &p_list_2d, Array &p_list_3d, bool p_inc_2d, bool p_inc_3d) {
	if (p_inc_2d) {
		Node2D *n2d = Object::cast_to<Node2D>(p_node);
		if (n2d) {
			Dictionary info;
			info["name"] = n2d->get_name();
			info["class"] = n2d->get_class();
			info["path"] = String(n2d->get_path());
			info["position"] = n2d->get_global_position();
			info["rotation"] = n2d->get_rotation();
			p_list_2d.push_back(info);
		}
	}
	if (p_inc_3d) {
		Node3D *n3d = Object::cast_to<Node3D>(p_node);
		if (n3d) {
			Dictionary info;
			info["name"] = n3d->get_name();
			info["class"] = n3d->get_class();
			info["path"] = String(n3d->get_path());
			info["position"] = n3d->get_global_position();

			Dictionary rot_vec;
			rot_vec["x"] = n3d->get_rotation().x;
			rot_vec["y"] = n3d->get_rotation().y;
			rot_vec["z"] = n3d->get_rotation().z;

			info["rotation"] = rot_vec;
			p_list_3d.push_back(info);
		}
	}

	for (int i = 0; i < p_node->get_child_count(); ++i) {
		_collect_spatial_nodes(p_node->get_child(i), p_list_2d, p_list_3d, p_inc_2d, p_inc_3d);
	}
}

void JustAMCPSpatialTools::_collect_node3d(Node *p_node, Vector<Node3D *> &p_list) {
	Node3D *n3d = Object::cast_to<Node3D>(p_node);
	if (n3d) {
		p_list.push_back(n3d);
	}
	for (int i = 0; i < p_node->get_child_count(); ++i) {
		_collect_node3d(p_node->get_child(i), p_list);
	}
}

Dictionary JustAMCPSpatialTools::spatial_analyze_layout(const Dictionary &p_args) {
	Dictionary result;
	String root_path = p_args.get("node_path", "");
	bool include_2d = p_args.has("include_2d") ? bool(p_args["include_2d"]) : true;
	bool include_3d = p_args.has("include_3d") ? bool(p_args["include_3d"]) : true;

	Node *root = _get_node(root_path);
	if (!root) {
		result["ok"] = false;
		result["error"] = "Target root node not found.";
		return result;
	}

	Array nodes_2d;
	Array nodes_3d;

	Rect2 bounds_2d;
	AABB bounds_3d;
	bool first_2d = true;
	bool first_3d = true;

	_collect_spatial_nodes(root, nodes_2d, nodes_3d, include_2d, include_3d);

	for (int i = 0; i < nodes_2d.size(); ++i) {
		Dictionary info = nodes_2d[i];
		Vector2 pos = info["position"];
		Rect2 rect = Rect2(pos, Vector2(0, 0));
		if (first_2d) {
			bounds_2d = rect;
			first_2d = false;
		} else {
			bounds_2d = bounds_2d.merge(rect);
		}
	}

	for (int i = 0; i < nodes_3d.size(); ++i) {
		Dictionary info = nodes_3d[i];
		Vector3 pos = info["position"];
		AABB aabb = AABB(pos, Vector3(0, 0, 0));
		if (first_3d) {
			bounds_3d = aabb;
			first_3d = false;
		} else {
			bounds_3d.merge_with(aabb);
		}
	}

	result["ok"] = true;
	result["nodes_2d"] = nodes_2d.size();
	result["nodes_3d"] = nodes_3d.size();

	if (!first_2d) {
		Dictionary b2d;
		Dictionary p2d;
		p2d["x"] = bounds_2d.position.x;
		p2d["y"] = bounds_2d.position.y;
		Dictionary s2d;
		s2d["x"] = bounds_2d.size.x;
		s2d["y"] = bounds_2d.size.y;
		b2d["position"] = p2d;
		b2d["size"] = s2d;
		result["bounds_2d"] = b2d;
	}

	if (!first_3d) {
		Dictionary b3d;
		Dictionary p3d;
		p3d["x"] = bounds_3d.position.x;
		p3d["y"] = bounds_3d.position.y;
		p3d["z"] = bounds_3d.position.z;
		Dictionary s3d;
		s3d["x"] = bounds_3d.size.x;
		s3d["y"] = bounds_3d.size.y;
		s3d["z"] = bounds_3d.size.z;
		b3d["position"] = p3d;
		b3d["size"] = s3d;
		result["bounds_3d"] = b3d;
	}

	result["layout_2d"] = nodes_2d.slice(0, 50);
	result["layout_3d"] = nodes_3d.slice(0, 50);
	return result;
}

Dictionary JustAMCPSpatialTools::spatial_suggest_placement(const Dictionary &p_args) {
	Dictionary result;
	String node_type = p_args.get("node_type", "");
	String context = p_args.get("context", "");
	String parent_path = p_args.get("parent_path", "");

	Node *parent = _get_node(parent_path);
	if (!parent) {
		result["ok"] = false;
		result["error"] = "Parent node not found.";
		return result;
	}

	Vector<Vector2> child_positions_2d;
	Vector<Vector3> child_positions_3d;

	for (int i = 0; i < parent->get_child_count(); ++i) {
		Node *child = parent->get_child(i);
		Node2D *n2d = Object::cast_to<Node2D>(child);
		Node3D *n3d = Object::cast_to<Node3D>(child);

		if (n2d) {
			child_positions_2d.push_back(n2d->get_position());
		} else if (n3d) {
			child_positions_3d.push_back(n3d->get_position());
		}
	}

	Dictionary suggested;
	if (child_positions_3d.size() > 0) {
		Vector3 avg = Vector3(0, 0, 0);
		for (int i = 0; i < child_positions_3d.size(); ++i) {
			avg += child_positions_3d[i];
		}
		avg /= child_positions_3d.size();
		suggested["x"] = avg.x + 2.0;
		suggested["y"] = avg.y;
		suggested["z"] = avg.z + 2.0;
	} else if (child_positions_2d.size() > 0) {
		Vector2 avg = Vector2(0, 0);
		for (int i = 0; i < child_positions_2d.size(); ++i) {
			avg += child_positions_2d[i];
		}
		avg /= child_positions_2d.size();
		suggested["x"] = avg.x + 64.0;
		suggested["y"] = avg.y;
	} else {
		suggested["x"] = 0;
		suggested["y"] = 0;
		suggested["z"] = 0;
	}

	result["ok"] = true;
	result["node_type"] = node_type;
	result["parent"] = String(parent->get_path());
	result["suggested_position"] = suggested;
	result["existing_children"] = parent->get_child_count();
	result["context"] = context;
	return result;
}

Dictionary JustAMCPSpatialTools::spatial_detect_overlaps(const Dictionary &p_args) {
	Dictionary result;
	String root_path = p_args.get("node_path", "");
	double threshold = p_args.get("threshold", 0.01);

	Node *root = _get_node(root_path);
	if (!root) {
		result["ok"] = false;
		result["error"] = "Target root node not found.";
		return result;
	}

	Vector<Node3D *> nodes_3d;
	_collect_node3d(root, nodes_3d);
	Array overlaps;

	for (int i = 0; i < nodes_3d.size(); ++i) {
		for (int j = i + 1; j < nodes_3d.size(); ++j) {
			Node3D *a = nodes_3d[i];
			Node3D *b = nodes_3d[j];
			double dist = a->get_global_position().distance_to(b->get_global_position());
			if (dist < threshold) {
				Dictionary over;
				over["node_a"] = String(a->get_path());
				over["node_b"] = String(b->get_path());
				over["distance"] = dist;
				over["type"] = "position_overlap";
				overlaps.push_back(over);
			}
		}
	}

	result["ok"] = true;
	result["checked_nodes"] = nodes_3d.size();
	result["overlap_count"] = overlaps.size();
	result["overlaps"] = overlaps;
	return result;
}

Dictionary JustAMCPSpatialTools::spatial_measure_distance(const Dictionary &p_args) {
	Dictionary result;
	String from_path = p_args.get("from_path", "");
	String to_path = p_args.get("to_path", "");

	Node *from_node = _get_node(from_path);
	Node *to_node = _get_node(to_path);

	if (!from_node) {
		result["ok"] = false;
		result["error"] = "From node not found.";
		return result;
	}
	if (!to_node) {
		result["ok"] = false;
		result["error"] = "To node not found.";
		return result;
	}

	result["ok"] = true;
	result["from"] = from_path;
	result["to"] = to_path;

	Node3D *from_3d = Object::cast_to<Node3D>(from_node);
	Node3D *to_3d = Object::cast_to<Node3D>(to_node);

	Node2D *from_2d = Object::cast_to<Node2D>(from_node);
	Node2D *to_2d = Object::cast_to<Node2D>(to_node);

	if (from_3d && to_3d) {
		double dist = from_3d->get_global_position().distance_to(to_3d->get_global_position());
		result["distance"] = dist;

		Dictionary f_pos;
		Vector3 p1 = from_3d->get_global_position();
		f_pos["x"] = p1.x;
		f_pos["y"] = p1.y;
		f_pos["z"] = p1.z;

		Dictionary t_pos;
		Vector3 p2 = to_3d->get_global_position();
		t_pos["x"] = p2.x;
		t_pos["y"] = p2.y;
		t_pos["z"] = p2.z;

		result["from_position"] = f_pos;
		result["to_position"] = t_pos;
		result["dimension"] = "3d";
	} else if (from_2d && to_2d) {
		double dist = from_2d->get_global_position().distance_to(to_2d->get_global_position());
		result["distance"] = dist;

		Dictionary f_pos;
		Vector2 p1 = from_2d->get_global_position();
		f_pos["x"] = p1.x;
		f_pos["y"] = p1.y;

		Dictionary t_pos;
		Vector2 p2 = to_2d->get_global_position();
		t_pos["x"] = p2.x;
		t_pos["y"] = p2.y;

		result["from_position"] = f_pos;
		result["to_position"] = t_pos;
		result["dimension"] = "2d";
	} else {
		result["ok"] = false;
		result["error"] = "Both nodes must be exclusively either Node2D or Node3D.";
	}

	return result;
}

Dictionary JustAMCPSpatialTools::spatial_bake_navigation(const Dictionary &p_args) {
	Dictionary result;
	String node_path = p_args.get("node_path", "");
	bool on_thread = p_args.get("on_thread", false);

	if (node_path.is_empty()) {
		result["ok"] = false;
		result["error"] = "Requires node_path parameter targeting a NavigationRegion.";
		return result;
	}

	Node *node = _get_node(node_path);
	if (!node) {
		result["ok"] = false;
		result["error"] = "Node not found at path: " + node_path;
		return result;
	}

	if (NavigationRegion3D *nav3d = Object::cast_to<NavigationRegion3D>(node)) {
		nav3d->bake_navigation_mesh(on_thread);
		result["ok"] = true;
		result["message"] = "NavigationRegion3D bake requested.";
		return result;
	}

	if (NavigationRegion2D *nav2d = Object::cast_to<NavigationRegion2D>(node)) {
		nav2d->bake_navigation_polygon(on_thread);
		result["ok"] = true;
		result["message"] = "NavigationRegion2D bake requested.";
		return result;
	}

	result["ok"] = false;
	result["error"] = "Target node is not a NavigationRegion2D or NavigationRegion3D.";
	return result;
}

Dictionary JustAMCPSpatialTools::navigation_set_layers(const Dictionary &p_args) {
	Dictionary result;
	String node_path = p_args.get("node_path", "");
	int layer_integer = p_args.get("layers", 1);

	if (node_path.is_empty()) {
		result["ok"] = false;
		result["error"] = "Requires node_path parameter targeting a NavigationRegion.";
		return result;
	}

	Node *node = _get_node(node_path);
	if (!node) {
		result["ok"] = false;
		result["error"] = "Node not found at path: " + node_path;
		return result;
	}

	if (NavigationRegion3D *nav3d = Object::cast_to<NavigationRegion3D>(node)) {
		nav3d->set_navigation_layers(layer_integer);
		result["ok"] = true;
		result["message"] = "NavigationRegion3D layers updated.";
		return result;
	}

	if (NavigationRegion2D *nav2d = Object::cast_to<NavigationRegion2D>(node)) {
		nav2d->set_navigation_layers(layer_integer);
		result["ok"] = true;
		result["message"] = "NavigationRegion2D layers updated.";
		return result;
	}

	result["ok"] = false;
	result["error"] = "Target node is not a NavigationRegion2D or NavigationRegion3D.";
	return result;
}

Dictionary JustAMCPSpatialTools::navigation_get_info(const Dictionary &p_args) {
	Dictionary result;
	String root_path = p_args.get("node_path", "");
	Node *root = _get_node(root_path);
	if (!root) {
		result["ok"] = false;
		result["error"] = "Target root node not found.";
		return result;
	}

	Array regions;
	Array agents;
	Vector<Node *> stack;
	stack.push_back(root);
	while (!stack.is_empty()) {
		Node *node = stack[stack.size() - 1];
		stack.resize(stack.size() - 1);
		if (NavigationRegion3D *region3d = Object::cast_to<NavigationRegion3D>(node)) {
			Dictionary info;
			info["path"] = String(region3d->get_path());
			info["class"] = region3d->get_class();
			info["layers"] = region3d->get_navigation_layers();
			info["enabled"] = region3d->is_enabled();
			info["has_mesh"] = region3d->get_navigation_mesh().is_valid();
			regions.push_back(info);
		} else if (NavigationRegion2D *region2d = Object::cast_to<NavigationRegion2D>(node)) {
			Dictionary info;
			info["path"] = String(region2d->get_path());
			info["class"] = region2d->get_class();
			info["layers"] = region2d->get_navigation_layers();
			info["enabled"] = region2d->is_enabled();
			info["has_polygon"] = region2d->get_navigation_polygon().is_valid();
			regions.push_back(info);
		} else if (NavigationAgent3D *agent3d = Object::cast_to<NavigationAgent3D>(node)) {
			Dictionary info;
			info["path"] = String(agent3d->get_path());
			info["class"] = agent3d->get_class();
			info["path_desired_distance"] = agent3d->get_path_desired_distance();
			info["target_desired_distance"] = agent3d->get_target_desired_distance();
			agents.push_back(info);
		} else if (NavigationAgent2D *agent2d = Object::cast_to<NavigationAgent2D>(node)) {
			Dictionary info;
			info["path"] = String(agent2d->get_path());
			info["class"] = agent2d->get_class();
			info["path_desired_distance"] = agent2d->get_path_desired_distance();
			info["target_desired_distance"] = agent2d->get_target_desired_distance();
			agents.push_back(info);
		}
		for (int i = 0; i < node->get_child_count(); i++) {
			stack.push_back(node->get_child(i));
		}
	}

	result["ok"] = true;
	result["regions"] = regions;
	result["agents"] = agents;
	result["region_count"] = regions.size();
	result["agent_count"] = agents.size();
	return result;
}

Dictionary JustAMCPSpatialTools::execute_tool(const String &p_tool_name, const Dictionary &p_args) {
	if (p_tool_name == "spatial_analyze_layout") {
		return spatial_analyze_layout(p_args);
	}
	if (p_tool_name == "spatial_suggest_placement") {
		return spatial_suggest_placement(p_args);
	}
	if (p_tool_name == "spatial_detect_overlaps") {
		return spatial_detect_overlaps(p_args);
	}
	if (p_tool_name == "spatial_measure_distance") {
		return spatial_measure_distance(p_args);
	}
	if (p_tool_name == "spatial_bake_navigation") {
		return spatial_bake_navigation(p_args);
	}
	if (p_tool_name == "navigation_set_layers") {
		return navigation_set_layers(p_args);
	}
	if (p_tool_name == "navigation_get_info") {
		return navigation_get_info(p_args);
	}
	return Dictionary();
}

#endif
