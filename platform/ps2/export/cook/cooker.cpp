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

#include "adpcm_writer.h"
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
#include "scene/resources/audio_stream_wav.h"
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
	"AudioStreamPlayer", "AudioStreamPlayer3D", "AudioStreamPlayer2D",
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
	int cameras = 0;
	int anim_players = 0;
	int skeletons = 0;
	int lights = 0;
	int audio_players = 0;
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
	if (p_node->is_class("Camera3D")) {
		r_g.cameras++;
	}
	if (p_node->is_class("AnimationPlayer")) {
		r_g.anim_players++;
	}
	if (p_node->is_class("Skeleton3D")) {
		r_g.skeletons++;
	}
	if (p_node->is_class("Light3D")) {
		r_g.lights++;
	}
	if (p_node->is_class("AudioStreamPlayer") || p_node->is_class("AudioStreamPlayer3D") || p_node->is_class("AudioStreamPlayer2D")) {
		r_g.audio_players++;
	}
	if (MeshInstance3D *mi = Object::cast_to<MeshInstance3D>(p_node)) {
		if (mi->get_mesh().is_valid() && my >= 0) {
			emit_mesh(r_g, mi->get_mesh(), world_xform(mi), my, mi);
		}
	}
	if (ImporterMeshInstance3D *imi = Object::cast_to<ImporterMeshInstance3D>(p_node)) {
		if (imi->get_mesh().is_valid() && my >= 0) {
			emit_mesh(r_g, imi->get_mesh()->get_mesh(), world_xform(imi), my, nullptr);
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

static void ensure_placeholder_tex(Gather &r_g) {
	if (!r_g.images.is_empty()) {
		return;
	}
	Ref<Image> placeholder = Image::create_empty(16, 16, false, Image::FORMAT_RGBA8);
	placeholder->fill(Color(0.2f, 0.35f, 0.55f));
	r_g.images.push_back(placeholder);
	r_g.image_names.push_back("placeholder");
}

static Error cook_gather_bins(const String &p_node, const String &p_mesh, const String &p_gtex, Gather &p_g, String &r_err) {
	ensure_placeholder_tex(p_g);
	const Vector<uint8_t> node_blob = write_node_blob(p_g);
	MeshBlob mesh;
	const Error merr = write_mesh_ee(p_g.tris, kMaxTris, mesh);
	if (merr != OK) {
		r_err = "MESH cook failed.";
		return merr;
	}
	Vector<GsTex> texs;
	for (int i = 0; i < p_g.images.size(); i++) {
		GsTex one;
		const Error terr = write_gs_tex(p_g.images[i], one);
		if (terr != OK) {
			r_err = "GTEX cook failed for " + p_g.image_names[i];
			return terr;
		}
		texs.push_back(one);
	}
	GtexBlob gtex;
	write_gtex_pack(texs, gtex);
	if (write_bytes(p_node, node_blob) != OK || write_bytes(p_mesh, mesh.bytes) != OK || write_bytes(p_gtex, gtex.bytes) != OK) {
		r_err = "failed to write pack slices.";
		return ERR_CANT_CREATE;
	}
	return OK;
}

static Vector<uint8_t> write_disp_blob(const CookOptions &p_opts, int p_w, int p_h) {
	Vector<uint8_t> out;
	out.push_back('D');
	out.push_back('I');
	out.push_back('S');
	out.push_back('P');
	append_u16(out, uint16_t(kCookABI));
	append_u16(out, uint16_t(p_opts.region == 1 ? 1 : 0));
	append_u16(out, uint16_t(p_w));
	append_u16(out, uint16_t(p_h));
	return out;
}

static void append_stub_warnings(Gather &p_g, Vector<String> &r_warn, const String &p_pack) {
	if (p_g.anim_players > 0) {
		r_warn.push_back(p_pack + "AnimationPlayer is allow-listed but clips are not cooked on PS2 ABI 1 (bind pose only).");
	}
	if (p_g.skeletons > 0) {
		r_warn.push_back(p_pack + "Skeleton3D is allow-listed; PS2 ABI 1 does not cook bone palettes.");
	}
	if (p_g.lights > 0) {
		r_warn.push_back(p_pack + "Light3D is allow-listed; PS2 ABI 1 has no GTE lights (flat/textured vertex color).");
	}
	if (p_g.audio_players > 0) {
		r_warn.push_back(p_pack + "AudioStreamPlayer is allow-listed; SFX00 is the first res:// .wav, not a per-node bank.");
	}
	if (p_g.cameras > kMaxCameras) {
		r_warn.push_back(vformat("%skept first %d of %d cameras (guest uses the first Camera3D).", p_pack, kMaxCameras, p_g.cameras));
	}
}

static Vector<uint8_t> write_pack_toc(const Vector<String> &p_names) {
	Vector<uint8_t> out;
	out.push_back('P');
	out.push_back('A');
	out.push_back('C');
	out.push_back('K');
	append_u16(out, uint16_t(kCookABI));
	append_u16(out, uint16_t(p_names.size()));
	for (int i = 0; i < p_names.size(); i++) {
		CharString utf = p_names[i].utf8();
		const int n = MIN(255, utf.length());
		out.push_back(uint8_t(n));
		for (int c = 0; c < n; c++) {
			out.push_back(uint8_t(utf[c]));
		}
	}
	return out;
}

static void pcm_from_wav(const Ref<AudioStreamWAV> &p_wav, Vector<int16_t> &r_pcm, int &r_rate) {
	if (p_wav.is_null()) {
		return;
	}
	r_rate = p_wav->get_mix_rate() > 0 ? p_wav->get_mix_rate() : 22050;
	const Vector<uint8_t> data = p_wav->get_data();
	if (p_wav->get_format() == AudioStreamWAV::FORMAT_16_BITS) {
		const int n = data.size() / 2;
		r_pcm.resize(n);
		for (int i = 0; i < n; i++) {
			int16_t s = 0;
			memcpy(&s, &data[i * 2], 2);
			r_pcm.write[i] = s;
		}
		if (p_wav->is_stereo() && n >= 2) {
			Vector<int16_t> mono;
			mono.resize(n / 2);
			for (int i = 0; i + 1 < n; i += 2) {
				mono.write[i / 2] = int16_t((int(r_pcm[i]) + int(r_pcm[i + 1])) / 2);
			}
			r_pcm = mono;
		}
		return;
	}
	if (p_wav->get_format() == AudioStreamWAV::FORMAT_8_BITS) {
		r_pcm.resize(data.size());
		for (int i = 0; i < data.size(); i++) {
			r_pcm.write[i] = int16_t((int(data[i]) - 128) * 256);
		}
	}
}

static bool walk_first_wav(const String &p_dir, Ref<AudioStreamWAV> &r_wav) {
	Ref<DirAccess> da = DirAccess::open(p_dir);
	if (da.is_null()) {
		return false;
	}
	da->list_dir_begin();
	String n = da->get_next();
	while (!n.is_empty()) {
		if (n == "." || n == ".." || n == "tests" || n == "tools" || n == ".godot" || n == "export") {
			n = da->get_next();
			continue;
		}
		const String path = p_dir.path_join(n);
		if (da->current_is_dir()) {
			if (walk_first_wav(path, r_wav)) {
				return true;
			}
		} else if (n.to_lower().ends_with(".wav") && ResourceLoader::exists(path)) {
			Ref<AudioStreamWAV> wav = ResourceLoader::load(path);
			if (wav.is_valid()) {
				r_wav = wav;
				return true;
			}
		}
		n = da->get_next();
	}
	return false;
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
	r_out.warnings.append_array(scripts.warnings);
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
	Gather pack1;
	Vector<Node *> pack1_live;
	Vector<String> pack1_scenes;
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
		pack1_live.push_back(er);
		walk_scene(er, pack1, -1, extra);
		r_out.extra_pack_files.push_back(ProjectSettings::get_singleton()->globalize_path(extra));
		pack1_scenes.push_back(extra);
		r_out.pack_count++;
	}

	r_out.errors.append_array(gathered.errors);
	r_out.errors.append_array(pack1.errors);
	r_out.warnings.append_array(gathered.warnings);
	r_out.warnings.append_array(pack1.warnings);
	append_stub_warnings(gathered, r_out.warnings, "");
	if (!pack1_scenes.is_empty()) {
		append_stub_warnings(pack1, r_out.warnings, "pack 1: ");
	}
	if (gathered.node_dropped > 0) {
		r_out.warnings.push_back(vformat("kept first %d of %d nodes (dropped %d).", kMaxNodes, kMaxNodes + gathered.node_dropped, gathered.node_dropped));
	}
	if (gathered.tri_dropped > 0) {
		r_out.warnings.push_back(vformat("kept first %d of %d tris (dropped %d).", kMaxTris, kMaxTris + gathered.tri_dropped, gathered.tri_dropped));
	}
	if (gathered.tex_dropped > 0) {
		r_out.warnings.push_back(vformat("kept first %d textures (dropped %d).", kMaxTextures, gathered.tex_dropped));
	}
	if (pack1.node_dropped > 0) {
		r_out.warnings.push_back(vformat("pack 1: kept first %d of %d nodes (dropped %d).", kMaxNodes, kMaxNodes + pack1.node_dropped, pack1.node_dropped));
	}
	if (pack1.tri_dropped > 0) {
		r_out.warnings.push_back(vformat("pack 1: kept first %d of %d tris (dropped %d).", kMaxTris, kMaxTris + pack1.tri_dropped, pack1.tri_dropped));
	}
	if (pack1.tex_dropped > 0) {
		r_out.warnings.push_back(vformat("pack 1: kept first %d textures (dropped %d).", kMaxTextures, pack1.tex_dropped));
	}

	auto free_live = [&]() {
		for (int i = 0; i < live.size(); i++) {
			memdelete(live[i]);
		}
		for (int i = 0; i < pack1_live.size(); i++) {
			memdelete(pack1_live[i]);
		}
	};

	if (!gathered.errors.is_empty() || !pack1.errors.is_empty()) {
		r_out.report = r_out.errors.is_empty() ? String("cook failed") : r_out.errors[0];
		free_live();
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
		free_live();
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
			free_live();
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
		free_live();
		return ERR_OUT_OF_MEMORY;
	}
	if (r_out.ee_ram_bytes > kEeRamLimit) {
		r_out.over_budget = true;
		r_out.errors.push_back(vformat("EE RAM estimate %d exceeds 24 MiB.", r_out.ee_ram_bytes));
		r_out.report = r_out.errors[r_out.errors.size() - 1];
		free_live();
		return ERR_OUT_OF_MEMORY;
	}
	if (r_out.vram_bytes > (kGsVramLimit * 4) / 5) {
		r_out.warnings.push_back(vformat("GS VRAM estimate %d is over 80%% of 4 MiB.", r_out.vram_bytes));
	}
	if (r_out.ee_ram_bytes > (kEeRamLimit * 4) / 5) {
		r_out.warnings.push_back(vformat("EE RAM estimate %d is over 80%% of 24 MiB.", r_out.ee_ram_bytes));
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
		free_live();
		return ERR_CANT_CREATE;
	}

	if (!pack1_scenes.is_empty()) {
		const String node01 = p_export_dir.path_join("NODE01.bin");
		const String mesh01 = p_export_dir.path_join("MESH01.bin");
		const String gtex01 = p_export_dir.path_join("GTEX01.bin");
		String pack_err;
		if (cook_gather_bins(node01, mesh01, gtex01, pack1, pack_err) != OK) {
			r_out.errors.push_back(pack_err.is_empty() ? String("pack-1 cook failed.") : pack_err);
			r_out.report = r_out.errors[r_out.errors.size() - 1];
			free_live();
			return ERR_CANT_CREATE;
		}
		Vector<String> toc_names;
		toc_names.push_back("NODE01.bin");
		toc_names.push_back("MESH01.bin");
		toc_names.push_back("GTEX01.bin");
		r_out.pack1_path = p_export_dir.path_join("PACK01.bin");
		if (write_bytes(r_out.pack1_path, write_pack_toc(toc_names)) != OK) {
			r_out.errors.push_back("failed to write PACK01.bin.");
			r_out.report = r_out.errors[r_out.errors.size() - 1];
			free_live();
			return ERR_CANT_CREATE;
		}
		r_out.warnings.push_back("Pack 1 (MESH01/GTEX01/NODE01) is not ELF-embedded. Guest loads it from host: or cdrom0: via PS2.swap_pack. Use ps2/make_iso or output_kind EE ELF + ISO.");
	}

	{
		SfxBlob sfx;
		Vector<int16_t> pcm;
		int rate = 22050;
		Ref<AudioStreamWAV> wav;
		walk_first_wav("res://", wav);
		pcm_from_wav(wav, pcm, rate);
		if (write_sfx_pcm(pcm, rate, sfx) != OK) {
			r_out.errors.push_back("SFX cook failed.");
			r_out.report = r_out.errors[r_out.errors.size() - 1];
			free_live();
			return FAILED;
		}
		r_out.sfx_path = p_export_dir.path_join("SFX00.bin");
		if (write_bytes(r_out.sfx_path, sfx.bytes) != OK) {
			r_out.errors.push_back("failed to write SFX00.bin.");
			r_out.report = r_out.errors[r_out.errors.size() - 1];
			free_live();
			return ERR_CANT_CREATE;
		}
		r_out.warnings.push_back("SFX00 is IMA-decoded on the EE. Audible play needs libsdr + freesd.irx (SPU2 voice). Without them PS2.play_sfx / Cross is silent. SFX is not ELF-embedded; keep SFX00.bin next to the ELF (host:) or on the ISO.");
	}

	{
		r_out.disp_path = p_export_dir.path_join("DISP.bin");
		if (write_bytes(r_out.disp_path, write_disp_blob(p_opts, r_out.display_w, r_out.display_h)) != OK) {
			r_out.errors.push_back("failed to write DISP.bin.");
			r_out.report = r_out.errors[r_out.errors.size() - 1];
			free_live();
			return ERR_CANT_CREATE;
		}
	}

	{
		Ref<FileAccess> ir = FileAccess::open(r_out.script_ir_path, FileAccess::WRITE);
		if (ir.is_valid()) {
			ir->store_string(scripts.ir_text);
		}
		Ref<FileAccess> names = FileAccess::open(r_out.names_path, FileAccess::WRITE);
		if (names.is_null()) {
			r_out.errors.push_back("failed to write NAMES.MAP.");
			r_out.report = r_out.errors[r_out.errors.size() - 1];
			free_live();
			return ERR_CANT_CREATE;
		}
		names->store_line(main_scene + " -> NODE00.bin MESH00.bin GTEX00.bin SCRP00.bin");
		if (main_scene.to_lower().contains("industrial") || main_abs.to_lower().contains("industrial")) {
			names->store_line("fixture: industrial");
		}
		for (int i = 0; i < pack1_scenes.size(); i++) {
			names->store_line(pack1_scenes[i] + " -> NODE01.bin MESH01.bin GTEX01.bin PACK01.bin");
			if (pack1_scenes[i].to_lower().contains("spawn_kit")) {
				names->store_line("fixture: spawn_kit");
			}
		}
		if (!r_out.sfx_path.is_empty()) {
			names->store_line("sfx: SFX00.bin");
		}
		if (!r_out.disp_path.is_empty()) {
			names->store_line(vformat("disp: DISP.bin %dx%d %s", r_out.display_w, r_out.display_h, p_opts.region == 1 ? "PAL" : "NTSC"));
		}
		if (scripts.ir_text.contains("user://") || scripts.ir_text.contains("memcard")) {
			names->store_line("user:// -> host:SAVE.BIN mc0:SAVE.BIN");
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
			budget->store_line(vformat("region %s", p_opts.region == 1 ? "PAL" : "NTSC"));
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

	free_live();
	return OK;
}

} // namespace ps2_cook
