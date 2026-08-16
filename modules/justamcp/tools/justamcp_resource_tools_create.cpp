/**************************************************************************/
/*  justamcp_resource_tools_create.cpp                                    */
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

#include "../justamcp_editor_plugin.h"
#include "../justamcp_editor_scene_access.h"
#include "../justamcp_read_limits.h"
#include "justamcp_resource_tools.h"

#include "core/config/project_settings.h"
#include "core/io/resource_loader.h"
#include "core/object/class_db.h"

#ifdef MODULE_ASSETTAGS_ENABLED
#include "modules/assettags/asset_tag_registry.h"
#endif
#include "core/io/dir_access.h"
#include "core/io/file_access.h"
#include "core/io/image_loader.h"
#include "core/io/json.h"
#include "core/io/resource_saver.h"
#include "editor/editor_interface.h"
#include "editor/editor_node.h"
#include "editor/editor_undo_redo_manager.h"
#include "editor/file_system/editor_file_system.h"
#include "scene/2d/sprite_2d.h"
#include "scene/2d/tile_map.h"
#include "scene/3d/mesh_instance_3d.h"
#include "scene/3d/sprite_3d.h"
#include "scene/resources/2d/tile_set.h"
#include "scene/resources/material.h"
#include "scene/resources/packed_scene.h"
#include "scene/resources/shader.h"
#include "scene/resources/texture.h"
#include "scene/resources/theme.h"

Dictionary JustAMCPResourceTools::create_resource(const Dictionary &p_args) {
	String res_type = p_args.get("type", p_args.get("resourceType", "Resource"));
	String res_path = _ensure_res_path(p_args.get("resource_path", p_args.get("resourcePath", "")));
	String node_path = p_args.get("path", "");
	String property = p_args.get("property", "");
	bool overwrite = p_args.get("overwrite", false);

	if (res_path == "res://" && node_path.is_empty()) {
		Dictionary ret;
		ret["ok"] = false;
		ret["error"] = "Either resource_path or path+property is required.";
		return ret;
	}

	Object *_resource_obj = ClassDB::instantiate(StringName(res_type));
	Ref<Resource> resource = Object::cast_to<Resource>(_resource_obj);
	if (resource.is_null()) {
		if (_resource_obj) {
			memdelete(_resource_obj);
		}
		Dictionary ret;
		ret["ok"] = false;
		ret["error"] = "Failed to instantiate " + res_type;
		return ret;
	}

	if (p_args.has("properties")) {
		_set_resource_properties(resource, p_args["properties"]);
	}

	if (!res_path.is_empty() && res_path != "res://") {
		if (!overwrite && FileAccess::exists(res_path)) {
			Dictionary ret;
			ret["ok"] = false;
			ret["error"] = "Resource already exists. Pass overwrite=true to replace it.";
			ret["resource_path"] = res_path;
			return ret;
		}
		Error save_result = ResourceSaver::save(resource, res_path);
		if (save_result != OK) {
			Dictionary ret;
			ret["ok"] = false;
			ret["error"] = "Save failed";
			return ret;
		}
		_refresh_filesystem(res_path);
	}

	if (!node_path.is_empty() && !property.is_empty()) {
		Node *scene_root = nullptr;
		if (editor_plugin && editor_plugin->get_editor_interface()) {
			scene_root = JustAMCPEditorSceneAccess::get_edited_root();
		}
		if (scene_root) {
			Node *target = scene_root->get_node_or_null(NodePath(node_path));
			if (target) {
				EditorUndoRedoManager *ur = editor_plugin->get_editor_interface()->get_editor_undo_redo();
				if (ur) {
					ur->create_action("Create Resource: " + res_type);
					ur->add_do_property(target, property, resource);
					ur->add_undo_property(target, property, target->get(property));
					ur->add_do_reference(resource.ptr());
					ur->commit_action();
				} else {
					target->set(property, resource);
				}
			}
		}
	}

	Dictionary ret;
	ret["ok"] = true;
	ret["type"] = res_type;
	if (!res_path.is_empty()) {
		ret["resource_path"] = res_path;
	}
	return ret;
}

