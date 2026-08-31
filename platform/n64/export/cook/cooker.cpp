/**************************************************************************/
/*  cooker.cpp                                                            */
/**************************************************************************/
/*                         This file is part of:                          */
/*                               BLAZIUM                                  */
/*                        https://blazium.app                             */
/**************************************************************************/
/* Copyright (c) 2024-present Blazium Engine contributors.                */
/* SPDX-License-Identifier: MIT                                           */
/**************************************************************************/

#include "cooker.h"

#include "mesh_writer.h"
#include "ntex_writer.h"

#include <cstring>

#include "core/config/project_settings.h"
#include "core/io/dir_access.h"
#include "core/io/file_access.h"
#include "core/io/image.h"
#include "core/io/resource_loader.h"
#include "scene/3d/camera_3d.h"
#include "scene/3d/mesh_instance_3d.h"
#include "scene/3d/node_3d.h"
#include "scene/resources/mesh.h"
#include "scene/resources/packed_scene.h"

namespace n64_cook {

struct NodeRow {
	int parent = -1;
	uint8_t kind = KIND_OTHER;
	Vector3 pos;
	Vector3 rot;
	Vector3 scale = Vector3(1, 1, 1);
	String name;
};

struct Gather {
	Vector<NodeRow> nodes;
	Vector<Vector3> verts;
	Vector<String> errors;
};

static void gather_mesh(MeshInstance3D *p_mi, Gather &r_g) {
	if (!p_mi) {
		return;
	}
	const Ref<Mesh> mesh = p_mi->get_mesh();
	if (mesh.is_null()) {
		return;
	}
	const Transform3D xf = p_mi->get_transform();
	const int surfaces = mesh->get_surface_count();
	for (int s = 0; s < surfaces; s++) {
		const Array arrays = mesh->surface_get_arrays(s);
		if (arrays.size() <= Mesh::ARRAY_VERTEX) {
			continue;
		}
		const PackedVector3Array verts = arrays[Mesh::ARRAY_VERTEX];
		PackedInt32Array idx;
		if (arrays.size() > Mesh::ARRAY_INDEX) {
			idx = arrays[Mesh::ARRAY_INDEX];
		}
		if (idx.size() >= 3) {
			for (int i = 0; i + 2 < idx.size(); i += 3) {
				if (r_g.verts.size() / 3 >= kMaxTris) {
					r_g.errors.push_back(vformat("N64 cook refuses more than %d tris (no silent drop).", kMaxTris));
					return;
				}
				for (int k = 0; k < 3; k++) {
					const int vi = idx[i + k];
					if (vi < 0 || vi >= verts.size()) {
						r_g.errors.push_back("MESH index out of range.");
						return;
					}
					r_g.verts.push_back(xf.xform(verts[vi]));
				}
			}
		} else {
			for (int i = 0; i + 2 < verts.size(); i += 3) {
				if (r_g.verts.size() / 3 >= kMaxTris) {
					r_g.errors.push_back(vformat("N64 cook refuses more than %d tris (no silent drop).", kMaxTris));
					return;
				}
				r_g.verts.push_back(xf.xform(verts[i]));
				r_g.verts.push_back(xf.xform(verts[i + 1]));
				r_g.verts.push_back(xf.xform(verts[i + 2]));
			}
		}
	}
}

static void append_u16(Vector<uint8_t> &r_out, uint16_t p_v) {
	r_out.push_back(uint8_t(p_v & 0xFF));
	r_out.push_back(uint8_t((p_v >> 8) & 0xFF));
}

static void append_i16(Vector<uint8_t> &r_out, int16_t p_v) {
	append_u16(r_out, uint16_t(p_v));
}

static void append_f32(Vector<uint8_t> &r_out, float p_v) {
	uint32_t bits = 0;
	memcpy(&bits, &p_v, sizeof(bits));
	r_out.push_back(uint8_t(bits & 0xFF));
	r_out.push_back(uint8_t((bits >> 8) & 0xFF));
	r_out.push_back(uint8_t((bits >> 16) & 0xFF));
	r_out.push_back(uint8_t((bits >> 24) & 0xFF));
}

static uint8_t kind_of(Node *p_node) {
	if (Object::cast_to<Camera3D>(p_node)) {
		return KIND_CAMERA3D;
	}
	if (Object::cast_to<MeshInstance3D>(p_node)) {
		return KIND_MESH_INSTANCE3D;
	}
	if (Object::cast_to<Node3D>(p_node)) {
		return KIND_NODE3D;
	}
	return KIND_OTHER;
}

static void walk_scene(Node *p_node, Gather &r_g, int p_parent) {
	if (!p_node) {
		return;
	}
	if (r_g.nodes.size() >= kMaxNodes) {
		r_g.errors.push_back(vformat("N64 cook refuses more than %d nodes (no silent drop).", kMaxNodes));
		return;
	}

	NodeRow row;
	row.parent = p_parent;
	row.kind = kind_of(p_node);
	row.name = p_node->get_name();
	if (Node3D *n3 = Object::cast_to<Node3D>(p_node)) {
		const Transform3D xf = n3->get_transform();
		row.pos = xf.origin;
		row.rot = xf.basis.get_euler();
		row.scale = xf.basis.get_scale();
	}
	const int my = r_g.nodes.size();
	r_g.nodes.push_back(row);
	if (MeshInstance3D *mi = Object::cast_to<MeshInstance3D>(p_node)) {
		gather_mesh(mi, r_g);
	}

	const int child_count = p_node->get_child_count();
	for (int i = 0; i < child_count; i++) {
		if (!r_g.errors.is_empty()) {
			return;
		}
		walk_scene(p_node->get_child(i), r_g, my);
	}
}

static Vector<uint8_t> write_node_blob(const Gather &p_g) {
	Vector<uint8_t> out;
	out.push_back('N');
	out.push_back('O');
	out.push_back('D');
	out.push_back('E');
	append_u16(out, uint16_t(kCookABI));
	append_u16(out, uint16_t(p_g.nodes.size()));
	for (int i = 0; i < p_g.nodes.size(); i++) {
		const NodeRow &n = p_g.nodes[i];
		append_u16(out, uint16_t(i));
		append_i16(out, int16_t(n.parent));
		out.push_back(n.kind);
		out.push_back(0);
		append_f32(out, n.pos.x);
		append_f32(out, n.pos.y);
		append_f32(out, n.pos.z);
		append_f32(out, n.rot.x);
		append_f32(out, n.rot.y);
		append_f32(out, n.rot.z);
		append_f32(out, n.scale.x);
		append_f32(out, n.scale.y);
		append_f32(out, n.scale.z);
		const CharString utf = n.name.utf8();
		for (int c = 0; c < 32; c++) {
			out.push_back(c < utf.length() ? uint8_t(utf[c]) : 0);
		}
	}
	return out;
}

Error cook_project(const String &p_export_dir, CookResult &r_out) {
	Ref<DirAccess> da = DirAccess::create(DirAccess::ACCESS_FILESYSTEM);
	const Error mk = da->make_dir_recursive(p_export_dir);
	ERR_FAIL_COND_V(mk != OK && mk != ERR_ALREADY_EXISTS, mk);

	const String main_scene = ProjectSettings::get_singleton() ? String(ProjectSettings::get_singleton()->get("application/run/main_scene")) : String();
	if (main_scene.is_empty()) {
		r_out.errors.push_back("N64 cook requires application/run/main_scene.");
		r_out.report = r_out.errors[0];
		return FAILED;
	}
	if (!ResourceLoader::exists(main_scene)) {
		r_out.errors.push_back("main_scene does not exist: " + main_scene);
		r_out.report = r_out.errors[0];
		return FAILED;
	}

	Ref<PackedScene> ps = ResourceLoader::load(main_scene);
	if (ps.is_null()) {
		r_out.errors.push_back("failed to load main_scene: " + main_scene);
		r_out.report = r_out.errors[0];
		return FAILED;
	}

	Node *root = ps->instantiate();
	if (!root) {
		r_out.errors.push_back("failed to instantiate main_scene: " + main_scene);
		r_out.report = r_out.errors[0];
		return FAILED;
	}

	Gather gathered;
	walk_scene(root, gathered, -1);
	memdelete(root);

	if (!gathered.errors.is_empty()) {
		r_out.errors = gathered.errors;
		r_out.report = gathered.errors[0];
		return FAILED;
	}
	if (gathered.nodes.is_empty()) {
		r_out.errors.push_back("N64 cook found no nodes in main_scene.");
		r_out.report = r_out.errors[0];
		return FAILED;
	}

	const Vector<uint8_t> blob = write_node_blob(gathered);
	const String node_path = p_export_dir.path_join("NODE00.bin");
	Ref<FileAccess> f = FileAccess::open(node_path, FileAccess::WRITE);
	if (f.is_null()) {
		r_out.errors.push_back("cannot write " + node_path);
		r_out.report = r_out.errors[0];
		return ERR_CANT_CREATE;
	}
	f->store_buffer(blob);

	r_out.node_path = node_path;
	r_out.nodes = gathered.nodes.size();

	if (gathered.verts.is_empty()) {
		r_out.errors.push_back("N64 cook requires at least one MeshInstance3D triangle.");
		r_out.report = r_out.errors[0];
		return FAILED;
	}
	Vector<uint8_t> mesh_blob;
	int tris = 0;
	const Error mesh_err = write_mesh_blob(gathered.verts, mesh_blob, tris);
	if (mesh_err != OK) {
		r_out.errors.push_back(mesh_err == ERR_OUT_OF_MEMORY ? vformat("N64 cook refuses more than %d tris (no silent drop).", kMaxTris) : String("MESH cook failed."));
		r_out.report = r_out.errors[0];
		return mesh_err;
	}
	const String mesh_path = p_export_dir.path_join("MESH00.bin");
	Ref<FileAccess> mf = FileAccess::open(mesh_path, FileAccess::WRITE);
	if (mf.is_null()) {
		r_out.errors.push_back("cannot write " + mesh_path);
		r_out.report = r_out.errors[0];
		return ERR_CANT_CREATE;
	}
	mf->store_buffer(mesh_blob);
	r_out.mesh_path = mesh_path;
	r_out.tris = tris;

	Ref<Image> albedo = Image::create_empty(16, 16, false, Image::FORMAT_RGBA8);
	albedo->fill(Color(0.2f, 0.45f, 0.85f));
	Vector<uint8_t> ntex_blob;
	const Error ntex_err = write_ntex_blob(albedo, ntex_blob);
	if (ntex_err != OK) {
		r_out.errors.push_back(ntex_err == ERR_OUT_OF_MEMORY ? String("NTEX exceeds 4 KiB TMEM.") : String("NTEX cook failed."));
		r_out.report = r_out.errors[0];
		return ntex_err;
	}
	const String ntex_path = p_export_dir.path_join("NTEX00.bin");
	Ref<FileAccess> tf = FileAccess::open(ntex_path, FileAccess::WRITE);
	if (tf.is_null()) {
		r_out.errors.push_back("cannot write " + ntex_path);
		r_out.report = r_out.errors[0];
		return ERR_CANT_CREATE;
	}
	tf->store_buffer(ntex_blob);
	r_out.ntex_path = ntex_path;

	const String names_path = p_export_dir.path_join("NAMES.MAP");
	Ref<FileAccess> names = FileAccess::open(names_path, FileAccess::WRITE);
	if (names.is_null()) {
		r_out.errors.push_back("cannot write " + names_path);
		r_out.report = r_out.errors[0];
		return ERR_CANT_CREATE;
	}
	names->store_line(main_scene + " -> NODE00.bin MESH00.bin NTEX00.bin");
	for (int i = 0; i < gathered.nodes.size(); i++) {
		names->store_line(vformat("node %d %s", i, gathered.nodes[i].name));
	}
	r_out.names_path = names_path;
	r_out.report = vformat("cooked %d nodes, %d tris to NODE/MESH/NTEX + NAMES.MAP", r_out.nodes, r_out.tris);
	return OK;
}

} // namespace n64_cook
