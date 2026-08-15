/**************************************************************************/
/*  dddbrowser_exporter.cpp                                               */
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

#include "core/object/class_db.h"
#include "dddbrowser_exporter.h"

#include "dddbrowser_audio.h"
#include "dddbrowser_export_convert.h"
#include "dddbrowser_font.h"
#include "dddbrowser_level.h"
#include "dddbrowser_model.h"
#include "dddbrowser_picturebox.h"
#include "dddbrowser_portal.h"
#include "dddbrowser_script.h"
#include "dddbrowser_spawn.h"
#include "dddbrowser_textbox.h"
#include "dddbrowser_volume.h"

#include "core/config/project_settings.h"
#include "core/io/dir_access.h"
#include "core/io/file_access.h"
#include "core/io/image.h"
#include "core/io/json.h"
#include "core/math/math_funcs.h"
#include "core/templates/hash_map.h"
#include "scene/3d/audio_stream_player_3d.h"
#include "scene/3d/camera_3d.h"
#include "scene/3d/label_3d.h"
#include "scene/3d/light_3d.h"
#include "scene/3d/marker_3d.h"
#include "scene/3d/mesh_instance_3d.h"
#include "scene/3d/physics/collision_shape_3d.h"
#include "scene/3d/sprite_3d.h"
#include "scene/resources/mesh.h"
#include "scene/resources/texture.h"

void DDDBrowserExporter::_bind_methods() {
	ClassDB::bind_method(D_METHOD("export_scene", "root", "export_dir", "generate_html"), &DDDBrowserExporter::export_scene, DEFVAL(true));
	ClassDB::bind_method(D_METHOD("build_scene_dictionary", "root", "export_dir"), &DDDBrowserExporter::build_scene_dictionary);
	ClassDB::bind_method(D_METHOD("validate_scene_dictionary", "scene"), &DDDBrowserExporter::validate_scene_dictionary);
	ClassDB::bind_method(D_METHOD("get_validation_errors", "scene"), &DDDBrowserExporter::get_validation_errors);
	ClassDB::bind_method(D_METHOD("get_last_error"), &DDDBrowserExporter::get_last_error);
	ClassDB::bind_method(D_METHOD("get_last_warning"), &DDDBrowserExporter::get_last_warning);
}

String DDDBrowserExporter::_sanitize_id(const String &p_name) {
	String out;
	for (int i = 0; i < p_name.length(); i++) {
		char32_t c = p_name[i];
		if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_' || c == '-') {
			out += c;
		} else {
			out += '_';
		}
	}
	if (out.is_empty()) {
		out = "node";
	}
	return out;
}

String DDDBrowserExporter::_unique_id(const String &p_base, HashSet<String> &r_used) {
	String id = _sanitize_id(p_base);
	if (!r_used.has(id)) {
		r_used.insert(id);
		return id;
	}
	int n = 2;
	while (true) {
		String candidate = id + "_" + itos(n);
		if (!r_used.has(candidate)) {
			r_used.insert(candidate);
			return candidate;
		}
		n++;
	}
}

Dictionary DDDBrowserExporter::_vec3(const Vector3 &p_v) {
	Dictionary d;
	d["x"] = p_v.x;
	d["y"] = p_v.y;
	d["z"] = p_v.z;
	return d;
}

Dictionary DDDBrowserExporter::_safe_scale(const Vector3 &p_s) {
	Dictionary d;
	d["x"] = MAX(1e-6, Math::abs(p_s.x) < 1e-6 ? 1.0 : p_s.x);
	d["y"] = MAX(1e-6, Math::abs(p_s.y) < 1e-6 ? 1.0 : p_s.y);
	d["z"] = MAX(1e-6, Math::abs(p_s.z) < 1e-6 ? 1.0 : p_s.z);
	return d;
}

Dictionary DDDBrowserExporter::_color_vec3(const Color &p_c) {
	return _vec3(Vector3(p_c.r, p_c.g, p_c.b));
}

Dictionary DDDBrowserExporter::_transform_fields(Node3D *p_node) {
	Dictionary inst;
	Transform3D xf = p_node->get_global_transform();
	inst["position"] = _vec3(xf.origin);
	Vector3 euler = xf.basis.get_euler();
	inst["rotation"] = _vec3(Vector3(Math::rad_to_deg(euler.x), Math::rad_to_deg(euler.y), Math::rad_to_deg(euler.z)));
	inst["scale"] = _safe_scale(xf.basis.get_scale());
	return inst;
}

String DDDBrowserExporter::_build_uri(const String &p_base_url, const String &p_export_dir, const String &p_filepath) {
	String rel = p_filepath;
	if (rel.begins_with(p_export_dir)) {
		rel = rel.substr(p_export_dir.length());
		while (rel.begins_with("/") || rel.begins_with("\\")) {
			rel = rel.substr(1);
		}
	}
	rel = rel.replace("\\", "/");
	if (p_base_url.is_empty()) {
		return rel;
	}
	return p_base_url.path_join(rel);
}

bool DDDBrowserExporter::_is_remote_uri(const String &p_path) {
	return p_path.begins_with("http://") || p_path.begins_with("https://") || p_path.begins_with("data:");
}

String DDDBrowserExporter::_resolve_source_path(const String &p_path) {
	if (p_path.is_empty()) {
		return p_path;
	}
	if (p_path.begins_with("res://") || p_path.begins_with("user://")) {
		return ProjectSettings::get_singleton()->globalize_path(p_path);
	}
	return p_path;
}

String DDDBrowserExporter::_script_filename_from_path(const String &p_path) {
	String name = p_path.get_file();
	if (name.is_empty()) {
		name = "script.luau";
	}
	if (!name.to_lower().ends_with(".luau")) {
		name = _sanitize_id(name.get_basename()) + ".luau";
	}
	return name;
}

String DDDBrowserExporter::_media_type_for_extension(const String &p_ext) {
	String e = p_ext.to_lower();
	if (e == "obj") {
		return "model/obj";
	}
	if (e == "luau" || e == "lua") {
		return "application/x-luau";
	}
	if (e == "wav") {
		return "audio/wav";
	}
	if (e == "ogg") {
		return "audio/ogg";
	}
	if (e == "ttf") {
		return "font/ttf";
	}
	if (e == "otf") {
		return "font/otf";
	}
	if (e == "png") {
		return "image/png";
	}
	if (e == "jpg" || e == "jpeg") {
		return "image/jpeg";
	}
	if (e == "tga") {
		return "image/tga";
	}
	return "application/octet-stream";
}

Error DDDBrowserExporter::_write_text(const String &p_path, const String &p_text) const {
	Ref<FileAccess> f = FileAccess::open(p_path, FileAccess::WRITE);
	ERR_FAIL_COND_V_MSG(f.is_null(), ERR_CANT_CREATE, vformat("Cannot write %s", p_path));
	f->store_string(p_text);
	return OK;
}

