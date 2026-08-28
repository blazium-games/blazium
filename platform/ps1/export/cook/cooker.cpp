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
#include "script_cook.h"
#include "tim_writer.h"
#include "vag_writer.h"

#include "core/config/project_settings.h"
#include "core/io/image.h"
#include "core/io/dir_access.h"
#include "core/io/file_access.h"
#include "core/io/resource_loader.h"
#include "core/math/math_funcs.h"
#include "core/typedefs.h"
#include "scene/2d/node_2d.h"
#include "scene/2d/sprite_2d.h"
#include "scene/3d/audio_stream_player_3d.h"
#include "scene/3d/camera_3d.h"
#include "scene/3d/mesh_instance_3d.h"
#include "scene/3d/node_3d.h"
#include "scene/audio/audio_stream_player.h"
#include "scene/resources/audio_stream_wav.h"
#include "scene/resources/material.h"
#include "scene/resources/mesh.h"
#include "scene/resources/packed_scene.h"
#include "scene/resources/texture.h"

namespace ps1_cook {

static const int kVramLimit = 1024 * 1024;
static const int kRamLimit = 2 * 1024 * 1024;
static const int kSpuLimit = 508 * 1024;

struct SceneGather {
	Ref<Image> image;
	Vector<CookTri> tris;
	Camera3D *cam_current = nullptr;
	Camera3D *cam_any = nullptr;
	Ref<AudioStreamWAV> wav;
	Vector<Sprite2D *> sprites;
};

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
		if (loaded->load(path) == OK && !loaded->is_empty()) {
			return loaded;
		}
	}
	return Ref<Image>();
}

static Ref<Image> image_from_material(const Ref<Material> &p_mat) {
	const Ref<BaseMaterial3D> std = p_mat;
	if (std.is_null()) {
		return Ref<Image>();
	}
	return image_from_texture(std->get_texture(BaseMaterial3D::TEXTURE_ALBEDO));
}

static void emit_tris(const Transform3D &p_xf, const PackedVector3Array &verts, const PackedVector2Array &uvs, const PackedInt32Array &idx, Vector<CookTri> &r_tris) {
	if (idx.size() >= 3) {
		for (int i = 0; i + 2 < idx.size(); i += 3) {
			CookTri t;
			for (int k = 0; k < 3; k++) {
				const int vi = idx[i + k];
				if (vi < 0 || vi >= verts.size()) {
					continue;
				}
				t.v[k] = p_xf.xform(verts[vi]);
				t.uv[k] = (vi < uvs.size()) ? uvs[vi] : Vector2(0, 0);
			}
			r_tris.push_back(t);
		}
		return;
	}
	for (int i = 0; i + 2 < verts.size(); i += 3) {
		CookTri t;
		t.v[0] = p_xf.xform(verts[i]);
		t.v[1] = p_xf.xform(verts[i + 1]);
		t.v[2] = p_xf.xform(verts[i + 2]);
		t.uv[0] = (i < uvs.size()) ? uvs[i] : Vector2(0, 0);
		t.uv[1] = (i + 1 < uvs.size()) ? uvs[i + 1] : Vector2(1, 0);
		t.uv[2] = (i + 2 < uvs.size()) ? uvs[i + 2] : Vector2(0, 1);
		r_tris.push_back(t);
	}
}

static Transform3D world_xform_3d(Node3D *p_node) {
	Transform3D xf;
	for (Node *n = p_node; n; n = n->get_parent()) {
		if (Node3D *n3 = Object::cast_to<Node3D>(n)) {
			xf = n3->get_transform() * xf;
		}
	}
	return xf;
}

static Transform2D world_xform_2d(Node2D *p_node) {
	Transform2D xf;
	for (Node *n = p_node; n; n = n->get_parent()) {
		if (Node2D *n2 = Object::cast_to<Node2D>(n)) {
			xf = n2->get_transform() * xf;
		}
	}
	return xf;
}