Dictionary JustAMCPResourceTools::modify_resource(const Dictionary &p_args) {
	String res_path = _ensure_res_path(p_args.get("resourcePath", ""));
	if (res_path == "res://") {
		Dictionary ret;
		ret["ok"] = false;
		ret["error"] = "resourcePath is required";
		return ret;
	}

	Ref<Resource> resource = ResourceLoader::load(res_path);
	if (resource.is_null()) {
		Dictionary ret;
		ret["ok"] = false;
		ret["error"] = "Resource not found";
		ret["resourcePath"] = res_path;
		return ret;
	}

	_set_resource_properties(resource, p_args.get("properties", ""));
	Error save_result = ResourceSaver::save(resource, res_path);
	if (save_result != OK) {
		Dictionary ret;
		ret["ok"] = false;
		ret["error"] = "Failed to save resource";
		ret["code"] = save_result;
		return ret;
	}

	_refresh_filesystem(res_path);
	Dictionary ret;
	ret["ok"] = true;
	ret["resourcePath"] = res_path;
	return ret;
}

Dictionary JustAMCPResourceTools::create_material(const Dictionary &p_args) {
	String mat_path = _ensure_res_path(p_args.get("materialPath", ""));
	String material_type = p_args.get("materialType", "StandardMaterial3D");
	if (mat_path == "res://") {
		Dictionary ret;
		ret["ok"] = false;
		ret["error"] = "materialPath is required";
		return ret;
	}

	Object *_material_obj = ClassDB::instantiate(StringName(material_type));
	Ref<Material> material = Object::cast_to<Material>(_material_obj);
	if (material.is_null()) {
		if (_material_obj) {
			memdelete(_material_obj);
		}
		Dictionary ret;
		ret["ok"] = false;
		ret["error"] = "Failed to instantiate material type";
		ret["materialType"] = material_type;
		return ret;
	}

	Ref<ShaderMaterial> smat = material;
	if (smat.is_valid()) {
		String shader_path = p_args.get("shader", "");
		if (!shader_path.is_empty()) {
			Ref<Shader> shader_res = ResourceLoader::load(_ensure_res_path(shader_path));
			if (shader_res.is_valid()) {
				smat->set_shader(shader_res);
			}
		}
	}

	if (p_args.has("properties")) {
		_set_resource_properties(material, p_args["properties"]);
	}

	Error save_result = ResourceSaver::save(material, mat_path);
	if (save_result != OK) {
		Dictionary ret;
		ret["ok"] = false;
		ret["error"] = "Failed to save material";
		ret["code"] = save_result;
		return ret;
	}

	_refresh_filesystem(mat_path);
	Dictionary ret;
	ret["ok"] = true;
	ret["materialPath"] = mat_path;
	ret["materialType"] = material_type;
	return ret;
}

Dictionary JustAMCPResourceTools::create_shader(const Dictionary &p_args) {
	String shader_path = _ensure_res_path(p_args.get("shaderPath", ""));
	String shader_type = p_args.get("shaderType", "canvas_item");
	if (shader_path == "res://") {
		Dictionary ret;
		ret["ok"] = false;
		ret["error"] = "shaderPath is required";
		return ret;
	}

	String code_text = p_args.get("code", "");
	String template_name = p_args.get("template", "");

	if (code_text.is_empty()) {
		if (!template_name.is_empty()) {
			if (template_name == "basic") {
				code_text = vformat("shader_type %s;\n\nvoid fragment() {\n\tCOLOR = vec4(1.0);\n}\n", shader_type);
			} else if (template_name == "color_shift") {
				code_text = vformat("shader_type %s;\n\nuniform vec4 color_shift : source_color = vec4(0.1, 0.0, 0.2, 0.0);\n\nvoid fragment() {\n\tvec4 base = texture(TEXTURE, UV);\n\tCOLOR = vec4(clamp(base.rgb + color_shift.rgb, vec3(0.0), vec3(1.0)), base.a);\n}\n", shader_type);
			} else if (template_name == "outline") {
				code_text = vformat("shader_type %s;\n\nuniform vec4 outline_color : source_color = vec4(0.0, 0.0, 0.0, 1.0);\nuniform float outline_width : hint_range(0.0, 8.0) = 1.0;\n\nvoid fragment() {\n\tvec2 px = TEXTURE_PIXEL_SIZE * outline_width;\n\tfloat a = texture(TEXTURE, UV).a;\n\tfloat edge = max(max(texture(TEXTURE, UV + vec2(px.x, 0.0)).a, texture(TEXTURE, UV - vec2(px.x, 0.0)).a), max(texture(TEXTURE, UV + vec2(0.0, px.y)).a, texture(TEXTURE, UV - vec2(0.0, px.y)).a));\n\tvec4 base = texture(TEXTURE, UV);\n\tCOLOR = mix(outline_color * edge, base, a);\n}\n", shader_type);
			} else {
				code_text = vformat("shader_type %s;\n\nvoid fragment() {\n}\n", shader_type);
			}
		} else {
			code_text = vformat("shader_type %s;\n\nvoid fragment() {\n}\n", shader_type);
		}
	}

	Ref<FileAccess> file = FileAccess::open(shader_path, FileAccess::WRITE);
	if (file.is_null()) {
		Dictionary ret;
		ret["ok"] = false;
		ret["error"] = "Failed to open shader file for writing";
		ret["shaderPath"] = shader_path;
		return ret;
	}
	file->store_string(code_text);
	file->close();

	_refresh_filesystem(shader_path);
	Dictionary ret;
	ret["ok"] = true;
	ret["shaderPath"] = shader_path;
	ret["shaderType"] = shader_type;
	return ret;
}