Error DDDBrowserExporter::_copy_file(const String &p_from, const String &p_to) const {
	Ref<DirAccess> da = DirAccess::create(DirAccess::ACCESS_FILESYSTEM);
	ERR_FAIL_COND_V(da.is_null(), ERR_CANT_CREATE);
	return da->copy(p_from, p_to);
}

Error DDDBrowserExporter::_export_mesh_obj(MeshInstance3D *p_mi, const String &p_path) const {
	ERR_FAIL_NULL_V(p_mi, ERR_INVALID_PARAMETER);
	Ref<Mesh> mesh = p_mi->get_mesh();
	ERR_FAIL_COND_V(mesh.is_null(), ERR_INVALID_DATA);

	String body;
	body += "# DDDBrowser OBJ export\n";
	int vertex_offset = 1;
	const int surfaces = mesh->get_surface_count();
	for (int s = 0; s < surfaces; s++) {
		Array arrays = mesh->surface_get_arrays(s);
		PackedVector3Array vertices = arrays[Mesh::ARRAY_VERTEX];
		PackedVector3Array normals = arrays[Mesh::ARRAY_NORMAL];
		PackedVector2Array uvs = arrays[Mesh::ARRAY_TEX_UV];
		PackedInt32Array indices = arrays[Mesh::ARRAY_INDEX];

		for (int i = 0; i < vertices.size(); i++) {
			const Vector3 &v = vertices[i];
			body += vformat("v %.6f %.6f %.6f\n", v.x, v.y, v.z);
		}
		if (uvs.size() == vertices.size()) {
			for (int i = 0; i < uvs.size(); i++) {
				body += vformat("vt %.6f %.6f\n", uvs[i].x, uvs[i].y);
			}
		}
		if (normals.size() == vertices.size()) {
			for (int i = 0; i < normals.size(); i++) {
				const Vector3 &n = normals[i];
				body += vformat("vn %.6f %.6f %.6f\n", n.x, n.y, n.z);
			}
		}

		body += "o " + _sanitize_id(p_mi->get_name()) + "\n";
		const bool has_uv = uvs.size() == vertices.size();
		const bool has_n = normals.size() == vertices.size();
		if (indices.size()) {
			for (int i = 0; i + 2 < indices.size(); i += 3) {
				int a = indices[i] + vertex_offset;
				int b = indices[i + 1] + vertex_offset;
				int c = indices[i + 2] + vertex_offset;
				if (has_uv && has_n) {
					body += vformat("f %d/%d/%d %d/%d/%d %d/%d/%d\n", a, a, a, b, b, b, c, c, c);
				} else if (has_n) {
					body += vformat("f %d//%d %d//%d %d//%d\n", a, a, b, b, c, c);
				} else {
					body += vformat("f %d %d %d\n", a, b, c);
				}
			}
		} else {
			for (int i = 0; i + 2 < vertices.size(); i += 3) {
				int a = i + vertex_offset;
				int b = i + 1 + vertex_offset;
				int c = i + 2 + vertex_offset;
				body += vformat("f %d %d %d\n", a, b, c);
			}
		}
		vertex_offset += vertices.size();
	}
	return _write_text(p_path, body);
}

Dictionary DDDBrowserExporter::_light_instance(Light3D *p_light) const {
	Dictionary inst = _transform_fields(p_light);
	inst["id"] = _sanitize_id(p_light->get_name());

	Dictionary light;
	Color c = p_light->get_color();
	light["color"] = _vec3(Vector3(c.r, c.g, c.b));
	light["intensity"] = MAX(0.1, p_light->get_param(Light3D::PARAM_ENERGY));
	light["enabled"] = p_light->is_visible();
	light["shadowEnabled"] = p_light->has_shadow();

	Transform3D xf = p_light->get_global_transform();
	if (Object::cast_to<DirectionalLight3D>(p_light)) {
		inst["type"] = "directionalLight";
		Vector3 dir = -xf.basis.get_column(2).normalized();
		light["direction"] = _vec3(dir);
	} else if (Object::cast_to<SpotLight3D>(p_light)) {
		inst["type"] = "spotLight";
		SpotLight3D *spot = Object::cast_to<SpotLight3D>(p_light);
		light["range"] = MAX(0.1, spot->get_param(Light3D::PARAM_RANGE));
		light["cutoff"] = CLAMP(Math::rad_to_deg(spot->get_param(Light3D::PARAM_SPOT_ANGLE)) * 0.8, 0.0, 90.0);
		light["outerCutoff"] = CLAMP(Math::rad_to_deg(spot->get_param(Light3D::PARAM_SPOT_ANGLE)), 0.0, 90.0);
		Vector3 dir = -xf.basis.get_column(2).normalized();
		light["direction"] = _vec3(dir);
		Dictionary att;
		att["constant"] = 1.0;
		att["linear"] = 0.09;
		att["quadratic"] = 0.032;
		light["attenuation"] = att;
	} else {
		inst["type"] = "pointLight";
		light["range"] = MAX(0.1, p_light->get_param(Light3D::PARAM_RANGE));
		Dictionary att;
		att["constant"] = 1.0;
		att["linear"] = 0.09;
		att["quadratic"] = 0.032;
		light["attenuation"] = att;
	}
	inst["light"] = light;
	return inst;
}

String DDDBrowserExporter::_ensure_script_asset(const String &p_source_path, bool p_pin_sha256, const String &p_preferred_id, const String &p_base_url, const String &p_export_dir, Array &r_assets, HashSet<String> &r_used_ids, HashMap<String, String> &r_path_to_asset_id, HashSet<String> &r_used_filenames, String *r_exported_filename) const {
	if (p_source_path.is_empty()) {
		last_error = "Script path is empty";
		return String();
	}

	const String dedupe_key = _is_remote_uri(p_source_path) ? p_source_path : _resolve_source_path(p_source_path);
	if (r_path_to_asset_id.has(dedupe_key)) {
		const String existing_id = r_path_to_asset_id[dedupe_key];
		if (r_exported_filename) {
			for (int i = 0; i < r_assets.size(); i++) {
				Dictionary a = r_assets[i];
				if (String(a.get("id", "")) == existing_id) {
					*r_exported_filename = _script_filename_from_path(String(a.get("uri", "")));
					break;
				}
			}
		}
		return existing_id;
	}

	String id = _unique_id(p_preferred_id.is_empty() ? p_source_path.get_file().get_basename() : p_preferred_id, r_used_ids);
	Dictionary asset;
	asset["id"] = id;
	asset["type"] = "script";
	asset["mediaType"] = "application/x-luau";

	String exported_filename = _script_filename_from_path(p_source_path);
	if (_is_remote_uri(p_source_path)) {
		asset["uri"] = p_source_path;
		r_used_filenames.insert(exported_filename);
	} else {
		String src = dedupe_key;
		if (!FileAccess::exists(src)) {
			last_error = vformat("Script file not found: %s", p_source_path);
			return String();
		}

		if (r_used_filenames.has(exported_filename)) {
			exported_filename = id + ".luau";
		}
		int suffix = 2;
		while (r_used_filenames.has(exported_filename)) {
			exported_filename = id + "_" + itos(suffix) + ".luau";
			suffix++;
		}
		r_used_filenames.insert(exported_filename);
		String dest = p_export_dir.path_join("scripts").path_join(exported_filename);
		if (_copy_file(src, dest) != OK) {
			last_error = vformat("Failed copying script: %s", p_source_path);
			return String();
		}
		asset["uri"] = _build_uri(p_base_url, p_export_dir, dest);
		if (p_pin_sha256) {
			String hash = FileAccess::get_sha256(dest);
			if (!hash.is_empty()) {
				asset["sha256"] = hash;
			}
		}
	}

	r_assets.push_back(asset);
	r_path_to_asset_id[dedupe_key] = id;
	if (r_exported_filename) {
		*r_exported_filename = exported_filename;
	}
	return id;
}

