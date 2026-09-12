/**************************************************************************/
/*  justamcp_physics_tools.cpp                                            */
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

#include "justamcp_physics_tools.h"

#ifdef TOOLS_ENABLED
#include "editor/editor_undo_redo_manager.h"
#endif

#include "scene/2d/physics/area_2d.h"
#include "scene/2d/physics/character_body_2d.h"
#include "scene/2d/physics/collision_shape_2d.h"
#include "scene/2d/physics/physics_body_2d.h"
#include "scene/2d/physics/ray_cast_2d.h"
#include "scene/2d/physics/rigid_body_2d.h"
#include "scene/2d/physics/static_body_2d.h"
#include "scene/2d/sprite_2d.h"
#include "scene/3d/physics/area_3d.h"
#include "scene/3d/physics/character_body_3d.h"
#include "scene/3d/physics/collision_shape_3d.h"
#include "scene/3d/physics/physics_body_3d.h"
#include "scene/3d/physics/ray_cast_3d.h"
#include "scene/3d/physics/rigid_body_3d.h"
#include "scene/3d/physics/static_body_3d.h"
#include "scene/3d/visual_instance_3d.h"
#include "scene/gui/control.h"
#include "scene/gui/texture_rect.h"

#include "scene/resources/2d/capsule_shape_2d.h"
#include "scene/resources/2d/circle_shape_2d.h"
#include "scene/resources/2d/convex_polygon_shape_2d.h"
#include "scene/resources/2d/rectangle_shape_2d.h"
#include "scene/resources/2d/segment_shape_2d.h"

#include "scene/resources/3d/box_shape_3d.h"
#include "scene/resources/3d/capsule_shape_3d.h"
#include "scene/resources/3d/sphere_shape_3d.h"

#include "../justamcp_editor_scene_access.h"
#include "../justamcp_mcp_tool_macros.h"
#include "justamcp_agent_helpers.h"
#include "justamcp_scene_file_io.h"
#include "justamcp_tilemap_access.h"

void JustAMCPPhysicsTools::_bind_methods() {}

JustAMCPPhysicsTools::JustAMCPPhysicsTools() {}
JustAMCPPhysicsTools::~JustAMCPPhysicsTools() {}

Node *JustAMCPPhysicsTools::_find_node_by_path(const String &p_path) {
	Node *root = JustAMCPEditorSceneAccess::get_edited_root();
	if (!root) {
		return nullptr;
	}

	if (p_path == "." || p_path == root->get_name()) {
		return root;
	}
	if (root->has_node(p_path)) {
		return root->get_node(p_path);
	}

	if (p_path.begins_with(String(root->get_name()) + "/")) {
		String rel = p_path.substr(String(root->get_name()).length() + 1);
		if (root->has_node(rel)) {
			return root->get_node(rel);
		}
	}
	return nullptr;
}

String JustAMCPPhysicsTools::_detect_dimension(Node *p_node) {
	Node *current = p_node;
	while (current) {
		if (current->is_class("Node2D") || current->is_class("Control")) {
			return "2d";
		}
		if (current->is_class("Node3D")) {
			return "3d";
		}
		current = current->get_parent();
	}
	return "";
}

Dictionary JustAMCPPhysicsTools::execute_tool(const String &p_tool_name, const Dictionary &p_args) {
	if (p_tool_name == "setup_collision") {
		return _setup_collision(p_args);
	}
	if (p_tool_name == "set_physics_layers") {
		return _set_physics_layers(p_args);
	}
	if (p_tool_name == "autofit") {
		return _autofit(p_args);
	}
	if (p_tool_name == "get_physics_layers") {
		return _get_physics_layers(p_args);
	}
	if (p_tool_name == "add_raycast") {
		return _add_raycast(p_args);
	}
	if (p_tool_name == "setup_physics_body") {
		return _setup_physics_body(p_args);
	}
	if (p_tool_name == "get_collision_info") {
		return _get_collision_info(p_args);
	}
	if (p_tool_name == "validate_physics_setup") {
		return validate_physics_setup(p_args);
	}

	Dictionary err;
	err["code"] = -32601;
	err["message"] = "Method not found: " + p_tool_name;
	Dictionary res;
	res["error"] = err;
	return Dictionary();
}