Dictionary JustAMCPResourceTools::create_tileset(const Dictionary &p_args) {
	String tileset_path = _ensure_res_path(p_args.get("tilesetPath", ""));
	if (tileset_path == "res://") {
		Dictionary ret;
		ret["ok"] = false;
		ret["error"] = "tilesetPath is required";
		return ret;
	}

	Ref<TileSet> tileset;
	tileset.instantiate();

	Variant sources_var = p_args.get("sources", Array());
	if (sources_var.get_type() == Variant::ARRAY) {
		Array sources = sources_var;
		for (int i = 0; i < sources.size(); i++) {
			if (sources[i].get_type() != Variant::DICTIONARY) {
				continue;
			}
			Dictionary source = sources[i];

			Ref<TileSetAtlasSource> atlas;
			atlas.instantiate();
			String tex_path = _ensure_res_path(source.get("texture", ""));
			Ref<Texture2D> tex = ResourceLoader::load(tex_path);
			if (tex.is_null()) {
				continue;
			}

			atlas->set_texture(tex);

			Dictionary tile_size = source.get("tileSize", Dictionary());
			atlas->set_texture_region_size(Vector2i(tile_size.get("x", 0), tile_size.get("y", 0)));

			if (source.has("separation")) {
				Dictionary sep = source["separation"];
				atlas->set_margins(Vector2i(sep.get("x", 0), sep.get("y", 0)));
			}
			if (source.has("offset")) {
				Dictionary off = source["offset"];
				atlas->set_margins(Vector2i(off.get("x", 0), off.get("y", 0)));
			}

			tileset->add_source(atlas);
		}
	}

	Error save_result = ResourceSaver::save(tileset, tileset_path);
	if (save_result != OK) {
		Dictionary ret;
		ret["ok"] = false;
		ret["error"] = "Failed to save TileSet";
		ret["code"] = save_result;
		return ret;
	}

	_refresh_filesystem(tileset_path);
	Dictionary ret;
	ret["ok"] = true;
	ret["tilesetPath"] = tileset_path;
	return ret;
}

Dictionary JustAMCPResourceTools::set_tilemap_cells(const Dictionary &p_args) {
	String scene_path = _ensure_res_path(p_args.get("scenePath", ""));
	String node_path = p_args.get("tilemapNodePath", "");
	if (scene_path == "res://") {
		Dictionary ret;
		ret["ok"] = false;
		ret["error"] = "scenePath is required";
		return ret;
	}

	Ref<PackedScene> scene_res = ResourceLoader::load(scene_path);
	if (scene_res.is_null()) {
		Dictionary ret;
		ret["ok"] = false;
		ret["error"] = "Scene not found";
		ret["scenePath"] = scene_path;
		return ret;
	}

	Node *root = scene_res->instantiate();
	if (!root) {
		Dictionary ret;
		ret["ok"] = false;
		ret["error"] = "Failed to instantiate scene";
		return ret;
	}

	TileMap *tilemap = nullptr;
	if (node_path == "." || node_path.is_empty()) {
		tilemap = Object::cast_to<TileMap>(root);
	} else {
		tilemap = Object::cast_to<TileMap>(root->get_node_or_null(node_path));
	}

	if (!tilemap) {
		memdelete(root);
		Dictionary ret;
		ret["ok"] = false;
		ret["error"] = "TileMap node not found";
		ret["tilemapNodePath"] = node_path;
		return ret;
	}

	int layer = p_args.get("layer", 0);
	Variant cells_var = p_args.get("cells", Array());
	int cell_count = 0;

	if (cells_var.get_type() == Variant::ARRAY) {
		Array cells = cells_var;
		cell_count = cells.size();
		for (int i = 0; i < cells.size(); i++) {
			if (cells[i].get_type() != Variant::DICTIONARY) {
				continue;
			}
			Dictionary cell = cells[i];
			Dictionary coords = cell.get("coords", Dictionary());
			Dictionary atlas_coords = cell.get("atlasCoords", Dictionary());
			tilemap->set_cell(
					layer,
					Vector2i(coords.get("x", 0), coords.get("y", 0)),
					cell.get("sourceId", -1),
					Vector2i(atlas_coords.get("x", 0), atlas_coords.get("y", 0)),
					cell.get("alternativeTile", 0));
		}
	}

	Error save_result = _save_scene_root(root, scene_path);
	memdelete(root);
	if (save_result != OK) {
		Dictionary ret;
		ret["ok"] = false;
		ret["error"] = "Failed to save scene";
		ret["code"] = save_result;
		return ret;
	}

	_refresh_filesystem(scene_path);
	Dictionary ret;
	ret["ok"] = true;
	ret["cellCount"] = cell_count;
	return ret;
}

