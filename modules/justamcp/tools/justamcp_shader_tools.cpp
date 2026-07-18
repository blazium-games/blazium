/**************************************************************************/
/*  justamcp_shader_tools.cpp                                             */
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

#include "justamcp_shader_tools.h"
#include "../justamcp_editor_filesystem.h"
#include "../justamcp_editor_scene_access.h"
#include "../justamcp_read_limits.h"
#include "core/io/dir_access.h"
#include "core/io/file_access.h"
#include "core/io/resource_loader.h"
#include "core/math/expression.h"
#include "scene/3d/mesh_instance_3d.h"
#include "scene/main/canvas_item.h"
#include "scene/resources/material.h"
#include "scene/resources/shader.h"

#include "../justamcp_mcp_tool_macros.h"

JustAMCPShaderTools::JustAMCPShaderTools() {
}

JustAMCPShaderTools::~JustAMCPShaderTools() {
}

Node *JustAMCPShaderTools::_find_node_by_path(const String &p_path) {
	if (p_path == "." || p_path.is_empty()) {
		return JustAMCPEditorSceneAccess::get_edited_root();
	}
	Node *root = JustAMCPEditorSceneAccess::get_edited_root();
	if (!root) {
		return nullptr;
	}
	return root->get_node_or_null(NodePath(p_path));
}

Dictionary JustAMCPShaderTools::execute_tool(const String &p_tool_name, const Dictionary &p_args) {
	if (p_tool_name == "create_shader") {
		return create_shader(p_args);
	}
	if (p_tool_name == "read_shader") {
		return read_shader(p_args);
	}
	if (p_tool_name == "edit_shader") {
		return edit_shader(p_args);
	}
	if (p_tool_name == "assign_shader_material") {
		return assign_shader_material(p_args);
	}
	if (p_tool_name == "set_shader_param") {
		return set_shader_param(p_args);
	}
	if (p_tool_name == "get_shader_params") {
		return get_shader_params(p_args);
	}

	return Dictionary();
}

Dictionary JustAMCPShaderTools::create_shader(const Dictionary &p_params) {
	if (!p_params.has("path")) {
		return MCP_INVALID_PARAMS("Missing path");
	}
	String path = p_params["path"];
	String content = p_params.has("content") ? String(p_params["content"]) : "";
	String shader_type = p_params.has("shader_type") ? String(p_params["shader_type"]) : "spatial";

	if (content.is_empty()) {
		if (shader_type == "spatial") {
			content = "shader_type spatial;\n\nvoid vertex() {\n\t// Called for every vertex\n}\n\nvoid fragment() {\n\t// Called for every pixel\n\tALBEDO = vec3(1.0);\n}\n";
		} else if (shader_type == "canvas_item") {
			content = "shader_type canvas_item;\n\nvoid vertex() {\n\t// Called for every vertex\n}\n\nvoid fragment() {\n\t// Called for every pixel\n\tCOLOR = vec4(1.0);\n}\n";
		} else if (shader_type == "particles") {
			content = "shader_type particles;\n\nvoid start() {\n\t// Called when particle spawns\n}\n\nvoid process() {\n\t// Called every frame per particle\n}\n";
		} else if (shader_type == "sky") {
			content = "shader_type sky;\n\nvoid sky() {\n\tCOLOR = vec3(0.3, 0.5, 0.8);\n}\n";
		}
	}

	String dir_path = path.get_base_dir();
	Ref<DirAccess> da = DirAccess::create(DirAccess::ACCESS_RESOURCES);
	if (!da->dir_exists(dir_path)) {
		da->make_dir_recursive(dir_path);
	}

	Ref<FileAccess> file = FileAccess::open(path, FileAccess::WRITE);
	if (file.is_null()) {
		return MCP_ERROR(-32000, "Cannot create shader file: " + path);
	}

	file->store_string(content);
	file->close();

	JustAMCPEditorFilesystem::refresh_path(path);

	Dictionary res;
	res["path"] = path;
	res["shader_type"] = shader_type;
	res["created"] = true;
	return MCP_SUCCESS(res);
}

