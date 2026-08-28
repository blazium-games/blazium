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
#include "str_writer.h"
#include "tim_writer.h"
#include "vag_writer.h"
#include "xa_writer.h"

#include "core/config/project_settings.h"
#include "core/io/image.h"
#include "core/io/dir_access.h"
#include "core/io/file_access.h"
#include "core/io/resource_loader.h"
#include "core/math/math_funcs.h"
#include "core/templates/hash_set.h"
#include "core/typedefs.h"
#include "scene/2d/node_2d.h"
#include "scene/2d/sprite_2d.h"
#include "scene/3d/audio_stream_player_3d.h"
#include "scene/3d/camera_3d.h"
#include "scene/3d/light_3d.h"
#include "scene/gui/video_stream_player.h"
#include "scene/3d/mesh_instance_3d.h"
#include "scene/3d/node_3d.h"
#include "scene/animation/animation_player.h"
#include "scene/audio/audio_stream_player.h"
#include "scene/resources/audio_stream_wav.h"
#include "scene/resources/material.h"
#include "scene/resources/mesh.h"
#include "scene/resources/packed_scene.h"
#include "core/object/script_language.h"
#include "scene/resources/texture.h"

namespace ps1_cook {

static const int kVramLimit = 1024 * 1024;
static const int kRamLimit = 2 * 1024 * 1024;
static const int kSpuLimit = 508 * 1024;

struct CookLight {
	Vector3 dir;
	Color color;
};

struct SceneGather {
	Vector<CookTri> tris;
	Vector<Ref<Image>> images;
	Vector<String> image_names;
	Vector<String> ignored_textures;
	Camera3D *cam_current = nullptr;
	Camera3D *cam_any = nullptr;
	int camera_count = 0;
	Ref<AudioStreamWAV> wav;
	bool had_non_wav_audio = false;
	Vector<Sprite2D *> sprites;
	int sprite_count = 0;
	Vector<CookLight> lights;
	int light_count = 0;
	int animation_player_count = 0;
	HashSet<String> uncooked_types;
	Vector<String> attached_scripts;
	VideoStreamPlayer *video = nullptr;
};

static const char *kAllowed[] = {
	"Node", "Node3D", "MeshInstance3D", "Camera3D", "AnimationPlayer",
	"Sprite2D", "Node2D", "AudioStreamPlayer", "AudioStreamPlayer3D",
	"DirectionalLight3D", "OmniLight3D", "SpotLight3D", "Light3D",
	"VideoStreamPlayer", "Control", "CanvasItem"
};

static bool type_allowed(const String &p_cls) {
	for (const char *a : kAllowed) {
		if (p_cls == a) {
			return true;
		}
	}
	return false;
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
		if (loaded->load(path) == OK && !loaded->is_empty()) {
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
	}
	return image_from_texture(tex);
}

static uint8_t register_image(SceneGather &r_g, const Ref<Image> &p_img, const String &p_name) {
	if (p_img.is_null() || p_img->is_empty()) {
		return 0;
	}
	if (!p_name.is_empty()) {
		for (int i = 0; i < r_g.image_names.size(); i++) {
			if (r_g.image_names[i] == p_name) {
				return uint8_t(i);
			}
		}
	}
	if (r_g.images.size() < kMaxTims) {
		r_g.images.push_back(p_img);
		r_g.image_names.push_back(p_name.is_empty() ? vformat("tex_%d", r_g.images.size()) : p_name);
		return uint8_t(r_g.images.size() - 1);
	}
	if (!p_name.is_empty()) {
		bool seen = false;
		for (int i = 0; i < r_g.ignored_textures.size(); i++) {
			if (r_g.ignored_textures[i] == p_name) {
				seen = true;
				break;
			}
		}
		if (!seen) {
			r_g.ignored_textures.push_back(p_name);
		}
	}
	return 0;
}

static void emit_tris(const Transform3D &p_xf, const PackedVector3Array &verts, const PackedVector2Array &uvs, const PackedInt32Array &idx, uint8_t p_tex, Vector<CookTri> &r_tris) {
	if (idx.size() >= 3) {
		for (int i = 0; i + 2 < idx.size(); i += 3) {
			CookTri t;
			t.tex_slot = p_tex;
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
		t.tex_slot = p_tex;
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
	if (p_stream.is_null()) {
		return;
	}
	const Ref<AudioStreamWAV> wav = p_stream;
	if (wav.is_valid() && (wav->get_format() == AudioStreamWAV::FORMAT_8_BITS || wav->get_format() == AudioStreamWAV::FORMAT_16_BITS)) {
		if (r_g.wav.is_null()) {
			r_g.wav = wav;
		}
		return;
	}
	r_g.had_non_wav_audio = true;
}

static void add_light(SceneGather &r_g, const Vector3 &p_dir, const Color &p_color) {
	r_g.light_count++;
	if (r_g.lights.size() >= kMaxLights) {
		return;
	}
	CookLight l;
	Vector3 d = p_dir;
	if (d.length_squared() < 0.0001f) {
		d = Vector3(0, -1, -1);
	}
	l.dir = d.normalized();
	l.color = p_color;
	r_g.lights.push_back(l);
}

static void walk_scene(Node *p_node, SceneGather &r_g) {
	if (!p_node) {
		return;
	}
	const String cls = p_node->get_class();
	if (!type_allowed(cls)) {
		r_g.uncooked_types.insert(cls);
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
			Ref<Material> mat = mi->get_active_material(s);
			if (mat.is_null()) {
				mat = mesh->surface_get_material(s);
			}
			String tex_name;
			const Ref<Image> img = image_from_material(mat, tex_name);
			const uint8_t slot = register_image(r_g, img, tex_name);
			emit_tris(xf, verts, uvs, idx, slot, r_g.tris);
		}
	}
	if (Camera3D *cam = Object::cast_to<Camera3D>(p_node)) {
		r_g.camera_count++;
		if (!r_g.cam_any) {
			r_g.cam_any = cam;
		}
		if (cam->is_current() && !r_g.cam_current) {
			r_g.cam_current = cam;
		}
	}
	if (DirectionalLight3D *dl = Object::cast_to<DirectionalLight3D>(p_node)) {
		const Transform3D xf = world_xform_3d(dl);
		const Vector3 dir = -xf.basis.get_column(Vector3::AXIS_Z);
		Color c = dl->get_color();
		c *= float(dl->get_param(Light3D::PARAM_ENERGY));
		add_light(r_g, dir, c);
	} else if (OmniLight3D *ol = Object::cast_to<OmniLight3D>(p_node)) {
		const Transform3D xf = world_xform_3d(ol);
		Color c = ol->get_color();
		c *= float(ol->get_param(Light3D::PARAM_ENERGY));
		add_light(r_g, xf.origin, c);
	} else if (SpotLight3D *sl = Object::cast_to<SpotLight3D>(p_node)) {
		const Transform3D xf = world_xform_3d(sl);
		const Vector3 dir = -xf.basis.get_column(Vector3::AXIS_Z);
		Color c = sl->get_color();
		c *= float(sl->get_param(Light3D::PARAM_ENERGY));
		add_light(r_g, dir, c);
	} else if (Object::cast_to<Light3D>(p_node)) {
		r_g.light_count++;
	}
	if (AudioStreamPlayer *ap = Object::cast_to<AudioStreamPlayer>(p_node)) {
		try_wav(r_g, ap->get_stream());
	}
	if (AudioStreamPlayer3D *ap3 = Object::cast_to<AudioStreamPlayer3D>(p_node)) {
		try_wav(r_g, ap3->get_stream());
	}
	if (Sprite2D *spr = Object::cast_to<Sprite2D>(p_node)) {
		r_g.sprite_count++;
		if (r_g.sprites.size() < kMaxSprites) {
			r_g.sprites.push_back(spr);
		}
	}
	if (VideoStreamPlayer *vp = Object::cast_to<VideoStreamPlayer>(p_node)) {
		if (!r_g.video) {
			r_g.video = vp;
		}
	}
	if (p_node->get_script()) {
		const Ref<Script> sc = p_node->get_script();
		if (sc.is_valid() && !sc->get_path().is_empty()) {
			r_g.attached_scripts.push_back(sc->get_path());
		}
	}
	if (Object::cast_to<AnimationPlayer>(p_node)) {
		r_g.animation_player_count++;
	}
	for (int i = 0; i < p_node->get_child_count(); i++) {
		walk_scene(p_node->get_child(i), r_g);
	}
}

static Ref<Image> try_load_png(const String &p_path) {
	if (ResourceLoader::exists(p_path)) {
		Ref<Texture2D> tex = ResourceLoader::load(p_path);
		Ref<Image> from_tex = image_from_texture(tex);
		if (from_tex.is_valid()) {
			return from_tex;
		}
		Ref<Image> as_img = ResourceLoader::load(p_path);
		if (as_img.is_valid() && !as_img->is_empty()) {
			return as_img;
		}
	}
	if (!FileAccess::exists(p_path)) {
		return Ref<Image>();
	}
	Ref<Image> loaded;
	loaded.instantiate();
	if (loaded->load(p_path) == OK && !loaded->is_empty()) {
		return loaded;
	}
	return Ref<Image>();
}

static Ref<Image> load_industrial_image(Vector<CookTri> &r_tris, Vector<Ref<Image>> &r_images, Vector<String> &r_names, String &r_src_name) {
	const String glb = "res://assets/Industrial_exterior_v2.glb";
	if (ResourceLoader::exists(glb)) {
		Ref<PackedScene> ps = ResourceLoader::load(glb);
		if (ps.is_valid()) {
			Node *root = ps->instantiate();
			SceneGather g;
			walk_scene(root, g);
			r_tris = g.tris;
			r_images = g.images;
			r_names = g.image_names;
			memdelete(root);
			if (!r_images.is_empty()) {
				r_src_name = glb;
				return r_images[0];
			}
		}
	}
	const String png = "res://assets/Industrial_exterior_v2_Concrete.png";
	Ref<Image> from_png = try_load_png(png);
	if (from_png.is_valid()) {
		r_src_name = png;
		r_images.push_back(from_png);
		r_names.push_back(png);
		return from_png;
	}
	Ref<Image> fallback = Image::create_empty(64, 64, false, Image::FORMAT_RGBA8);
	fallback->fill(Color(0.85f, 0.55f, 0.15f));
	r_src_name = "res://generated/fallback.png";
	r_images.push_back(fallback);
	r_names.push_back(r_src_name);
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
	u16(1);
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
	Vector2 uv;
	uint8_t tex_slot = 0;
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
		spr.push_back(uint8_t(CLAMP(int(p_sprites[i].uv.x), 0, 255)));
		spr.push_back(uint8_t(CLAMP(int(p_sprites[i].uv.y), 0, 255)));
		u16(uint16_t(p_sprites[i].tex_slot & 3));
	}
}

static int16_t clamp_i16(float v) {
	return int16_t(CLAMP(int(v), -32767, 32767));
}

static void write_warnings_file(const String &p_path, const CookResult &p_out) {
	Ref<FileAccess> f = FileAccess::open(p_path, FileAccess::WRITE);
	if (f.is_null()) {
		return;
	}
	for (int i = 0; i < p_out.errors.size(); i++) {
		f->store_line("ERROR: " + p_out.errors[i]);
	}
	for (int i = 0; i < p_out.warnings.size(); i++) {
		f->store_line("WARNING: " + p_out.warnings[i]);
	}
	for (int i = 0; i < p_out.infos.size(); i++) {
		f->store_line("INFO: " + p_out.infos[i]);
	}
}

Error cook_project(const String &p_export_dir, CookResult &r_out, const CookOptions &p_opts) {
	Ref<DirAccess> da = DirAccess::create(DirAccess::ACCESS_FILESYSTEM);
	const Error mk = da->make_dir_recursive(p_export_dir);
	ERR_FAIL_COND_V(mk != OK && mk != ERR_ALREADY_EXISTS, mk);

	const String main_scene = ProjectSettings::get_singleton() ? String(ProjectSettings::get_singleton()->get("application/run/main_scene")) : String();
	if (main_scene.is_empty()) {
		r_out.errors.push_back("PS1 export requires application/run/main_scene.");
		r_out.report = r_out.errors[0];
		return FAILED;
	}

	SceneGather gathered;
	String src_name;
	Node *root = nullptr;
	bool used_industrial = false;
	if (ResourceLoader::exists(main_scene)) {
		Ref<PackedScene> ps = ResourceLoader::load(main_scene);
		if (ps.is_valid()) {
			root = ps->instantiate();
			walk_scene(root, gathered);
			if (!gathered.tris.is_empty()) {
				src_name = main_scene;
			}
		}
	} else {
		r_out.errors.push_back("main_scene does not exist: " + main_scene);
		r_out.report = r_out.errors[0];
		return FAILED;
	}

	Vector<CookTri> src_tris = gathered.tris;
	Vector<Ref<Image>> images = gathered.images;
	Vector<String> image_names = gathered.image_names;
	if (src_tris.is_empty()) {
		images.clear();
		image_names.clear();
		load_industrial_image(src_tris, images, image_names, src_name);
		used_industrial = true;
		r_out.warnings.push_back("main_scene has no MeshInstance3D; used Industrial GLB/PNG fallback.");
	} else if (images.is_empty()) {
		const String png = "res://assets/Industrial_exterior_v2_Concrete.png";
		Ref<Image> from_png = try_load_png(png);
		if (from_png.is_valid()) {
			images.push_back(from_png);
			image_names.push_back(png);
		} else {
			Ref<Image> fallback = Image::create_empty(64, 64, false, Image::FORMAT_RGBA8);
			fallback->fill(Color(0.85f, 0.55f, 0.15f));
			images.push_back(fallback);
			image_names.push_back("res://generated/fallback.png");
		}
	}

	struct CamCook {
		bool valid = false;
		Vector3 pos;
		Vector3 euler;
		float fov = 75.0f;
	} cam_cook;
	if (Camera3D *cam = gathered.cam_current ? gathered.cam_current : gathered.cam_any) {
		cam_cook.valid = true;
		const Transform3D xf = world_xform_3d(cam);
		cam_cook.pos = xf.origin;
		cam_cook.euler = xf.basis.get_euler();
		cam_cook.fov = float(cam->get_fov());
	}
	Vector<SpriteCook> sprite_cooks;
	for (int i = 0; i < gathered.sprites.size(); i++) {
		Sprite2D *s = gathered.sprites[i];
		const Rect2 r = s->get_rect();
		SpriteCook sc;
		sc.pos = world_xform_2d(s).xform(r.position);
		sc.size = r.size;
		sc.uv = Vector2(0, 0);
		String tex_name;
		const Ref<Image> simg = image_from_texture(s->get_texture());
		if (s->is_region_enabled()) {
			const Rect2 rr = s->get_region_rect();
			sc.uv = rr.position;
		}
		sc.tex_slot = register_image(gathered, simg, s->get_texture().is_valid() ? s->get_texture()->get_path() : tex_name);
		sprite_cooks.push_back(sc);
	}
	if (!sprite_cooks.is_empty() && !used_industrial) {
		images = gathered.images;
		image_names = gathered.image_names;
	}
	Vector<CookLight> lights = gathered.lights;
	if (root) {
		memdelete(root);
		root = nullptr;
	}

	if (src_tris.size() > kMaxTris) {
		r_out.warnings.push_back(vformat("kept first %d of %d triangles (dropped %d). Reduce mesh density.", kMaxTris, src_tris.size(), src_tris.size() - kMaxTris));
	}
	if (!gathered.ignored_textures.is_empty()) {
		r_out.warnings.push_back("used first 4 albedo textures, ignored: " + String(", ").join(gathered.ignored_textures));
	}
	if (gathered.sprite_count > kMaxSprites) {
		r_out.warnings.push_back(vformat("kept first %d of %d Sprite2D nodes.", kMaxSprites, gathered.sprite_count));
	}
	if (gathered.light_count > kMaxLights) {
		r_out.warnings.push_back(vformat("kept first %d of %d lights (GTE has 3).", kMaxLights, gathered.light_count));
	}
	if (gathered.camera_count > 1) {
		r_out.warnings.push_back(vformat("used one Camera3D; ignored %d extra camera(s).", gathered.camera_count - 1));
	}
	if (gathered.animation_player_count > 0) {
		r_out.warnings.push_back("AnimationPlayer is not cooked (no skeletal/anim guest).");
	}
	if (!gathered.uncooked_types.is_empty()) {
		PackedStringArray types;
		for (const String &t : gathered.uncooked_types) {
			types.push_back(t);
		}
		r_out.warnings.push_back("uncooked node types: " + String(", ").join(types));
	}
	if (gathered.had_non_wav_audio) {
		r_out.warnings.push_back("OGG/MP3/other audio is not cooked; VAG is WAV-only or a beep.");
	}
	const int display_w = p_opts.width > 0 ? p_opts.width : 320;
	const int display_h = p_opts.region == 1 ? 256 : 240;
	r_out.display_w = display_w;
	r_out.display_h = display_h;
	if (display_w == 640 && display_h == 256) {
		r_out.warnings.push_back("PAL 640 uses full VRAM height for framebuffers; CLUTs sit at x=640.");
	}

	Vector<TimBlob> tims;
	int vram_total = 0;
	const int clut_x = (p_opts.region == 1 || display_w >= 640) ? 640 : 0;
	for (int i = 0; i < images.size() && i < kMaxTims; i++) {
		TimBlob one;
		int dest_x = 640 + 64 * i;
		int dest_y = 0;
		if (display_w >= 640) {
			const int below = ((display_h * 2) + 63) & ~63;
			dest_x = 0;
			dest_y = below + 64 * i;
			if (dest_y + 64 > 512) {
				dest_x = 640 + 64 * i;
				dest_y = 0;
			}
		}
		if (dest_x < display_w && dest_y < display_h * 2) {
			r_out.errors.push_back(vformat("VRAM clash: TIM %d dest (%d,%d) overlaps %dx%d double-buffer.", i, dest_x, dest_y, display_w, display_h));
			r_out.report = r_out.errors[r_out.errors.size() - 1];
			return ERR_OUT_OF_MEMORY;
		}
		if (dest_x + 64 > 1024 || dest_y + 64 > 512) {
			r_out.errors.push_back(vformat("VRAM clash: TIM %d dest (%d,%d) exceeds 1024x512.", i, dest_x, dest_y));
			r_out.report = r_out.errors[r_out.errors.size() - 1];
			return ERR_OUT_OF_MEMORY;
		}
		const int clut_y = 480 - i;
		const Error terr = write_tim4(images[i], dest_x, dest_y, clut_x, clut_y, one);
		if (terr != OK) {
			r_out.errors.push_back("TIM cook failed for " + image_names[i]);
			r_out.report = r_out.errors[r_out.errors.size() - 1];
			return terr;
		}
		tims.push_back(one);
		vram_total += one.vram_bytes;
	}
	TimBlob packed;
	if (!tims.is_empty()) {
		const Error perr = write_tpak(tims, packed);
		if (perr != OK) {
			r_out.errors.push_back("TPAK pack failed.");
			r_out.report = r_out.errors[0];
			return perr;
		}
	}

	MeshBlob mesh;
	if (!src_tris.is_empty()) {
		const Error merr = write_mesh_svector(src_tris, kMaxTris, mesh);
		if (merr != OK) {
			r_out.errors.push_back("Mesh cook failed.");
			r_out.report = r_out.errors[r_out.errors.size() - 1];
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
			r_out.errors.push_back("VAG cook failed.");
			return verr;
		}
	} else {
		const Error verr = write_vag_beep(vag);
		if (verr != OK) {
			r_out.errors.push_back("VAG beep failed.");
			return verr;
		}
		if (!gathered.had_non_wav_audio) {
			r_out.infos.push_back("No AudioStreamWAV; cooked a short beep.");
		}
	}

	r_out.tim_path = p_export_dir.path_join("TEX00.TIM");
	r_out.names_path = p_export_dir.path_join("NAMES.MAP");
	r_out.budget_path = p_export_dir.path_join("BUDGET.txt");
	r_out.warnings_path = p_export_dir.path_join("WARNINGS.txt");

	Ref<FileAccess> tf = FileAccess::open(r_out.tim_path, FileAccess::WRITE);
	ERR_FAIL_COND_V(tf.is_null(), ERR_CANT_CREATE);
	tf->store_buffer(packed.bytes);

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
	{
		Vector<Ref<Image>> fmv_frames;
		Ref<DirAccess> fmv_dir = DirAccess::open("res://fmv");
		if (fmv_dir.is_valid()) {
			fmv_dir->list_dir_begin();
			for (String n = fmv_dir->get_next(); !n.is_empty(); n = fmv_dir->get_next()) {
				if (n.ends_with(".png") || n.ends_with(".jpg")) {
					Ref<Image> img = try_load_png(String("res://fmv").path_join(n));
					if (img.is_valid()) {
						fmv_frames.push_back(img);
					}
				}
			}
		}
		if (fmv_frames.is_empty() && gathered.video) {
			r_out.warnings.push_back("VideoStreamPlayer present but compressed video is not decoded; add res://fmv/*.png.");
		}
		r_out.fmv_path = p_export_dir.path_join("FMV00.MAP");
		Ref<FileAccess> ff = FileAccess::open(r_out.fmv_path, FileAccess::WRITE);
		ERR_FAIL_COND_V(ff.is_null(), ERR_CANT_CREATE);
		if (fmv_frames.is_empty()) {
			ff->store_string("FMV: no VideoStreamPlayer frames and no res://fmv images; skipped.\n");
			r_out.infos.push_back("FMV skipped (no source).");
		} else {
			StrBlob str;
			const Error serr = write_str_frames(fmv_frames, str);
			if (serr != OK) {
				r_out.errors.push_back("STR encode failed.");
				ff->store_string("FMV: encode failed.\n");
				return serr;
			}
			r_out.str_path = p_export_dir.path_join("FMV00.STR");
			Ref<FileAccess> sf = FileAccess::open(r_out.str_path, FileAccess::WRITE);
			ERR_FAIL_COND_V(sf.is_null(), ERR_CANT_CREATE);
			sf->store_buffer(str.bytes);
			ff->store_string(vformat("FMV: MIT STR %d frame(s) %dx%d (%d bytes)\n", str.frames, str.width, str.height, str.bytes.size()));
			r_out.infos.push_back(vformat("STR %d frames %d bytes", str.frames, str.bytes.size()));
			if (gathered.wav.is_valid()) {
				Vector<int16_t> pcm;
				int rate = 22050;
				pcm_from_wav(gathered.wav, pcm, rate);
				XaBlob xa;
				if (write_xa_pcm(pcm, rate, xa) == OK) {
					r_out.xa_path = p_export_dir.path_join("FMV00.XA");
					Ref<FileAccess> xf = FileAccess::open(r_out.xa_path, FileAccess::WRITE);
					if (xf.is_valid()) {
						xf->store_buffer(xa.bytes);
						r_out.infos.push_back(vformat("XA %d bytes", xa.bytes.size()));
					}
				}
			}
		}
	}

	ScriptIR ir;
	const Error serr = cook_scripts(p_export_dir, ir);
	if (serr != OK) {
		r_out.errors.push_back("Script cook failed.");
		return serr;
	}
	for (int i = 0; i < ir.rejected.size(); i++) {
		const String &rej = ir.rejected[i];
		const bool fixture = rej.contains("illegal_") || rej.contains("/tests/") || rej.contains("test_") || rej.contains("run_tests.gd");
		if (fixture) {
			continue;
		}
		r_out.errors.push_back("Rejected game script: " + rej);
		r_out.report = r_out.errors[r_out.errors.size() - 1];
		return ERR_UNCONFIGURED;
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
		ir.cam_rx = clamp_i16(cam_cook.euler.x * 4096.0f / float(Math::TAU));
		ir.cam_ry = clamp_i16(cam_cook.euler.y * 4096.0f / float(Math::TAU));
		ir.cam_rz = clamp_i16(cam_cook.euler.z * 4096.0f / float(Math::TAU));
	}
	ir.display_w = uint16_t(display_w);
	ir.display_h = uint16_t(display_h);
	ir.region = uint8_t(p_opts.region == 1 ? 1 : 0);
	ir.light_count = uint16_t(lights.size());
	for (int i = 0; i < lights.size() && i < kMaxLights; i++) {
		ir.lights[i].dx = clamp_i16(lights[i].dir.x * 4096.0f);
		ir.lights[i].dy = clamp_i16(-lights[i].dir.y * 4096.0f);
		ir.lights[i].dz = clamp_i16(lights[i].dir.z * 4096.0f);
		ir.lights[i].r = uint8_t(CLAMP(int(lights[i].color.r * 255.0f), 0, 255));
		ir.lights[i].g = uint8_t(CLAMP(int(lights[i].color.g * 255.0f), 0, 255));
		ir.lights[i].b = uint8_t(CLAMP(int(lights[i].color.b * 255.0f), 0, 255));
	}
	if (lights.is_empty()) {
		r_out.infos.push_back("No scene lights; guest keeps the built-in 3-light demo matrix.");
	}

	const Error werr = write_script_ir(p_export_dir, ir);
	if (werr != OK) {
		r_out.errors.push_back("SCRIPT.IR write failed.");
		return werr;
	}
	r_out.script_path = p_export_dir.path_join("SCRIPT.IR");
	r_out.gdbc_path = ir.gdbc_path;
	r_out.luau_path = ir.luau_path;

	Ref<FileAccess> nf = FileAccess::open(r_out.names_path, FileAccess::WRITE);
	ERR_FAIL_COND_V(nf.is_null(), ERR_CANT_CREATE);
	nf->store_line(src_name + " -> TEX00.TIM MESH00.bin SFX00.VAG SPRITE00.bin FMV00.MAP SCRIPT.IR SCRIPT.GD.BC SCRIPT.LU.BC");

	r_out.texels = packed.pixel_w * MAX(1, packed.pixel_h);
	r_out.tris = src_tris.size();
	r_out.cooked_tris = mesh.tri_count;
	r_out.vram_bytes = packed.vram_bytes;
	int extra_blob = 0;
	if (!r_out.gdbc_path.is_empty() && FileAccess::exists(r_out.gdbc_path)) {
		extra_blob += FileAccess::get_file_as_bytes(r_out.gdbc_path).size();
	}
	if (!r_out.luau_path.is_empty() && FileAccess::exists(r_out.luau_path)) {
		extra_blob += FileAccess::get_file_as_bytes(r_out.luau_path).size();
	}
	if (!r_out.str_path.is_empty() && FileAccess::exists(r_out.str_path)) {
		extra_blob += FileAccess::get_file_as_bytes(r_out.str_path).size();
	}
	if (!r_out.xa_path.is_empty() && FileAccess::exists(r_out.xa_path)) {
		extra_blob += FileAccess::get_file_as_bytes(r_out.xa_path).size();
	}
	const int ram_blob = packed.bytes.size() + mesh.bytes.size() + vag.bytes.size() + extra_blob;
	r_out.over_budget = packed.vram_bytes > kVramLimit || ram_blob > kRamLimit || vag.bytes.size() > kSpuLimit;

	r_out.infos.push_back(vformat("guest_abi=%d src=%s tims=%d tris=%d cooked=%d lights=%d sprites=%d display=%s %dx%d gd=%d luau=%d%s",
			kCookABI, src_name, tims.size(), src_tris.size(), mesh.tri_count, lights.size(), sprite_cooks.size(),
			p_opts.region == 1 ? "PAL" : "NTSC", display_w, display_h, ir.gd_count, ir.luau_count,
			used_industrial ? " industrial_fallback" : ""));

	r_out.report = vformat("PS1 cook budget: TPAK %d TIM(s) (%d bytes) tris=%d cooked=%d VRAM=%d/%d RAM_blob=%d/%d SPU=%d/%d src=%s%s",
			tims.size(), packed.bytes.size(), src_tris.size(), mesh.tri_count, packed.vram_bytes, kVramLimit, ram_blob, kRamLimit, vag.bytes.size(), kSpuLimit,
			src_name, r_out.over_budget ? " OVER BUDGET" : "");

	Ref<FileAccess> bf = FileAccess::open(r_out.budget_path, FileAccess::WRITE);
	ERR_FAIL_COND_V(bf.is_null(), ERR_CANT_CREATE);
	bf->store_string(r_out.report + "\n");

	if (r_out.over_budget) {
		r_out.errors.push_back("Cook exceeded VRAM, RAM, or SPU budget.");
		write_warnings_file(r_out.warnings_path, r_out);
		return ERR_OUT_OF_MEMORY;
	}
	if (!r_out.errors.is_empty()) {
		write_warnings_file(r_out.warnings_path, r_out);
		return FAILED;
	}
	write_warnings_file(r_out.warnings_path, r_out);
	return OK;
}

} // namespace ps1_cook