Dictionary JustAMCPResourceTools::set_theme_color(const Dictionary &p_args) {
	String theme_path = _ensure_res_path(p_args.get("themePath", ""));
	if (theme_path == "res://") {
		Dictionary ret;
		ret["ok"] = false;
		ret["error"] = "themePath is required";
		return ret;
	}

	Ref<Theme> theme = _load_theme(theme_path);
	Dictionary c = p_args.get("color", Dictionary());
	Color color = Color((float)c.get("r", 1.0), (float)c.get("g", 1.0), (float)c.get("b", 1.0), (float)c.get("a", 1.0));
	theme->set_color(StringName(p_args.get("colorName", "")), StringName(p_args.get("controlType", "")), color);

	Error save_result = ResourceSaver::save(theme, theme_path);
	if (save_result != OK) {
		Dictionary ret;
		ret["ok"] = false;
		ret["error"] = "Failed to save theme";
		ret["code"] = save_result;
		return ret;
	}

	_refresh_filesystem(theme_path);
	Dictionary ret;
	ret["ok"] = true;
	return ret;
}

Dictionary JustAMCPResourceTools::set_theme_font_size(const Dictionary &p_args) {
	String theme_path = _ensure_res_path(p_args.get("themePath", ""));
	if (theme_path == "res://") {
		Dictionary ret;
		ret["ok"] = false;
		ret["error"] = "themePath is required";
		return ret;
	}

	Ref<Theme> theme = _load_theme(theme_path);
	theme->set_font_size(StringName(p_args.get("fontSizeName", "")), StringName(p_args.get("controlType", "")), p_args.get("size", 0));

	Error save_result = ResourceSaver::save(theme, theme_path);
	if (save_result != OK) {
		Dictionary ret;
		ret["ok"] = false;
		ret["error"] = "Failed to save theme";
		ret["code"] = save_result;
		return ret;
	}

	_refresh_filesystem(theme_path);
	Dictionary ret;
	ret["ok"] = true;
	return ret;
}

String JustAMCPResourceTools::_get_theme_shader_code(const String &p_theme, const String &p_effect) {
	String base_color = "vec3(0.8, 0.8, 0.8)";
	if (p_theme == "medieval") {
		base_color = "vec3(0.52, 0.42, 0.30)";
	} else if (p_theme == "cyberpunk") {
		base_color = "vec3(0.08, 0.12, 0.22)";
	} else if (p_theme == "nature") {
		base_color = "vec3(0.20, 0.44, 0.24)";
	} else if (p_theme == "scifi") {
		base_color = "vec3(0.35, 0.55, 0.72)";
	} else if (p_theme == "horror") {
		base_color = "vec3(0.12, 0.05, 0.08)";
	} else if (p_theme == "cartoon") {
		base_color = "vec3(0.95, 0.65, 0.20)";
	}

	String effect_block = "ALBEDO = base_col;";
	if (p_effect == "glow") {
		effect_block = "ALBEDO = base_col; EMISSION = base_col * (0.5 + 0.5 * sin(TIME * 2.0));";
	} else if (p_effect == "hologram") {
		effect_block = "float scan = sin(UV.y * 120.0 + TIME * 6.0) * 0.5 + 0.5; ALBEDO = base_col * 0.5; EMISSION = base_col * (0.8 + scan); ALPHA = 0.65;";
	} else if (p_effect == "wind_sway") {
		effect_block = "ALBEDO = base_col + vec3(0.05 * sin(TIME + UV.x * 10.0));";
	} else if (p_effect == "torch_fire") {
		effect_block = "float flicker = 0.8 + 0.2 * sin(TIME * 17.0 + UV.y * 13.0); ALBEDO = base_col * flicker; EMISSION = vec3(1.0, 0.5, 0.1) * (flicker - 0.6);";
	} else if (p_effect == "dissolve") {
		effect_block = "float n = fract(sin(dot(UV * 123.4, vec2(12.9898, 78.233))) * 43758.5453); float cut = 0.45 + 0.25 * sin(TIME); ALBEDO = base_col; ALPHA = step(cut, n); EMISSION = vec3(1.0, 0.4, 0.1) * step(cut - 0.03, n) * (1.0 - step(cut + 0.03, n));";
	} else if (p_effect == "outline") {
		effect_block = "float e = abs(sin(UV.x * 80.0)) * abs(sin(UV.y * 80.0)); ALBEDO = mix(base_col, vec3(0.0), step(0.85, e));";
	}

	return vformat("shader_type spatial;\nrender_mode cull_back, depth_draw_opaque;\n\nvoid fragment() {\n\tvec3 base_col = %s;\n\t%s\n}\n", base_color, effect_block);
}

