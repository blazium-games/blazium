/**************************************************************************/
/*  cooker.cpp                                                            */
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

#include "cooker.h"

#include "adpcm_writer.h"
#include "gdir_compile.h"
#include "gs_tex_writer.h"
#include "mesh_writer.h"
#include "script_cook.h"

#include "core/config/project_settings.h"
#include "core/io/dir_access.h"
#include "core/io/file_access.h"
#include "core/io/image.h"
#include "core/io/resource_loader.h"
#include "core/math/quaternion.h"
#include "core/object/script_language.h"
#include "core/templates/hash_set.h"
#include "core/variant/variant.h"
#include "scene/3d/camera_3d.h"
#include "scene/3d/importer_mesh_instance_3d.h"
#include "scene/3d/light_3d.h"
#include "scene/3d/mesh_instance_3d.h"
#include "scene/3d/multimesh_instance_3d.h"
#include "scene/3d/node_3d.h"
#include "scene/3d/path_3d.h"
#include "scene/3d/world_environment.h"
#include "scene/animation/animation_player.h"
#include "scene/gui/button.h"
#include "scene/gui/color_rect.h"
#include "scene/gui/control.h"
#include "scene/gui/label.h"
#include "scene/gui/texture_button.h"
#include "scene/main/canvas_layer.h"
#include "scene/main/node.h"
#include "scene/resources/3d/importer_mesh.h"
#include "scene/resources/3d/sky_material.h"
#include "scene/resources/animation.h"
#include "scene/resources/audio_stream_wav.h"
#include "scene/resources/curve.h"
#include "scene/resources/environment.h"
#include "scene/resources/material.h"
#include "scene/resources/mesh.h"
#include "scene/resources/packed_scene.h"
#include "scene/resources/sky.h"
#include "scene/resources/style_box_flat.h"
#include "scene/resources/texture.h"

#include <cstring>