Dictionary JustAMCPPhysicsTools::_setup_collision(const Dictionary &p_params) {
	if (!p_params.has("node_path")) {
		return MCP_INVALID_PARAMS("Missing param: node_path");
	}
	String node_path = p_params["node_path"];

	if (!p_params.has("shape")) {
		return MCP_INVALID_PARAMS("Missing param: shape");
	}
	String shape_name = p_params["shape"];

	Node *root = JustAMCPEditorSceneAccess::get_edited_root();
	if (!root) {
		return MCP_ERROR(-32000, "No scene is currently open");
	}

	Node *node = _find_node_by_path(node_path);
	if (!node) {
		return MCP_NOT_FOUND("Node '" + node_path + "'");
	}

	String dim = _detect_dimension(node);
	if (dim.is_empty()) {
		dim = p_params.has("dimension") ? String(p_params["dimension"]) : "2d";
	}

	bool is_valid_parent = false;
	if (dim == "2d") {
		if (node->is_class("PhysicsBody2D") || node->is_class("Area2D") || node->is_class("StaticBody2D") ||
				node->is_class("CharacterBody2D") || node->is_class("RigidBody2D") || node->is_class("AnimatableBody2D")) {
			is_valid_parent = true;
		}
	} else {
		if (node->is_class("PhysicsBody3D") || node->is_class("Area3D") || node->is_class("StaticBody3D") ||
				node->is_class("CharacterBody3D") || node->is_class("RigidBody3D") || node->is_class("AnimatableBody3D")) {
			is_valid_parent = true;
		}
	}

	if (!is_valid_parent) {
		return MCP_INVALID_PARAMS("Node '" + node_path + "' (" + node->get_class() + ") is not a physics body or area.");
	}

	Node *collision_node = nullptr;
	String shape_class_name;

	if (dim == "2d") {
		CollisionShape2D *c2d = memnew(CollisionShape2D);
		Ref<Shape2D> shape;
		if (shape_name == "rectangle" || shape_name == "rect") {
			Ref<RectangleShape2D> r;
			r.instantiate();
			r->set_size(Vector2(float(p_params.get("width", 32.0)), float(p_params.get("height", 32.0))));
			shape = r;
		} else if (shape_name == "circle") {
			Ref<CircleShape2D> c;
			c.instantiate();
			c->set_radius(float(p_params.get("radius", 16.0)));
			shape = c;
		} else if (shape_name == "capsule") {
			Ref<CapsuleShape2D> c;
			c.instantiate();
			c->set_radius(float(p_params.get("radius", 16.0)));
			c->set_height(float(p_params.get("height", 40.0)));
			shape = c;
		} else {
			memdelete(c2d);
			return MCP_INVALID_PARAMS("Unknown 2D shape or not supported yet: " + shape_name);
		}
		c2d->set_shape(shape);
		c2d->set_disabled(bool(p_params.get("disabled", false)));
		c2d->set_one_way_collision(bool(p_params.get("one_way_collision", false)));
		collision_node = c2d;
		shape_class_name = shape->get_class();
	} else {
		CollisionShape3D *c3d = memnew(CollisionShape3D);
		Ref<Shape3D> shape;
		if (shape_name == "box" || shape_name == "rectangle" || shape_name == "rect") {
			Ref<BoxShape3D> b;
			b.instantiate();
			b->set_size(Vector3(float(p_params.get("width", 1.0)), float(p_params.get("height", 1.0)), float(p_params.get("depth", 1.0))));
			shape = b;
		} else if (shape_name == "sphere" || shape_name == "circle") {
			Ref<SphereShape3D> s;
			s.instantiate();
			s->set_radius(float(p_params.get("radius", 0.5)));
			shape = s;
		} else if (shape_name == "capsule") {
			Ref<CapsuleShape3D> c;
			c.instantiate();
			c->set_radius(float(p_params.get("radius", 0.5)));
			c->set_height(float(p_params.get("height", 2.0)));
			shape = c;
		} else {
			memdelete(c3d);
			return MCP_INVALID_PARAMS("Unknown 3D shape or not supported yet: " + shape_name);
		}
		c3d->set_shape(shape);
		c3d->set_disabled(bool(p_params.get("disabled", false)));
		collision_node = c3d;
		shape_class_name = shape->get_class();
	}

	collision_node->set_name("CollisionShape");

#ifdef TOOLS_ENABLED
	EditorUndoRedoManager *ur = EditorUndoRedoManager::get_singleton();
	if (ur) {
		ur->create_action("MCP: Add CollisionShape to " + node->get_name());
		ur->add_do_method(node, "add_child", collision_node);
		ur->add_do_method(collision_node, "set_owner", root);
		ur->add_do_reference(collision_node);
		ur->add_undo_method(node, "remove_child", collision_node);
		ur->commit_action();
	} else {
#endif
		node->add_child(collision_node);
		collision_node->set_owner(root);
#ifdef TOOLS_ENABLED
	}
#endif

	Dictionary res;
	res["node_path"] = root->get_path_to(collision_node);
	res["shape_type"] = shape_class_name;
	res["dimension"] = dim.to_upper();
	return MCP_SUCCESS(res);
}