static void try_wav(SceneGather &r_g, const Ref<AudioStream> &p_stream) {
	if (r_g.wav.is_valid() || p_stream.is_null()) {
		return;
	}
	const Ref<AudioStreamWAV> wav = p_stream;
	if (wav.is_valid() && (wav->get_format() == AudioStreamWAV::FORMAT_8_BITS || wav->get_format() == AudioStreamWAV::FORMAT_16_BITS)) {
		r_g.wav = wav;
	}
}

static void walk_scene(Node *p_node, SceneGather &r_g) {
	if (!p_node) {
		return;
	}
	MeshInstance3D *mi = Object::cast_to<MeshInstance3D>(p_node);
	if (mi && mi->get_mesh().is_valid()) {
		const Transform3D xf = world_xform_3d(mi);
		const Ref<Mesh> mesh = mi->get_mesh();
		for (int s = 0; s < mesh->get_surface_count(); s++) {
			Array arr = mesh->surface_get_arrays(s);
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
			emit_tris(xf, verts, uvs, idx, r_g.tris);
			Ref<Material> mat = mi->get_active_material(s);
			if (mat.is_null()) {
				mat = mesh->surface_get_material(s);
			}
			if (r_g.image.is_null()) {
				r_g.image = image_from_material(mat);
			}
		}
	}
	if (Camera3D *cam = Object::cast_to<Camera3D>(p_node)) {
		if (!r_g.cam_any) {
			r_g.cam_any = cam;
		}
		if (cam->is_current() && !r_g.cam_current) {
			r_g.cam_current = cam;
		}
	}
	if (AudioStreamPlayer *ap = Object::cast_to<AudioStreamPlayer>(p_node)) {
		try_wav(r_g, ap->get_stream());
	}
	if (AudioStreamPlayer3D *ap3 = Object::cast_to<AudioStreamPlayer3D>(p_node)) {
		try_wav(r_g, ap3->get_stream());
	}
	if (Sprite2D *spr = Object::cast_to<Sprite2D>(p_node)) {
		if (r_g.sprites.size() < 2) {
			r_g.sprites.push_back(spr);
		}
	}
	for (int i = 0; i < p_node->get_child_count(); i++) {
		walk_scene(p_node->get_child(i), r_g);
	}
}

static Ref<Image> try_load_png(const String &p_path) {
	if (!FileAccess::exists(p_path) && !ResourceLoader::exists(p_path)) {
		return Ref<Image>();
	}
	Ref<Image> loaded;
	loaded.instantiate();
	if (loaded->load(p_path) == OK && !loaded->is_empty()) {
		return loaded;
	}
	Ref<Texture2D> tex = ResourceLoader::load(p_path);
	return image_from_texture(tex);
}

static Ref<Image> load_industrial_image(Vector<CookTri> &r_tris, String &r_src_name) {
	const String glb = "res://assets/Industrial_exterior_v2.glb";
	if (ResourceLoader::exists(glb)) {
		Ref<PackedScene> ps = ResourceLoader::load(glb);
		if (ps.is_valid()) {
			Node *root = ps->instantiate();
			SceneGather g;
			walk_scene(root, g);
			r_tris = g.tris;
			memdelete(root);
			if (g.image.is_valid() && !g.image->is_empty()) {
				r_src_name = glb;
				return g.image;
			}
		}
	}
	const String png = "res://assets/Industrial_exterior_v2_Concrete.png";
	Ref<Image> from_png = try_load_png(png);
	if (from_png.is_valid()) {
		r_src_name = png;
		return from_png;
	}
	Ref<Image> fallback = Image::create_empty(64, 64, false, Image::FORMAT_RGBA8);
	fallback->fill(Color(0.85f, 0.55f, 0.15f));
	r_src_name = "res://generated/fallback.png";
	return fallback;
}