namespace ps2_cook {

static const char *kAllowed[] = {
	"Node",
	"Node2D",
	"Node3D",
	"MeshInstance3D",
	"MultiMeshInstance3D",
	"ImporterMeshInstance3D",
	"Camera3D",
	"AnimationPlayer",
	"Skeleton3D",
	"BoneAttachment3D",
	"WorldEnvironment",
	"Marker3D",
	"DirectionalLight3D",
	"OmniLight3D",
	"SpotLight3D",
	"Light3D",
	"AudioStreamPlayer",
	"AudioStreamPlayer3D",
	"AudioStreamPlayer2D",
	"Control",
	"CanvasLayer",
	"Label",
	"ColorRect",
	"Button",
	"TextureButton",
	"Path3D",
	"PathFollow3D",
	"Path2D",
	"PathFollow2D",
	"Sprite2D",
	"AnimatedSprite2D",
	"Sprite3D",
	"Camera2D",
	"Marker2D",
	"TileMap",
	"TileMapLayer",
	"Area2D",
	"Area3D",
	"CollisionShape2D",
	"CollisionShape3D",
	"CharacterBody2D",
	"CharacterBody3D",
	"StaticBody2D",
	"StaticBody3D",
	"CPUParticles2D",
	"CPUParticles3D",
	"GPUParticles2D",
	"GPUParticles3D",
	"NavigationRegion2D",
	"NavigationRegion3D",
	"NavigationAgent2D",
	"NavigationAgent3D",
	"VehicleBody3D",
	"VehicleWheel3D",
	"PS2Spawner",
	"PS2CameraVolume",
	"PS2Follow",
	"PS2Waypoint",
	"PS2Path",
	"PS2NavRegion",
	"PS2Vehicle",
	"PS2Particles",
	"PS2Tween",
	"PS2SpawnPoint",
	"PS2Checkpoint",
	"PS2Portal",
	"PS2SavePoint",
	"PS2MusicZone",
	"PS2LoadZone",
	"PS2UnloadZone",
	"PS2Hazard",
	"PS2Pickup",
	"PS2Talk",
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
			p_cls.begins_with("SubViewport") ||
			p_cls.begins_with("SoftBody") ||
			p_cls == "Decal" || p_cls == "ReflectionProbe" ||
			p_cls == "FogVolume" || p_cls == "VoxelGI";
}

struct Gather {
	struct NodeRow {
		int parent = -1;
		uint8_t kind = 0;
		uint8_t kit = 0;
		String name;
		Vector3 pos;
		Vector3 rot;
		Vector3 scale = Vector3(1, 1, 1);
		String scene_hint;
	};
	Vector<String> kit_scenes;
	Vector<NodeRow> nodes;
	Vector<CookTri> tris;
	Vector<Ref<Image>> images;
	Vector<String> image_names;
	struct CamRow {
		int node_id = -1;
		Vector3 pos;
		Vector3 euler;
		float fov = 55.0f;
		uint16_t flags = 0;
		String name;
	};
	Vector<CamRow> cams;
	struct HitRow {
		int node_id = -1;
		uint8_t kit = 0;
		Vector3 mn;
		Vector3 mx;
		uint8_t flags = 2;
		uint8_t kind = 0;
		uint8_t layer = 1;
		uint16_t frame_lo = 0;
		uint16_t frame_hi = 0xffff;
	};
	struct SpriteRow {
		int node_id = -1;
		uint16_t tex = 0;
		int16_t x = 0;
		int16_t y = 0;
		int16_t w = 32;
		int16_t h = 32;
		uint8_t flip = 0;
		uint8_t frame = 0;
		int16_t z = 0;
	};
	Vector<SpriteRow> sprites;
	Vector<HitRow> hits;
	struct HudRow {
		int node_id = -1;
		int16_t x = 8;
		int16_t y = 8;
		int16_t w = 64;
		int16_t h = 16;
		uint8_t kind = 0;
		uint8_t r = 255;
		uint8_t g = 255;
		uint8_t b = 255;
		String text;
	};
	Vector<HudRow> hud;
	// MultiMeshInstance3D: the mesh is cooked once (node = the instance
	// node); every instance is a 3x4 delta from that cooked copy, drawn by
	// the guest with the node's matrix swapped in. MESH trailer 'INST'.
	struct InstRow {
		int node_id = -1;
		Transform3D delta;
	};
	Vector<InstRow> insts;
	HashSet<int> inst_src;
	// Nodes whose material has Disable Fog. MESH trailer 'NFOG'.
	HashSet<int> nofog;
	struct TileRow {
		int16_t x = 0;
		int16_t y = 0;
		uint8_t flags = 1;
		uint8_t atlas = 0;
	};
	Vector<TileRow> tiles;
	struct NavVert {
		Vector3 pos;
		int node_id = -1;
	};
	Vector<NavVert> nav;
	struct AnimKey {
		float t = 0.0f;
		Vector3 pos;
		Vector3 rot;
	};
	struct AnimClip {
		String name;
		float length = 1.0f;
		int node_id = -1;
		Vector<AnimKey> keys;
	};
	Vector<AnimClip> anims;
	struct PartRow {
		int node_id = -1;
		uint8_t mode = 0;
		uint8_t tex = 0;
		uint16_t maxp = 16;
		float rate = 8.0f;
		float life = 1.0f;
		float stretch = 1.0f;
		uint32_t color = 0xffffffff;
	};
	Vector<PartRow> parts;
	struct PathPoly {
		int node_id = -1;
		Vector<Vector3> pts;
	};
	Vector<PathPoly> paths;
	struct NavmVert {
		Vector3 pos;
	};
	Vector<NavmVert> navm_verts;
	struct NavmTri {
		uint16_t a = 0;
		uint16_t b = 0;
		uint16_t c = 0;
	};
	Vector<NavmTri> navm_tris;
	struct VehlRow {
		int node_id = -1;
		uint8_t mode = 0;
		float mass = 1.0f;
		float thrust = 8.0f;
		float wx[4] = { -0.5f, 0.5f, -0.5f, 0.5f };
		float wz[4] = { 0.6f, 0.6f, -0.6f, -0.6f };
	};
	Vector<VehlRow> vehls;
	// Scripts (GDIR) and lighting.
	Vector<GdirNode> gnodes;
	struct PendingConn {
		int src = -1;
		String signal;
		Node *target = nullptr;
		String method;
	};
	Vector<PendingConn> pending_conns;
	HashMap<Node *, int> node_ids;
	bool has_clear = false;
	Color clear;
	int camera_node = -1;
	float camera_fov = 70.0f;
	struct LightRow {
		int type = 0; // 0 directional, 1 omni, 2 spot
		Vector3 pos;
		Vector3 dir; // towards the light (directional)
		Color color;
		float energy = 1.0f;
		float range = 5.0f;
		float atten = 1.0f;
	};
	Vector<LightRow> light_rows;
	Color ambient = Color(0, 0, 0);
	bool lit = false;
	Vector<String> errors;
	Vector<String> warnings;
	int node_dropped = 0;
	int tri_dropped = 0;
	int tex_dropped = 0;
	int hit_dropped = 0;
	int hud_dropped = 0;
	int tile_dropped = 0;
	int nav_dropped = 0;
	int anim_dropped = 0;
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
	if (Object::cast_to<Camera3D>(p_node) || p_node->is_class("Camera2D")) {
		return 3;
	}
	if (p_node->is_class("AnimationPlayer")) {
		return 4;
	}
	if (p_node->is_class("PathFollow3D") || p_node->is_class("PathFollow2D") || p_node->is_class("PS2Path")) {
		return 5;
	}
	if (p_node->is_class("PS2Follow")) {
		return 6;
	}
	if (p_node->is_class("NavigationAgent3D") || p_node->is_class("NavigationAgent2D")) {
		return 8;
	}
	if (p_node->is_class("PS2Vehicle") || p_node->is_class("VehicleBody3D")) {
		return 9;
	}
	if (p_node->is_class("PS2Particles") || p_node->is_class("CPUParticles2D") || p_node->is_class("CPUParticles3D") ||
			p_node->is_class("GPUParticles2D") || p_node->is_class("GPUParticles3D")) {
		return 10;
	}
	if (Object::cast_to<Node3D>(p_node)) {
		return 1;
	}
	return 0;
}

static String kit_point_group(Node *p_node) {
	if (!p_node) {
		return String();
	}
	if (p_node->is_class("PS2SpawnPoint")) {
		return "spawn";
	}
	if (p_node->is_class("PS2Checkpoint")) {
		return "checkpoint";
	}
	if (p_node->is_class("PS2Portal")) {
		return "portal";
	}
	if (p_node->is_class("CharacterBody2D") || p_node->is_class("CharacterBody3D")) {
		return "player";
	}
	if (p_node->is_class("PS2Hazard")) {
		return "hazard";
	}
	if (p_node->is_class("PS2Pickup")) {
		return "pickup";
	}
	if (p_node->is_class("PS2MusicZone")) {
		return "music";
	}
	if (p_node->is_class("PS2UnloadZone")) {
		return "unload";
	}
	if (p_node->is_class("PS2LoadZone")) {
		return "load";
	}
	if (p_node->is_class("PS2Talk")) {
		return "talk";
	}
	if (p_node->is_class("PS2SavePoint")) {
		return "save";
	}
	const String nm = String(p_node->get_name()).to_lower();
	if (nm == "spawn" || p_node->is_in_group("spawn") || p_node->has_meta("ps1_spawn") || p_node->has_meta("ps2_spawn")) {
		return "spawn";
	}
	if (nm == "checkpoint" || p_node->is_in_group("checkpoint") || p_node->has_meta("ps1_checkpoint") || p_node->has_meta("ps2_checkpoint")) {
		return "checkpoint";
	}
	if (nm == "portal" || p_node->is_in_group("portal") || p_node->has_meta("ps1_dest") || p_node->has_meta("ps1_portal") || p_node->has_meta("ps2_portal")) {
		return "portal";
	}
	if (nm == "player" || nm == "hero" || p_node->is_in_group("player") || p_node->has_meta("ps1_player") || p_node->has_meta("ps2_player")) {
		return "player";
	}
	if (nm == "hazard" || p_node->is_in_group("hazard") || p_node->has_meta("ps1_hurt") || p_node->has_meta("ps2_hurt")) {
		return "hazard";
	}
	if (nm == "pickup" || p_node->is_in_group("pickup") || p_node->has_meta("ps1_line") || p_node->has_meta("ps2_pickup")) {
		return "pickup";
	}
	if (nm == "music" || nm == "musiczone" || p_node->is_in_group("music") || p_node->has_meta("ps1_music") || p_node->has_meta("ps2_music")) {
		return "music";
	}
	if (nm == "unload" || nm == "unloadvolume" || p_node->is_in_group("unload") || p_node->has_meta("ps1_unload") || p_node->has_meta("ps2_unload")) {
		return "unload";
	}
	if (nm == "load" || nm == "loadzone" || p_node->is_in_group("load") || p_node->has_meta("ps1_load") || p_node->has_meta("ps2_load")) {
		return "load";
	}
	if (nm == "talk" || nm == "talkzone" || p_node->is_in_group("talk") || p_node->has_meta("ps1_say") || p_node->has_meta("ps2_say")) {
		return "talk";
	}
	if (nm == "spawner" || nm == "instance" || p_node->is_in_group("spawner") || p_node->has_meta("ps1_template") || p_node->has_meta("ps2_template") || p_node->is_class("PS2Spawner")) {
		return "spawner";
	}
	if (nm == "save" || nm == "savezone" || nm == "loadslot" || p_node->is_in_group("save") || p_node->has_meta("ps1_save") || p_node->has_meta("ps2_save")) {
		return "save";
	}
	if (p_node->is_class("PS2CameraVolume") || nm == "cameravolume" || p_node->is_in_group("camera_volume")) {
		return "camera_volume";
	}
	if (p_node->is_class("PS2Follow") || nm == "follow" || p_node->is_in_group("follow")) {
		return "follow";
	}
	if (p_node->is_class("PS2Waypoint") || p_node->is_in_group("waypoint") || p_node->is_in_group("waypoints")) {
		return "waypoint";
	}
	if (p_node->is_class("PS2Path") || p_node->is_class("Path3D") || p_node->is_class("Path2D")) {
		return "path";
	}
	if (p_node->is_class("PS2NavRegion") || p_node->is_class("NavigationRegion3D") || p_node->is_class("NavigationRegion2D")) {
		return "nav_region";
	}
	if (p_node->is_class("PS2Vehicle") || p_node->is_class("VehicleBody3D")) {
		return "vehicle";
	}
	if (p_node->is_class("PS2Particles") || p_node->is_class("CPUParticles2D") || p_node->is_class("CPUParticles3D")) {
		return "particles";
	}
	if (p_node->is_class("PS2Tween")) {
		return "tween";
	}
	return String();
}

static uint8_t kit_id_of(const String &p_group) {
	if (p_group == "spawn") {
		return 1;
	}
	if (p_group == "checkpoint") {
		return 2;
	}
	if (p_group == "portal") {
		return 3;
	}
	if (p_group == "player") {
		return 4;
	}
	if (p_group == "hazard") {
		return 5;
	}
	if (p_group == "pickup") {
		return 6;
	}
	if (p_group == "music") {
		return 7;
	}
	if (p_group == "unload") {
		return 8;
	}
	if (p_group == "load") {
		return 9;
	}
	if (p_group == "talk") {
		return 10;
	}
	if (p_group == "spawner") {
		return 11;
	}
	if (p_group == "save") {
		return 12;
	}
	if (p_group == "camera_volume") {
		return 13;
	}
	if (p_group == "follow") {
		return 14;
	}
	if (p_group == "waypoint") {
		return 15;
	}
	if (p_group == "path") {
		return 16;
	}
	if (p_group == "nav_region") {
		return 17;
	}
	if (p_group == "vehicle") {
		return 18;
	}
	if (p_group == "particles") {
		return 19;
	}
	if (p_group == "tween") {
		return 20;
	}
	return 0;
}

static void collect_kit_scene(Node *p_node, Gather &r_g) {
	const char *keys[] = { "ps1_dest", "ps2_dest", "ps1_load", "ps2_load", "ps1_template", "ps2_template" };
	for (const char *k : keys) {
		if (!p_node->has_meta(k) || p_node->get_meta(k).get_type() != Variant::STRING) {
			continue;
		}
		const String path = p_node->get_meta(k);
		if (path.ends_with(".tscn") && !r_g.kit_scenes.has(path)) {
			r_g.kit_scenes.push_back(path);
		}
	}
	if (p_node->is_class("PS2Spawner") || p_node->is_class("PS2LoadZone") || p_node->is_class("PS2UnloadZone")) {
		const String path = p_node->get("scene");
		if (path.ends_with(".tscn") && !r_g.kit_scenes.has(path)) {
			r_g.kit_scenes.push_back(path);
		}
	}
	if (p_node->is_class("PS2Portal")) {
		const String path = p_node->get("dest");
		if (path.ends_with(".tscn") && !r_g.kit_scenes.has(path)) {
			r_g.kit_scenes.push_back(path);
		}
	}
}

static void apply_kit_exports(Node *p_node, Gather::NodeRow &r_row) {
	if (p_node->is_class("PS2Spawner") || p_node->is_class("PS2LoadZone") || p_node->is_class("PS2UnloadZone")) {
		const String path = p_node->get("scene");
		if (!path.is_empty()) {
			r_row.scene_hint = path;
		}
	}
	if (p_node->is_class("PS2Portal")) {
		const String path = p_node->get("dest");
		if (!path.is_empty()) {
			r_row.scene_hint = path;
		}
	}
	if (p_node->is_class("PS2CameraVolume")) {
		r_row.scene_hint = String::num_int64(int(p_node->get("camera_index")));
	}
	if (p_node->is_class("PS2SavePoint")) {
		r_row.scene_hint = String::num_int64(int(p_node->get("slot")));
	}
	if (p_node->is_class("PS2Hazard")) {
		r_row.scene_hint = String::num_int64(int(p_node->get("amount")));
	}
	if (p_node->is_class("PS2Talk")) {
		const String line = p_node->get("line");
		if (!line.is_empty()) {
			r_row.scene_hint = line;
		}
	}
	if (p_node->is_class("PS2Follow")) {
		r_row.scene_hint = String::num(float(p_node->get("speed")), 2);
	}
}

static String hud_button_action(Node *p_node) {
	if (p_node->has_meta("ps2_action")) {
		return String(p_node->get_meta("ps2_action")).strip_edges().to_lower();
	}
	const char *groups[] = { "pause", "resume", "save", "load", "respawn", "say", "portal" };
	for (const char *g : groups) {
		if (p_node->is_in_group(g)) {
			return String(g);
		}
	}
	const String nm = String(p_node->get_name()).to_lower();
	for (const char *g : groups) {
		if (nm == g || nm.begins_with(String(g))) {
			return String(g);
		}
	}
	return String();
}

static String hud_button_text(Node *p_node, const String &p_label) {
	const String action = hud_button_action(p_node);
	const String label = p_label.is_empty() ? String(p_node->get_name()) : p_label;
	if (action.is_empty()) {
		return label;
	}
	return action + "|" + label;
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

static void emit_tris(Gather &r_g, const PackedVector3Array &verts, const PackedVector2Array &uvs, const PackedInt32Array &idx, uint16_t p_tex, uint16_t p_node, const PackedColorArray &p_cols = PackedColorArray()) {
	auto rgb_of = [&](int i, uint8_t *o) {
		if (i >= 0 && i < p_cols.size()) {
			const Color c = p_cols[i];
			o[0] = uint8_t(CLAMP(int(c.r * 255.0f + 0.5f), 0, 255));
			o[1] = uint8_t(CLAMP(int(c.g * 255.0f + 0.5f), 0, 255));
			o[2] = uint8_t(CLAMP(int(c.b * 255.0f + 0.5f), 0, 255));
		} else {
			o[0] = o[1] = o[2] = 255;
		}
	};
	auto push_tri = [&](int ia, int ib, int ic, const Vector3 &a, const Vector3 &b, const Vector3 &c, const Vector2 &ua, const Vector2 &ub, const Vector2 &uc) {
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
		rgb_of(ia, t.rgb[0]);
		rgb_of(ib, t.rgb[1]);
		rgb_of(ic, t.rgb[2]);
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
			push_tri(i0, i1, i2, verts[i0], verts[i1], verts[i2],
					i0 < uvs.size() ? uvs[i0] : Vector2(),
					i1 < uvs.size() ? uvs[i1] : Vector2(),
					i2 < uvs.size() ? uvs[i2] : Vector2());
		}
		return;
	}
	for (int i = 0; i + 2 < verts.size(); i += 3) {
		push_tri(i, i + 1, i + 2, verts[i], verts[i + 1], verts[i + 2],
				i < uvs.size() ? uvs[i] : Vector2(),
				i + 1 < uvs.size() ? uvs[i + 1] : Vector2(1, 0),
				i + 2 < uvs.size() ? uvs[i + 2] : Vector2(0, 1));
	}
}

// Lights and environment of a scene, gathered before its meshes so vertex
// colors can carry baked lighting (the guest draws textures MODULATEd by
// vertex colour; there are no runtime lights on PS2 ABI 1).
static void collect_lights(Node *p_node, Gather &r_g) {
	if (!p_node) {
		return;
	}
	if (Light3D *l = Object::cast_to<Light3D>(p_node)) {
		if (l->is_visible()) {
			Gather::LightRow row;
			const Transform3D xf = world_xform(l);
			row.color = l->get_color();
			row.energy = l->get_param(Light3D::PARAM_ENERGY);
			if (Object::cast_to<DirectionalLight3D>(l)) {
				row.type = 0;
				row.dir = xf.basis.get_column(2).normalized();
			} else {
				row.type = Object::cast_to<SpotLight3D>(l) ? 2 : 1;
				row.pos = xf.origin;
				row.range = MAX(0.01f, float(l->get_param(Light3D::PARAM_RANGE)));
				row.atten = l->get_param(Light3D::PARAM_ATTENUATION);
			}
			r_g.light_rows.push_back(row);
			r_g.lit = true;
		}
	}
	if (WorldEnvironment *we = Object::cast_to<WorldEnvironment>(p_node)) {
		const Ref<Environment> env = we->get_environment();
		if (env.is_valid()) {
			const Environment::BGMode bg = env->get_background();
			if (bg == Environment::BG_COLOR) {
				r_g.clear = env->get_bg_color();
				r_g.has_clear = true;
			} else if (bg == Environment::BG_SKY && env->get_sky().is_valid()) {
				const Ref<ProceduralSkyMaterial> psm = env->get_sky()->get_material();
				if (psm.is_valid()) {
					r_g.clear = psm->get_sky_horizon_color();
					r_g.has_clear = true;
				}
			} else if (bg == Environment::BG_CLEAR_COLOR) {
				r_g.clear = GLOBAL_GET("rendering/environment/defaults/default_clear_color");
				r_g.has_clear = true;
			}
			const Color amb = env->get_ambient_light_color() * env->get_ambient_light_energy();
			r_g.ambient = amb;
			if (env->get_ambient_source() == Environment::AMBIENT_SOURCE_SKY || env->get_ambient_source() == Environment::AMBIENT_SOURCE_BG) {
				r_g.ambient = (r_g.has_clear ? r_g.clear : Color(0.3f, 0.3f, 0.3f)) * env->get_ambient_light_energy() * 0.5f;
			}
			r_g.lit = true;
		}
	}
	for (int i = 0; i < p_node->get_child_count(); i++) {
		collect_lights(p_node->get_child(i), r_g);
	}
}

static Color light_at(const Gather &p_g, const Vector3 &p_pos, const Vector3 &p_n) {
	Color c = p_g.ambient;
	for (const Gather::LightRow &l : p_g.light_rows) {
		float k = 0.0f;
		if (l.type == 0) {
			k = MAX(0.0f, p_n.dot(l.dir));
		} else {
			const Vector3 to = l.pos - p_pos;
			const float d = to.length();
			if (d >= l.range || d < 1e-5f) {
				continue;
			}
			const float lambert = MAX(0.0f, p_n.dot(to / d));
			// Godot's omni window: (1 - (d/r)^4)^2, with distance falloff.
			const float x = d / l.range;
			const float window = (1.0f - x * x * x * x);
			k = lambert * window * window * Math::pow(MAX(1.0f - x, 0.0001f), MAX(l.atten, 0.01f));
		}
		c += l.color * (l.energy * k);
	}
	return c;
}

static const char *kWhite = "@white";

static uint16_t white_slot(Gather &r_g) {
	Ref<Image> white = Image::create_empty(8, 8, false, Image::FORMAT_RGBA8);
	white->fill(Color(1, 1, 1));
	return register_image(r_g, white, kWhite);
}

// Material -> (texture slot, base colour, lit?)
static uint16_t material_look(Gather &r_g, const Ref<Material> &p_mat, Color &r_color, bool &r_lit, bool &r_vcol, Color &r_emit) {
	r_color = Color(1, 1, 1);
	r_lit = true;
	r_vcol = false;
	r_emit = Color(0, 0, 0);
	const Ref<BaseMaterial3D> std = p_mat;
	if (std.is_valid()) {
		r_color = std->get_albedo();
		r_lit = std->get_shading_mode() != BaseMaterial3D::SHADING_MODE_UNSHADED;
		r_vcol = std->get_flag(BaseMaterial3D::FLAG_ALBEDO_FROM_VERTEX_COLOR);
		if (std->get_feature(BaseMaterial3D::FEATURE_EMISSION)) {
			r_emit = std->get_emission() * std->get_emission_energy_multiplier();
		}
		const Ref<Texture2D> tex = std->get_texture(BaseMaterial3D::TEXTURE_ALBEDO);
		if (tex.is_valid()) {
			String name = tex->get_path();
			if (name.is_empty()) {
				name = tex->get_name();
			}
			const Ref<Image> img = image_from_texture(tex);
			if (img.is_valid()) {
				return register_image(r_g, img, name);
			}
		}
		return white_slot(r_g);
	}
	const Ref<ShaderMaterial> sm = p_mat;
	if (sm.is_valid()) {
		// Shaders do not run on PS2: take the usual albedo/colour/texture
		// uniforms when present and draw the surface with them.
		static const char *color_names[] = { "albedo", "albedo_color", "color", "base_color", "tint" };
		for (const char *nm : color_names) {
			const Variant v = sm->get_shader_parameter(nm);
			if (v.get_type() == Variant::COLOR) {
				r_color = v;
				break;
			}
		}
		static const char *tex_names[] = { "texture_albedo", "albedo_texture", "texture", "main_texture", "tex" };
		for (const char *nm : tex_names) {
			const Ref<Texture2D> tex = sm->get_shader_parameter(nm);
			if (tex.is_valid()) {
				const Ref<Image> img = image_from_texture(tex);
				if (img.is_valid()) {
					return register_image(r_g, img, tex->get_path());
				}
			}
		}
		r_lit = false;
		return white_slot(r_g);
	}
	return white_slot(r_g);
}

static void emit_mesh(Gather &r_g, const Ref<Mesh> &p_mesh, const Transform3D &p_xf, int p_my, Node *p_owner) {
	if (p_mesh.is_null() || p_my < 0) {
		return;
	}
	const Basis nbasis = p_xf.basis.inverse().transposed();
	for (int s = 0; s < p_mesh->get_surface_count(); s++) {
		Array arr = p_mesh->surface_get_arrays(s);
		PackedVector3Array verts;
		PackedVector3Array normals;
		PackedVector2Array uvs;
		PackedInt32Array idx;
		PackedColorArray vcols;
		if (arr.size() > Mesh::ARRAY_VERTEX) {
			verts = arr[Mesh::ARRAY_VERTEX];
		}
		if (arr.size() > Mesh::ARRAY_NORMAL && arr[Mesh::ARRAY_NORMAL].get_type() == Variant::PACKED_VECTOR3_ARRAY) {
			normals = arr[Mesh::ARRAY_NORMAL];
		}
		if (arr.size() > Mesh::ARRAY_TEX_UV && arr[Mesh::ARRAY_TEX_UV].get_type() == Variant::PACKED_VECTOR2_ARRAY) {
			uvs = arr[Mesh::ARRAY_TEX_UV];
		}
		if (arr.size() > Mesh::ARRAY_INDEX && arr[Mesh::ARRAY_INDEX].get_type() == Variant::PACKED_INT32_ARRAY) {
			idx = arr[Mesh::ARRAY_INDEX];
		}
		if (arr.size() > Mesh::ARRAY_COLOR && arr[Mesh::ARRAY_COLOR].get_type() == Variant::PACKED_COLOR_ARRAY) {
			vcols = arr[Mesh::ARRAY_COLOR];
		}
		if (p_xf != Transform3D()) {
			for (int i = 0; i < verts.size(); i++) {
				verts.write[i] = p_xf.xform(verts[i]);
			}
			for (int i = 0; i < normals.size(); i++) {
				normals.write[i] = nbasis.xform(normals[i]).normalized();
			}
		}
		Ref<Material> mat;
		if (MeshInstance3D *mi = Object::cast_to<MeshInstance3D>(p_owner)) {
			mat = mi->get_active_material(s);
		} else if (GeometryInstance3D *gi = Object::cast_to<GeometryInstance3D>(p_owner)) {
			mat = gi->get_material_override();
		}
		if (mat.is_null()) {
			mat = p_mesh->surface_get_material(s);
		}
		const Ref<BaseMaterial3D> bm = mat;
		if (bm.is_valid() && bm->get_flag(BaseMaterial3D::FLAG_DISABLE_FOG)) {
			r_g.nofog.insert(p_my);
		}
		Color base;
		Color emit;
		bool lit = true;
		bool use_vcol = false;
		const uint16_t slot = material_look(r_g, mat, base, lit, use_vcol, emit);
		PackedColorArray cols;
		cols.resize(verts.size());
		for (int i = 0; i < verts.size(); i++) {
			Color c = base;
			if (use_vcol && i < vcols.size()) {
				c *= vcols[i];
			}
			if (lit && r_g.lit) {
				const Vector3 n = i < normals.size() ? normals[i] : Vector3(0, 1, 0);
				const Color l = light_at(r_g, verts[i], n);
				c = Color(c.r * l.r, c.g * l.g, c.b * l.b);
			}
			c += emit;
			cols.write[i] = c;
		}
		emit_tris(r_g, verts, uvs, idx, slot, uint16_t(p_my), cols);
	}
}

// Effective CanvasItem visibility without a SceneTree (parents included).
static bool canvas_visible(Node *p_node) {
	for (Node *n = p_node; n; n = n->get_parent()) {
		if (CanvasItem *ci = Object::cast_to<CanvasItem>(n)) {
			if (!ci->is_visible()) {
				return false;
			}
		}
		if (CanvasLayer *cl = Object::cast_to<CanvasLayer>(n)) {
			if (!cl->is_visible()) {
				return false;
			}
		}
	}
	return true;
}

static Vector2 control_global(Control *p_c) {
	Vector2 pos;
	for (Node *n = p_c; n; n = n->get_parent()) {
		if (Control *c = Object::cast_to<Control>(n)) {
			pos += c->get_position();
		} else if (CanvasLayer *cl = Object::cast_to<CanvasLayer>(n)) {
			pos += cl->get_offset();
			break;
		} else if (!Object::cast_to<CanvasItem>(n)) {
			break;
		}
	}
	return pos;
}

// HUD kind byte: bits 0-1 kind (1 label, 2 rect, 3 button), bits 2-3 align,
// bits 4-6 glyph scale - 1 (5x7 font), bit 7 hidden.
static int hud_scale(int p_font_size) {
	return CLAMP(int(Math::round(float(p_font_size) / 8.0f)), 1, 8) - 1;
}

static Gather::HudRow hud_row_for(Control *p_c, int p_kind, Gather &r_g, int p_my) {
	(void)r_g;
	Gather::HudRow row;
	row.node_id = p_my;
	row.kind = uint8_t(p_kind);
	const Vector2 gp = control_global(p_c);
	row.x = int16_t(gp.x);
	row.y = int16_t(gp.y);
	row.w = int16_t(MAX(8.0, p_c->get_size().x));
	row.h = int16_t(MAX(8.0, p_c->get_size().y));
	if (!canvas_visible(p_c)) {
		row.kind |= 0x80;
	}
	return row;
}

static void push_hud(Gather &r_g, const Gather::HudRow &p_row, int p_my) {
	if (r_g.hud.size() >= kMaxHud) {
		r_g.hud_dropped++;
		return;
	}
	if (p_my >= 0 && p_my < r_g.gnodes.size()) {
		r_g.gnodes.write[p_my].hud = r_g.hud.size();
	}
	r_g.hud.push_back(p_row);
}

static void record_gdir_node(Node *p_node, Gather &r_g, int p_my, int p_parent) {
	GdirNode g;
	g.parent = p_parent;
	g.name = p_node->get_name();
	if (p_parent < 0) {
		g.path = ".";
	} else {
		const String pp = r_g.gnodes[p_parent].path;
		g.path = pp == "." ? g.name : pp + "/" + g.name;
	}
	g.flags = 0;
	bool visible = true;
	if (Node3D *n3 = Object::cast_to<Node3D>(p_node)) {
		g.flags |= 2;
		visible = n3->is_visible();
		g.lpos = n3->get_position();
		g.lrot = n3->get_rotation();
		g.lscale = n3->get_scale();
		g.w0 = world_xform(n3);
	} else if (Control *c = Object::cast_to<Control>(p_node)) {
		g.flags |= 4;
		visible = c->is_visible();
		g.lpos = Vector3(c->get_position().x, c->get_position().y, 0.0f);
		g.lscale = Vector3(c->get_size().x, c->get_size().y, 1.0f);
		const Vector2 gp = control_global(c);
		g.w0.basis.rows[0] = Vector3(c->get_position().x, c->get_position().y, 0.0f);
		g.w0.origin = Vector3(gp.x, gp.y, 0.0f);
	} else if (CanvasItem *ci = Object::cast_to<CanvasItem>(p_node)) {
		visible = ci->is_visible();
	} else if (CanvasLayer *cl = Object::cast_to<CanvasLayer>(p_node)) {
		visible = cl->is_visible();
	}
	if (visible) {
		g.flags |= 1;
	}
	if (Object::cast_to<Camera3D>(p_node)) {
		g.flags |= 8;
	}
	const Ref<Script> scr = p_node->get_script();
	if (scr.is_valid() && scr->get_language() && scr->get_language()->get_name() == "GDScript") {
		g.script_path = scr->get_path();
		g.script_source = scr->get_source_code();
		if (g.script_source.is_empty() && !g.script_path.is_empty() && !g.script_path.contains("::")) {
			g.script_source = FileAccess::get_file_as_string(g.script_path);
		}
		List<PropertyInfo> props;
		scr->get_script_property_list(&props);
		for (const PropertyInfo &pi : props) {
			if (pi.usage & PROPERTY_USAGE_EDITOR) {
				g.exported[pi.name] = p_node->get(pi.name);
			}
		}
	}
	r_g.node_ids[p_node] = p_my;
	r_g.gnodes.push_back(g);
	// Button.pressed connections made in the scene file; targets resolve after the walk.
	if (p_node->has_signal("pressed")) {
		List<Object::Connection> conns;
		p_node->get_signal_connection_list("pressed", &conns);
		for (const Object::Connection &c : conns) {
			Gather::PendingConn pc;
			pc.src = p_my;
			pc.signal = "pressed";
			pc.target = Object::cast_to<Node>(c.callable.get_object());
			pc.method = c.callable.get_method();
			r_g.pending_conns.push_back(pc);
		}
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
		row.kit = kit_id_of(kit_point_group(p_node));
		row.name = p_node->get_name();
		row.scene_hint = p_scene;
		collect_kit_scene(p_node, r_g);
		apply_kit_exports(p_node, row);
		if (Node3D *n3 = Object::cast_to<Node3D>(p_node)) {
			const Transform3D xf = world_xform(n3);
			row.pos = xf.origin;
			row.rot = xf.basis.get_euler();
			row.scale = xf.basis.get_scale();
		} else if (p_node->is_class("Node2D")) {
			const Variant posv = p_node->get("global_position");
			if (posv.get_type() == Variant::VECTOR2) {
				const Vector2 p = posv;
				row.pos = Vector3(p.x, p.y, 0.0f);
			}
			row.rot = Vector3(0.0f, 0.0f, float(p_node->get("global_rotation")));
			const Variant sc = p_node->get("scale");
			if (sc.get_type() == Variant::VECTOR2) {
				const Vector2 s = sc;
				row.scale = Vector3(s.x, s.y, 1.0f);
			}
		}
		my = r_g.nodes.size();
		r_g.nodes.push_back(row);
		record_gdir_node(p_node, r_g, my, p_parent);
	} else {
		r_g.node_dropped++;
	}
	if (Camera3D *cam = Object::cast_to<Camera3D>(p_node)) {
		r_g.cameras++;
		if (r_g.cams.size() < kMaxCameras && my >= 0) {
			Gather::CamRow row;
			row.node_id = my;
			row.name = p_node->get_name();
			row.pos = world_xform(cam).origin;
			row.euler = world_xform(cam).basis.get_euler();
			row.fov = cam->get_fov();
			if (cam->is_current()) {
				row.flags = 1;
				if (r_g.camera_node < 0) {
					r_g.camera_node = my;
					r_g.camera_fov = cam->get_fov();
				}
			}
			r_g.cams.push_back(row);
		}
	}
	if (p_node->is_class("Camera2D")) {
		r_g.cameras++;
		if (r_g.cams.size() < kMaxCameras && my >= 0) {
			Gather::CamRow row;
			row.node_id = my;
			row.name = p_node->get_name();
			row.flags = 2;
			if (bool(p_node->get("enabled"))) {
				row.flags |= 1;
			}
			const Variant posv = p_node->get("global_position");
			if (posv.get_type() == Variant::VECTOR2) {
				const Vector2 p = posv;
				row.pos = Vector3(p.x, p.y, 8.0f);
			}
			const float zoom = float(p_node->get("zoom").operator Vector2().x);
			row.fov = zoom > 0.01f ? (55.0f / zoom) : 55.0f;
			r_g.cams.push_back(row);
		}
	}
	if (p_node->is_class("Sprite2D") || p_node->is_class("AnimatedSprite2D") || p_node->is_class("Sprite3D")) {
		if (r_g.sprites.size() < kMaxSprites && my >= 0) {
			Gather::SpriteRow s;
			s.node_id = my;
			String tex_name = p_node->get_name();
			Ref<Texture2D> tex = p_node->get("texture");
			if (tex.is_valid()) {
				Ref<Image> img = tex->get_image();
				if (img.is_valid() && !img->is_empty()) {
					s.tex = register_image(r_g, img, tex_name);
					s.w = int16_t(CLAMP(img->get_width(), 1, 256));
					s.h = int16_t(CLAMP(img->get_height(), 1, 256));
				}
			}
			if (s.w < 1 || s.h < 1 || (!tex.is_valid())) {
				Ref<Image> ph = Image::create_empty(16, 16, false, Image::FORMAT_RGBA8);
				ph->fill(Color(0.85f, 0.35f, 0.75f));
				s.tex = register_image(r_g, ph, tex_name + "_ph");
				s.w = 16;
				s.h = 16;
			}
			const Variant posv = p_node->get("global_position");
			if (posv.get_type() == Variant::VECTOR2) {
				const Vector2 p = posv;
				s.x = int16_t(p.x);
				s.y = int16_t(p.y);
			} else if (Node3D *n3 = Object::cast_to<Node3D>(p_node)) {
				const Vector3 p = world_xform(n3).origin;
				s.x = int16_t(p.x);
				s.y = int16_t(-p.y);
				s.z = int16_t(p.z);
			}
			if (bool(p_node->get("flip_h"))) {
				s.flip |= 1;
			}
			if (bool(p_node->get("flip_v"))) {
				s.flip |= 2;
			}
			s.frame = uint8_t(int(p_node->get("frame")));
			r_g.sprites.push_back(s);
		}
	}
	if (p_node->is_class("TileMap") || p_node->is_class("TileMapLayer")) {
		Array cells;
		if (p_node->is_class("TileMapLayer")) {
			cells = p_node->call("get_used_cells");
		} else {
			cells = p_node->call("get_used_cells", 0);
		}
		uint8_t atlas = 0;
		const Variant tsv = p_node->get("tile_set");
		Object *ts = tsv.get_validated_object();
		if (ts && int(ts->call("get_source_count")) > 0) {
			const int sid = int(ts->call("get_source_id", 0));
			const Variant srcv = ts->call("get_source", sid);
			Object *src = srcv.get_validated_object();
			if (src) {
				Ref<Texture2D> tex = src->get("texture");
				if (tex.is_valid()) {
					Ref<Image> img = tex->get_image();
					if (img.is_valid() && !img->is_empty()) {
						atlas = uint8_t(register_image(r_g, img, String(p_node->get_name()) + "_atlas"));
					}
				}
			}
		}
		if (!ts || (atlas == 0 && r_g.image_names.find(String(p_node->get_name()) + "_atlas") < 0)) {
			Ref<Image> ph = Image::create_empty(16, 16, false, Image::FORMAT_RGBA8);
			ph->fill(Color(0.25f, 0.55f, 0.3f));
			atlas = uint8_t(register_image(r_g, ph, String(p_node->get_name()) + "_atlas"));
		}
		for (int i = 0; i < cells.size(); i++) {
			if (r_g.tiles.size() >= kMaxTiles) {
				r_g.tile_dropped++;
				break;
			}
			const Vector2i c = cells[i];
			Gather::TileRow t;
			t.x = int16_t(c.x);
			t.y = int16_t(c.y);
			t.flags = 1;
			t.atlas = atlas;
			r_g.tiles.push_back(t);
		}
	}
	if (p_node->is_class("CollisionShape2D") || p_node->is_class("CollisionShape3D") ||
			p_node->is_class("Area2D") || p_node->is_class("Area3D")) {
		if (r_g.hits.size() < kMaxHits && my >= 0) {
			Gather::HitRow h;
			h.node_id = my;
			h.kit = r_g.nodes[my].kit;
			if (h.kit == 0 && p_parent >= 0 && p_parent < r_g.nodes.size()) {
				h.kit = r_g.nodes[p_parent].kit;
			}
			h.flags = 2;
			h.kind = (p_node->is_class("Area2D") || p_node->is_class("Area3D")) ? 1 : 0;
			h.layer = 1;
			{
				Variant layer = p_node->get("collision_layer");
				if (layer.get_type() == Variant::NIL && p_node->get_parent()) {
					layer = p_node->get_parent()->get("collision_layer");
				}
				if (layer.get_type() == Variant::INT || layer.get_type() == Variant::FLOAT) {
					const int bits = int(layer);
					h.layer = uint8_t(bits & 255);
					if (h.layer == 0) {
						h.layer = 1;
					}
				}
			}
			if (p_node->is_class("CollisionShape2D") || p_node->is_class("Area2D")) {
				h.flags |= 1;
			}
			const Variant posv = p_node->get("global_position");
			Vector3 origin;
			if (posv.get_type() == Variant::VECTOR2) {
				const Vector2 p = posv;
				origin = Vector3(p.x, p.y, 0);
			} else if (Node3D *n3 = Object::cast_to<Node3D>(p_node)) {
				origin = world_xform(n3).origin;
			}
			float hx = 0.5f, hy = 0.5f, hz = 0.5f;
			const Variant shape = p_node->get("shape");
			if (shape.get_type() == Variant::OBJECT) {
				Object *sh = shape;
				if (sh) {
					if (sh->is_class("RectangleShape2D") || sh->is_class("BoxShape3D")) {
						const Variant sz = sh->get("size");
						if (sz.get_type() == Variant::VECTOR2) {
							const Vector2 s = sz;
							hx = s.x * 0.5f;
							hy = s.y * 0.5f;
							hz = 0.5f;
						} else if (sz.get_type() == Variant::VECTOR3) {
							const Vector3 s = sz;
							hx = s.x * 0.5f;
							hy = s.y * 0.5f;
							hz = s.z * 0.5f;
						}
					}
				}
			}
			h.mn = origin - Vector3(hx, hy, hz);
			h.mx = origin + Vector3(hx, hy, hz);
			r_g.hits.push_back(h);
		} else if (my >= 0) {
			r_g.hit_dropped++;
		}
	}
	if (my >= 0 && my < r_g.nodes.size()) {
		const uint8_t kit = r_g.nodes[my].kit;
		if (kit >= 1 && kit <= 13 && kit != 4) {
			bool has_hit = false;
			for (int hi = 0; hi < r_g.hits.size(); hi++) {
				if (r_g.hits[hi].node_id == my) {
					has_hit = true;
					break;
				}
			}
			if (!has_hit) {
				if (r_g.hits.size() < kMaxHits) {
					Gather::HitRow h;
					h.node_id = my;
					h.kit = kit;
					h.flags = 2;
					h.kind = 1;
					h.layer = 1;
					const bool is2d = p_node->is_class("Node2D") || p_node->is_class("Area2D") || p_node->is_class("CollisionShape2D") || p_node->is_class("CharacterBody2D");
					if (is2d) {
						h.flags |= 1;
					}
					const float hx = is2d ? 8.0f : 0.5f;
					const float hy = is2d ? 8.0f : 0.5f;
					const float hz = 0.5f;
					h.mn = r_g.nodes[my].pos - Vector3(hx, hy, hz);
					h.mx = r_g.nodes[my].pos + Vector3(hx, hy, hz);
					r_g.hits.push_back(h);
				} else {
					r_g.hit_dropped++;
				}
			}
		}
	}
	if (AnimationPlayer *ap = Object::cast_to<AnimationPlayer>(p_node)) {
		r_g.anim_players++;
		List<StringName> names;
		ap->get_animation_list(&names);
		for (const StringName &nm : names) {
			if (r_g.anims.size() >= kMaxAnim) {
				r_g.anim_dropped++;
				break;
			}
			Gather::AnimClip clip;
			clip.name = nm;
			clip.node_id = my;
			const Ref<Animation> anim = ap->get_animation(nm);
			if (anim.is_valid()) {
				clip.length = float(anim->get_length());
				for (int t = 0; t < anim->get_track_count() && clip.keys.size() < 64; t++) {
					const Animation::TrackType tt = anim->track_get_type(t);
					if (tt != Animation::TYPE_POSITION_3D && tt != Animation::TYPE_ROTATION_3D) {
						continue;
					}
					const int kc = anim->track_get_key_count(t);
					for (int k = 0; k < kc && clip.keys.size() < 64; k++) {
						Gather::AnimKey key;
						key.t = float(anim->track_get_key_time(t, k));
						const Variant val = anim->track_get_key_value(t, k);
						if (tt == Animation::TYPE_POSITION_3D && val.get_type() == Variant::VECTOR3) {
							key.pos = val;
						} else if (tt == Animation::TYPE_ROTATION_3D && val.get_type() == Variant::QUATERNION) {
							key.rot = Quaternion(val).get_euler();
						}
						clip.keys.push_back(key);
					}
				}
			}
			r_g.anims.push_back(clip);
		}
	}
	if (Label *lab = Object::cast_to<Label>(p_node)) {
		Gather::HudRow row = hud_row_for(lab, 1, r_g, my);
		const Color fc = lab->get_theme_color("font_color");
		row.r = uint8_t(CLAMP(int(fc.r * 255.0f), 0, 255));
		row.g = uint8_t(CLAMP(int(fc.g * 255.0f), 0, 255));
		row.b = uint8_t(CLAMP(int(fc.b * 255.0f), 0, 255));
		row.text = lab->get_text();
		const HorizontalAlignment ha = lab->get_horizontal_alignment();
		row.kind |= uint8_t((ha == HORIZONTAL_ALIGNMENT_CENTER ? 1 : (ha == HORIZONTAL_ALIGNMENT_RIGHT ? 2 : 0)) << 2);
		row.kind |= uint8_t(hud_scale(lab->get_theme_font_size("font_size")) << 4);
		push_hud(r_g, row, my);
	}
	if (Button *btn = Object::cast_to<Button>(p_node)) {
		Gather::HudRow row = hud_row_for(btn, 3, r_g, my);
		row.h = int16_t(MAX(16, int(row.h)));
		const Ref<StyleBoxFlat> sb = btn->get_theme_stylebox("normal");
		const Color bc = sb.is_valid() ? sb->get_bg_color() : Color(0.19f, 0.19f, 0.25f);
		row.r = uint8_t(CLAMP(int(bc.r * 255.0f), 0, 255));
		row.g = uint8_t(CLAMP(int(bc.g * 255.0f), 0, 255));
		row.b = uint8_t(CLAMP(int(bc.b * 255.0f), 0, 255));
		row.text = hud_button_text(btn, btn->get_text());
		row.kind |= uint8_t(1 << 2); // buttons center their text
		row.kind |= uint8_t(hud_scale(btn->get_theme_font_size("font_size")) << 4);
		push_hud(r_g, row, my);
	}
	if (TextureButton *tb = Object::cast_to<TextureButton>(p_node)) {
		Gather::HudRow row = hud_row_for(tb, 3, r_g, my);
		row.h = int16_t(MAX(16, int(row.h)));
		row.text = hud_button_text(tb, String(tb->get_name()));
		row.kind |= uint8_t(1 << 2);
		push_hud(r_g, row, my);
	}
	if (ColorRect *bar = Object::cast_to<ColorRect>(p_node)) {
		Gather::HudRow row = hud_row_for(bar, 2, r_g, my);
		const Color c = bar->get_color();
		row.r = uint8_t(CLAMP(int(c.r * 255.0f), 0, 255));
		row.g = uint8_t(CLAMP(int(c.g * 255.0f), 0, 255));
		row.b = uint8_t(CLAMP(int(c.b * 255.0f), 0, 255));
		push_hud(r_g, row, my);
	}
	if (Path3D *path = Object::cast_to<Path3D>(p_node)) {
		const Ref<Curve3D> curve = path->get_curve();
		if (curve.is_valid()) {
			const Transform3D xf = world_xform(path);
			Gather::PathPoly poly;
			poly.node_id = my;
			for (int i = 0; i < curve->get_point_count(); i++) {
				const Vector3 wp = xf.xform(curve->get_point_position(i));
				if (poly.pts.size() < kMaxPathPoints) {
					poly.pts.push_back(wp);
				}
				if (r_g.nav.size() >= kMaxNav) {
					r_g.nav_dropped++;
				} else {
					Gather::NavVert v;
					v.pos = wp;
					v.node_id = my;
					r_g.nav.push_back(v);
				}
			}
			if (!poly.pts.is_empty()) {
				r_g.paths.push_back(poly);
			}
		}
	}
	if (p_node->is_class("Path2D")) {
		const Variant curvev = p_node->get("curve");
		if (curvev.get_type() == Variant::OBJECT) {
			Object *curve = curvev;
			if (curve && curve->has_method("get_point_count") && curve->has_method("get_point_position")) {
				Gather::PathPoly poly;
				poly.node_id = my;
				const int n = int(curve->call("get_point_count"));
				const Variant posv = p_node->get("global_position");
				Vector2 origin;
				if (posv.get_type() == Variant::VECTOR2) {
					origin = posv;
				}
				for (int i = 0; i < n; i++) {
					const Vector2 lp = curve->call("get_point_position", i);
					const Vector3 wp(origin.x + lp.x, origin.y + lp.y, 0.0f);
					if (poly.pts.size() < kMaxPathPoints) {
						poly.pts.push_back(wp);
					}
					if (r_g.nav.size() < kMaxNav) {
						Gather::NavVert v;
						v.pos = wp;
						v.node_id = my;
						r_g.nav.push_back(v);
					} else {
						r_g.nav_dropped++;
					}
				}
				if (!poly.pts.is_empty()) {
					r_g.paths.push_back(poly);
				}
			}
		}
	}
	if (p_node->is_class("PS2Path") && !Object::cast_to<Path3D>(p_node) && !p_node->is_class("Path2D")) {
		if (Node3D *n3 = Object::cast_to<Node3D>(p_node)) {
			Gather::PathPoly poly;
			poly.node_id = my;
			poly.pts.push_back(world_xform(n3).origin);
			r_g.paths.push_back(poly);
		}
	}
	if (!Object::cast_to<Path3D>(p_node) && !p_node->is_class("Path2D") &&
			(p_node->is_in_group("nav") || p_node->is_in_group("waypoint") || p_node->is_class("PS2Waypoint") ||
					String(p_node->get_name()).begins_with("Nav"))) {
		if (Node3D *n3 = Object::cast_to<Node3D>(p_node)) {
			if (r_g.nav.size() < kMaxNav) {
				Gather::NavVert v;
				v.pos = world_xform(n3).origin;
				v.node_id = my;
				r_g.nav.push_back(v);
			} else {
				r_g.nav_dropped++;
			}
		} else if (p_node->is_class("Node2D") && r_g.nav.size() < kMaxNav) {
			const Variant posv = p_node->get("global_position");
			if (posv.get_type() == Variant::VECTOR2) {
				const Vector2 p = posv;
				Gather::NavVert v;
				v.pos = Vector3(p.x, p.y, 0.0f);
				v.node_id = my;
				r_g.nav.push_back(v);
			}
		} else if (r_g.nav.size() >= kMaxNav) {
			r_g.nav_dropped++;
		}
	}
	if (p_node->is_class("NavigationRegion3D") || p_node->is_class("NavigationRegion2D") || p_node->is_class("PS2NavRegion")) {
		const Variant meshv = p_node->get("navigation_mesh");
		if (meshv.get_type() == Variant::OBJECT) {
			Object *mesh = meshv;
			if (mesh && mesh->has_method("get_vertices")) {
				const PackedVector3Array verts = mesh->call("get_vertices");
				const int base = r_g.navm_verts.size();
				for (int i = 0; i < verts.size() && r_g.navm_verts.size() < kMaxNavmVerts; i++) {
					Gather::NavmVert nv;
					nv.pos = verts[i];
					r_g.navm_verts.push_back(nv);
				}
				if (mesh->has_method("get_polygon_count") && mesh->has_method("get_polygon")) {
					const int pc = int(mesh->call("get_polygon_count"));
					for (int i = 0; i < pc && r_g.navm_tris.size() < kMaxNavmTris; i++) {
						const PackedInt32Array poly = mesh->call("get_polygon", i);
						if (poly.size() >= 3) {
							Gather::NavmTri t;
							t.a = uint16_t(base + poly[0]);
							t.b = uint16_t(base + poly[1]);
							t.c = uint16_t(base + poly[2]);
							r_g.navm_tris.push_back(t);
						}
					}
				}
			}
		}
	}
	if (p_node->is_class("CPUParticles2D") || p_node->is_class("CPUParticles3D") ||
			p_node->is_class("GPUParticles2D") || p_node->is_class("GPUParticles3D") ||
			p_node->is_class("PS2Particles")) {
		if (r_g.parts.size() < kMaxPartEmit && my >= 0) {
			Gather::PartRow pr;
			pr.node_id = my;
			pr.maxp = uint16_t(CLAMP(int(p_node->get("amount")), 1, 64));
			if (p_node->is_class("PS2Particles")) {
				pr.maxp = uint16_t(CLAMP(int(p_node->get("amount")), 1, 64));
				pr.life = float(p_node->get("lifetime"));
				pr.mode = uint8_t(int(p_node->get("mode")) & 3);
			} else {
				pr.life = float(p_node->get("lifetime"));
				const String nm = String(p_node->get_name()).to_lower();
				if (nm.contains("flip")) {
					pr.mode = 1;
				} else if (nm.contains("trail")) {
					pr.mode = 2;
				} else if (nm.contains("point")) {
					pr.mode = 3;
				}
				if (p_node->has_meta("ps2_part_mode")) {
					pr.mode = uint8_t(int(p_node->get_meta("ps2_part_mode")) & 3);
				}
			}
			if (pr.life <= 0.01f) {
				pr.life = 1.0f;
			}
			pr.rate = float(pr.maxp) / pr.life;
			r_g.parts.push_back(pr);
		}
		if (p_node->is_class("GPUParticles2D") || p_node->is_class("GPUParticles3D")) {
			r_g.warnings.push_back("GPUParticles cooked as PART CPU emit (no GPU particle server on PS2 ABI 1).");
		}
	}
	if (p_node->is_class("PS2Vehicle") || p_node->is_class("VehicleBody3D")) {
		if (r_g.vehls.size() < kMaxVehl && my >= 0) {
			Gather::VehlRow vr;
			vr.node_id = my;
			if (p_node->is_class("PS2Vehicle")) {
				vr.mode = uint8_t(int(p_node->get("mode")) & 3);
				vr.mass = float(p_node->get("mass"));
				vr.thrust = float(p_node->get("thrust"));
			} else {
				vr.mass = float(p_node->get("mass"));
				const String nm = String(p_node->get_name()).to_lower();
				if (nm.contains("hover")) {
					vr.mode = 1;
				} else if (nm.contains("6dof") || nm.contains("space")) {
					vr.mode = 2;
				}
			}
			if (vr.mass < 0.1f) {
				vr.mass = 1.0f;
			}
			if (vr.thrust < 0.1f) {
				vr.thrust = 8.0f;
			}
			r_g.vehls.push_back(vr);
		}
	}
	if (p_node->is_in_group("tile") || String(p_node->get_name()).begins_with("Tile")) {
		if (Node3D *n3 = Object::cast_to<Node3D>(p_node)) {
			if (r_g.tiles.size() < kMaxTiles) {
				Gather::TileRow t;
				const Vector3 p = world_xform(n3).origin;
				t.x = int16_t(Math::floor(p.x));
				t.y = int16_t(Math::floor(p.z));
				t.flags = 1;
				{
					Ref<Image> ph = Image::create_empty(16, 16, false, Image::FORMAT_RGBA8);
					ph->fill(Color(0.25f, 0.55f, 0.3f));
					t.atlas = uint8_t(register_image(r_g, ph, "tile_ph"));
				}
				r_g.tiles.push_back(t);
			} else {
				r_g.tile_dropped++;
			}
		}
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
			const Transform3D xf = world_xform(mi);
			emit_mesh(r_g, mi->get_mesh(), xf, my, mi);
			if (r_g.hits.size() < kMaxHits) {
				const AABB aabb = xf.xform(mi->get_mesh()->get_aabb());
				Gather::HitRow h;
				h.node_id = my;
				h.kit = r_g.nodes[my].kit;
				h.mn = aabb.position;
				h.mx = aabb.position + aabb.size;
				r_g.hits.push_back(h);
			} else {
				r_g.hit_dropped++;
			}
		}
	}
	if (MultiMeshInstance3D *mmi = Object::cast_to<MultiMeshInstance3D>(p_node)) {
		const Ref<MultiMesh> mm = mmi->get_multimesh();
		if (mm.is_valid() && mm->get_mesh().is_valid() && mm->get_transform_format() == MultiMesh::TRANSFORM_3D && my >= 0) {
			const Transform3D xf = world_xform(mmi);
			const Transform3D inv = xf.affine_inverse();
			emit_mesh(r_g, mm->get_mesh(), xf, my, mmi);
			r_g.inst_src.insert(my);
			const int count = mm->get_visible_instance_count() >= 0 ? MIN(mm->get_visible_instance_count(), mm->get_instance_count()) : mm->get_instance_count();
			for (int i = 0; i < count; i++) {
				const Transform3D t = mm->get_instance_transform(i);
				if (Math::abs(t.basis.determinant()) < 1e-6f) {
					continue; // scaled to nothing: a placement that was left out
				}
				Gather::InstRow row;
				row.node_id = my;
				row.delta = xf * t * inv;
				r_g.insts.push_back(row);
			}
		}
	}
	if (ImporterMeshInstance3D *imi = Object::cast_to<ImporterMeshInstance3D>(p_node)) {
		if (imi->get_mesh().is_valid() && my >= 0) {
			const Transform3D xf = world_xform(imi);
			emit_mesh(r_g, imi->get_mesh()->get_mesh(), xf, my, nullptr);
			if (r_g.hits.size() < kMaxHits) {
				const AABB aabb = xf.xform(imi->get_mesh()->get_mesh()->get_aabb());
				Gather::HitRow h;
				h.node_id = my;
				h.kit = r_g.nodes[my].kit;
				h.mn = aabb.position;
				h.mx = aabb.position + aabb.size;
				r_g.hits.push_back(h);
			} else {
				r_g.hit_dropped++;
			}
		}
	}
	for (int i = 0; i < p_node->get_child_count(); i++) {
		walk_scene(p_node->get_child(i), r_g, my >= 0 ? my : p_parent, p_scene);
	}
}