String DDDBrowserExporter::_ensure_font_asset(DDDBrowserFont *p_font, const String &p_base_url, const String &p_export_dir, Array &r_assets, HashSet<String> &r_used_ids) const {
	ERR_FAIL_NULL_V(p_font, String());
	String preferred = p_font->get_asset_id().is_empty() ? String(p_font->get_name()) : p_font->get_asset_id();
	String id = _unique_id(preferred, r_used_ids);
	Dictionary asset;
	asset["id"] = id;
	asset["type"] = "font";

	String path = p_font->get_source_path();
	if (path.is_empty()) {
		return String();
	}
	if (_is_remote_uri(path)) {
		asset["uri"] = path;
		asset["mediaType"] = path.to_lower().ends_with(".otf") ? "font/otf" : "font/ttf";
	} else {
		String src = _resolve_source_path(path);
		if (!FileAccess::exists(src)) {
			return String();
		}
		String ext = src.get_extension().to_lower();
		if (ext.is_empty()) {
			ext = "ttf";
		}
		String dest = p_export_dir.path_join("fonts").path_join(id + "." + ext);
		if (_copy_file(src, dest) != OK) {
			return String();
		}
		asset["uri"] = _build_uri(p_base_url, p_export_dir, dest);
		asset["mediaType"] = _media_type_for_extension(ext);
	}
	Dictionary font_props;
	font_props["size"] = MAX(0.01, p_font->get_size());
	font_props["style"] = p_font->style_string();
	asset["font"] = font_props;
	r_assets.push_back(asset);
	return id;
}