Dictionary JustAMCPShaderTools::read_shader(const Dictionary &p_params) {
	if (!p_params.has("path")) {
		return MCP_INVALID_PARAMS("Missing path");
	}
	String path = p_params["path"];

	if (!FileAccess::exists(path)) {
		return MCP_ERROR(-32000, "Shader not found: " + path);
	}

	String content;
	int64_t size = 0;
	Dictionary limit_err;
	if (!justamcp_read_utf8_within_limit(path, JUSTAMCP_MAX_SYNC_READ_BYTES, content, size, limit_err)) {
		if (limit_err.has("max_bytes")) {
			return MCP_ERROR(int(limit_err.get("error_code", -32602)), String(limit_err.get("error", "")));
		}
		return MCP_ERROR(-32000, "Cannot read shader: " + path);
	}

	Dictionary res;
	res["path"] = path;
	res["content"] = content;
	res["size"] = size;
	return MCP_SUCCESS(res);
}

Dictionary JustAMCPShaderTools::edit_shader(const Dictionary &p_params) {
	if (!p_params.has("path")) {
		return MCP_INVALID_PARAMS("Missing path");
	}
	String path = p_params["path"];

	if (!FileAccess::exists(path)) {
		return MCP_ERROR(-32000, "Shader not found: " + path);
	}

	int changes_made = 0;

	if (p_params.has("content")) {
		Ref<FileAccess> file = FileAccess::open(path, FileAccess::WRITE);
		if (file.is_null()) {
			return MCP_ERROR(-32000, "Cannot write shader: " + path);
		}
		file->store_string(p_params["content"]);
		file->close();
		changes_made = 1;
	} else if (p_params.has("replacements") && p_params["replacements"].get_type() == Variant::ARRAY) {
		String content;
		int64_t size = 0;
		Dictionary limit_err;
		if (!justamcp_read_utf8_within_limit(path, JUSTAMCP_MAX_SYNC_READ_BYTES, content, size, limit_err)) {
			if (limit_err.has("max_bytes")) {
				return MCP_ERROR(int(limit_err.get("error_code", -32602)), String(limit_err.get("error", "")));
			}
			return MCP_ERROR(-32000, "Cannot read shader: " + path);
		}

		Array replacements = p_params["replacements"];
		for (int i = 0; i < replacements.size(); i++) {
			if (replacements[i].get_type() == Variant::DICTIONARY) {
				Dictionary rep = replacements[i];
				String search = rep.get("search", "");
				String replace = rep.get("replace", "");
				if (!search.is_empty() && content.contains(search)) {
					content = content.replace(search, replace);
					changes_made++;
				}
			}
		}

		Ref<FileAccess> file = FileAccess::open(path, FileAccess::WRITE);
		if (file.is_valid()) {
			file->store_string(content);
			file->close();
		}
	}

	if (changes_made > 0) {
		JustAMCPEditorFilesystem::refresh_path(path);
		if (ResourceLoader::exists(path)) {
			Ref<Shader> shader = ResourceLoader::load(path);
			if (shader.is_valid()) {
				shader->reload_from_file();
			}
		}
	}

	Dictionary res;
	res["path"] = path;
	res["changes_made"] = changes_made;
	return MCP_SUCCESS(res);
}

