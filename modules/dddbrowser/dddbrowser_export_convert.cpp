/**************************************************************************/
/*  dddbrowser_export_convert.cpp                                         */
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

#include "dddbrowser_export_convert.h"
#include "dddbrowser_exporter.h"

#include "core/config/project_settings.h"
#include "core/io/file_access.h"
#include "core/io/image.h"
#include "core/io/resource_loader.h"
#include "core/object/script_language.h"
#include "scene/3d/audio_stream_player_3d.h"
#include "scene/3d/mesh_instance_3d.h"
#include "scene/3d/physics/collision_shape_3d.h"
#include "scene/3d/world_environment.h"
#include "scene/resources/3d/box_shape_3d.h"
#include "scene/resources/3d/capsule_shape_3d.h"
#include "scene/resources/3d/cylinder_shape_3d.h"
#include "scene/resources/3d/sphere_shape_3d.h"
#include "scene/resources/environment.h"
#include "scene/resources/material.h"
#include "scene/resources/mesh.h"
#include "scene/resources/sky.h"
#include "scene/resources/texture.h"
#include "servers/audio/audio_stream.h"

#ifdef MODULE_LUAU_MODULE_ENABLED
#include "modules/luau_module/luau_script.h"
#endif

Dictionary DDDBrowserExportConvert::collider_from_shape_node(CollisionShape3D *p_shape, String *r_warning) {
	Dictionary out;
	if (!p_shape) {
		return out;
	}
	Ref<Shape3D> shape = p_shape->get_shape();
	if (shape.is_null()) {
		return out;
	}
	if (const BoxShape3D *box = Object::cast_to<BoxShape3D>(shape.ptr())) {
		out["type"] = "box";
		Dictionary box_props;
		Dictionary size;
		Vector3 s = box->get_size();
		size["x"] = MAX(0.01, s.x);
		size["y"] = MAX(0.01, s.y);
		size["z"] = MAX(0.01, s.z);
		box_props["size"] = size;
		out["box"] = box_props;
	} else if (const SphereShape3D *sphere = Object::cast_to<SphereShape3D>(shape.ptr())) {
		out["type"] = "sphere";
		Dictionary sphere_props;
		sphere_props["radius"] = MAX(0.01, sphere->get_radius());
		out["sphere"] = sphere_props;
	} else if (const CapsuleShape3D *capsule = Object::cast_to<CapsuleShape3D>(shape.ptr())) {
		out["type"] = "capsule";
		Dictionary capsule_props;
		capsule_props["radius"] = MAX(0.01, capsule->get_radius());
		capsule_props["halfHeight"] = MAX(0.01, capsule->get_height() * 0.5);
		out["capsule"] = capsule_props;
	} else if (const CylinderShape3D *cylinder = Object::cast_to<CylinderShape3D>(shape.ptr())) {
		out["type"] = "cylinder";
		Dictionary cylinder_props;
		cylinder_props["radius"] = MAX(0.01, cylinder->get_radius());
		cylinder_props["halfHeight"] = MAX(0.01, cylinder->get_height() * 0.5);
		out["cylinder"] = cylinder_props;
	} else if (r_warning) {
		*r_warning = vformat("Unsupported collision shape '%s' on '%s' (use box/sphere/capsule/cylinder)", shape->get_class(), p_shape->get_name());
	}
	return out;
}