Dictionary JustAMCPPhysicsTools::_autofit(const Dictionary &p_params) {
	String node_path = p_params.get("path", "");
	Node *node = _find_node_by_path(node_path);
	if (!node) {
		return MCP_NOT_FOUND(node_path);
	}

	bool is_3d = node->is_class("CollisionShape3D");
	bool is_2d = node->is_class("CollisionShape2D");
	if (!is_3d && !is_2d) {
		return MCP_INVALID_PARAMS("Node must be CollisionShape2D or 3D");
	}

	String source_path = p_params.get("source_path", "");
	Node *source = nullptr;
	if (source_path.is_empty()) {
		Node *parent = node->get_parent();
		if (parent) {
			for (int i = 0; i < parent->get_child_count(); i++) {
				Node *c = parent->get_child(i);
				if (c == node) {
					continue;
				}
				if (is_3d && c->is_class("VisualInstance3D")) {
					source = c;
					break;
				}
				if (is_2d && (c->is_class("Sprite2D") || c->is_class("TextureRect"))) {
					source = c;
					break;
				}
			}
		}
	} else {
		source = _find_node_by_path(source_path);
	}

	if (!source) {
		return MCP_INVALID_PARAMS("No measurement source found.");
	}

	String shape_type = p_params.get("shape_type", is_3d ? "box" : "rectangle");

	EditorUndoRedoManager *ur = nullptr;
#ifdef TOOLS_ENABLED
	ur = EditorUndoRedoManager::get_singleton();
#endif

	if (is_3d) {
		CollisionShape3D *cs = Object::cast_to<CollisionShape3D>(node);
		VisualInstance3D *vi = Object::cast_to<VisualInstance3D>(source);
		if (cs && vi) {
			AABB aabb = vi->get_aabb();
			Vector3 sz = aabb.size * vi->get_scale();
			Ref<Shape3D> shape;
			if (shape_type == "box") {
				Ref<BoxShape3D> b;
				b.instantiate();
				b->set_size(sz);
				shape = b;
			} else if (shape_type == "sphere") {
				Ref<SphereShape3D> s;
				s.instantiate();
				s->set_radius(sz.length() * 0.5);
				shape = s;
			}
			if (shape.is_valid()) {
				if (ur) {
					ur->create_action("Autofit Shape 3D");
					ur->add_do_property(cs, "shape", shape);
					ur->add_undo_property(cs, "shape", cs->get_shape());
					ur->add_do_reference(shape.ptr());
					ur->commit_action();
				} else {
					cs->set_shape(shape);
				}
			}
		}
	} else {
		CollisionShape2D *cs = Object::cast_to<CollisionShape2D>(node);
		if (cs) {
			Vector2 sz;
			if (Sprite2D *s = Object::cast_to<Sprite2D>(source)) {
				sz = s->get_rect().size * s->get_scale();
			} else if (Control *c = Object::cast_to<Control>(source)) {
				sz = c->get_size();
			}
			Ref<Shape2D> shape;
			if (shape_type == "rectangle") {
				Ref<RectangleShape2D> r;
				r.instantiate();
				r->set_size(sz);
				shape = r;
			} else if (shape_type == "circle") {
				Ref<CircleShape2D> c;
				c.instantiate();
				c->set_radius(sz.length() * 0.5);
				shape = c;
			}
			if (shape.is_valid()) {
				if (ur) {
					ur->create_action("Autofit Shape 2D");
					ur->add_do_property(cs, "shape", shape);
					ur->add_undo_property(cs, "shape", cs->get_shape());
					ur->add_do_reference(shape.ptr());
					ur->commit_action();
				} else {
					cs->set_shape(shape);
				}
			}
		}
	}

	return MCP_SUCCESS(node_path);
}