static Ref<Image> fallback_image(String &r_src_name) {
	const String png = "res://assets/Industrial_exterior_v2_Concrete.png";
	Ref<Image> from_png = try_load_png(png);
	if (from_png.is_valid()) {
		if (r_src_name.is_empty()) {
			r_src_name = png;
		}
		return from_png;
	}
	Ref<Image> fallback = Image::create_empty(64, 64, false, Image::FORMAT_RGBA8);
	fallback->fill(Color(0.85f, 0.55f, 0.15f));
	if (r_src_name.is_empty()) {
		r_src_name = "res://generated/fallback.png";
	}
	return fallback;
}

static void pcm_from_wav(const Ref<AudioStreamWAV> &p_wav, Vector<int16_t> &r_pcm, int &r_rate) {
	r_rate = p_wav->get_mix_rate() > 0 ? p_wav->get_mix_rate() : 22050;
	const Vector<uint8_t> data = p_wav->get_data();
	const bool stereo = p_wav->is_stereo();
	if (p_wav->get_format() == AudioStreamWAV::FORMAT_16_BITS) {
		const int16_t *s = reinterpret_cast<const int16_t *>(data.ptr());
		const int frames = data.size() / (int(sizeof(int16_t)) * (stereo ? 2 : 1));
		r_pcm.resize(frames);
		for (int i = 0; i < frames; i++) {
			if (stereo) {
				r_pcm.write[i] = int16_t((int(s[i * 2]) + int(s[i * 2 + 1])) / 2);
			} else {
				r_pcm.write[i] = s[i];
			}
		}
		return;
	}
	const int frames = data.size() / (stereo ? 2 : 1);
	r_pcm.resize(frames);
	for (int i = 0; i < frames; i++) {
		int v;
		if (stereo) {
			v = (int(data[i * 2]) + int(data[i * 2 + 1])) / 2 - 128;
		} else {
			v = int(data[i]) - 128;
		}
		r_pcm.write[i] = int16_t(v << 8);
	}
}

static void write_industrial_sprites(Vector<uint8_t> &spr) {
	auto u16 = [&](uint16_t v) {
		spr.push_back(uint8_t(v & 0xff));
		spr.push_back(uint8_t(v >> 8));
	};
	auto i16 = [&](int16_t v) { u16(uint16_t(v)); };
	u16(2);
	u16(1); // bit0: bounce slot 1
	i16(8);
	i16(8);
	u16(64);
	u16(8);
	spr.push_back(0);
	spr.push_back(0);
	u16(0);
	i16(140);
	i16(100);
	u16(16);
	u16(16);
	spr.push_back(16);
	spr.push_back(16);
	u16(0);
}

struct SpriteCook {
	Vector2 pos;
	Vector2 size;
};

static void write_scene_sprites(const Vector<SpriteCook> &p_sprites, Vector<uint8_t> &spr) {
	auto u16 = [&](uint16_t v) {
		spr.push_back(uint8_t(v & 0xff));
		spr.push_back(uint8_t(v >> 8));
	};
	auto i16 = [&](int16_t v) { u16(uint16_t(v)); };
	u16(uint16_t(p_sprites.size()));
	u16(0);
	for (int i = 0; i < p_sprites.size(); i++) {
		i16(int16_t(CLAMP(int(p_sprites[i].pos.x), -1024, 1024)));
		i16(int16_t(CLAMP(int(p_sprites[i].pos.y), -1024, 1024)));
		const int w = CLAMP(int(p_sprites[i].size.x), 1, 255);
		const int h = CLAMP(int(p_sprites[i].size.y), 1, 255);
		u16(uint16_t(w));
		u16(uint16_t(h));
		spr.push_back(0);
		spr.push_back(0);
		u16(0);
	}
}

static int16_t clamp_i16(float v) {
	return int16_t(CLAMP(int(v), -32767, 32767));
}