Dictionary JustAMCPShaderTools::assign_shader_material(const Dictionary &p_params) {
	if (!p_params.has("node_path")) {
		return MCP_INVALID_PARAMS("Missing node_path");
	}
	if (!p_params.has("shader_path")) {
		return MCP_INVALID_PARAMS("Missing shader_path");
	}

	String node_path = p_params["node_path"];
	String shader_path = p_params["shader_path"];

	Node *node = _find_node_by_path(node_path);
	if (!node) {
		return MCP_ERROR(-32000, "Node not found");
	}

	if (!ResourceLoader::exists(shader_path)) {
		return MCP_ERROR(-32000, "Shader not found: " + shader_path);
	}
	Ref<Shader> shader = ResourceLoader::load(shader_path);
	if (shader.is_null()) {
		return MCP_ERROR(-32000, "Failed to load shader");
	}

	Ref<ShaderMaterial> material;
	material.instantiate();
	material->set_shader(shader);

	CanvasItem *ci = Object::cast_to<CanvasItem>(node);
	MeshInstance3D *mi = Object::cast_to<MeshInstance3D>(node);

	if (ci) {
		ci->set_material(material);
	} else if (mi) {
		mi->set_material_override(material);
	} else {
		bool valid = false;
		node->set("material", material, &valid);
		if (!valid) {
			return MCP_INVALID_PARAMS("Node does not support materials");
		}
	}

	Dictionary res;
	res["node_path"] = node_path;
	res["shader_path"] = shader_path;
	res["assigned"] = true;
	return MCP_SUCCESS(res);
}

Dictionary JustAMCPShaderTools::set_shader_param(const Dictionary &p_params) {
	if (!p_params.has("node_path")) {
		return MCP_INVALID_PARAMS("Missing node_path");
	}
	if (!p_params.has("param")) {
		return MCP_INVALID_PARAMS("Missing param");
	}

	String node_path = p_params["node_path"];
	String param_name = p_params["param"];

	Node *node = _find_node_by_path(node_path);
	if (!node) {
		return MCP_ERROR(-32000, "Node not found");
	}

	Ref<ShaderMaterial> material;
	CanvasItem *ci = Object::cast_to<CanvasItem>(node);
	MeshInstance3D *mi = Object::cast_to<MeshInstance3D>(node);

	if (ci) {
		material = ci->get_material();
	} else if (mi) {
		material = mi->get_material_override();
	}

	if (material.is_null()) {
		return MCP_ERROR(-32000, "Node has no ShaderMaterial");
	}

	Variant value = p_params.get("value", Variant());
	if (value.get_type() == Variant::STRING) {
		String s = value;
		Ref<Expression> expr;
		expr.instantiate();
		if (expr->parse(s) == OK) {
			Variant parsed = expr->execute(Array(), nullptr, false, true);
			if (parsed.get_type() != Variant::NIL && !expr->has_execute_failed()) {
				value = parsed;
			}
		}
	}

	material->set_shader_parameter(param_name, value);

	Dictionary res;
	res["node_path"] = node_path;
	res["param"] = param_name;
	res["value"] = String(value);
	return MCP_SUCCESS(res);
}

Dictionary JustAMCPShaderTools::get_shader_params(const Dictionary &p_params) {
	if (!p_params.has("node_path")) {
		return MCP_INVALID_PARAMS("Missing node_path");
	}
	String node_path = p_params["node_path"];

	Node *node = _find_node_by_path(node_path);
	if (!node) {
		return MCP_ERROR(-32000, "Node not found");
	}

	Ref<ShaderMaterial> material;
	CanvasItem *ci = Object::cast_to<CanvasItem>(node);
	MeshInstance3D *mi = Object::cast_to<MeshInstance3D>(node);

	if (ci) {
		material = ci->get_material();
	} else if (mi) {
		material = mi->get_material_override();
	}

	if (material.is_null()) {
		return MCP_ERROR(-32000, "Node has no ShaderMaterial");
	}

	Dictionary shader_params;
	List<PropertyInfo> plist;
	material->get_property_list(&plist);
	for (const PropertyInfo &prop : plist) {
		if (prop.name.begins_with("shader_parameter/")) {
			String key = prop.name.substr(17);
			shader_params[key] = material->get(prop.name);
		}
	}

	Dictionary res;
	res["node_path"] = node_path;
	res["params"] = shader_params;
	return MCP_SUCCESS(res);
}

#endif