Dictionary JustAMCPPhysicsTools::_set_physics_layers(const Dictionary &p_params) {
	if (!p_params.has("node_path")) {
		return MCP_INVALID_PARAMS("Missing param: node_path");
	}
	String node_path = p_params["node_path"];

	Node *node = _find_node_by_path(node_path);
	if (!node) {
		return MCP_NOT_FOUND("Node '" + node_path + "'");
	}

	uint32_t current_layer = 1;
	uint32_t current_mask = 1;

	bool valid_layer = false;
	bool valid_mask = false;
	Variant vlayer = node->get("collision_layer", &valid_layer);
	Variant vmask = node->get("collision_mask", &valid_mask);
	if (valid_layer) {
		current_layer = vlayer;
	}
	if (valid_mask) {
		current_mask = vmask;
	}

	if (!valid_layer && !valid_mask) {
		return MCP_INVALID_PARAMS("Node does not support collision_layer/mask properties.");
	}

	if (p_params.has("layer")) {
		current_layer = int(p_params["layer"]);
	}
	if (p_params.has("mask")) {
		current_mask = int(p_params["mask"]);
	}

	if (valid_layer) {
		node->set("collision_layer", current_layer);
	}
	if (valid_mask) {
		node->set("collision_mask", current_mask);
	}

	Dictionary res;
	res["node_path"] = node_path;
	res["layer"] = current_layer;
	res["mask"] = current_mask;
	return MCP_SUCCESS(res);
}

Dictionary JustAMCPPhysicsTools::_get_physics_layers(const Dictionary &p_params) {
	if (!p_params.has("node_path")) {
		return MCP_INVALID_PARAMS("Missing param: node_path");
	}
	String node_path = p_params["node_path"];

	Node *node = _find_node_by_path(node_path);
	if (!node) {
		return MCP_NOT_FOUND("Node '" + node_path + "'");
	}

	uint32_t current_layer = 0;
	uint32_t current_mask = 0;

	bool valid_layer = false;
	bool valid_mask = false;
	Variant vlayer = node->get("collision_layer", &valid_layer);
	Variant vmask = node->get("collision_mask", &valid_mask);
	if (valid_layer) {
		current_layer = vlayer;
	}
	if (valid_mask) {
		current_mask = vmask;
	}

	Dictionary res;
	res["node_path"] = node_path;
	res["layer"] = current_layer;
	res["mask"] = current_mask;
	return MCP_SUCCESS(res);
}

Dictionary JustAMCPPhysicsTools::_add_raycast(const Dictionary &p_params) {
	if (!p_params.has("parent_path")) {
		return MCP_INVALID_PARAMS("Missing param: parent_path");
	}
	String parent_path = p_params["parent_path"];

	Node *root = JustAMCPEditorSceneAccess::get_edited_root();
	if (!root) {
		return MCP_ERROR(-32000, "No scene is currently open");
	}

	Node *parent = _find_node_by_path(parent_path);
	if (!parent) {
		return MCP_NOT_FOUND("Node '" + parent_path + "'");
	}

	String dim = _detect_dimension(parent);
	if (dim.is_empty()) {
		dim = p_params.has("dimension") ? String(p_params["dimension"]) : "2d";
	}

	Node *rc = nullptr;
	if (dim == "2d") {
		RayCast2D *r = memnew(RayCast2D);
		if (p_params.has("target_position")) {
			Dictionary tp = p_params["target_position"];
			r->set_target_position(Vector2(float(tp.get("x", 0)), float(tp.get("y", 50.0))));
		}
		if (p_params.has("enabled")) {
			r->set_enabled(bool(p_params["enabled"]));
		}
		if (p_params.has("collision_mask")) {
			r->set_collision_mask(int(p_params["collision_mask"]));
		}
		rc = r;
	} else {
		RayCast3D *r = memnew(RayCast3D);
		if (p_params.has("target_position")) {
			Dictionary tp = p_params["target_position"];
			r->set_target_position(Vector3(float(tp.get("x", 0)), float(tp.get("y", -50.0)), float(tp.get("z", 0))));
		}
		if (p_params.has("enabled")) {
			r->set_enabled(bool(p_params["enabled"]));
		}
		if (p_params.has("collision_mask")) {
			r->set_collision_mask(int(p_params["collision_mask"]));
		}
		rc = r;
	}

	rc->set_name(p_params.get("name", "RayCast"));
	parent->add_child(rc, true);
	rc->set_owner(root);

	Dictionary res;
	res["node_path"] = root->get_path_to(rc);
	res["dimension"] = dim.to_upper();
	return MCP_SUCCESS(res);
}

