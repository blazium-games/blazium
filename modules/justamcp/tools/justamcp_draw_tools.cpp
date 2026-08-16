/**************************************************************************/
/*  justamcp_draw_tools.cpp                                               */
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

#include "justamcp_draw_tools.h"

#include "../justamcp_editor_plugin.h"
#include "../justamcp_editor_scene_access.h"

#include "core/io/dir_access.h" // IWYU pragma: keep
#include "core/io/file_access.h" // IWYU pragma: keep
#include "editor/editor_interface.h"
#include "editor/editor_undo_redo_manager.h"
#include "scene/gui/control.h"

#include "modules/gdscript/gdscript.h"

void JustAMCPDrawTools::_bind_methods() {}

const char *DRAW_RECIPE_SCRIPT_SOURCE = R"(
@tool
extends Control

func _draw() -> void:
	if not has_meta("_ops"):
		return
	var ops = get_meta("_ops")
	if not ops is Array:
		return

	for op in ops:
		if not op is Dictionary:
			continue
		var type = op.get("draw", "")
		match type:
			"line":
				draw_line(op.from, op.to, op.color, op.get("width", 1.0), op.get("antialiased", false))
			"rect":
				draw_rect(op.rect, op.color, op.get("filled", true), op.get("width", 1.0))
			"circle":
				draw_circle(op.center, op.radius, op.color)
			"arc":
				draw_arc(op.center, op.radius, op.start_angle, op.end_angle, op.get("point_count", 32), op.color, op.get("width", 1.0), op.get("antialiased", false))
			"string":
				# Standard draw_string requires a font. We'll use Theme's default.
				var font = get_theme_default_font()
				var font_size = get_theme_default_font_size()
				if op.has("font_size"): font_size = op.font_size
				draw_string(font, op.position, op.text, op.get("align", HORIZONTAL_ALIGNMENT_LEFT), op.get("max_width", -1), font_size, op.color)
)";

Dictionary JustAMCPDrawTools::execute_tool(const String &p_tool_name, const Dictionary &p_args) {
	if (p_tool_name == "control_draw_recipe") {
		return control_draw_recipe(p_args);
	}

	return Dictionary();
}

Dictionary JustAMCPDrawTools::control_draw_recipe(const Dictionary &p_args) {
	String path = p_args.get("path", "");
	Array ops = p_args.get("ops", Array());
	bool clear_existing = p_args.get("clear_existing", true);

	if (path.is_empty()) {
		Dictionary ret;
		ret["ok"] = false;
		ret["error"] = "Missing required param: path";
		return ret;
	}

	Node *scene_root = nullptr;
	if (editor_plugin && editor_plugin->get_editor_interface()) {
		scene_root = JustAMCPEditorSceneAccess::get_edited_root();
	}

	if (!scene_root) {
		Dictionary ret;
		ret["ok"] = false;
		ret["error"] = "No open scene.";
		return ret;
	}

	Node *node = scene_root->get_node_or_null(NodePath(path));
	if (!node) {
		Dictionary ret;
		ret["ok"] = false;
		ret["error"] = "Node not found at path: " + path;
		return ret;
	}

	Control *control = Object::cast_to<Control>(node);
	if (!control) {
		Dictionary ret;
		ret["ok"] = false;
		ret["error"] = "Target node must be a Control, got " + node->get_class();
		return ret;
	}

	Array recipe_ops = ops;
	if (!clear_existing && node->has_meta("_ops")) {
		Variant existing = node->get_meta("_ops");
		if (existing.get_type() == Variant::ARRAY) {
			recipe_ops = existing;
			for (int i = 0; i < ops.size(); i++) {
				recipe_ops.push_back(ops[i]);
			}
		}
	}

	Ref<GDScript> gds;
	gds.instantiate();
	gds->set_source_code(DRAW_RECIPE_SCRIPT_SOURCE);
	gds->reload();

	EditorUndoRedoManager *ur = nullptr;
	if (editor_plugin && editor_plugin->get_editor_interface()) {
		ur = editor_plugin->get_editor_interface()->get_editor_undo_redo();
	}

	if (ur) {
		ur->create_action("Attach Draw Recipe to " + node->get_name());
		ur->add_do_method(node, "set_script", gds);
		ur->add_do_method(node, "set_meta", "_ops", recipe_ops);
		ur->add_do_method(node, "queue_redraw");
		ur->add_undo_method(node, "set_script", node->get_script());
		if (node->has_meta("_ops")) {
			ur->add_undo_method(node, "set_meta", "_ops", node->get_meta("_ops"));
		} else {
			ur->add_undo_method(node, "remove_meta", "_ops");
		}
		ur->add_undo_method(node, "queue_redraw");
		ur->commit_action();
	} else {
		node->set_script(gds);
		node->set_meta("_ops", recipe_ops);
		control->queue_redraw();
	}

	Dictionary ret;
	ret["ok"] = true;
	ret["ops_count"] = recipe_ops.size();
	ret["message"] = "Attached draw recipe with " + itos(recipe_ops.size()) + " operations to " + path;
	return ret;
}

JustAMCPDrawTools::JustAMCPDrawTools() {}
JustAMCPDrawTools::~JustAMCPDrawTools() {}