Dictionary DDDBrowserExporter::build_scene_dictionary(Node *p_root, const String &p_export_dir) {
	last_error = String();
	last_warning = String();
	export_warnings.clear();
	Dictionary scene;
	DDDBrowserLevel *level = Object::cast_to<DDDBrowserLevel>(p_root);
	String base_url;
	String scene_name = "Exported Scene";
	String schema_version = "1.0";
	String version = "1.0";

	if (level) {
		scene_name = level->get_scene_name();
		version = level->get_version();
		schema_version = level->get_schema_version();
		base_url = level->get_base_url();
		scene["name"] = scene_name;
		scene["version"] = version;
		scene["schemaVersion"] = schema_version;
		if (!level->get_scene_id().is_empty()) {
			scene["id"] = level->get_scene_id();
		} else {
			scene["id"] = _sanitize_id(scene_name).to_lower();
		}
		if (!level->get_author().is_empty()) {
			scene["author"] = level->get_author();
		}
		if (!level->get_description().is_empty()) {
			scene["description"] = level->get_description();
		}
		scene["rating"] = level->rating_string();
		if (!level->get_thumbnail_url().is_empty()) {
			scene["thumbnail"] = level->get_thumbnail_url();
		}
		if (!level->get_manifest_url().is_empty()) {
			scene["manifestUrl"] = level->get_manifest_url();
		}
		Dictionary sky = level->build_skybox_dictionary();
		if (!sky.is_empty()) {
			scene["skybox"] = sky;
		}
		if (level->get_export_movement_bounds()) {
			Dictionary bounds;
			bounds["min"] = _vec3(level->get_bounds_min());
			bounds["max"] = _vec3(level->get_bounds_max());
			scene["movementBounds"] = bounds;
		}
		if (!level->get_world_id().is_empty()) {
			Dictionary world;
			world["id"] = level->get_world_id();
			world["position"] = _vec3(level->get_world_position());
			scene["world"] = world;
		}
		scene["gameType"] = level->game_type_string();
		Dictionary autosave = level->build_autosave_notification_dictionary();
		if (!autosave.is_empty()) {
			scene["autosaveNotification"] = autosave;
		}
	} else {
		scene["name"] = scene_name;
		scene["version"] = version;
		scene["schemaVersion"] = schema_version;
		String fallback_name = p_root ? String(p_root->get_name()) : scene_name;
		scene["id"] = _sanitize_id(fallback_name).to_lower();
	}

	Array assets;
	Array instances;
	HashSet<String> used_asset_ids;
	HashMap<ObjectID, String> font_node_to_asset;
	HashMap<String, String> script_path_to_asset_id;
	HashSet<String> used_script_filenames;
	HashSet<ObjectID> exported_mesh_ids;
	bool has_spawn = false;
	Camera3D *first_camera = nullptr;
	Marker3D *spawn_marker = nullptr;

	Ref<DirAccess> da = DirAccess::create(DirAccess::ACCESS_FILESYSTEM);
	da->make_dir_recursive(p_export_dir.path_join("meshes"));
	da->make_dir_recursive(p_export_dir.path_join("scripts"));
	da->make_dir_recursive(p_export_dir.path_join("audio"));
	da->make_dir_recursive(p_export_dir.path_join("fonts"));
	da->make_dir_recursive(p_export_dir.path_join("textures"));

	Vector<Node *> nodes;
	nodes.push_back(p_root);
	for (int i = 0; i < nodes.size(); i++) {
		Node *n = nodes[i];
		for (int c = 0; c < n->get_child_count(); c++) {
			nodes.push_back(n->get_child(c));
		}
	}

	auto attach_node_script = [&](Node *n, Dictionary &inst, const String &preferred_id) -> bool {
		String path = DDDBrowserExportConvert::resolve_luau_script_path(n);
		if (path.is_empty()) {
			return true;
		}
		String script_id = _ensure_script_asset(path, true, preferred_id, base_url, p_export_dir, assets, used_asset_ids, script_path_to_asset_id, used_script_filenames);
		if (script_id.is_empty()) {
			return false;
		}
		Dictionary script_props;
		script_props["file"] = script_id;
		Dictionary data = DDDBrowserExportConvert::collect_script_export_data(n);
		if (!data.is_empty()) {
			script_props["data"] = data;
		}
		inst["script"] = script_props;
		return true;
	};

	auto export_model_from_mi = [&](MeshInstance3D *mi, Dictionary collider_override) -> bool {
		if (!mi || mi->get_mesh().is_null() || exported_mesh_ids.has(mi->get_instance_id())) {
			return true;
		}
		exported_mesh_ids.insert(mi->get_instance_id());
		String id = _unique_id(mi->get_name(), used_asset_ids);
		String obj_path = p_export_dir.path_join("meshes").path_join(id + ".obj");
		String warn;
		if (DDDBrowserExportConvert::export_mesh_with_mtl(this, mi, obj_path, base_url, p_export_dir, &warn) != OK) {
			export_warnings.push_back(vformat("Failed exporting mesh '%s'", mi->get_name()));
			return true;
		}
		if (!warn.is_empty()) {
			export_warnings.push_back(warn);
		}
		Dictionary asset;
		asset["id"] = id;
		asset["type"] = "model";
		asset["uri"] = _build_uri(base_url, p_export_dir, obj_path);
		asset["mediaType"] = "model/obj";
		Dictionary collider = collider_override;
		if (collider.is_empty()) {
			if (DDDBrowserModel *model = Object::cast_to<DDDBrowserModel>(mi)) {
				collider = model->build_collider_dictionary();
			}
			if (collider.is_empty()) {
				String collider_warn;
				collider = DDDBrowserExportConvert::collider_from_mesh_node(mi, &collider_warn);
				if (!collider_warn.is_empty()) {
					export_warnings.push_back(collider_warn);
				}
			}
		}
		if (!collider.is_empty()) {
			asset["collider"] = collider;
		}
		assets.push_back(asset);

		Dictionary inst = _transform_fields(mi);
		inst["id"] = id;
		inst["type"] = "model";
		inst["asset"] = id;
		if (!collider.is_empty()) {
			inst["collider"] = collider;
		}

		bool has_luau = !DDDBrowserExportConvert::resolve_luau_script_path(mi).is_empty();
		if (DDDBrowserModel *model = Object::cast_to<DDDBrowserModel>(mi)) {
			if (has_luau && !model->get_script_path().is_empty()) {
				export_warnings.push_back(vformat("Model '%s': preferring attached LuauScript over script_path", model->get_name()));
			}
			if (has_luau) {
				if (!attach_node_script(mi, inst, id + "_script")) {
					return false;
				}
			} else if (!model->get_script_path().is_empty()) {
				String script_id = _ensure_script_asset(model->get_script_path(), model->get_pin_script_sha256(), id + "_script", base_url, p_export_dir, assets, used_asset_ids, script_path_to_asset_id, used_script_filenames);
				if (script_id.is_empty()) {
					return false;
				}
				Dictionary script_props;
				script_props["file"] = script_id;
				if (!model->get_script_data().is_empty()) {
					script_props["data"] = model->get_script_data();
				}
				inst["script"] = script_props;
			}
		} else if (!attach_node_script(mi, inst, id + "_script")) {
			return false;
		}
		instances.push_back(inst);
		return true;
	};

	for (int i = 0; i < nodes.size(); i++) {
		Node *n = nodes[i];
		if (DDDBrowserFont *font = Object::cast_to<DDDBrowserFont>(n)) {
			String id = _ensure_font_asset(font, base_url, p_export_dir, assets, used_asset_ids);
			if (!id.is_empty()) {
				font_node_to_asset[font->get_instance_id()] = id;
			}
			continue;
		}
		if (DDDBrowserScript *script_node = Object::cast_to<DDDBrowserScript>(n)) {
			if (script_node->get_source_path().is_empty()) {
				last_error = vformat("DDDBrowserScript '%s' has empty source_path", script_node->get_name());
				return scene;
			}
			String preferred = script_node->get_asset_id().is_empty() ? String(script_node->get_name()) : script_node->get_asset_id();
			String id = _ensure_script_asset(script_node->get_source_path(), script_node->get_pin_sha256(), preferred, base_url, p_export_dir, assets, used_asset_ids, script_path_to_asset_id, used_script_filenames);
			if (id.is_empty()) {
				return scene;
			}
		}
	}

	if (level && !level->get_gamemode_file().is_empty()) {
		String gm_filename;
		String gm_id = _ensure_script_asset(level->get_gamemode_file(), true, "gamemode", base_url, p_export_dir, assets, used_asset_ids, script_path_to_asset_id, used_script_filenames, &gm_filename);
		if (gm_id.is_empty() || gm_filename.is_empty()) {
			if (last_error.is_empty()) {
				last_error = vformat("Failed to export gamemode script: %s", level->get_gamemode_file());
			}
			return scene;
		}
		Dictionary gm;
		gm["file"] = gm_filename;
		scene["gamemode"] = gm;
	}

	for (int i = 0; i < nodes.size(); i++) {
		Node *n = nodes[i];
		if (Object::cast_to<DDDBrowserFont>(n) || Object::cast_to<DDDBrowserScript>(n)) {
			continue;
		}
		if (n->is_class("RigidBody3D") || n->is_class("CharacterBody3D")) {
			export_warnings.push_back(vformat("Skipping physics body '%s' (%s); use StaticBody3D/MeshInstance3D for DDD export", n->get_name(), n->get_class()));
			continue;
		}
		if (n->is_class("AnimationPlayer")) {
			export_warnings.push_back(vformat("Skipping AnimationPlayer '%s' (unsupported in DDD export)", n->get_name()));
			continue;
		}
		if (n->is_class("CSGShape3D") || n->is_class("CSGCombiner3D") || n->is_class("CSGPrimitive3D")) {
			export_warnings.push_back(vformat("Skipping CSG node '%s'; bake to MeshInstance3D first", n->get_name()));
			continue;
		}
		if (DDDBrowserSpawn *spawn = Object::cast_to<DDDBrowserSpawn>(n)) {
			scene["spawn"] = _vec3(spawn->get_global_transform().origin);
			has_spawn = true;
			continue;
		}
		if (Camera3D *cam = Object::cast_to<Camera3D>(n)) {
			if (!first_camera) {
				first_camera = cam;
			}
			continue;
		}
		if (Marker3D *marker = Object::cast_to<Marker3D>(n)) {
			if (String(marker->get_name()).to_lower() == "spawn") {
				spawn_marker = marker;
			}
			continue;
		}
		if (MeshInstance3D *mi = Object::cast_to<MeshInstance3D>(n)) {
			Node *parent = mi->get_parent();
			if (parent && (parent->is_class("StaticBody3D") || parent->is_class("AnimatableBody3D"))) {
				continue;
			}
			if (parent && (parent->is_class("RigidBody3D") || parent->is_class("CharacterBody3D"))) {
				export_warnings.push_back(vformat("Skipping mesh '%s' under %s '%s' (rigid/character physics not exported)", mi->get_name(), parent->get_class(), parent->get_name()));
				continue;
			}
			if (!export_model_from_mi(mi, Dictionary())) {
				return scene;
			}
			continue;
		}
		if (n->is_class("StaticBody3D") || n->is_class("AnimatableBody3D")) {
			MeshInstance3D *child_mi = nullptr;
			Dictionary body_collider;
			for (int c = 0; c < n->get_child_count(); c++) {
				Node *child = n->get_child(c);
				if (!child_mi) {
					child_mi = Object::cast_to<MeshInstance3D>(child);
				}
				if (CollisionShape3D *cs = Object::cast_to<CollisionShape3D>(child)) {
					String collider_warn;
					Dictionary col = DDDBrowserExportConvert::collider_from_shape_node(cs, &collider_warn);
					if (!col.is_empty() && body_collider.is_empty()) {
						body_collider = col;
					} else if (!collider_warn.is_empty()) {
						export_warnings.push_back(collider_warn);
					}
				}
			}
			if (child_mi) {
				if (!export_model_from_mi(child_mi, body_collider)) {
					return scene;
				}
			} else {
				export_warnings.push_back(vformat("Static/Animatable body '%s' has no MeshInstance3D child", n->get_name()));
			}
			continue;
		}
		if (Light3D *light = Object::cast_to<Light3D>(n)) {
			Dictionary inst = _light_instance(light);
			inst["id"] = _unique_id(light->get_name(), used_asset_ids);
			if (!attach_node_script(light, inst, String(inst["id"]) + "_script")) {
				return scene;
			}
			instances.push_back(inst);
			continue;
		}
		if (DDDBrowserPortal *portal = Object::cast_to<DDDBrowserPortal>(n)) {
			if (portal->get_destination_url().is_empty()) {
				continue;
			}
			Dictionary inst = _transform_fields(portal);
			inst["id"] = _unique_id(portal->get_name(), used_asset_ids);
			inst["type"] = "portal";
			Dictionary p;
			p["destinationUrl"] = portal->get_destination_url();
			p["radius"] = portal->get_radius();
			const bool auto_t = portal->get_trigger_mode() == DDDBrowserPortal::TRIGGER_AUTO;
			const bool manual_t = portal->get_trigger_mode() == DDDBrowserPortal::TRIGGER_MANUAL;
			const bool script_t = portal->get_trigger_mode() == DDDBrowserPortal::TRIGGER_SCRIPT;
			p["autoTrigger"] = auto_t;
			p["manualTrigger"] = manual_t;
			p["scriptTrigger"] = script_t;
			inst["portal"] = p;
			instances.push_back(inst);
			continue;
		}
		if (DDDBrowserVolume *vol = Object::cast_to<DDDBrowserVolume>(n)) {
			Dictionary inst = _transform_fields(vol);
			inst["id"] = _unique_id(vol->get_name(), used_asset_ids);
			inst["type"] = vol->type_string();
			inst[vol->properties_key()] = vol->build_properties_dictionary();
			instances.push_back(inst);
			continue;
		}
		if (DDDBrowserTextbox *tb = Object::cast_to<DDDBrowserTextbox>(n)) {
			String asset_id = _unique_id(String(tb->get_name()) + "_asset", used_asset_ids);
			Dictionary asset;
			asset["id"] = asset_id;
			asset["type"] = "textbox";
			asset["uri"] = "data:text/plain," + asset_id;
			asset["mediaType"] = "application/json";
			Dictionary tb_props;
			tb_props["text"] = tb->get_text();
			if (!tb->get_title().is_empty()) {
				tb_props["title"] = tb->get_title();
			}
			tb_props["size"] = MAX(0.01, tb->get_font_size());
			tb_props["color"] = _color_vec3(tb->get_color());

			String font_id = tb->get_font_asset_id();
			if (font_id.is_empty()) {
				for (int c = 0; c < tb->get_child_count(); c++) {
					if (DDDBrowserFont *child_font = Object::cast_to<DDDBrowserFont>(tb->get_child(c))) {
						if (font_node_to_asset.has(child_font->get_instance_id())) {
							font_id = font_node_to_asset[child_font->get_instance_id()];
							break;
						}
					}
				}
			}
			if (!font_id.is_empty()) {
				tb_props["font"] = font_id;
			}
			asset["textbox"] = tb_props;
			assets.push_back(asset);

			Dictionary inst = _transform_fields(tb);
			inst["id"] = _unique_id(tb->get_name(), used_asset_ids);
			inst["type"] = "textbox";
			Dictionary inst_tb;
			inst_tb["asset"] = asset_id;
			if (!font_id.is_empty()) {
				inst_tb["font"] = font_id;
			}
			inst["textbox"] = inst_tb;
			if (!attach_node_script(tb, inst, String(inst["id"]) + "_script")) {
				return scene;
			}
			instances.push_back(inst);
			continue;
		}
		if (Label3D *label = Object::cast_to<Label3D>(n)) {
			String asset_id = _unique_id(String(label->get_name()) + "_asset", used_asset_ids);
			Dictionary asset;
			asset["id"] = asset_id;
			asset["type"] = "textbox";
			asset["uri"] = "data:text/plain," + asset_id;
			asset["mediaType"] = "application/json";
			Dictionary tb_props;
			tb_props["text"] = label->get_text();
			tb_props["size"] = MAX(0.01, label->get_font_size() / 32.0);
			tb_props["color"] = _color_vec3(label->get_modulate());
			asset["textbox"] = tb_props;
			assets.push_back(asset);

			Dictionary inst = _transform_fields(label);
			inst["id"] = _unique_id(label->get_name(), used_asset_ids);
			inst["type"] = "textbox";
			Dictionary inst_tb;
			inst_tb["asset"] = asset_id;
			inst["textbox"] = inst_tb;
			if (!attach_node_script(label, inst, String(inst["id"]) + "_script")) {
				return scene;
			}
			instances.push_back(inst);
			continue;
		}
		if (DDDBrowserPicturebox *pb = Object::cast_to<DDDBrowserPicturebox>(n)) {
			String tex_path = pb->get_texture_path();
			if (tex_path.is_empty()) {
				continue;
			}
			String tex_asset_id = _unique_id(String(pb->get_name()) + "_texture", used_asset_ids);
			Dictionary tex_asset;
			tex_asset["id"] = tex_asset_id;
			tex_asset["type"] = "texture";
			if (_is_remote_uri(tex_path)) {
				tex_asset["uri"] = tex_path;
				tex_asset["mediaType"] = _media_type_for_extension(tex_path.get_extension());
			} else {
				String src = _resolve_source_path(tex_path);
				if (!FileAccess::exists(src)) {
					continue;
				}
				String ext = src.get_extension().to_lower();
				if (ext.is_empty()) {
					ext = "png";
				}
				String dest = p_export_dir.path_join("textures").path_join(tex_asset_id + "." + ext);
				if (_copy_file(src, dest) != OK) {
					continue;
				}
				tex_asset["uri"] = _build_uri(base_url, p_export_dir, dest);
				tex_asset["mediaType"] = _media_type_for_extension(ext);
			}
			assets.push_back(tex_asset);

			String pb_asset_id = _unique_id(String(pb->get_name()) + "_asset", used_asset_ids);
			Dictionary pb_asset;
			pb_asset["id"] = pb_asset_id;
			pb_asset["type"] = "picturebox";
			pb_asset["uri"] = "data:text/plain," + pb_asset_id;
			pb_asset["mediaType"] = "application/json";
			Dictionary pb_props;
			pb_props["texture"] = tex_asset_id;
			pb_props["width"] = MAX(0.01, pb->get_width());
			pb_props["height"] = MAX(0.01, pb->get_height());
			pb_asset["picturebox"] = pb_props;
			assets.push_back(pb_asset);

			Dictionary inst = _transform_fields(pb);
			inst["id"] = _unique_id(pb->get_name(), used_asset_ids);
			inst["type"] = "picturebox";
			Dictionary inst_pb;
			inst_pb["asset"] = pb_asset_id;
			inst["picturebox"] = inst_pb;
			if (!attach_node_script(pb, inst, String(inst["id"]) + "_script")) {
				return scene;
			}
			instances.push_back(inst);
			continue;
		}
		if (Sprite3D *sprite = Object::cast_to<Sprite3D>(n)) {
			Ref<Texture2D> tex = sprite->get_texture();
			if (tex.is_null()) {
				export_warnings.push_back(vformat("Sprite3D '%s' has no texture", sprite->get_name()));
				continue;
			}
			String tex_path = tex->get_path();
			String tex_asset_id = _unique_id(String(sprite->get_name()) + "_texture", used_asset_ids);
			Dictionary tex_asset;
			tex_asset["id"] = tex_asset_id;
			tex_asset["type"] = "texture";
			bool tex_ok = false;
			if (!tex_path.is_empty() && _is_remote_uri(tex_path)) {
				tex_asset["uri"] = tex_path;
				tex_asset["mediaType"] = _media_type_for_extension(tex_path.get_extension());
				tex_ok = true;
			} else if (!tex_path.is_empty()) {
				String src = _resolve_source_path(tex_path);
				if (FileAccess::exists(src)) {
					String ext = src.get_extension().to_lower();
					if (ext.is_empty()) {
						ext = "png";
					}
					String dest = p_export_dir.path_join("textures").path_join(tex_asset_id + "." + ext);
					if (_copy_file(src, dest) == OK) {
						tex_asset["uri"] = _build_uri(base_url, p_export_dir, dest);
						tex_asset["mediaType"] = _media_type_for_extension(ext);
						tex_ok = true;
					}
				}
			}
			if (!tex_ok) {
				Ref<Image> img = tex->get_image();
				if (img.is_valid() && !img->is_empty()) {
					String dest = p_export_dir.path_join("textures").path_join(tex_asset_id + ".png");
					if (img->save_png(dest) == OK) {
						tex_asset["uri"] = _build_uri(base_url, p_export_dir, dest);
						tex_asset["mediaType"] = "image/png";
						tex_ok = true;
					}
				}
			}
			if (!tex_ok) {
				export_warnings.push_back(vformat("Sprite3D '%s' texture could not be exported", sprite->get_name()));
				continue;
			}
			assets.push_back(tex_asset);

			String pb_asset_id = _unique_id(String(sprite->get_name()) + "_asset", used_asset_ids);
			Dictionary pb_asset;
			pb_asset["id"] = pb_asset_id;
			pb_asset["type"] = "picturebox";
			pb_asset["uri"] = "data:text/plain," + pb_asset_id;
			pb_asset["mediaType"] = "application/json";
			Dictionary pb_props;
			pb_props["texture"] = tex_asset_id;
			const float pixel = sprite->get_pixel_size();
			pb_props["width"] = MAX(0.01, pixel * float(tex->get_width()));
			pb_props["height"] = MAX(0.01, pixel * float(tex->get_height()));
			pb_asset["picturebox"] = pb_props;
			assets.push_back(pb_asset);

			Dictionary inst = _transform_fields(sprite);
			inst["id"] = _unique_id(sprite->get_name(), used_asset_ids);
			inst["type"] = "picturebox";
			Dictionary inst_pb;
			inst_pb["asset"] = pb_asset_id;
			inst["picturebox"] = inst_pb;
			if (!attach_node_script(sprite, inst, String(inst["id"]) + "_script")) {
				return scene;
			}
			instances.push_back(inst);
			continue;
		}
		if (DDDBrowserAudio *audio = Object::cast_to<DDDBrowserAudio>(n)) {
			String src_path = audio->get_source_path();
			if (src_path.is_empty()) {
				continue;
			}
			String asset_id = _unique_id(String(audio->get_name()) + "_asset", used_asset_ids);
			Dictionary asset;
			asset["id"] = asset_id;
			asset["type"] = "audio";

			String format_str = audio->get_format() == DDDBrowserAudio::FORMAT_OGG ? "ogg" : "wav";
			if (_is_remote_uri(src_path)) {
				asset["uri"] = src_path;
				if (src_path.to_lower().ends_with(".ogg")) {
					format_str = "ogg";
				} else if (src_path.to_lower().ends_with(".wav")) {
					format_str = "wav";
				}
				asset["mediaType"] = format_str == "ogg" ? "audio/ogg" : "audio/wav";
			} else {
				String src = _resolve_source_path(src_path);
				if (!FileAccess::exists(src)) {
					continue;
				}
				String ext = src.get_extension().to_lower();
				if (ext == "ogg") {
					format_str = "ogg";
				} else if (ext == "wav") {
					format_str = "wav";
				} else {
					ext = format_str;
				}
				String dest = p_export_dir.path_join("audio").path_join(asset_id + "." + ext);
				if (_copy_file(src, dest) != OK) {
					continue;
				}
				asset["uri"] = _build_uri(base_url, p_export_dir, dest);
				asset["mediaType"] = format_str == "ogg" ? "audio/ogg" : "audio/wav";
			}
			Dictionary audio_asset_props;
			audio_asset_props["format"] = format_str;
			audio_asset_props["ambient"] = audio->get_ambient();
			asset["audio"] = audio_asset_props;
			assets.push_back(asset);

			Dictionary inst = _transform_fields(audio);
			inst["id"] = _unique_id(audio->get_name(), used_asset_ids);
			inst["type"] = "audio";
			Dictionary inst_audio;
			inst_audio["asset"] = asset_id;
			inst_audio["loop"] = audio->get_loop();
			inst_audio["volume"] = CLAMP(audio->get_volume(), 0.0f, 1.0f);
			inst_audio["autoPlay"] = audio->get_auto_play();
			inst["audio"] = inst_audio;
			if (!attach_node_script(audio, inst, String(inst["id"]) + "_script")) {
				return scene;
			}
			instances.push_back(inst);
			continue;
		}
		if (AudioStreamPlayer3D *player = Object::cast_to<AudioStreamPlayer3D>(n)) {
			String src_path = DDDBrowserExportConvert::resolve_audio_stream_path(player);
			if (src_path.is_empty()) {
				export_warnings.push_back(vformat("AudioStreamPlayer3D '%s' has no stream path (wav/ogg required)", player->get_name()));
				continue;
			}
			String asset_id = _unique_id(String(player->get_name()) + "_asset", used_asset_ids);
			Dictionary asset;
			asset["id"] = asset_id;
			asset["type"] = "audio";
			String format_str = "wav";
			if (_is_remote_uri(src_path)) {
				asset["uri"] = src_path;
				if (src_path.to_lower().ends_with(".ogg")) {
					format_str = "ogg";
				}
				asset["mediaType"] = format_str == "ogg" ? "audio/ogg" : "audio/wav";
			} else {
				String src = _resolve_source_path(src_path);
				if (!FileAccess::exists(src)) {
					export_warnings.push_back(vformat("Missing audio stream file for '%s': %s", player->get_name(), src_path));
					continue;
				}
				String ext = src.get_extension().to_lower();
				if (ext == "ogg") {
					format_str = "ogg";
				} else if (ext == "wav") {
					format_str = "wav";
				} else {
					export_warnings.push_back(vformat("Unsupported audio format for '%s' (.%s); need wav/ogg", player->get_name(), ext));
					continue;
				}
				String dest = p_export_dir.path_join("audio").path_join(asset_id + "." + ext);
				if (_copy_file(src, dest) != OK) {
					export_warnings.push_back(vformat("Failed copying audio for '%s'", player->get_name()));
					continue;
				}
				asset["uri"] = _build_uri(base_url, p_export_dir, dest);
				asset["mediaType"] = format_str == "ogg" ? "audio/ogg" : "audio/wav";
			}
			Dictionary audio_asset_props;
			audio_asset_props["format"] = format_str;
			audio_asset_props["ambient"] = false;
			asset["audio"] = audio_asset_props;
			assets.push_back(asset);

			Dictionary inst = _transform_fields(player);
			inst["id"] = _unique_id(player->get_name(), used_asset_ids);
			inst["type"] = "audio";
			Dictionary inst_audio;
			inst_audio["asset"] = asset_id;
			inst_audio["loop"] = true;
			inst_audio["volume"] = CLAMP(float(Math::db_to_linear(player->get_volume_db())), 0.0f, 1.0f);
			inst_audio["autoPlay"] = player->is_autoplay_enabled();
			inst["audio"] = inst_audio;
			if (!attach_node_script(player, inst, String(inst["id"]) + "_script")) {
				return scene;
			}
			instances.push_back(inst);
			continue;
		}
	}

	if (!has_spawn) {
		if (spawn_marker) {
			scene["spawn"] = _vec3(spawn_marker->get_global_transform().origin);
			has_spawn = true;
		} else if (first_camera) {
			scene["spawn"] = _vec3(first_camera->get_global_transform().origin);
			has_spawn = true;
		}
	}

	if (!scene.has("skybox")) {
		Dictionary sky;
		if (DDDBrowserExportConvert::try_skybox_from_world_environment(p_root, sky)) {
			String uri = sky.get("uri", "");
			if (!uri.is_empty() && !_is_remote_uri(uri)) {
				String src = _resolve_source_path(uri);
				if (FileAccess::exists(src)) {
					String ext = src.get_extension().to_lower();
					if (ext.is_empty()) {
						ext = "png";
					}
					String dest = p_export_dir.path_join("textures").path_join("skybox." + ext);
					if (_copy_file(src, dest) == OK) {
						sky["uri"] = _build_uri(base_url, p_export_dir, dest);
						scene["skybox"] = sky;
					} else {
						export_warnings.push_back("Failed copying WorldEnvironment skybox texture");
					}
				} else {
					export_warnings.push_back(vformat("WorldEnvironment skybox texture missing: %s", uri));
				}
			} else if (!uri.is_empty()) {
				scene["skybox"] = sky;
			}
		}
	}

	scene["assets"] = assets;
	if (!instances.is_empty()) {
		scene["instances"] = instances;
	}
	if (!export_warnings.is_empty()) {
		last_warning = String("\n").join(export_warnings);
	}
	return scene;
}

