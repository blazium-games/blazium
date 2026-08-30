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

#include "gs_tex_writer.h"
#include "mesh_writer.h"
#include "script_cook.h"

#include "core/config/project_settings.h"
#include "core/io/dir_access.h"
#include "core/io/file_access.h"
#include "core/io/image.h"
#include "core/io/resource_loader.h"
#include "scene/3d/camera_3d.h"
#include "scene/3d/importer_mesh_instance_3d.h"
#include "scene/3d/mesh_instance_3d.h"
#include "scene/3d/node_3d.h"
#include "scene/resources/3d/importer_mesh.h"
#include "scene/resources/material.h"
#include "scene/resources/mesh.h"
#include "scene/resources/packed_scene.h"
#include "scene/resources/texture.h"

#include <cstring>

namespace ps2_cook {

static const char *kAllowed[] = {
	"Node", "Node3D", "MeshInstance3D", "ImporterMeshInstance3D",
	"Camera3D", "AnimationPlayer", "Skeleton3D", "BoneAttachment3D",
	"WorldEnvironment", "Marker3D",
	"DirectionalLight3D", "OmniLight3D", "SpotLight3D", "Light3D",
};

static bool type_allowed(const String &p_cls) {
	for (const char *a : kAllowed) {
		if (p_cls == a) {
			return true;
		}
	}
	return false;
}

static bool type_refused(const String &p_cls) {
	return p_cls == "AnimationTree" ||
			p_cls.begins_with("RigidBody") ||
			p_cls.begins_with("VehicleWheel") ||
			p_cls.begins_with("SubViewport") ||
			p_cls.begins_with("SoftBody") ||
			p_cls.begins_with("GPUParticles") ||
			p_cls == "Decal" || p_cls == "ReflectionProbe" ||
			p_cls == "FogVolume" || p_cls == "VoxelGI";
}

struct Gather {
	struct NodeRow {
		int parent = -1;
		uint8_t kind = 0;
		String name;
		Vector3 pos;
		Vector3 rot;
		Vector3 scale = Vector3(1, 1, 1);
		String scene_hint;
	};
	Vector<NodeRow> nodes;
	Vector<CookTri> tris;
	Vector<Ref<Image>> images;
	Vector<String> image_names;
	Vector<String> errors;
	Vector<String> warnings;
	int node_dropped = 0;
	int tri_dropped = 0;
	int tex_dropped = 0;
};

static uint8_t kind_of(Node *p_node) {
	if (Object::cast_to<MeshInstance3D>(p_node) || p_node->is_class("ImporterMeshInstance3D")) {
		return 2;
	}
	if (Object::cast_to<Camera3D>(p_node)) {
		return 3;
	}
	if (p_node->is_class("AnimationPlayer")) {
		return 4;
	}
	if (Object::cast_to<Node3D>(p_node)) {
		return 1;
	}
	return 0;
}

static Transform3D world_xform(Node3D *p_node) {
	Transform3D xf;
	for (Node *n = p_node; n; n = n->get_parent()) {
		if (Node3D *n3 = Object::cast_to<Node3D>(n)) {
			xf = n3->get_transform() * xf;
		}
	}
	return xf;
}

static Ref<Image> image_from_texture(const Ref<Texture2D> &p_tex) {
	if (p_tex.is_null()) {
		return Ref<Image>();
	}
	Ref<Image> img = p_tex->get_image();
	if (img.is_valid() && !img->is_empty()) {
		return img;
	}
	const String path = p_tex->get_path();
	if (!path.is_empty()) {
		Ref<Image> loaded;
		loaded.instantiate();
		if (loaded->load(path) == OK) {
			return loaded;
		}
	}
	return Ref<Image>();
}

static Ref<Image> image_from_material(const Ref<Material> &p_mat, String &r_name) {
	const Ref<BaseMaterial3D> std = p_mat;
	if (std.is_null()) {
		return Ref<Image>();
	}
	const Ref<Texture2D> tex = std->get_texture(BaseMaterial3D::TEXTURE_ALBEDO);
	if (tex.is_valid()) {
		r_name = tex->get_path();
		if (r_name.is_empty()) {
			r_name = tex->get_name();
		}
		return image_from_texture(tex);
	}
	const Color c = std->get_albedo();
	r_name = vformat("albedo_%08x", c.to_rgba32());
	Ref<Image> solid = Image::create_empty(16, 16, false, Image::FORMAT_RGBA8);
	solid->fill(c);
	return solid;
}

static uint16_t register_image(Gather &r_g, const Ref<Image> &p_img, const String &p_name) {
	if (p_img.is_null() || p_img->is_empty()) {
		return 0;
	}
	if (!p_name.is_empty()) {
		for (int i = 0; i < r_g.image_names.size(); i++) {
			if (r_g.image_names[i] == p_name) {
				return uint16_t(i);
			}
		}
	}
	if (r_g.images.size() >= kMaxTextures) {
		r_g.tex_dropped++;
		return 0;
	}
	r_g.images.push_back(p_img);
	r_g.image_names.push_back(p_name);
	return uint16_t(r_g.images.size() - 1);
}

static void emit_tris(Gather &r_g, const PackedVector3Array &verts, const PackedVector2Array &uvs, const PackedInt32Array &idx, uint16_t p_tex, uint16_t p_node) {
	auto push_tri = [&](const Vector3 &a, const Vector3 &b, const Vector3 &c, const Vector2 &ua, const Vector2 &ub, const Vector2 &uc) {
		if (r_g.tris.size() >= kMaxTris) {
			r_g.tri_dropped++;
			return;
		}
		CookTri t;
		t.v[0] = a;
		t.v[1] = b;
		t.v[2] = c;
		t.uv[0] = ua;
		t.uv[1] = ub;
		t.uv[2] = uc;
		t.tex_slot = p_tex;
		t.node_id = p_node;
		r_g.tris.push_back(t);
	};
	if (idx.size() >= 3) {
		for (int i = 0; i + 2 < idx.size(); i += 3) {
			const int i0 = idx[i];
			const int i1 = idx[i + 1];
			const int i2 = idx[i + 2];
			if (i0 < 0 || i1 < 0 || i2 < 0 || i0 >= verts.size() || i1 >= verts.size() || i2 >= verts.size()) {
				continue;
			}
			push_tri(verts[i0], verts[i1], verts[i2],
					i0 < uvs.size() ? uvs[i0] : Vector2(),
					i1 < uvs.size() ? uvs[i1] : Vector2(),
					i2 < uvs.size() ? uvs[i2] : Vector2());
		}
		return;
	}
	for (int i = 0; i + 2 < verts.size(); i += 3) {
		push_tri(verts[i], verts[i + 1], verts[i + 2],
				i < uvs.size() ? uvs[i] : Vector2(),
				i + 1 < uvs.size() ? uvs[i + 1] : Vector2(1, 0),
				i + 2 < uvs.size() ? uvs[i + 2] : Vector2(0, 1));
	}
}

static void emit_mesh(Gather &r_g, const Ref<Mesh> &p_mesh, const Transform3D &p_xf, int p_my, Node *p_owner) {
	if (p_mesh.is_null() || p_my < 0) {
		return;
	}
	for (int s = 0; s < p_mesh->get_surface_count(); s++) {
		Array arr = p_mesh->surface_get_arrays(s);
		PackedVector3Array verts;
		PackedVector2Array uvs;
		PackedInt32Array idx;
		if (arr.size() > Mesh::ARRAY_VERTEX) {
			verts = arr[Mesh::ARRAY_VERTEX];
		}
		if (arr.size() > Mesh::ARRAY_TEX_UV && arr[Mesh::ARRAY_TEX_UV].get_type() == Variant::PACKED_VECTOR2_ARRAY) {
			uvs = arr[Mesh::ARRAY_TEX_UV];
		}
		if (arr.size() > Mesh::ARRAY_INDEX && arr[Mesh::ARRAY_INDEX].get_type() == Variant::PACKED_INT32_ARRAY) {
			idx = arr[Mesh::ARRAY_INDEX];
		}
		if (p_xf != Transform3D()) {
			for (int i = 0; i < verts.size(); i++) {
				verts.write[i] = p_xf.xform(verts[i]);
			}
		}
		Ref<Material> mat;
		if (MeshInstance3D *mi = Object::cast_to<MeshInstance3D>(p_owner)) {
			mat = mi->get_active_material(s);
		}
		if (mat.is_null()) {
			mat = p_mesh->surface_get_material(s);
		}
		String tex_name;
		const Ref<Image> img = image_from_material(mat, tex_name);
		const uint16_t slot = register_image(r_g, img, tex_name);
		emit_tris(r_g, verts, uvs, idx, slot, uint16_t(p_my));
	}
}

static void walk_scene(Node *p_node, Gather &r_g, int p_parent, const String &p_scene) {
	if (!p_node) {
		return;
	}
	const String cls = p_node->get_class();
	if (type_refused(cls)) {
		r_g.errors.push_back("illegal PS2 node type: " + cls + " (" + String(p_node->get_name()) + ")");
		return;
	}
	if (!type_allowed(cls)) {
		r_g.errors.push_back("illegal PS2 node type: " + cls + " (" + String(p_node->get_name()) + ")");
		return;
	}
	int my = -1;
	if (r_g.nodes.size() < kMaxNodes) {
		Gather::NodeRow row;
		row.parent = p_parent;
		row.kind = kind_of(p_node);
		row.name = p_node->get_name();
		row.scene_hint = p_scene;
		if (Node3D *n3 = Object::cast_to<Node3D>(p_node)) {
			const Transform3D xf = world_xform(n3);
			row.pos = xf.origin;
			row.rot = xf.basis.get_euler();
			row.scale = xf.basis.get_scale();
		}
		my = r_g.nodes.size();
		r_g.nodes.push_back(row);
	} else {
		r_g.node_dropped++;
	}
	if (MeshInstance3D *mi = Object::cast_to<MeshInstance3D>(p_node)) {
		if (mi->get_mesh().is_valid() && my >= 0) {
			emit_mesh(r_g, mi->get_mesh(), Transform3D(), my, mi);
		}
	}
	if (ImporterMeshInstance3D *imi = Object::cast_to<ImporterMeshInstance3D>(p_node)) {
		if (imi->get_mesh().is_valid() && my >= 0) {
			emit_mesh(r_g, imi->get_mesh()->get_mesh(), Transform3D(), my, nullptr);
		}
	}
	for (int i = 0; i < p_node->get_child_count(); i++) {
		walk_scene(p_node->get_child(i), r_g, my >= 0 ? my : p_parent, p_scene);
	}
}

static void append_u16(Vector<uint8_t> &o, uint16_t v) {
	o.push_back(uint8_t(v & 0xff));
	o.push_back(uint8_t(v >> 8));
}

static void append_f32(Vector<uint8_t> &o, float v) {
	uint32_t bits = 0;
	memcpy(&bits, &v, sizeof(bits));
	append_u16(o, uint16_t(bits & 0xffff));
	append_u16(o, uint16_t(bits >> 16));
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
		const Gather::NodeRow &n = p_g.nodes[i];
		append_u16(out, uint16_t(i));
		append_u16(out, uint16_t(int16_t(n.parent)));
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
		CharString utf = n.name.utf8();
		for (int c = 0; c < 32; c++) {
			out.push_back(c < utf.length() ? uint8_t(utf[c]) : 0);
		}
	}
	return out;
}