Dictionary DDDBrowserExportConvert::collider_from_mesh_node(MeshInstance3D *p_mi, String *r_warning) {
	Dictionary out;
	if (!p_mi) {
		return out;
	}
	String local_warn;

	for (int i = 0; i < p_mi->get_child_count(); i++) {
		if (CollisionShape3D *cs = Object::cast_to<CollisionShape3D>(p_mi->get_child(i))) {
			local_warn = String();
			out = collider_from_shape_node(cs, &local_warn);
			if (!out.is_empty()) {
				return out;
			}
			if (!local_warn.is_empty() && r_warning && r_warning->is_empty()) {
				*r_warning = local_warn;
			}
		}
	}
	Node *parent = p_mi->get_parent();
	if (parent) {
		for (int i = 0; i < parent->get_child_count(); i++) {
			if (CollisionShape3D *cs = Object::cast_to<CollisionShape3D>(parent->get_child(i))) {
				local_warn = String();
				out = collider_from_shape_node(cs, &local_warn);
				if (!out.is_empty()) {
					return out;
				}
				if (!local_warn.is_empty() && r_warning && r_warning->is_empty()) {
					*r_warning = local_warn;
				}
			}
		}
	}
	return out;
}

Error DDDBrowserExportConvert::export_mesh_with_mtl(DDDBrowserExporter *p_exporter, MeshInstance3D *p_mi, const String &p_obj_path, const String &p_base_url, const String &p_export_dir, String *r_warning) {
	ERR_FAIL_NULL_V(p_exporter, ERR_INVALID_PARAMETER);
	ERR_FAIL_NULL_V(p_mi, ERR_INVALID_PARAMETER);
	Ref<Mesh> mesh = p_mi->get_mesh();
	ERR_FAIL_COND_V(mesh.is_null(), ERR_INVALID_DATA);

	const String id_base = p_obj_path.get_file().get_basename();
	const String mtl_name = id_base + ".mtl";
	const String mtl_path = p_obj_path.get_base_dir().path_join(mtl_name);

	String body;
	body += "# DDDBrowser OBJ export\n";
	body += "mtllib " + mtl_name + "\n";

	String mtl_body;
	mtl_body += "# DDDBrowser MTL export\n";

	int vertex_offset = 1;
	const int surfaces = mesh->get_surface_count();
	for (int s = 0; s < surfaces; s++) {
		Array arrays = mesh->surface_get_arrays(s);
		PackedVector3Array vertices = arrays[Mesh::ARRAY_VERTEX];
		PackedVector3Array normals = arrays[Mesh::ARRAY_NORMAL];
		PackedVector2Array uvs = arrays[Mesh::ARRAY_TEX_UV];
		PackedInt32Array indices = arrays[Mesh::ARRAY_INDEX];

		String mat_name = "mat_" + itos(s);
		body += "usemtl " + mat_name + "\n";
		mtl_body += "newmtl " + mat_name + "\n";

		Ref<Material> mat = p_mi->get_active_material(s);
		Color albedo(1, 1, 1, 1);
		Ref<Texture2D> albedo_tex;
		if (mat.is_valid()) {
			if (BaseMaterial3D *base = Object::cast_to<BaseMaterial3D>(mat.ptr())) {
				albedo = base->get_albedo();
				albedo_tex = base->get_texture(BaseMaterial3D::TEXTURE_ALBEDO);
			}
		}
		mtl_body += vformat("Kd %.6f %.6f %.6f\n", albedo.r, albedo.g, albedo.b);

		if (albedo_tex.is_valid()) {
			String tex_path = albedo_tex->get_path();
			String dest_name = id_base + "_albedo_" + itos(s);
			String dest_rel;
			if (!tex_path.is_empty() && (tex_path.begins_with("res://") || tex_path.begins_with("user://"))) {
				String src = ProjectSettings::get_singleton()->globalize_path(tex_path);
				String ext = src.get_extension().to_lower();
				if (ext.is_empty()) {
					ext = "png";
				}
				dest_rel = "textures/" + dest_name + "." + ext;
				String dest_abs = p_export_dir.path_join(dest_rel);
				if (FileAccess::exists(src)) {
					p_exporter->_copy_file(src, dest_abs);
					mtl_body += "map_Kd ../" + dest_rel.replace("\\", "/") + "\n";
				} else if (r_warning) {
					*r_warning = vformat("Missing albedo texture file: %s", tex_path);
				}
			} else {
				Ref<Image> img = albedo_tex->get_image();
				if (img.is_valid() && !img->is_empty()) {
					dest_rel = "textures/" + dest_name + ".png";
					String dest_abs = p_export_dir.path_join(dest_rel);
					img->save_png(dest_abs);
					mtl_body += "map_Kd ../" + dest_rel.replace("\\", "/") + "\n";
				}
			}
		}

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

		body += "o " + id_base + "\n";
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

	Error err = p_exporter->_write_text(p_obj_path, body);
	if (err != OK) {
		return err;
	}
	return p_exporter->_write_text(mtl_path, mtl_body);
}

String DDDBrowserExportConvert::resolve_audio_stream_path(Node *p_node) {
	AudioStreamPlayer3D *player = Object::cast_to<AudioStreamPlayer3D>(p_node);
	if (!player) {
		return String();
	}
	Ref<AudioStream> stream = player->get_stream();
	if (stream.is_null()) {
		return String();
	}
	return stream->get_path();
}

String DDDBrowserExportConvert::resolve_luau_script_path(Node *p_node) {
	ERR_FAIL_NULL_V(p_node, String());
	Ref<Script> script = p_node->get_script();
	if (script.is_null()) {
		return String();
	}
#ifdef MODULE_LUAU_MODULE_ENABLED
	if (Object::cast_to<LuauScript>(script.ptr())) {
		String path = script->get_path();
		if (!path.is_empty()) {
			return path;
		}

		return String();
	}
#endif
	String path = script->get_path();
	if (path.ends_with(".luau") || path.ends_with(".lua")) {
		return path;
	}
	return String();
}

Dictionary DDDBrowserExportConvert::collect_script_export_data(Node *p_node) {
	Dictionary data;
	ERR_FAIL_NULL_V(p_node, data);
	Ref<Script> script = p_node->get_script();
	if (script.is_null()) {
		return data;
	}
	List<PropertyInfo> props;
	p_node->get_property_list(&props);
	for (const PropertyInfo &pi : props) {
		if (!(pi.usage & PROPERTY_USAGE_SCRIPT_VARIABLE)) {
			continue;
		}
		Variant v = p_node->get(pi.name);
		switch (v.get_type()) {
			case Variant::BOOL:
			case Variant::INT:
			case Variant::FLOAT:
			case Variant::STRING:
			case Variant::STRING_NAME:
			case Variant::DICTIONARY:
			case Variant::ARRAY:
				data[String(pi.name)] = v;
				break;
			default:
				break;
		}
	}
	return data;
}

bool DDDBrowserExportConvert::try_skybox_from_world_environment(Node *p_root, Dictionary &r_skybox) {
	ERR_FAIL_NULL_V(p_root, false);
	Vector<Node *> nodes;
	nodes.push_back(p_root);
	for (int i = 0; i < nodes.size(); i++) {
		for (int c = 0; c < nodes[i]->get_child_count(); c++) {
			nodes.push_back(nodes[i]->get_child(c));
		}
	}
	for (int i = 0; i < nodes.size(); i++) {
		WorldEnvironment *we = Object::cast_to<WorldEnvironment>(nodes[i]);
		if (!we) {
			continue;
		}
		Ref<Environment> env = we->get_environment();
		if (env.is_null()) {
			continue;
		}
		Ref<Sky> sky = env->get_sky();
		if (sky.is_null()) {
			continue;
		}

		Variant panorama = sky->get("sky_material");
		if (panorama.get_type() == Variant::OBJECT) {
			Object *mat_obj = panorama;
			if (mat_obj) {
				Variant tex_v = mat_obj->get("panorama");
				if (tex_v.get_type() == Variant::OBJECT) {
					Ref<Texture2D> tex = tex_v;
					if (tex.is_valid() && !tex->get_path().is_empty()) {
						r_skybox["uri"] = tex->get_path();
						return true;
					}
				}
			}
		}
	}
	return false;
}