PackedStringArray DDDBrowserExporter::get_validation_errors(const Dictionary &p_scene) const {
	PackedStringArray errors;
	if (!p_scene.has("name") || String(p_scene["name"]).is_empty()) {
		errors.push_back("missing name");
	}
	if (!p_scene.has("version")) {
		errors.push_back("missing version");
	}
	if (!p_scene.has("schemaVersion")) {
		errors.push_back("missing schemaVersion");
	}
	if (!p_scene.has("assets") || p_scene["assets"].get_type() != Variant::ARRAY) {
		errors.push_back("missing assets array");
	}
	if (p_scene.has("instances") && p_scene["instances"].get_type() == Variant::ARRAY) {
		Array insts = p_scene["instances"];
		for (int i = 0; i < insts.size(); i++) {
			Dictionary inst = insts[i];
			if (!inst.has("type")) {
				errors.push_back(vformat("instances/%d missing type", i));
			}
			if (!inst.has("id") || !inst.has("position") || !inst.has("rotation") || !inst.has("scale")) {
				errors.push_back(vformat("instances/%d missing transform fields", i));
			}
			String t = inst.get("type", "");
			if (t == "model" && !inst.has("asset")) {
				errors.push_back(vformat("instances/%d model missing asset", i));
			}
			if ((t == "pointLight" || t == "directionalLight" || t == "spotLight") && !inst.has("light")) {
				errors.push_back(vformat("instances/%d light missing light props", i));
			}
			if (t == "portal" && !inst.has("portal")) {
				errors.push_back(vformat("instances/%d portal missing portal props", i));
			}
			if (t == "textbox") {
				if (!inst.has("textbox")) {
					errors.push_back(vformat("instances/%d textbox missing textbox props", i));
				} else {
					Dictionary tb = inst["textbox"];
					if (!tb.has("asset")) {
						errors.push_back(vformat("instances/%d textbox missing textbox.asset", i));
					}
				}
			}
			if (t == "picturebox") {
				if (!inst.has("picturebox")) {
					errors.push_back(vformat("instances/%d picturebox missing picturebox props", i));
				} else {
					Dictionary pb = inst["picturebox"];
					if (!pb.has("asset")) {
						errors.push_back(vformat("instances/%d picturebox missing picturebox.asset", i));
					}
				}
			}
			if (t == "audio") {
				if (!inst.has("audio")) {
					errors.push_back(vformat("instances/%d audio missing audio props", i));
				} else {
					Dictionary a = inst["audio"];
					if (!a.has("asset")) {
						errors.push_back(vformat("instances/%d audio missing audio.asset", i));
					}
				}
			}
			if (t.ends_with("Volume") || t == "teleportVolume" || t == "interactionVolume" || t == "autosaveVolume") {
				if (!inst.has(t)) {
					errors.push_back(vformat("instances/%d volume missing %s props", i, t));
				} else {
					Dictionary props = inst[t];
					if ((t == "singleTriggerVolume" || t == "exitTriggerVolume" || t == "lookAtVolume" || t == "multiTriggerVolume" || t == "cooldownTriggerVolume" || t == "stayTriggerVolume" || t == "timedEntryTriggerVolume" || t == "interactionVolume") && !props.has("eventName")) {
						errors.push_back(vformat("instances/%d %s missing eventName", i, t));
					}
					if (t == "lookedAtVolume" && (!props.has("eventName") || !props.has("targetInstanceId"))) {
						errors.push_back(vformat("instances/%d lookedAtVolume missing required fields", i));
					}
					if (t == "cooldownTriggerVolume" && !props.has("cooldownSeconds")) {
						errors.push_back(vformat("instances/%d cooldownTriggerVolume missing cooldownSeconds", i));
					}
					if (t == "stayTriggerVolume" && !props.has("stayInterval")) {
						errors.push_back(vformat("instances/%d stayTriggerVolume missing stayInterval", i));
					}
					if (t == "timedEntryTriggerVolume" && !props.has("requiredStayTime")) {
						errors.push_back(vformat("instances/%d timedEntryTriggerVolume missing requiredStayTime", i));
					}
					if (t == "counterTriggerVolume" && (!props.has("counter_word") || !props.has("required_count"))) {
						errors.push_back(vformat("instances/%d counterTriggerVolume missing counter fields", i));
					}
					if (t == "sequenceTriggerVolume" && (!props.has("sequence_group_id") || !props.has("sequence_index"))) {
						errors.push_back(vformat("instances/%d sequenceTriggerVolume missing sequence fields", i));
					}
					if (t == "teleportVolume" && !props.has("target_position")) {
						errors.push_back(vformat("instances/%d teleportVolume missing target_position", i));
					}
					if (t == "interactionVolume" && !props.has("action")) {
						errors.push_back(vformat("instances/%d interactionVolume missing action", i));
					}
				}
			}
		}
	}
	HashSet<String> script_asset_ids;
	HashSet<String> script_filenames;
	if (p_scene.has("assets") && p_scene["assets"].get_type() == Variant::ARRAY) {
		Array asset_list = p_scene["assets"];
		for (int i = 0; i < asset_list.size(); i++) {
			Dictionary asset = asset_list[i];
			if (!asset.has("id") || !asset.has("type") || !asset.has("uri") || !asset.has("mediaType")) {
				errors.push_back(vformat("assets/%d missing required fields", i));
			}
			String t = asset.get("type", "");
			if (t == "textbox" && !asset.has("textbox")) {
				errors.push_back(vformat("assets/%d textbox missing textbox props", i));
			}
			if (t == "picturebox" && !asset.has("picturebox")) {
				errors.push_back(vformat("assets/%d picturebox missing picturebox props", i));
			}
			if (t == "audio" && !asset.has("audio")) {
				errors.push_back(vformat("assets/%d audio missing audio props", i));
			}
			if (t == "script") {
				String aid = asset.get("id", "");
				if (!aid.is_empty()) {
					script_asset_ids.insert(aid);
				}
				String uri = asset.get("uri", "");
				String fname = _script_filename_from_path(uri);
				if (!fname.is_empty()) {
					script_filenames.insert(fname);
				}
				String mt = asset.get("mediaType", "");
				if (mt != "application/x-luau") {
					errors.push_back(vformat("assets/%d script mediaType must be application/x-luau", i));
				}
			}
		}
	}
	if (p_scene.has("instances") && p_scene["instances"].get_type() == Variant::ARRAY) {
		Array insts = p_scene["instances"];
		for (int i = 0; i < insts.size(); i++) {
			Dictionary inst = insts[i];
			if (!inst.has("script")) {
				continue;
			}
			Dictionary script_props = inst["script"];
			String file = script_props.get("file", "");
			if (file.is_empty() || !script_asset_ids.has(file)) {
				errors.push_back(vformat("instances/%d script.file must reference a script asset id", i));
			}
		}
	}
	if (p_scene.has("gamemode")) {
		Dictionary gm = p_scene["gamemode"];
		String file = gm.get("file", "");
		if (file.is_empty() || !file.to_lower().ends_with(".luau")) {
			errors.push_back("gamemode.file must be a .luau filename");
		} else if (!script_filenames.has(file)) {
			errors.push_back(vformat("gamemode.file '%s' does not match an exported script filename", file));
		}
	}
	return errors;
}