Dictionary JustAMCPResourceTools::apply_theme_shader(const Dictionary &p_args) {
	String scene_path = _ensure_res_path(p_args.get("scenePath", ""));
	String node_path = p_args.get("nodePath", "");
	String theme = p_args.get("theme", "nature");
	String effect = p_args.get("effect", "none");
	if (scene_path == "res://") {
		Dictionary ret;
		ret["ok"] = false;
		ret["error"] = "scenePath is required";
		return ret;
	}

	Ref<PackedScene> scene_res = ResourceLoader::load(scene_path);
	if (scene_res.is_null()) {
		Dictionary ret;
		ret["ok"] = false;
		ret["error"] = "Scene not found";
		ret["scenePath"] = scene_path;
		return ret;
	}

	Node *root = scene_res->instantiate();
	if (!root) {
		Dictionary ret;
		ret["ok"] = false;
		ret["error"] = "Failed to instantiate scene";
		return ret;
	}

	Node *target = nullptr;
	if (node_path == "." || node_path.is_empty()) {
		target = root;
	} else {
		target = root->get_node_or_null(node_path);
	}

	if (!target) {
		memdelete(root);
		Dictionary ret;
		ret["ok"] = false;
		ret["error"] = "Target node not found";
		ret["nodePath"] = node_path;
		return ret;
	}

	Ref<Shader> shader;
	shader.instantiate();
	shader->set_code(_get_theme_shader_code(theme, effect));
	Ref<ShaderMaterial> material;
	material.instantiate();
	material->set_shader(shader);

	Dictionary params = _parse_properties_dict(p_args.get("shaderParams", ""));
	Array pkeys = params.keys();
	for (int i = 0; i < pkeys.size(); i++) {
		material->set_shader_parameter(StringName(pkeys[i]), _parse_value(params[pkeys[i]]));
	}

	bool applied = false;
	if (MeshInstance3D *m3d = Object::cast_to<MeshInstance3D>(target)) {
		m3d->set_material_override(material);
		applied = true;
	} else if (Sprite2D *s2d = Object::cast_to<Sprite2D>(target)) {
		s2d->set_material(material);
		applied = true;
	} else if (Sprite3D *s3d = Object::cast_to<Sprite3D>(target)) {
		s3d->set_material_override(material);
		applied = true;
	} else if (CanvasItem *ci = Object::cast_to<CanvasItem>(target)) {
		ci->set_material(material);
		applied = true;
	} else if (GeometryInstance3D *gi3d = Object::cast_to<GeometryInstance3D>(target)) {
		gi3d->set_material_override(material);
		applied = true;
	}

	if (!applied) {
		memdelete(root);
		Dictionary ret;
		ret["ok"] = false;
		ret["error"] = "Unsupported node type for material application";
		ret["nodePath"] = node_path;
		return ret;
	}

	Error save_result = _save_scene_root(root, scene_path);
	memdelete(root);
	if (save_result != OK) {
		Dictionary ret;
		ret["ok"] = false;
		ret["error"] = "Failed to save scene";
		ret["code"] = save_result;
		return ret;
	}

	_refresh_filesystem(scene_path);
	Dictionary ret;
	ret["ok"] = true;
	ret["theme"] = theme;
	ret["effect"] = effect;
	return ret;
}

#endif