Dictionary JustAMCPPhysicsTools::_setup_physics_body(const Dictionary &p_params) {
	if (!p_params.has("parent_path")) {
		return MCP_INVALID_PARAMS("Missing param: parent_path");
	}
	String parent_path = p_params["parent_path"];
	if (!p_params.has("body_type")) {
		return MCP_INVALID_PARAMS("Missing param: body_type");
	}
	String body_type = p_params["body_type"];

	Node *root = JustAMCPEditorSceneAccess::get_edited_root();
	if (!root) {
		return MCP_ERROR(-32000, "No scene is currently open");
	}

	Node *parent = _find_node_by_path(parent_path);
	if (!parent) {
		return MCP_NOT_FOUND("Node '" + parent_path + "'");
	}

	String dim = _detect_dimension(parent);
	if (dim.is_empty()) {
		dim = p_params.has("dimension") ? String(p_params["dimension"]) : "2d";
	}

	Node *body = nullptr;
	String t = body_type.to_lower();
	if (dim == "2d") {
		if (t == "static") {
			body = memnew(StaticBody2D);
		} else if (t == "character" || t == "kinematic") {
			body = memnew(CharacterBody2D);
		} else if (t == "rigid") {
			body = memnew(RigidBody2D);
		} else if (t == "area") {
			body = memnew(Area2D);
		} else {
			return MCP_INVALID_PARAMS("Unknown 2D body type: " + body_type);
		}
	} else {
		if (t == "static") {
			body = memnew(StaticBody3D);
		} else if (t == "character" || t == "kinematic") {
			body = memnew(CharacterBody3D);
		} else if (t == "rigid") {
			body = memnew(RigidBody3D);
		} else if (t == "area") {
			body = memnew(Area3D);
		} else {
			return MCP_INVALID_PARAMS("Unknown 3D body type: " + body_type);
		}
	}

	body->set_name(p_params.get("name", String(body->get_class())));

	if (p_params.has("collision_layer")) {
		body->set("collision_layer", int(p_params["collision_layer"]));
	}
	if (p_params.has("collision_mask")) {
		body->set("collision_mask", int(p_params["collision_mask"]));
	}

	parent->add_child(body, true);
	body->set_owner(root);

	Dictionary res;
	res["node_path"] = root->get_path_to(body);
	res["class"] = body->get_class();
	res["dimension"] = dim.to_upper();
	return MCP_SUCCESS(res);
}

Dictionary JustAMCPPhysicsTools::_get_collision_info(const Dictionary &p_params) {
	if (!p_params.has("node_path")) {
		return MCP_INVALID_PARAMS("Missing param: node_path");
	}
	String node_path = p_params["node_path"];

	Node *node = _find_node_by_path(node_path);
	if (!node) {
		return MCP_NOT_FOUND("Node '" + node_path + "'");
	}

	bool valid_layer = false, valid_mask = false;
	Variant vlayer = node->get("collision_layer", &valid_layer);
	Variant vmask = node->get("collision_mask", &valid_mask);

	Dictionary info;
	info["node_path"] = node_path;
	info["type"] = node->get_class();
	if (valid_layer) {
		info["collision_layer"] = vlayer;
	}
	if (valid_mask) {
		info["collision_mask"] = vmask;
	}

	Array shapes;
	for (int i = 0; i < node->get_child_count(); i++) {
		Node *c = node->get_child(i);
		if (c->is_class("CollisionShape2D") || c->is_class("CollisionShape3D") ||
				c->is_class("CollisionPolygon2D") || c->is_class("CollisionPolygon3D")) {
			Dictionary shape_info;
			shape_info["name"] = c->get_name();
			shape_info["type"] = c->get_class();
			shape_info["disabled"] = c->get("disabled");
			shapes.push_back(shape_info);
		}
	}
	info["shapes"] = shapes;

	return MCP_SUCCESS(info);
}