Error cook_project(const String &p_export_dir, CookResult &r_out) {
	Ref<DirAccess> da = DirAccess::create(DirAccess::ACCESS_FILESYSTEM);
	const Error mk = da->make_dir_recursive(p_export_dir);
	ERR_FAIL_COND_V(mk != OK && mk != ERR_ALREADY_EXISTS, mk);

	SceneGather gathered;
	String src_name;
	const String main_scene = ProjectSettings::get_singleton() ? String(ProjectSettings::get_singleton()->get("application/run/main_scene")) : String();
	Node *root = nullptr;
	if (!main_scene.is_empty() && ResourceLoader::exists(main_scene)) {
		Ref<PackedScene> ps = ResourceLoader::load(main_scene);
		if (ps.is_valid()) {
			root = ps->instantiate();
			walk_scene(root, gathered);
			if (!gathered.tris.is_empty()) {
				src_name = main_scene;
			}
		}
	}

	Vector<CookTri> src_tris = gathered.tris;
	Ref<Image> img = gathered.image;
	if (src_tris.is_empty()) {
		img = load_industrial_image(src_tris, src_name);
	} else if (img.is_null() || img->is_empty()) {
		img = fallback_image(src_name);
	}

	struct CamCook {
		bool valid = false;
		Vector3 pos;
		float fov = 75.0f;
	} cam_cook;
	if (Camera3D *cam = gathered.cam_current ? gathered.cam_current : gathered.cam_any) {
		cam_cook.valid = true;
		cam_cook.pos = world_xform_3d(cam).origin;
		cam_cook.fov = float(cam->get_fov());
	}
	Vector<SpriteCook> sprite_cooks;
	for (int i = 0; i < gathered.sprites.size(); i++) {
		Sprite2D *s = gathered.sprites[i];
		const Rect2 r = s->get_rect();
		SpriteCook sc;
		sc.pos = world_xform_2d(s).xform(r.position);
		sc.size = r.size;
		sprite_cooks.push_back(sc);
	}
	if (root) {
		memdelete(root);
		root = nullptr;
	}

	TimBlob tim;
	const Error terr = write_tim4(img, tim);
	if (terr != OK) {
		return terr;
	}

	MeshBlob mesh;
	if (!src_tris.is_empty()) {
		const Error merr = write_mesh_svector(src_tris, 600, mesh);
		if (merr != OK) {
			return merr;
		}
	}
	VagBlob vag;
	if (gathered.wav.is_valid()) {
		Vector<int16_t> pcm;
		int rate = 22050;
		pcm_from_wav(gathered.wav, pcm, rate);
		const Error verr = write_vag_pcm(pcm, rate, kSpuLimit, vag);
		if (verr != OK) {
			return verr;
		}
	} else {
		const Error verr = write_vag_beep(vag);
		if (verr != OK) {
			return verr;
		}
	}

	r_out.tim_path = p_export_dir.path_join("TEX00.TIM");
	r_out.names_path = p_export_dir.path_join("NAMES.MAP");
	r_out.budget_path = p_export_dir.path_join("BUDGET.txt");

	Ref<FileAccess> tf = FileAccess::open(r_out.tim_path, FileAccess::WRITE);
	ERR_FAIL_COND_V(tf.is_null(), ERR_CANT_CREATE);
	tf->store_buffer(tim.bytes);

	if (!mesh.bytes.is_empty()) {
		r_out.mesh_path = p_export_dir.path_join("MESH00.bin");
		Ref<FileAccess> mf = FileAccess::open(r_out.mesh_path, FileAccess::WRITE);
		ERR_FAIL_COND_V(mf.is_null(), ERR_CANT_CREATE);
		mf->store_buffer(mesh.bytes);
	}
	r_out.vag_path = p_export_dir.path_join("SFX00.VAG");
	Ref<FileAccess> vf = FileAccess::open(r_out.vag_path, FileAccess::WRITE);
	ERR_FAIL_COND_V(vf.is_null(), ERR_CANT_CREATE);
	vf->store_buffer(vag.bytes);

	r_out.sprite_path = p_export_dir.path_join("SPRITE00.bin");
	{
		Vector<uint8_t> spr;
		if (!sprite_cooks.is_empty()) {
			write_scene_sprites(sprite_cooks, spr);
		} else {
			write_industrial_sprites(spr);
		}
		Ref<FileAccess> sf = FileAccess::open(r_out.sprite_path, FileAccess::WRITE);
		ERR_FAIL_COND_V(sf.is_null(), ERR_CANT_CREATE);
		sf->store_buffer(spr);
	}
	r_out.fmv_path = p_export_dir.path_join("FMV00.MAP");
	{
		Ref<FileAccess> ff = FileAccess::open(r_out.fmv_path, FileAccess::WRITE);
		ERR_FAIL_COND_V(ff.is_null(), ERR_CANT_CREATE);
		ff->store_string("FMV: guest DecDCTReset (psxpress/MDEC). Encode STR/XA via blazium-toolchain ps1 fmv only.\n");
	}

	ScriptIR ir;
	const Error serr = cook_scripts(p_export_dir, ir);
	if (serr != OK) {
		return serr;
	}
	for (int i = 0; i < ir.rejected.size(); i++) {
		if (ir.rejected[i].contains("spin.gd") || ir.rejected[i].contains("spin.luau")) {
			r_out.report = "Rejected allow-listed demo script: " + ir.rejected[i];
			return ERR_UNCONFIGURED;
		}
	}

	if (cam_cook.valid) {
		const Vector3 rel = cam_cook.pos - mesh.center;
		const float s = mesh.scale > 0.0f ? mesh.scale : 1.0f;
		ir.pos_x = clamp_i16(-rel.x * s);
		ir.pos_y = clamp_i16(rel.y * s);
		ir.pos_z = clamp_i16(rel.z * s);
		if (ir.pos_z <= 0) {
			ir.pos_z = 512;
		}
		const float fov = Math::deg_to_rad(cam_cook.fov);
		const float half = MAX(0.05f, Math::tan(fov * 0.5f));
		ir.geom_screen = int16_t(CLAMP(int(120.0f / half), 80, 256));
	}

	const Error werr = write_script_ir(p_export_dir, ir);
	if (werr != OK) {
		return werr;
	}
	r_out.script_path = p_export_dir.path_join("SCRIPT.IR");

	Ref<FileAccess> nf = FileAccess::open(r_out.names_path, FileAccess::WRITE);
	ERR_FAIL_COND_V(nf.is_null(), ERR_CANT_CREATE);
	nf->store_line(src_name + " -> TEX00.TIM MESH00.bin SFX00.VAG SPRITE00.bin FMV00.MAP SCRIPT.IR");

	r_out.texels = tim.pixel_w * tim.pixel_h;
	r_out.tris = src_tris.size();
	r_out.cooked_tris = mesh.tri_count;
	r_out.vram_bytes = tim.vram_bytes;
	const int ram_blob = tim.bytes.size() + mesh.bytes.size() + vag.bytes.size();
	r_out.over_budget = tim.vram_bytes > kVramLimit || ram_blob > kRamLimit || vag.bytes.size() > kSpuLimit;

	r_out.report = vformat("PS1 cook budget: TIM %dx%d 4-bit (%d bytes) tris=%d cooked=%d VRAM=%d/%d RAM_blob=%d/%d SPU=%d/%d src=%s%s",
			tim.pixel_w, tim.pixel_h, tim.bytes.size(), src_tris.size(), mesh.tri_count, tim.vram_bytes, kVramLimit, ram_blob, kRamLimit, vag.bytes.size(), kSpuLimit,
			src_name, r_out.over_budget ? " OVER BUDGET" : "");

	Ref<FileAccess> bf = FileAccess::open(r_out.budget_path, FileAccess::WRITE);
	ERR_FAIL_COND_V(bf.is_null(), ERR_CANT_CREATE);
	bf->store_string(r_out.report + "\n");

	if (r_out.over_budget) {
		return ERR_OUT_OF_MEMORY;
	}
	return OK;
}

} // namespace ps1_cook