bool DDDBrowserExporter::validate_scene_dictionary(const Dictionary &p_scene) const {
	return get_validation_errors(p_scene).is_empty();
}

Error DDDBrowserExporter::export_scene(Node *p_root, const String &p_export_dir, bool p_generate_html) {
	last_error = String();
	ERR_FAIL_NULL_V(p_root, ERR_INVALID_PARAMETER);
	Ref<DirAccess> da = DirAccess::create(DirAccess::ACCESS_FILESYSTEM);
	Error err = da->make_dir_recursive(p_export_dir);
	if (err != OK && err != ERR_ALREADY_EXISTS) {
		last_error = "Cannot create export directory";
		return err;
	}

	Dictionary scene = build_scene_dictionary(p_root, p_export_dir);
	if (!last_error.is_empty()) {
		return ERR_INVALID_DATA;
	}
	PackedStringArray errors = get_validation_errors(scene);
	if (!errors.is_empty()) {
		last_error = errors[0];
		return ERR_INVALID_DATA;
	}

	String json_text = JSON::stringify(scene, "\t", false);
	String json_path = p_export_dir.path_join("scene.json");
	err = _write_text(json_path, json_text);
	if (err != OK) {
		last_error = "Failed writing scene.json";
		return err;
	}

	if (p_generate_html) {
		String name = scene.get("name", "Exported Scene");
		String id = scene.get("id", "exported_scene");
		String author = scene.get("author", "");
		String rating = scene.get("rating", "GENERAL");
		String desc = scene.get("description", "");
		String thumb = scene.get("thumbnail", "");
		String html;
		html += "<!DOCTYPE html>\n<html lang=\"en\">\n<head>\n";
		html += "    <meta charset=\"utf-8\">\n";
		html += "    <title>" + name.xml_escape() + "</title>\n\n";
		html += "    <meta name=\"scene:id\" content=\"" + id.xml_escape() + "\">\n";
		html += "    <meta name=\"scene:name\" content=\"" + name.xml_escape() + "\">\n";
		html += "    <meta name=\"scene:author\" content=\"" + author.xml_escape() + "\">\n";
		html += "    <meta name=\"scene:rating\" content=\"" + rating.xml_escape() + "\">\n";
		if (!thumb.is_empty()) {
			html += "    <meta name=\"scene:thumbnail\" content=\"" + thumb.xml_escape() + "\">\n";
		}
		html += "\n    <script id=\"blazium-scene\" type=\"application/vnd.blazium.scene+json\">\n";
		html += json_text + "\n";
		html += "    </script>\n</head>\n<body>\n";
		html += "    <h1>" + name.xml_escape() + "</h1>\n";
		if (!desc.is_empty()) {
			html += "    <p>" + desc.xml_escape() + "</p>\n";
		}
		html += "</body>\n</html>\n";
		err = _write_text(p_export_dir.path_join("index.html"), html);
		if (err != OK) {
			last_error = "Failed writing index.html";
			return err;
		}
	}
	return OK;
}

String DDDBrowserExporter::get_last_error() const {
	return last_error;
}
String DDDBrowserExporter::get_last_warning() const {
	return last_warning;
}