static void append_u32(Vector<uint8_t> &o, uint32_t v) {
	o.push_back(uint8_t(v & 0xff));
	o.push_back(uint8_t((v >> 8) & 0xff));
	o.push_back(uint8_t((v >> 16) & 0xff));
	o.push_back(uint8_t((v >> 24) & 0xff));
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
		out.push_back(n.kit);
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
		CharString hint = n.scene_hint.utf8();
		for (int c = 0; c < 64; c++) {
			out.push_back(c < hint.length() ? uint8_t(hint[c]) : 0);
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

// ---------------------------------------------------------- memory card
// MCSAVE.BIN tells the guest how to file PS2.save_slot saves on the memory
// card: the directory (one per game), the title the BIOS browser shows, and
// the browser icon. Layout: "MCSV", u16 version 1, u16 header size (76),
// char dir[32], char title[32], u32 icon size, then the icon.
//
// The icon is the project's application/config/icon on a flat square: a PS2
// .ico (ps2icon 0.5: header, per-vertex coordinate / normal / UV + colour,
// one animation frame, then a 128x128 16-bit texture). Without an icon the
// square is a plain colour.
static String mc_ascii(const String &p_text, int p_max, bool p_dir) {
	String out;
	for (int i = 0; i < p_text.length() && out.length() < p_max; i++) {
		const char32_t c = p_text[i];
		if ((c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9')) {
			out += String::chr(c);
		} else if (c >= 'a' && c <= 'z') {
			out += String::chr(p_dir ? c - 32 : c);
		} else if (!p_dir && c >= 32 && c < 127) {
			out += String::chr(c);
		}
	}
	return out;
}

static void append_s16(Vector<uint8_t> &o, int v) {
	append_u16(o, uint16_t(int16_t(CLAMP(v, -32768, 32767))));
}

static Vector<uint8_t> mc_icon(const Ref<Image> &p_img) {
	Vector<uint8_t> o;
	append_u32(o, 0x00010000); // file id
	append_u32(o, 1); // animation shapes
	append_u32(o, 0x07); // texture, uncompressed
	append_f32(o, 1.0f);
	append_u32(o, 6); // two triangles
	// a 2.4-unit square standing on y = 0; icon space has y pointing down
	const float x0 = -1.2f, x1 = 1.2f, y0 = -2.4f, y1 = 0.0f;
	const float quad[6][4] = {
		{ x0, y0, 0, 0 },
		{ x1, y0, 1, 0 },
		{ x0, y1, 0, 1 },
		{ x1, y0, 1, 0 },
		{ x1, y1, 1, 1 },
		{ x0, y1, 0, 1 },
	};
	for (int i = 0; i < 6; i++) {
		append_s16(o, int(quad[i][0] * 4096.0f));
		append_s16(o, int(quad[i][1] * 4096.0f));
		append_s16(o, 0);
		append_u16(o, 0);
		append_s16(o, 0); // normal toward the viewer
		append_s16(o, 0);
		append_s16(o, -4096);
		append_u16(o, 0);
		append_s16(o, int(quad[i][2] * 4096.0f));
		append_s16(o, int(quad[i][3] * 4096.0f));
		o.push_back(0x80);
		o.push_back(0x80);
		o.push_back(0x80);
		o.push_back(0x80);
	}
	// one still frame
	append_u32(o, 0x01);
	append_u32(o, 1); // frame length
	append_f32(o, 1.0f); // speed
	append_u32(o, 0); // play offset
	append_u32(o, 1); // frames
	append_u32(o, 0); // shape
	append_u32(o, 1); // keys
	append_f32(o, 1.0f); // time
	append_f32(o, 1.0f); // value
	// texture: 128x128, 5 bits a channel with red lowest, top bit set (opaque)
	Ref<Image> img;
	if (p_img.is_valid() && !p_img->is_empty()) {
		img = p_img->duplicate();
		if (img->is_compressed()) {
			img->decompress();
		}
		img->convert(Image::FORMAT_RGBA8);
		img->resize(128, 128, Image::INTERPOLATE_BILINEAR);
	}
	for (int y = 0; y < 128; y++) {
		for (int x = 0; x < 128; x++) {
			Color c(0.30f, 0.45f, 0.80f, 1.0f);
			if (img.is_valid()) {
				c = img->get_pixel(x, y);
				// transparent parts take the browser's dark background colour
				c = Color(0.06f, 0.05f, 0.12f).lerp(Color(c.r, c.g, c.b), c.a);
			}
			const int r = CLAMP(int(c.r * 31.0f + 0.5f), 0, 31);
			const int g = CLAMP(int(c.g * 31.0f + 0.5f), 0, 31);
			const int b = CLAMP(int(c.b * 31.0f + 0.5f), 0, 31);
			append_u16(o, uint16_t(0x8000 | (b << 10) | (g << 5) | r));
		}
	}
	return o;
}

static Vector<uint8_t> mc_save_config(const CookOptions &p_opts) {
	ProjectSettings *ps = ProjectSettings::get_singleton();
	String name = ps ? String(ps->get("application/config/name")) : String();
	if (name.strip_edges().is_empty()) {
		name = "Blazium game";
	}
	// region letters as the BIOS browser groups saves: BA America, BE Europe
	String dir = (p_opts.region == 1 ? "BE" : "BA") + mc_ascii(name, 30, true);
	if (dir.length() <= 2) {
		dir += "BLAZIUM";
	}
	const String title = mc_ascii(name, 32, false);
	Ref<Image> icon;
	const String icon_path = ps ? String(ps->get("application/config/icon")) : String();
	if (!icon_path.is_empty() && ResourceLoader::exists(icon_path)) {
		Ref<Texture2D> tex = ResourceLoader::load(icon_path);
		if (tex.is_valid()) {
			icon = image_from_texture(tex);
		}
	}
	const Vector<uint8_t> ico = mc_icon(icon);
	Vector<uint8_t> o;
	o.push_back('M');
	o.push_back('C');
	o.push_back('S');
	o.push_back('V');
	append_u16(o, 1);
	append_u16(o, 76);
	const CharString d = dir.ascii();
	const CharString t = title.ascii();
	for (int i = 0; i < 32; i++) {
		o.push_back(i < d.length() ? uint8_t(d[i]) : 0);
	}
	for (int i = 0; i < 32; i++) {
		o.push_back(i < t.length() ? uint8_t(t[i]) : 0);
	}
	append_u32(o, uint32_t(ico.size()));
	o.append_array(ico);
	return o;
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

// Optional sections after MESH's index list (guests that predate them stop
// reading at the indices):
//   'INST' u32 n, n x { u16 node, u16 0, f32 m[12] }  row-major 3x4 delta
//   'NFOG' u32 n, n x u16 node, padded to 4 bytes
static void append_mesh_extras(const Gather &p_g, MeshBlob &r_mesh) {
	Vector<uint8_t> &o = r_mesh.bytes;
	if (!p_g.insts.is_empty() || !p_g.inst_src.is_empty()) {
		o.push_back('I');
		o.push_back('N');
		o.push_back('S');
		o.push_back('T');
		append_u32(o, uint32_t(p_g.insts.size()));
		for (const Gather::InstRow &r : p_g.insts) {
			append_u16(o, uint16_t(r.node_id));
			append_u16(o, 0);
			const Basis &b = r.delta.basis;
			for (int row = 0; row < 3; row++) {
				append_f32(o, b.rows[row].x);
				append_f32(o, b.rows[row].y);
				append_f32(o, b.rows[row].z);
				append_f32(o, r.delta.origin[row]);
			}
		}
		// a source with no instances left still has to stay hidden
		for (const int n : p_g.inst_src) {
			bool used = false;
			for (const Gather::InstRow &r : p_g.insts) {
				if (r.node_id == n) {
					used = true;
					break;
				}
			}
			if (!used) {
				o.push_back('I');
				o.push_back('N');
				o.push_back('S');
				o.push_back('T');
				append_u32(o, 1);
				append_u16(o, uint16_t(n));
				append_u16(o, 0);
				for (int k = 0; k < 12; k++) {
					append_f32(o, 0.0f); // all-zero matrix: drawn as nothing
				}
			}
		}
	}
	if (!p_g.nofog.is_empty()) {
		o.push_back('N');
		o.push_back('F');
		o.push_back('O');
		o.push_back('G');
		append_u32(o, uint32_t(p_g.nofog.size()));
		for (const int n : p_g.nofog) {
			append_u16(o, uint16_t(n));
		}
		if (p_g.nofog.size() & 1) {
			append_u16(o, 0);
		}
	}
}

static Error cook_gather_bins(const String &p_node, const String &p_mesh, const String &p_gtex, Gather &p_g, String &r_err, uint32_t *r_ee, uint32_t *r_gs) {
	ensure_placeholder_tex(p_g);
	const Vector<uint8_t> node_blob = write_node_blob(p_g);
	MeshBlob mesh;
	const Error merr = write_mesh_ee(p_g.tris, kMaxTris, mesh);
	if (merr != OK) {
		r_err = "MESH cook failed.";
		return merr;
	}
	append_mesh_extras(p_g, mesh);
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
	if (r_ee) {
		*r_ee = uint32_t(node_blob.size() + mesh.bytes.size() + gtex.bytes.size());
	}
	if (r_gs) {
		*r_gs = uint32_t(gtex.vram_bytes);
	}
	return OK;
}

static void append_i16(Vector<uint8_t> &o, int16_t v) {
	append_u16(o, uint16_t(v));
}

static Vector<uint8_t> write_cam_blob(const Gather &p_g) {
	Vector<uint8_t> out;
	out.push_back('C');
	out.push_back('A');
	out.push_back('M');
	out.push_back(' ');
	append_u16(out, uint16_t(kCookABI));
	append_u16(out, uint16_t(p_g.cams.size()));
	for (int i = 0; i < p_g.cams.size(); i++) {
		const Gather::CamRow &c = p_g.cams[i];
		append_u16(out, uint16_t(int16_t(c.node_id)));
		append_u16(out, c.flags);
		append_f32(out, c.pos.x);
		append_f32(out, c.pos.y);
		append_f32(out, c.pos.z);
		append_f32(out, c.euler.x);
		append_f32(out, c.euler.y);
		append_f32(out, c.euler.z);
		append_f32(out, c.fov);
	}
	return out;
}

static Vector<uint8_t> write_hit_blob(const Gather &p_g) {
	Vector<uint8_t> out;
	out.push_back('H');
	out.push_back('I');
	out.push_back('T');
	out.push_back(' ');
	append_u16(out, uint16_t(kCookABI));
	append_u16(out, uint16_t(p_g.hits.size()));
	for (int i = 0; i < p_g.hits.size(); i++) {
		const Gather::HitRow &h = p_g.hits[i];
		append_u16(out, uint16_t(int16_t(h.node_id)));
		out.push_back(h.kit);
		out.push_back(0);
		append_f32(out, h.mn.x);
		append_f32(out, h.mn.y);
		append_f32(out, h.mn.z);
		append_f32(out, h.mx.x);
		append_f32(out, h.mx.y);
		append_f32(out, h.mx.z);
		out.push_back(h.flags);
		out.push_back(h.kind);
		out.push_back(h.layer);
		append_u16(out, h.frame_lo);
		append_u16(out, h.frame_hi);
	}
	return out;
}

static Vector<uint8_t> write_sprt_blob(const Gather &p_g) {
	Vector<uint8_t> out;
	out.push_back('S');
	out.push_back('P');
	out.push_back('R');
	out.push_back('T');
	append_u16(out, uint16_t(kCookABI));
	append_u16(out, uint16_t(p_g.sprites.size()));
	for (int i = 0; i < p_g.sprites.size(); i++) {
		const Gather::SpriteRow &s = p_g.sprites[i];
		append_u16(out, uint16_t(int16_t(s.node_id)));
		append_u16(out, s.tex);
		append_i16(out, s.x);
		append_i16(out, s.y);
		append_i16(out, s.w);
		append_i16(out, s.h);
		out.push_back(s.flip);
		out.push_back(s.frame);
		append_i16(out, s.z);
	}
	return out;
}

static Vector<uint8_t> write_hud_blob(const Gather &p_g) {
	Vector<uint8_t> out;
	out.push_back('H');
	out.push_back('U');
	out.push_back('D');
	out.push_back(' ');
	append_u16(out, uint16_t(kCookABI));
	append_u16(out, uint16_t(p_g.hud.size()));
	for (int i = 0; i < p_g.hud.size(); i++) {
		const Gather::HudRow &h = p_g.hud[i];
		append_i16(out, h.x);
		append_i16(out, h.y);
		append_i16(out, h.w);
		append_i16(out, h.h);
		out.push_back(h.kind);
		out.push_back(h.r);
		out.push_back(h.g);
		out.push_back(h.b);
		CharString utf = h.text.utf8();
		for (int c = 0; c < 64; c++) {
			out.push_back(c < utf.length() ? uint8_t(utf[c]) : 0);
		}
	}
	return out;
}

static Vector<uint8_t> write_tile_blob(const Gather &p_g) {
	Vector<uint8_t> out;
	out.push_back('T');
	out.push_back('I');
	out.push_back('L');
	out.push_back('E');
	append_u16(out, uint16_t(kCookABI));
	append_u16(out, uint16_t(p_g.tiles.size()));
	out.push_back(1);
	out.push_back(1);
	for (int i = 0; i < p_g.tiles.size(); i++) {
		const Gather::TileRow &t = p_g.tiles[i];
		append_i16(out, t.x);
		append_i16(out, t.y);
		out.push_back(t.flags);
		out.push_back(t.atlas);
	}
	return out;
}

static Vector<uint8_t> write_nav_blob(const Gather &p_g) {
	Vector<uint8_t> out;
	out.push_back('N');
	out.push_back('A');
	out.push_back('V');
	out.push_back(' ');
	append_u16(out, uint16_t(kCookABI));
	const int nv = MIN(p_g.nav.size(), kMaxNav);
	const int ne = nv > 1 ? nv - 1 : 0;
	append_u16(out, uint16_t(nv));
	append_u16(out, uint16_t(ne));
	for (int i = 0; i < nv; i++) {
		append_f32(out, p_g.nav[i].pos.x);
		append_f32(out, p_g.nav[i].pos.y);
		append_f32(out, p_g.nav[i].pos.z);
		append_u16(out, uint16_t(int16_t(p_g.nav[i].node_id)));
		append_u16(out, 0);
	}
	for (int i = 0; i < ne; i++) {
		append_u16(out, uint16_t(i));
		append_u16(out, uint16_t(i + 1));
	}
	return out;
}

static Vector<uint8_t> write_part_blob(const Gather &p_g) {
	Vector<uint8_t> out;
	out.push_back('P');
	out.push_back('A');
	out.push_back('R');
	out.push_back('T');
	append_u16(out, uint16_t(kCookABI));
	append_u16(out, uint16_t(p_g.parts.size()));
	for (int i = 0; i < p_g.parts.size(); i++) {
		const Gather::PartRow &p = p_g.parts[i];
		append_u16(out, uint16_t(int16_t(p.node_id)));
		out.push_back(p.mode);
		out.push_back(p.tex);
		append_u16(out, p.maxp);
		append_f32(out, p.rate);
		append_f32(out, p.life);
		append_f32(out, p.stretch);
		append_u32(out, p.color);
	}
	return out;
}

static Vector<uint8_t> write_path_blob(const Gather &p_g) {
	Vector<uint8_t> out;
	out.push_back('P');
	out.push_back('A');
	out.push_back('T');
	out.push_back('H');
	append_u16(out, uint16_t(kCookABI));
	int npts = 0;
	for (int i = 0; i < p_g.paths.size(); i++) {
		npts += p_g.paths[i].pts.size();
	}
	if (npts > kMaxPathPoints) {
		npts = kMaxPathPoints;
	}
	append_u16(out, uint16_t(npts));
	int written = 0;
	for (int i = 0; i < p_g.paths.size() && written < npts; i++) {
		for (int p = 0; p < p_g.paths[i].pts.size() && written < npts; p++) {
			append_f32(out, p_g.paths[i].pts[p].x);
			append_f32(out, p_g.paths[i].pts[p].y);
			append_f32(out, p_g.paths[i].pts[p].z);
			append_u16(out, uint16_t(int16_t(p_g.paths[i].node_id)));
			append_u16(out, 0);
			written++;
		}
	}
	return out;
}

static Vector<uint8_t> write_navm_blob(const Gather &p_g) {
	Vector<uint8_t> out;
	out.push_back('N');
	out.push_back('A');
	out.push_back('V');
	out.push_back('M');
	append_u16(out, uint16_t(kCookABI));
	const int nv = MIN(p_g.navm_verts.size(), kMaxNavmVerts);
	const int nt = MIN(p_g.navm_tris.size(), kMaxNavmTris);
	append_u16(out, uint16_t(nv));
	append_u16(out, uint16_t(nt));
	for (int i = 0; i < nv; i++) {
		append_f32(out, p_g.navm_verts[i].pos.x);
		append_f32(out, p_g.navm_verts[i].pos.y);
		append_f32(out, p_g.navm_verts[i].pos.z);
	}
	for (int i = 0; i < nt; i++) {
		append_u16(out, p_g.navm_tris[i].a);
		append_u16(out, p_g.navm_tris[i].b);
		append_u16(out, p_g.navm_tris[i].c);
	}
	return out;
}

static Vector<uint8_t> write_vehl_blob(const Gather &p_g) {
	Vector<uint8_t> out;
	out.push_back('V');
	out.push_back('E');
	out.push_back('H');
	out.push_back('L');
	append_u16(out, uint16_t(kCookABI));
	append_u16(out, uint16_t(p_g.vehls.size()));
	for (int i = 0; i < p_g.vehls.size(); i++) {
		const Gather::VehlRow &v = p_g.vehls[i];
		append_u16(out, uint16_t(int16_t(v.node_id)));
		out.push_back(v.mode);
		out.push_back(0);
		append_f32(out, v.mass);
		append_f32(out, v.thrust);
		for (int w = 0; w < 4; w++) {
			append_f32(out, v.wx[w]);
		}
		for (int w = 0; w < 4; w++) {
			append_f32(out, v.wz[w]);
		}
	}
	return out;
}

static Vector<uint8_t> write_anim_blob(const Gather &p_g) {
	Vector<uint8_t> out;
	out.push_back('A');
	out.push_back('N');
	out.push_back('I');
	out.push_back('M');
	append_u16(out, uint16_t(kCookABI));
	append_u16(out, uint16_t(p_g.anims.size()));
	for (int i = 0; i < p_g.anims.size(); i++) {
		CharString utf = p_g.anims[i].name.utf8();
		for (int c = 0; c < 32; c++) {
			out.push_back(c < utf.length() ? uint8_t(utf[c]) : 0);
		}
		append_f32(out, p_g.anims[i].length);
		append_u16(out, uint16_t(p_g.anims[i].keys.size()));
		append_u16(out, uint16_t(int16_t(p_g.anims[i].node_id)));
		for (int k = 0; k < p_g.anims[i].keys.size(); k++) {
			const Gather::AnimKey &key = p_g.anims[i].keys[k];
			append_f32(out, key.t);
			append_f32(out, key.pos.x);
			append_f32(out, key.pos.y);
			append_f32(out, key.pos.z);
			append_f32(out, key.rot.x);
			append_f32(out, key.rot.y);
			append_f32(out, key.rot.z);
		}
	}
	return out;
}

static Vector<uint8_t> write_fmv_blob() {
	Vector<uint8_t> out;
	out.push_back('F');
	out.push_back('M');
	out.push_back('V');
	out.push_back(' ');
	append_u16(out, uint16_t(kCookABI));
	append_u16(out, 0);
	return out;
}

static Vector<uint8_t> write_stream_blob(int p_extra_count, const Vector<String> &p_paths, const Vector<uint32_t> &p_ee, const Vector<uint32_t> &p_gs) {
	Vector<uint8_t> out;
	out.push_back('S');
	out.push_back('T');
	out.push_back('R');
	out.push_back('M');
	append_u16(out, uint16_t(kCookABI));
	append_u16(out, uint16_t(p_extra_count));
	for (int i = 1; i <= p_extra_count; i++) {
		append_u16(out, uint16_t(i));
	}
	for (int i = 0; i < p_extra_count; i++) {
		const String path = i < p_paths.size() ? p_paths[i] : String();
		CharString utf = path.utf8();
		const int n = MIN(127, utf.length());
		out.push_back(uint8_t(n));
		for (int c = 0; c < n; c++) {
			out.push_back(uint8_t(utf[c]));
		}
		append_u32(out, i < p_ee.size() ? p_ee[i] : 0);
		append_u32(out, i < p_gs.size() ? p_gs[i] : 0);
	}
	return out;
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
		r_warn.push_back(p_pack + "AnimationPlayer cooks Node3D TRS keys on PS2 ABI 1 (no AnimationTree / blend spaces).");
	}
	if (p_g.skeletons > 0) {
		r_warn.push_back(p_pack + "Skeleton3D skin is not cooked on PS2 ABI 1 (bind pose). Remove the skeleton or use AnimationPlayer on Node3D.");
	}
	if (p_g.lights > 0) {
		r_warn.push_back(p_pack + "Light3D is allow-listed; PS2 ABI 1 has no GTE lights (flat/textured vertex color).");
	}
	if (p_g.audio_players > 0) {
		r_warn.push_back(p_pack + "AudioStreamPlayer is allow-listed; SFX00 is the first res:// .wav, not a per-node bank.");
	}
	if (p_g.cameras > kMaxCameras) {
		r_warn.push_back(vformat("%skipped first %d of %d cameras (guest uses the first Camera3D).", p_pack, kMaxCameras, p_g.cameras));
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

static bool walk_music_wav(const String &p_dir, Ref<AudioStreamWAV> &r_wav) {
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
			if (walk_music_wav(path, r_wav)) {
				return true;
			}
		} else if (n.to_lower().ends_with(".wav") && path.to_lower().contains("music") && ResourceLoader::exists(path)) {
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
	collect_lights(root, gathered);
	walk_scene(root, gathered, -1, main_scene);

	r_out.pack_count = 1;
	const String main_abs = ProjectSettings::get_singleton()->globalize_path(main_scene);
	Vector<String> extra_scenes;
	for (int i = 0; i < scripts.extra_scenes.size(); i++) {
		const String extra = scripts.extra_scenes[i];
		if (extra != main_scene && !extra_scenes.has(extra)) {
			extra_scenes.push_back(extra);
		}
	}
	for (int i = 0; i < gathered.kit_scenes.size(); i++) {
		const String extra = gathered.kit_scenes[i];
		if (extra != main_scene && !extra_scenes.has(extra)) {
			extra_scenes.push_back(extra);
		}
	}

	Vector<Gather> extra_gathers;
	Vector<String> cooked_extras;
	Vector<uint32_t> extra_ee;
	Vector<uint32_t> extra_gs;
	Vector<Node *> extra_live;
	for (int i = 0; i < extra_scenes.size(); i++) {
		const String extra = extra_scenes[i];
		if (r_out.pack_count >= kMaxScenePacks) {
			r_out.warnings.push_back("kept first 99 scene packs.");
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
		extra_live.push_back(er);
		Gather one;
		collect_lights(er, one);
		walk_scene(er, one, -1, extra);
		for (int k = 0; k < one.kit_scenes.size(); k++) {
			const String more = one.kit_scenes[k];
			if (more != main_scene && !extra_scenes.has(more)) {
				extra_scenes.push_back(more);
			}
		}
		r_out.extra_pack_files.push_back(ProjectSettings::get_singleton()->globalize_path(extra));
		cooked_extras.push_back(extra);
		extra_gathers.push_back(one);
		r_out.pack_count++;
	}

	r_out.errors.append_array(gathered.errors);
	r_out.warnings.append_array(gathered.warnings);
	append_stub_warnings(gathered, r_out.warnings, "");
	for (int i = 0; i < extra_gathers.size(); i++) {
		r_out.errors.append_array(extra_gathers[i].errors);
		r_out.warnings.append_array(extra_gathers[i].warnings);
		append_stub_warnings(extra_gathers.write[i], r_out.warnings, vformat("pack %d: ", i + 1));
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
	for (int i = 0; i < extra_gathers.size(); i++) {
		const Gather &eg = extra_gathers[i];
		if (eg.node_dropped > 0) {
			r_out.warnings.push_back(vformat("pack %d: kept first %d of %d nodes (dropped %d).", i + 1, kMaxNodes, kMaxNodes + eg.node_dropped, eg.node_dropped));
		}
		if (eg.tri_dropped > 0) {
			r_out.warnings.push_back(vformat("pack %d: kept first %d of %d tris (dropped %d).", i + 1, kMaxTris, kMaxTris + eg.tri_dropped, eg.tri_dropped));
		}
		if (eg.tex_dropped > 0) {
			r_out.warnings.push_back(vformat("pack %d: kept first %d textures (dropped %d).", i + 1, kMaxTextures, eg.tex_dropped));
		}
	}

	// Per-pack sidecars: do not merge extra CAM/HIT/HUD into pack 0.

	auto free_live = [&]() {
		for (int i = 0; i < live.size(); i++) {
			memdelete(live[i]);
		}
		for (int i = 0; i < extra_live.size(); i++) {
			memdelete(extra_live[i]);
		}
	};

	bool extra_had_errors = false;
	for (int i = 0; i < extra_gathers.size(); i++) {
		if (!extra_gathers[i].errors.is_empty()) {
			extra_had_errors = true;
			break;
		}
	}
	if (!gathered.errors.is_empty() || extra_had_errors) {
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

	// Compiled scripts (GDIR): every pack's scripted nodes, one program.
	{
		Vector<GdirPack> gpacks;
		auto to_pack = [](const Gather &g, int p_pack, const String &p_scene) {
			GdirPack gp;
			gp.pack = p_pack;
			gp.scene = p_scene;
			gp.nodes = g.gnodes;
			gp.has_clear = g.has_clear;
			gp.clear = g.clear;
			gp.camera = g.camera_node;
			gp.fov = g.camera_fov;
			for (const Gather::PendingConn &pc : g.pending_conns) {
				const int *t = g.node_ids.getptr(pc.target);
				if (!t) {
					continue;
				}
				GdirConn c;
				c.src = pc.src;
				c.signal = pc.signal;
				c.target = *t;
				c.method = pc.method;
				gp.conns.push_back(c);
			}
			return gp;
		};
		gpacks.push_back(to_pack(gathered, 0, main_scene));
		for (int i = 0; i < extra_gathers.size(); i++) {
			gpacks.push_back(to_pack(extra_gathers[i], i + 1, cooked_extras[i]));
		}
		GdirResult gr;
		gdir_compile(gpacks, gr);
		r_out.warnings.append_array(gr.warnings);
		const bool require = GLOBAL_GET("ps2/scripts/require_compiled");
		if (!gr.errors.is_empty()) {
			if (require) {
				r_out.errors.append_array(gr.errors);
				r_out.report = gr.errors[0];
				free_live();
				return ERR_UNCONFIGURED;
			}
			for (const String &e : gr.errors) {
				r_out.warnings.push_back("script not compiled (keyword tape used instead): " + e);
			}
		} else if (!gr.blob.is_empty()) {
			// SCRP flag bit 0: a GDIR program follows the tape.
			if (scripts.scrp.size() >= 12) {
				scripts.scrp.write[6] |= 1;
			}
			scripts.scrp.append_array(gr.blob);
			scripts.ir_text += vformat("GDIR v1: %d scripts, %d functions, %d instances, %d bytes\n", gr.classes, gr.functions, gr.instances, gr.blob.size());
			scripts.ir_text += gr.listing;
			r_out.infos.push_back(vformat("compiled %d scripts (%d functions) for %d nodes into GDIR (%d bytes).", gr.classes, gr.functions, gr.instances, gr.blob.size()));
		}
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
	append_mesh_extras(gathered, mesh);

	Vector<GsTex> texs;
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

	// PS2.save_slot's directory, title and icon (read by the guest's mc_io)
	if (write_bytes(p_export_dir.path_join("MCSAVE.BIN"), mc_save_config(p_opts)) != OK) {
		r_out.warnings.push_back("failed to write MCSAVE.BIN; memory card saves get a generic name and no icon.");
	}

	if (extra_gathers.size() >= 80) {
		r_out.warnings.push_back(vformat("STREAM catalog has %d extras (80%% of 99). EE 24 MiB / GS 4 MiB are shared — unload rooms/NPCs you are not using.", extra_gathers.size()));
	}

	if (!extra_gathers.is_empty()) {
		for (int i = 0; i < extra_gathers.size(); i++) {
			if (extra_gathers[i].images.is_empty()) {
				Ref<Image> placeholder = Image::create_empty(16, 16, false, Image::FORMAT_RGBA8);
				placeholder->fill(Color(0.2f, 0.35f, 0.55f));
				extra_gathers.write[i].images.push_back(placeholder);
				extra_gathers.write[i].image_names.push_back("placeholder");
			}
			const int pack = i + 1;
			const String nodep = p_export_dir.path_join(vformat("NODE%02d.bin", pack));
			const String meshp = p_export_dir.path_join(vformat("MESH%02d.bin", pack));
			const String gtexp = p_export_dir.path_join(vformat("GTEX%02d.bin", pack));
			String pack_err;
			uint32_t pack_ee = 0;
			uint32_t pack_gs = 0;
			if (cook_gather_bins(nodep, meshp, gtexp, extra_gathers.write[i], pack_err, &pack_ee, &pack_gs) != OK) {
				r_out.errors.push_back(pack_err.is_empty() ? vformat("pack-%d cook failed.", pack) : pack_err);
				r_out.report = r_out.errors[r_out.errors.size() - 1];
				free_live();
				return ERR_CANT_CREATE;
			}
			const Gather &eg = extra_gathers[i];
			struct PackSys {
				String name;
				Vector<uint8_t> bytes;
			};
			PackSys psys[] = {
				{ vformat("CAM%02d.bin", pack), write_cam_blob(eg) },
				{ vformat("HIT%02d.bin", pack), write_hit_blob(eg) },
				{ vformat("HUD%02d.bin", pack), write_hud_blob(eg) },
				{ vformat("TILE%02d.bin", pack), write_tile_blob(eg) },
				{ vformat("NAV%02d.bin", pack), write_nav_blob(eg) },
				{ vformat("ANIM%02d.bin", pack), write_anim_blob(eg) },
				{ vformat("SPRT%02d.bin", pack), write_sprt_blob(eg) },
				{ vformat("PART%02d.bin", pack), write_part_blob(eg) },
				{ vformat("PATH%02d.bin", pack), write_path_blob(eg) },
				{ vformat("NAVM%02d.bin", pack), write_navm_blob(eg) },
				{ vformat("VEHL%02d.bin", pack), write_vehl_blob(eg) },
			};
			for (PackSys &s : psys) {
				pack_ee += uint32_t(s.bytes.size());
				if (write_bytes(p_export_dir.path_join(s.name), s.bytes) != OK) {
					r_out.errors.push_back("failed to write " + s.name + ".");
					r_out.report = r_out.errors[r_out.errors.size() - 1];
					free_live();
					return ERR_CANT_CREATE;
				}
			}
			if (pack_ee > uint32_t(kMaxExtraPackEe)) {
				r_out.over_budget = true;
				r_out.errors.push_back(vformat("extra pack %d EE %d exceeds 2 MiB STREAM cap.", pack, pack_ee));
				r_out.report = r_out.errors[r_out.errors.size() - 1];
				free_live();
				return ERR_OUT_OF_MEMORY;
			}
			extra_ee.push_back(pack_ee);
			extra_gs.push_back(pack_gs);
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
		r_out.stream_path = p_export_dir.path_join("STREAM.bin");
		if (write_bytes(r_out.stream_path, write_stream_blob(extra_gathers.size(), cooked_extras, extra_ee, extra_gs)) != OK) {
			r_out.errors.push_back("failed to write STREAM.bin.");
			r_out.report = r_out.errors[r_out.errors.size() - 1];
			free_live();
			return ERR_CANT_CREATE;
		}
		r_out.warnings.push_back("STREAM packs (MESH01../NODE01.. plus CAM/HIT/HUD/TILE/NAV/ANIM/SPRT/PART/PATH/NAVM/VEHL per pack) are not ELF-embedded. Guest loads them from host: (PCSX2 HostFs) then cdrom0: (ISO 8.3). Extra packs are capped at 2 MiB EE. EE 24 MiB and GS 4 MiB are shared — unload rooms/NPCs you are not using. This is whole-file STREAM, not XA/IPU. Use ps2/make_iso or output_kind EE ELF + ISO.");
	}

	{
		SfxBlob sfx;
		Vector<int16_t> pcm;
		int rate = 22050;
		const PackedStringArray bank = GLOBAL_GET("ps2/audio/sfx_bank");
		Error serr = OK;
		if (!bank.is_empty()) {
			// the project's sound list, in order: PS2.play_sfx(i) / ("res://...")
			Vector<SfxClip> clips;
			for (int i = 0; i < bank.size(); i++) {
				SfxClip clip;
				Ref<AudioStreamWAV> bw;
				if (ResourceLoader::exists(bank[i])) {
					bw = ResourceLoader::load(bank[i]);
				}
				pcm_from_wav(bw, clip.pcm, clip.sample_rate);
				if (clip.pcm.is_empty()) {
					r_out.warnings.push_back(vformat("ps2/audio/sfx_bank[%d] %s is not a 8/16-bit PCM .wav (re-import with compression off); it plays as silence.", i, bank[i]));
					clip.pcm.resize(64);
					clip.pcm.fill(0);
				}
				clips.push_back(clip);
			}
			serr = write_sfx_bank(clips, sfx);
		} else {
			Ref<AudioStreamWAV> wav;
			walk_first_wav("res://", wav);
			pcm_from_wav(wav, pcm, rate);
			serr = write_sfx_pcm(pcm, rate, sfx);
		}
		if (serr != OK) {
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
		r_out.warnings.push_back("SFX00 IMA is decoded on the EE then VAG-uploaded via libsdr RPC (sdrdrv + freesd). Do not call sceSdInit. Missing IRX/HostFs leaves PS2.play_sfx / Cross silent. SFX is not ELF-embedded; keep SFX00.bin next to the ELF (host:) or on the ISO.");
		SfxBlob music;
		Vector<int16_t> mpcm = pcm;
		int mrate = rate;
		Ref<AudioStreamWAV> mwav;
		if (walk_music_wav("res://", mwav)) {
			pcm_from_wav(mwav, mpcm, mrate);
		}
		if (write_music_pcm(mpcm, mrate, music) != OK) {
			r_out.errors.push_back("MUSIC cook failed.");
			r_out.report = r_out.errors[r_out.errors.size() - 1];
			free_live();
			return FAILED;
		}
		r_out.music_path = p_export_dir.path_join("MUSIC00.bin");
		if (write_bytes(r_out.music_path, music.bytes) != OK) {
			r_out.errors.push_back("failed to write MUSIC00.bin.");
			r_out.report = r_out.errors[r_out.errors.size() - 1];
			free_live();
			return ERR_CANT_CREATE;
		}
		r_out.warnings.push_back("MUSIC00 IMA is decoded on the EE then looped VAG-uploaded on SPU2 voice 1 via libsdr RPC (sdrdrv + freesd). Do not call sceSdInit. Missing IRX/HostFs leaves PS2.load_music silent. MUSIC is not ELF-embedded; keep MUSIC00.bin next to the ELF (host:) or on the ISO.");
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
		struct SysOut {
			String name;
			String *path;
			Vector<uint8_t> bytes;
		};
		SysOut sys[] = {
			{ "CAM00.bin", &r_out.cam_path, write_cam_blob(gathered) },
			{ "HIT00.bin", &r_out.hit_path, write_hit_blob(gathered) },
			{ "HUD00.bin", &r_out.hud_path, write_hud_blob(gathered) },
			{ "TILE00.bin", &r_out.tile_path, write_tile_blob(gathered) },
			{ "NAV00.bin", &r_out.nav_path, write_nav_blob(gathered) },
			{ "ANIM00.bin", &r_out.anim_path, write_anim_blob(gathered) },
			{ "SPRT00.bin", &r_out.sprt_path, write_sprt_blob(gathered) },
			{ "PART00.bin", &r_out.part_path, write_part_blob(gathered) },
			{ "PATH00.bin", &r_out.path_path, write_path_blob(gathered) },
			{ "NAVM00.bin", &r_out.navm_path, write_navm_blob(gathered) },
			{ "VEHL00.bin", &r_out.vehl_path, write_vehl_blob(gathered) },
			{ "FMV00.bin", &r_out.fmv_path, write_fmv_blob() },
		};
		for (SysOut &s : sys) {
			*s.path = p_export_dir.path_join(s.name);
			if (write_bytes(*s.path, s.bytes) != OK) {
				r_out.errors.push_back("failed to write " + s.name + ".");
				r_out.report = r_out.errors[r_out.errors.size() - 1];
				free_live();
				return ERR_CANT_CREATE;
			}
		}
		if (gathered.hit_dropped > 0) {
			r_out.warnings.push_back(vformat("kept first %d HIT AABBs (dropped %d).", kMaxHits, gathered.hit_dropped));
		}
		if (gathered.hud_dropped > 0) {
			r_out.warnings.push_back(vformat("kept first %d HUD widgets (dropped %d).", kMaxHud, gathered.hud_dropped));
		}
		if (gathered.anims.is_empty() && gathered.anim_players > 0) {
			r_out.warnings.push_back("ANIM clips are names/durations only on PS2 ABI 1 (bind pose; no bone palette).");
		}
		r_out.warnings.push_back("FMV/IPU is not shipped on PS2 ABI 1 (no license-clean decoder). PS2.play_fmv is a named no-op.");
		r_out.warnings.push_back("HUD/TILE/NAV/CAM/HIT/ANIM/SPRT sit next to the ELF (host:) or on the ISO; they are not ELF-embedded.");
		r_out.warnings.push_back("CAM00 look-at is Camera3D local -Z (Godot YXZ) plus FOV; guest gs_draw_set_camera applies them. current Camera3D is the default. VU1 is optional (dvp-as + packet2) else CPU GIF.");
		r_out.warnings.push_back("PS2.set_camera / set_default_camera / make_current / next_camera / prev_camera switch baked CAM slots. L1/R1 still cycle. look_camera / shake_camera are runtime CAM.");
		r_out.warnings.push_back("create_tween / create_timer are 16 linear slots (volume kind drives MUSIC). spawn_shot and play_xa stay refused.");
		r_out.warnings.push_back("PART/PATH/NAVM/VEHL are RAM-gated sidecars. Prefetch checks host: then cdrom0: plus can_fit. Camera2D is CAM flag 2 (ortho).");
		r_out.warnings.push_back("HIT00 AABBs clip PS2.move_and_slide / stick translate (enter-only). spawn_shot stays refused.");
		r_out.warnings.push_back("Kit volumes fire on HIT enter (overlap kit, not Cross-only). CharacterBody2D/3D cook as player kit 4 and auto-slide. export/guest is the resolved tree (embed + overlay + extra/*.cpp).");
		r_out.warnings.push_back("Spawn teleports from NODE xyz. Checkpoint stores xyz (not memcard). Start pauses. Button HUD text action|label (pause/save/load/respawn/say/portal). Gravity+jump when no HUD focus.");
		r_out.warnings.push_back("PS2.overlaps / raycast / tile_solid_at query HIT00 AABBs and TILE00 cells. spawn_shot stays refused.");
		r_out.warnings.push_back("PS2.set_fade / change_scene_fade are a GS alpha overlay; PS2.set_fog is GS distance fog. spawn_shot stays refused.");
		r_out.warnings.push_back("play_xa is not supported on PS2 ABI 1 (no CD-XA; use PS2.play_sfx / load_music IMA). play_fmv has no license-clean IPU decoder.");
		r_out.warnings.push_back("Button HUD slots use D-pad focus and ui_accept (Cross). Not Godot theme/CSS. Overflow drops extras (16 slots).");
		r_out.warnings.push_back("WAV-only audio cook (IMA then VAG). STR/XA encode is refused.");
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
		names->store_line(vformat("COST00 ee=%d gs=%d", r_out.ee_ram_bytes, r_out.vram_bytes));
		if (main_scene.to_lower().contains("industrial") || main_abs.to_lower().contains("industrial")) {
			names->store_line("fixture: industrial");
		}
		for (int i = 0; i < cooked_extras.size(); i++) {
			const int pack = i + 1;
			names->store_line(vformat("pack %d = %s", pack, cooked_extras[i]));
			String line = cooked_extras[i] + vformat(" -> NODE%02d.bin MESH%02d.bin GTEX%02d.bin CAM%02d.bin HIT%02d.bin HUD%02d.bin SPRT%02d.bin", pack, pack, pack, pack, pack, pack, pack);
			if (pack == 1) {
				line += " PACK01.bin";
			}
			names->store_line(line);
			const uint32_t ee = i < extra_ee.size() ? extra_ee[i] : 0;
			const uint32_t gs = i < extra_gs.size() ? extra_gs[i] : 0;
			names->store_line(vformat("COST%02d ee=%d gs=%d", pack, ee, gs));
			if (cooked_extras[i].to_lower().contains("spawn_kit")) {
				names->store_line("fixture: spawn_kit");
			}
			if (cooked_extras[i].to_lower().contains("kit_room2")) {
				names->store_line("fixture: kit_room2");
			}
			if (cooked_extras[i].to_lower().contains("systems_kit")) {
				names->store_line("fixture: systems_kit");
			}
			if (cooked_extras[i].to_lower().contains("platformer")) {
				names->store_line("fixture: platformer");
			}
			if (cooked_extras[i].to_lower().contains("adventure")) {
				names->store_line("fixture: adventure");
			}
			if (cooked_extras[i].to_lower().contains("shmup")) {
				names->store_line("fixture: shmup");
			}
		}
		if (!r_out.stream_path.is_empty()) {
			names->store_line(vformat("stream: STREAM.bin packs %d", extra_gathers.size()));
		}
		if (!extra_gathers.is_empty()) {
			names->store_line("kit: Spawn Checkpoint Portal Player Hazard Pickup Music Unload Load Talk Spawner Save");
		}
		names->store_line("guest: export/guest is the resolved tree (embed + overlay + extra/*.cpp)");
		if (!r_out.sfx_path.is_empty()) {
			names->store_line("sfx: SFX00.bin");
		}
		if (!r_out.music_path.is_empty()) {
			names->store_line("music: MUSIC00.bin");
		}
		if (!r_out.disp_path.is_empty()) {
			names->store_line(vformat("disp: DISP.bin %dx%d %s", r_out.display_w, r_out.display_h, p_opts.region == 1 ? "PAL" : "NTSC"));
		}
		if (scripts.ir_text.contains("user://") || scripts.ir_text.contains("memcard")) {
			names->store_line("user:// -> host:SAVE.BIN host:SAVE0.BIN mc0:SAVE.BIN");
		}
		if (!r_out.cam_path.is_empty()) {
			names->store_line(vformat("cam: CAM00.bin %d look+fov", gathered.cams.size()));
		}
		if (!r_out.hit_path.is_empty()) {
			names->store_line(vformat("hit: HIT00.bin %d", gathered.hits.size()));
		}
		if (!r_out.hud_path.is_empty()) {
			names->store_line(vformat("hud: HUD00.bin %d", gathered.hud.size()));
		}
		if (!r_out.tile_path.is_empty()) {
			names->store_line(vformat("tile: TILE00.bin %d", gathered.tiles.size()));
		}
		if (!r_out.nav_path.is_empty()) {
			names->store_line(vformat("nav: NAV00.bin %d", gathered.nav.size()));
		}
		if (!r_out.anim_path.is_empty()) {
			names->store_line(vformat("anim: ANIM00.bin %d", gathered.anims.size()));
		}
		if (!r_out.fmv_path.is_empty()) {
			names->store_line("fmv: FMV00.bin (IPU not shipped)");
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
			budget->store_line(vformat("packs %d", r_out.pack_count));
			budget->store_line(vformat("cams %d/%d", gathered.cams.size(), kMaxCameras));
			budget->store_line(vformat("hits %d/%d", gathered.hits.size(), kMaxHits));
			budget->store_line(vformat("hud %d/%d", gathered.hud.size(), kMaxHud));
			budget->store_line(vformat("tiles %d/%d", gathered.tiles.size(), kMaxTiles));
			budget->store_line(vformat("nav %d/%d", gathered.nav.size(), kMaxNav));
			budget->store_line(vformat("anims %d/%d", gathered.anims.size(), kMaxAnim));
			if (!r_out.music_path.is_empty()) {
				budget->store_line("music 1");
			}
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