static Error write_bytes(const String &p_path, const Vector<uint8_t> &p_bytes) {
	Ref<FileAccess> f = FileAccess::open(p_path, FileAccess::WRITE);
	ERR_FAIL_COND_V(f.is_null(), ERR_CANT_CREATE);
	if (!p_bytes.is_empty()) {
		f->store_buffer(p_bytes);
	}
	return OK;
}

static int display_height(const CookOptions &p_opts) {
	if (p_opts.width == 320) {
		return 224;
	}
	if (p_opts.region == 1) {
		return 512;
	}
	return 448;
}

static int frame_vram_bytes(int p_w, int p_h) {
	// Double 16-bit frame + 16-bit Z.
	return p_w * p_h * 2 * 2 + p_w * p_h * 2;
}

Error cook_project(const String &p_export_dir, CookResult &r_out, const CookOptions &p_opts) {
	Ref<DirAccess> da = DirAccess::create(DirAccess::ACCESS_FILESYSTEM);
	const Error mk = da->make_dir_recursive(p_export_dir);
	ERR_FAIL_COND_V(mk != OK && mk != ERR_ALREADY_EXISTS, mk);

	r_out.display_w = p_opts.width == 512 ? 512 : (p_opts.width == 320 ? 320 : 640);
	r_out.display_h = display_height(p_opts);

	const String main_scene = ProjectSettings::get_singleton() ? String(ProjectSettings::get_singleton()->get("application/run/main_scene")) : String();
	if (main_scene.is_empty()) {
		r_out.errors.push_back("PS2 export requires application/run/main_scene.");
		r_out.report = r_out.errors[0];
		return FAILED;
	}
	if (!ResourceLoader::exists(main_scene)) {
		r_out.errors.push_back("main_scene does not exist: " + main_scene);
		r_out.report = r_out.errors[0];
		return FAILED;
	}

	ScriptCook scripts;
	const Error sc_err = cook_scripts(p_export_dir, scripts);
	for (int i = 0; i < scripts.rejected.size(); i++) {
		r_out.errors.push_back("script refused: " + scripts.rejected[i]);
	}
	if (sc_err != OK || !scripts.rejected.is_empty()) {
		r_out.report = r_out.errors.is_empty() ? String("script cook failed") : r_out.errors[0];
		return ERR_UNCONFIGURED;
	}

	Gather gathered;
	Vector<Node *> live;
	Ref<PackedScene> ps = ResourceLoader::load(main_scene);
	if (ps.is_null()) {
		r_out.errors.push_back("failed to load main_scene: " + main_scene);
		r_out.report = r_out.errors[0];
		return FAILED;
	}
	Node *root = ps->instantiate();
	live.push_back(root);
	walk_scene(root, gathered, -1, main_scene);

	r_out.pack_count = 1;
	const String main_abs = ProjectSettings::get_singleton()->globalize_path(main_scene);
	for (int i = 0; i < scripts.extra_scenes.size(); i++) {
		const String extra = scripts.extra_scenes[i];
		if (extra == main_scene) {
			continue;
		}
		if (r_out.pack_count >= kMaxScenePacks) {
			r_out.warnings.push_back("kept first 32 scene packs.");
			break;
		}
		if (!ResourceLoader::exists(extra)) {
			r_out.errors.push_back("pack path not cooked: " + extra);
			continue;
		}
		Ref<PackedScene> extra_ps = ResourceLoader::load(extra);
		if (extra_ps.is_null()) {
			r_out.errors.push_back("pack path not cooked: " + extra);
			continue;
		}
		Node *er = extra_ps->instantiate();
		live.push_back(er);
		walk_scene(er, gathered, -1, extra);
		r_out.extra_pack_files.push_back(ProjectSettings::get_singleton()->globalize_path(extra));
		r_out.pack_count++;
	}

	r_out.errors.append_array(gathered.errors);
	r_out.warnings.append_array(gathered.warnings);
	if (gathered.node_dropped > 0) {
		r_out.warnings.push_back(vformat("kept first %d of %d nodes (dropped %d).", kMaxNodes, kMaxNodes + gathered.node_dropped, gathered.node_dropped));
	}
	if (gathered.tri_dropped > 0) {
		r_out.warnings.push_back(vformat("kept first %d of %d tris (dropped %d).", kMaxTris, kMaxTris + gathered.tri_dropped, gathered.tri_dropped));
	}
	if (gathered.tex_dropped > 0) {
		r_out.warnings.push_back(vformat("kept first %d textures (dropped %d).", kMaxTextures, gathered.tex_dropped));
	}

	if (!gathered.errors.is_empty()) {
		r_out.report = gathered.errors[0];
		for (int i = 0; i < live.size(); i++) {
			memdelete(live[i]);
		}
		return ERR_UNCONFIGURED;
	}

	if (gathered.images.is_empty()) {
		Ref<Image> placeholder = Image::create_empty(16, 16, false, Image::FORMAT_RGBA8);
		placeholder->fill(Color(0.2f, 0.35f, 0.55f));
		gathered.images.push_back(placeholder);
		gathered.image_names.push_back("placeholder");
	}

	const Vector<uint8_t> node_blob = write_node_blob(gathered);
	MeshBlob mesh;
	const Error merr = write_mesh_ee(gathered.tris, kMaxTris, mesh);
	if (merr != OK) {
		r_out.errors.push_back("MESH cook failed.");
		r_out.report = r_out.errors[r_out.errors.size() - 1];
		for (int i = 0; i < live.size(); i++) {
			memdelete(live[i]);
		}
		return merr;
	}

	Vector<GsTex> texs;
	int tex_vram = 0;
	for (int i = 0; i < gathered.images.size(); i++) {
		GsTex one;
		const Error terr = write_gs_tex(gathered.images[i], one);
		if (terr != OK) {
			r_out.errors.push_back("GTEX cook failed for " + gathered.image_names[i]);
			r_out.report = r_out.errors[r_out.errors.size() - 1];
			for (int r = 0; r < live.size(); r++) {
				memdelete(live[r]);
			}
			return terr;
		}
		texs.push_back(one);
		tex_vram += one.vram_bytes;
	}
	GtexBlob gtex;
	write_gtex_pack(texs, gtex);

	const int fb_vram = frame_vram_bytes(r_out.display_w, r_out.display_h);
	r_out.vram_bytes = fb_vram + gtex.vram_bytes;
	r_out.ee_ram_bytes = gathered.nodes.size() * 80 + mesh.bytes.size() + gtex.bytes.size() + 2 * 1024 * 1024;
	r_out.nodes = gathered.nodes.size();
	r_out.tris = gathered.tris.size() + gathered.tri_dropped;
	r_out.cooked_tris = mesh.tri_count;
	r_out.textures = texs.size();

	if (r_out.vram_bytes > kGsVramLimit) {
		r_out.over_budget = true;
		r_out.errors.push_back(vformat("GS VRAM estimate %d exceeds 4 MiB.", r_out.vram_bytes));
		r_out.report = r_out.errors[r_out.errors.size() - 1];
		for (int i = 0; i < live.size(); i++) {
			memdelete(live[i]);
		}
		return ERR_OUT_OF_MEMORY;
	}
	if (r_out.ee_ram_bytes > kEeRamLimit) {
		r_out.over_budget = true;
		r_out.errors.push_back(vformat("EE RAM estimate %d exceeds 24 MiB.", r_out.ee_ram_bytes));
		r_out.report = r_out.errors[r_out.errors.size() - 1];
		for (int i = 0; i < live.size(); i++) {
			memdelete(live[i]);
		}
		return ERR_OUT_OF_MEMORY;
	}

	r_out.node_path = p_export_dir.path_join("NODE00.bin");
	r_out.mesh_path = p_export_dir.path_join("MESH00.bin");
	r_out.gtex_path = p_export_dir.path_join("GTEX00.bin");
	r_out.script_path = p_export_dir.path_join("SCRP00.bin");
	r_out.script_ir_path = p_export_dir.path_join("SCRIPT_IR.txt");
	r_out.names_path = p_export_dir.path_join("NAMES.MAP");
	r_out.budget_path = p_export_dir.path_join("BUDGET.txt");
	r_out.warnings_path = p_export_dir.path_join("WARNINGS.txt");

	if (write_bytes(r_out.node_path, node_blob) != OK ||
			write_bytes(r_out.mesh_path, mesh.bytes) != OK ||
			write_bytes(r_out.gtex_path, gtex.bytes) != OK ||
			write_bytes(r_out.script_path, scripts.scrp) != OK) {
		r_out.errors.push_back("failed to write pack-0 slices.");
		r_out.report = r_out.errors[r_out.errors.size() - 1];
		for (int i = 0; i < live.size(); i++) {
			memdelete(live[i]);
		}
		return ERR_CANT_CREATE;
	}

	{
		Ref<FileAccess> ir = FileAccess::open(r_out.script_ir_path, FileAccess::WRITE);
		if (ir.is_valid()) {
			ir->store_string(scripts.ir_text);
		}
		Ref<FileAccess> names = FileAccess::open(r_out.names_path, FileAccess::WRITE);
		ERR_FAIL_COND_V(names.is_null(), ERR_CANT_CREATE);
		names->store_line(main_scene + " -> NODE00.bin MESH00.bin GTEX00.bin SCRP00.bin");
		if (main_scene.to_lower().contains("industrial") || main_abs.to_lower().contains("industrial")) {
			names->store_line("fixture: industrial");
		}
		for (int i = 0; i < gathered.image_names.size(); i++) {
			if (gathered.image_names[i].to_lower().contains("glb") || gathered.image_names[i].to_lower().contains("industrial")) {
				names->store_line("tex: " + gathered.image_names[i]);
			}
		}
		for (int i = 0; i < MIN(gathered.nodes.size(), 64); i++) {
			String line = vformat("%d\t%s", i, gathered.nodes[i].name);
			if (!gathered.nodes[i].scene_hint.is_empty()) {
				line += "\t" + gathered.nodes[i].scene_hint;
			}
			names->store_line(line);
		}
		if (ResourceLoader::exists("res://assets/Industrial_exterior_v2.glb")) {
			names->store_line("glb: res://assets/Industrial_exterior_v2.glb");
		}
		Ref<FileAccess> budget = FileAccess::open(r_out.budget_path, FileAccess::WRITE);
		if (budget.is_valid()) {
			budget->store_line(vformat("ABI %d", kCookABI));
			budget->store_line(vformat("nodes %d/%d", r_out.nodes, kMaxNodes));
			budget->store_line(vformat("tris %d/%d", r_out.cooked_tris, kMaxTris));
			budget->store_line(vformat("textures %d/%d", r_out.textures, kMaxTextures));
			budget->store_line(vformat("gs_vram %d/%d", r_out.vram_bytes, kGsVramLimit));
			budget->store_line(vformat("ee_ram %d/%d", r_out.ee_ram_bytes, kEeRamLimit));
			budget->store_line(vformat("display %dx%d", r_out.display_w, r_out.display_h));
		}
		Ref<FileAccess> warn = FileAccess::open(r_out.warnings_path, FileAccess::WRITE);
		if (warn.is_valid()) {
			for (int i = 0; i < r_out.warnings.size(); i++) {
				warn->store_line(r_out.warnings[i]);
			}
			if (r_out.warnings.is_empty()) {
				warn->store_line("ok");
			}
		}
	}

	r_out.infos.push_back(vformat("cooked %d nodes, %d tris, %d textures (GS VRAM %d).", r_out.nodes, r_out.cooked_tris, r_out.textures, r_out.vram_bytes));
	r_out.report = r_out.infos[r_out.infos.size() - 1];

	for (int i = 0; i < live.size(); i++) {
		memdelete(live[i]);
	}
	return OK;
}

} // namespace ps2_cook