Dictionary JustAMCPPhysicsTools::validate_physics_setup(const Dictionary &p_params) {
	const String file_path = p_params.get("file_path", p_params.get("scene_path", ""));
	if (file_path.is_empty()) {
		return MCP_INVALID_PARAMS("file_path is required");
	}
	Node *root = nullptr;
	Dictionary load_err = justamcp_load_scene_root(file_path, &root);
	if (!load_err.is_empty()) {
		return load_err.has("error") ? MCP_ERROR(-32000, String(load_err["error"])) : MCP_ERROR(-32000, "Failed to load scene");
	}

	Array issues;
	Array suggestions;
	auto check_body = [&](const String &p_path, bool p_is_character) {
		Node *node = justamcp_find_node_in_root(root, p_path);
		if (!node) {
			Dictionary issue;
			issue["path"] = p_path;
			issue["message"] = "Node not found.";
			issues.push_back(issue);
			return;
		}
		if (!node->has_method("get_collision_mask") && !ClassDB::has_property(node->get_class(), "collision_mask")) {
			Dictionary issue;
			issue["path"] = p_path;
			issue["message"] = "Node has no collision_layer/mask property";
			issues.push_back(issue);
			return;
		}
		const int mask = int(node->get("collision_mask"));
		const int layer = int(node->get("collision_layer"));
		if (mask == 0) {
			Dictionary issue;
			issue["path"] = p_path;
			issue["message"] = "collision_mask is 0";
			issues.push_back(issue);
			suggestions.push_back("Set collision_mask so " + p_path + " can collide with platforms.");
		}
		bool has_shape = false;
		for (int i = 0; i < node->get_child_count(); i++) {
			Node *child = node->get_child(i);
			if (child->is_class("CollisionShape2D") || child->is_class("CollisionShape3D")) {
				Variant shape = child->get("shape");
				if (shape.get_type() != Variant::NIL) {
					has_shape = true;
				} else {
					Dictionary issue;
					issue["path"] = String(root->get_path_to(child));
					issue["message"] = "Collision shape is empty.";
					issues.push_back(issue);
				}
			}
		}
		if (!has_shape) {
			Dictionary issue;
			issue["path"] = p_path;
			issue["message"] = "No valid CollisionShape child.";
			issues.push_back(issue);
		}
		if (p_is_character) {
			Dictionary info;
			info["path"] = p_path;
			info["collision_layer"] = layer;
			info["collision_mask"] = mask;
		}
	};

	Array characters = p_params.get("character_nodes", Array());
	Array platforms = p_params.get("platform_nodes", Array());
	for (int i = 0; i < characters.size(); i++) {
		check_body(String(characters[i]), true);
	}

	int platform_layers = 0;
	for (int i = 0; i < platforms.size(); i++) {
		const String path = String(platforms[i]);
		check_body(path, false);
		Node *node = justamcp_find_node_in_root(root, path);
		if (!node) {
			continue;
		}
		JustAMCPTileTarget tiles = justamcp_tile_target_from_node(node, 0);
		if (tiles.valid()) {
			Ref<TileSet> tileset = justamcp_tile_get_tileset(tiles);
			if (tileset.is_null() || tileset->get_physics_layers_count() == 0) {
				Dictionary issue;
				issue["path"] = path;
				issue["message"] = "TileSet has no physics layers configured.";
				issues.push_back(issue);
				suggestions.push_back("Configure TileSet physics layers on " + path + ".");
			} else {
				platform_layers |= int(tileset->get_physics_layer_collision_layer(0));
			}
		} else if (ClassDB::has_property(node->get_class(), "collision_layer")) {
			platform_layers |= int(node->get("collision_layer"));
		}
	}

	for (int i = 0; i < characters.size(); i++) {
		Node *node = justamcp_find_node_in_root(root, String(characters[i]));
		if (!node || !ClassDB::has_property(node->get_class(), "collision_mask")) {
			continue;
		}
		const int mask = int(node->get("collision_mask"));
		if (platform_layers != 0 && (mask & platform_layers) == 0) {
			Dictionary issue;
			issue["path"] = String(characters[i]);
			issue["message"] = "collision_mask does not overlap platform collision_layers";
			issues.push_back(issue);
			suggestions.push_back("Overlap " + String(characters[i]) + " collision_mask with platform layers.");
		}
	}

	memdelete(root);
	Dictionary res;
	res["issues"] = issues;
	res["suggestions"] = suggestions;
	res["valid"] = issues.is_empty();
	return justamcp_ok(res);
}
