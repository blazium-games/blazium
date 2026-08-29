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
#include "scene/2d/animated_sprite_2d.h"
#include "scene/2d/audio_stream_player_2d.h"
#include "scene/2d/camera_2d.h"
#include "scene/2d/cpu_particles_2d.h"
#include "scene/2d/gpu_particles_2d.h"
#include "scene/2d/light_2d.h"
#include "scene/2d/line_2d.h"
#include "scene/2d/marker_2d.h"
#include "scene/2d/mesh_instance_2d.h"
#include "scene/2d/multimesh_instance_2d.h"
#include "scene/2d/node_2d.h"
#include "scene/2d/parallax_2d.h"
#include "scene/2d/parallax_background.h"
#include "scene/2d/parallax_layer.h"
#include "scene/2d/physics/area_2d.h"
#include "scene/2d/physics/collision_polygon_2d.h"
#include "scene/2d/physics/collision_shape_2d.h"
#include "scene/2d/polygon_2d.h"
#include "scene/2d/sprite_2d.h"
#include "scene/3d/sprite_3d.h"
#include "scene/2d/tile_map.h"
#include "scene/2d/tile_map_layer.h"
#include "scene/3d/audio_stream_player_3d.h"
#include "scene/3d/camera_3d.h"
#include "scene/3d/cpu_particles_3d.h"
#include "scene/3d/gpu_particles_3d.h"
#include "scene/3d/importer_mesh_instance_3d.h"
#include "scene/3d/label_3d.h"
#include "scene/3d/light_3d.h"
#include "scene/3d/marker_3d.h"
#include "scene/3d/multimesh_instance_3d.h"
#include "scene/3d/physics/area_3d.h"
#include "scene/3d/physics/collision_polygon_3d.h"
#include "scene/3d/physics/collision_shape_3d.h"
#include "scene/3d/physics/vehicle_body_3d.h"
#include "scene/3d/world_environment.h"
#include "scene/resources/3d/importer_mesh.h"
#include "scene/resources/environment.h"
#include "scene/resources/multimesh.h"
#include "scene/gui/button.h"
#include "scene/gui/check_box.h"
#include "scene/gui/check_button.h"
#include "scene/gui/color_rect.h"
#include "scene/gui/control.h"
#include "scene/gui/label.h"
#include "scene/gui/link_button.h"
#include "scene/gui/option_button.h"
#include "scene/gui/panel.h"
#include "scene/gui/panel_container.h"
#include "scene/gui/base_button.h"
#include "scene/gui/progress_bar.h"
#include "scene/gui/range.h"
#include "scene/gui/slider.h"
#include "scene/gui/texture_button.h"
#include "scene/gui/texture_rect.h"
#include "scene/gui/video_stream_player.h"
#include "scene/3d/mesh_instance_3d.h"
#include "scene/3d/node_3d.h"
#include "scene/main/canvas_layer.h"
#include "scene/animation/animation_player.h"
#include "scene/3d/skeleton_3d.h"
#include "scene/resources/animation.h"
#include "scene/audio/audio_stream_player.h"
#include "scene/resources/2d/capsule_shape_2d.h"
#include "scene/resources/2d/circle_shape_2d.h"
#include "scene/resources/2d/rectangle_shape_2d.h"
#include "scene/resources/2d/tile_set.h"
#include "scene/resources/3d/box_shape_3d.h"
#include "scene/resources/3d/capsule_shape_3d.h"
#include "scene/resources/3d/sphere_shape_3d.h"
#include "scene/resources/sprite_frames.h"
#include "scene/resources/audio_stream_wav.h"
#include "scene/resources/material.h"
#include "scene/resources/mesh.h"
#include "scene/resources/packed_scene.h"
#include "core/object/script_language.h"
#include "scene/resources/texture.h"

#include <cstdio>
#include <ctime>

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
	Vector<AnimatedSprite2D *> anim_sprites;
	Vector<Sprite3D *> sprites3d;
	Vector<Ref<AudioStreamWAV>> extra_wavs;
	Vector<String> extra_wav_names;
	int sprite_count = 0;
	int aabb_dropped = 0;
	int cam_dropped = 0;
	struct CamRow {
		int node_id = -1;
		String name;
		bool is_default = false;
		Vector3 pos;
		Vector3 euler;
		uint8_t dim = 3;
		uint8_t drag = 255;
		uint8_t dead = 0;
		int16_t lim_l = 0, lim_t = 0, lim_r = 0, lim_b = 0;
	};
	Vector<CamRow> cameras;
	struct HitRow {
		int node_id = 0;
		uint8_t dim = 3;
		uint8_t kind = 0;
		uint8_t flags = 1;
		uint8_t layer = 1;
		Vector3 mn;
		Vector3 mx;
	};
	struct TextLine {
		String name;
		String text;
	};
	Vector<TextLine> text_lines;
	Vector<HitRow> hits;
	Vector<CookLight> lights;
	int light_count = 0;
	int animation_player_count = 0;
	bool had_skeleton = false;
	bool had_anim_skip = false;
	bool had_anim_tree = false;
	Vector<Node *> node_ptrs;
	Vector<AnimationPlayer *> anim_players;
	HashSet<String> uncooked_types;
	Vector<String> attached_scripts;
	VideoStreamPlayer *video = nullptr;
	int extra_wav = 0;
	int extra_video = 0;
	bool had_omni_or_spot = false;
	bool had_generic_light = false;
	Vector<String> *shared_groups = nullptr;
	Vector<String> group_names;
	int groups_dropped = 0;
	int slope_tiles = 0;
	bool had_body = false;
	bool had_csg = false;
	bool had_gridmap = false;
	bool had_multimesh = false;
	bool had_draw2d = false;
	bool had_label3d = false;
	bool had_world_env = false;
	bool fog_from_env = false;
	uint8_t fog_on = 0;
	int fog_start = 0;
	int fog_end = 2048;
	uint8_t fog_r = 32, fog_g = 0, fog_b = 48;
	int stream_count = 0;
	int stream_dropped = 0;
	int node_dropped = 0;
	int mesh_on_dropped = 0;
	int no_albedo_surfaces = 0;
	int extra_pbr = 0;
	int hud_dropped = 0;
	int tile_dropped = 0;
	int tilemap_count = 0;
	int canvas_layer_count = 0;
	bool had_control = false;
	struct HudRow {
		int16_t x = 0, y = 0, w = 16, h = 16;
		uint8_t kind = 0;
		uint8_t tex = 0xff;
		uint8_t rgb = uint8_t((3 << 5) | (3 << 2) | 1);
		uint8_t node_id = 0;
		uint8_t flags = 1;
		int16_t value = 0, vmin = 0, vmax = 100;
		String text;
		Vector<String> items;
	};
	Vector<HudRow> hud;
	struct TileRow {
		int16_t x = 0, y = 0;
		uint8_t u = 0, v = 0;
		uint8_t tex = 0;
		uint8_t node_id = 0;
		uint8_t flags = 0;
	};
	Vector<TileRow> tiles;
	uint8_t tile_w = 16;
	uint8_t tile_h = 16;
	struct CookedNode {
		int parent = -1;
		String name;
		uint8_t type = 0;
		uint8_t flags = 1;
		Vector3 pos;
		Vector3 euler;
		uint16_t tri_lo = 0;
		uint16_t tri_hi = 0;
		int16_t sprite_index = -1;
		int16_t script_index = -1;
		uint8_t group = 0;
		uint8_t scroll = 0;
		uint8_t ysort = 0;
		uint8_t kind = 0;
		uint8_t path_id = 0;
		String script_path;
	};
	Vector<CookedNode> nodes;
	struct WayRow {
		uint8_t node = 0;
		uint8_t path_id = 0;
	};
	Vector<WayRow> ways;
	int ways_dropped = 0;
	struct PathRow {
		uint8_t id = 0;
		uint8_t npts = 0;
		uint8_t closed = 0;
		Vector3 pts[16];
	};
	Vector<PathRow> paths;
	int paths_dropped = 0;
	struct NavVert {
		Vector3 pos;
		int node = -1;
	};
	Vector<NavVert> nav_verts;
	struct NavEdge {
		uint8_t a = 0;
		uint8_t b = 0;
	};
	Vector<NavEdge> nav_edges;
	int nav_dropped = 0;
	struct NavObs {
		Vector3 pos;
		float radius = 1.0f;
	};
	Vector<NavObs> nav_obs;
	int16_t canvas_ox = 0, canvas_oy = 0;
	int timer_count = 0;
	int ray_count = 0;
	bool had_nav = false;
	bool had_path = false;
	bool had_tween_note = false;
};

static const char *kAllowed[] = {
	"Node", "Node3D", "MeshInstance3D", "ImporterMeshInstance3D", "Camera3D", "Camera2D", "AnimationPlayer", "Skeleton3D",
	"Sprite2D", "AnimatedSprite2D", "Sprite3D", "Node2D", "Area2D", "Area3D",
	"CollisionShape2D", "CollisionShape3D", "CollisionPolygon2D", "CollisionPolygon3D",
	"VehicleBody3D", "VehicleBody2D",
	"CharacterBody2D", "CharacterBody3D", "StaticBody2D", "StaticBody3D",
	"AnimatableBody2D", "AnimatableBody3D", "RigidBody2D", "RigidBody3D",
	"PhysicalBone2D", "PhysicalBone3D",
	"GPUParticles3D", "GPUParticles2D", "CPUParticles3D", "CPUParticles2D",
	"AudioStreamPlayer", "AudioStreamPlayer3D", "AudioStreamPlayer2D",
	"DirectionalLight3D", "OmniLight3D", "SpotLight3D", "Light3D",
	"Light2D", "PointLight2D", "DirectionalLight2D",
	"MeshInstance2D", "Polygon2D", "Line2D", "Label3D",
	"MultiMeshInstance3D", "MultiMeshInstance2D",
	"CSGShape3D", "CSGCombiner3D", "CSGBox3D", "CSGSphere3D", "CSGCylinder3D", "CSGMesh3D", "CSGPolygon3D", "CSGTorus3D",
	"GridMap", "Marker2D", "Marker3D", "PathFollow2D", "PathFollow3D",
	"Path2D", "Path3D",
	"NavigationRegion2D", "NavigationRegion3D", "NavigationAgent2D", "NavigationAgent3D",
	"NavigationLink2D", "NavigationLink3D", "NavigationObstacle2D", "NavigationObstacle3D",
	"RayCast2D", "RayCast3D", "ShapeCast2D", "ShapeCast3D",
	"VisibleOnScreenNotifier2D", "VisibleOnScreenNotifier3D",
	"VisibleOnScreenEnabler2D", "VisibleOnScreenEnabler3D",
	"RemoteTransform2D", "RemoteTransform3D", "Timer", "LookAtModifier3D",
	"CanvasModulate", "AudioListener2D", "AudioListener3D", "YSort",
	"NinePatchRect", "RichTextLabel",
	"HBoxContainer", "VBoxContainer", "BoxContainer", "MarginContainer",
	"CenterContainer", "GridContainer", "AspectRatioContainer",
	"BoneAttachment3D", "SpringArm3D",
	"Parallax2D", "ParallaxLayer", "ParallaxBackground",
	"WorldEnvironment",
	"VideoStreamPlayer", "Control", "CanvasLayer", "Label", "Button",
	"TextureButton", "CheckBox", "CheckButton", "OptionButton", "LinkButton",
	"TextureRect", "ColorRect", "Panel", "PanelContainer", "ProgressBar",
	"HSlider", "VSlider", "TileMap", "TileMapLayer"
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
	return p_cls.begins_with("SubViewport") ||
			p_cls.begins_with("VehicleWheel") || p_cls.begins_with("SoftBody") ||
			p_cls.begins_with("GPUParticlesCollision") || p_cls == "Decal" ||
			p_cls == "ReflectionProbe" || p_cls == "FogVolume" || p_cls == "VoxelGI";
}

static bool is_catalog_only(Node *p_node) {
	if (!p_node) {
		return false;
	}
	return p_node->is_class("Path2D") || p_node->is_class("Path3D") ||
			p_node->is_class("NavigationRegion2D") || p_node->is_class("NavigationRegion3D") ||
			p_node->is_class("NavigationLink2D") || p_node->is_class("NavigationLink3D") ||
			p_node->is_class("NavigationObstacle2D") || p_node->is_class("NavigationObstacle3D") ||
			p_node->is_class("CanvasModulate") || p_node->is_class("AudioListener2D") || p_node->is_class("AudioListener3D") ||
			p_node->is_class("CanvasLayer");
}

static int find_node_ptr(const SceneGather &p_g, Node *p_node) {
	for (int i = 0; i < p_g.node_ptrs.size(); i++) {
		if (p_g.node_ptrs[i] == p_node) {
			return i;
		}
	}
	return -1;
}

static void cook_polyline(SceneGather &r_g, Node *p_node) {
	if (!p_node) {
		return;
	}
	SceneGather::PathRow row;
	row.id = uint8_t(r_g.paths.size());
	row.closed = 0;
	const Variant cv = p_node->get("curve");
	Object *curve = cv.get_type() == Variant::OBJECT ? cv.get_validated_object() : nullptr;
	Vector<Vector3> pts;
	if (curve) {
		if (curve->has_method("get_baked_points")) {
			const Variant baked = curve->call("get_baked_points");
			if (baked.get_type() == Variant::PACKED_VECTOR3_ARRAY) {
				const PackedVector3Array a = baked;
				for (int i = 0; i < a.size(); i++) {
					pts.push_back(a[i]);
				}
			} else if (baked.get_type() == Variant::PACKED_VECTOR2_ARRAY) {
				const PackedVector2Array a = baked;
				for (int i = 0; i < a.size(); i++) {
					pts.push_back(Vector3(a[i].x, a[i].y, 0));
				}
			}
		}
	}
	if (pts.size() > 16) {
		r_g.paths_dropped++;
		Vector<Vector3> sampled;
		sampled.push_back(pts[0]);
		for (int i = 1; i < 15; i++) {
			sampled.push_back(pts[i * (pts.size() - 1) / 15]);
		}
		sampled.push_back(pts[pts.size() - 1]);
		pts = sampled;
	}
	row.npts = uint8_t(MIN(pts.size(), 16));
	for (int i = 0; i < row.npts; i++) {
		row.pts[i] = pts[i];
	}
	if (row.npts >= 2 && r_g.paths.size() < 8) {
		r_g.paths.push_back(row);
		r_g.had_path = true;
	} else if (row.npts >= 2) {
		r_g.paths_dropped++;
	}
}

static void cook_nav_verts(SceneGather &r_g, const Vector<Vector3> &p_pts, int p_node) {
	const int start = r_g.nav_verts.size();
	for (int i = 0; i < p_pts.size(); i++) {
		if (r_g.nav_verts.size() >= 32) {
			r_g.nav_dropped++;
			break;
		}
		SceneGather::NavVert v;
		v.pos = p_pts[i];
		v.node = p_node;
		r_g.nav_verts.push_back(v);
	}
	const int end = r_g.nav_verts.size();
	for (int i = start; i + 1 < end && r_g.nav_edges.size() < 48; i++) {
		SceneGather::NavEdge e;
		e.a = uint8_t(i);
		e.b = uint8_t(i + 1);
		r_g.nav_edges.push_back(e);
	}
	if (end - start >= 3 && r_g.nav_edges.size() < 48) {
		SceneGather::NavEdge e;
		e.a = uint8_t(end - 1);
		e.b = uint8_t(start);
		r_g.nav_edges.push_back(e);
	}
	r_g.had_nav = r_g.had_nav || (end > start);
}

static bool node_cookable(Node *p_node) {
	if (!p_node) {
		return false;
	}
	if (type_allowed(p_node->get_class())) {
		return true;
	}
	return p_node->is_class("CSGShape3D") || p_node->is_class("GridMap") ||
			p_node->is_class("MultiMeshInstance3D") || p_node->is_class("MultiMeshInstance2D") ||
			p_node->is_class("ImporterMeshInstance3D") || p_node->is_class("Light2D") ||
			p_node->is_class("CollisionObject2D") || p_node->is_class("CollisionObject3D");
}

static bool is_physics_body(Node *p_node) {
	if (!p_node) {
		return false;
	}
	return p_node->is_class("CharacterBody2D") || p_node->is_class("CharacterBody3D") ||
			p_node->is_class("StaticBody2D") || p_node->is_class("StaticBody3D") ||
			p_node->is_class("AnimatableBody2D") || p_node->is_class("AnimatableBody3D") ||
			p_node->is_class("RigidBody2D") || p_node->is_class("RigidBody3D") ||
			p_node->is_class("PhysicalBone2D") || p_node->is_class("PhysicalBone3D") ||
			p_node->is_class("VehicleBody2D") || p_node->is_class("VehicleBody3D");
}

static Vector<String> &group_table(SceneGather &r_g) {
	return r_g.shared_groups ? *r_g.shared_groups : r_g.group_names;
}

static uint8_t cook_group_bits(Node *p_node, SceneGather &r_g) {
	if (!p_node) {
		return 0;
	}
	List<Node::GroupInfo> groups;
	p_node->get_groups(&groups);
	Vector<String> &names = group_table(r_g);
	uint8_t bits = 0;
	for (const List<Node::GroupInfo>::Element *E = groups.front(); E; E = E->next()) {
		const String name = E->get().name;
		if (name.is_empty()) {
			continue;
		}
		int bit = -1;
		for (int k = 0; k < names.size(); k++) {
			if (names[k] == name) {
				bit = k;
				break;
			}
		}
		if (bit < 0) {
			if (names.size() < 8) {
				bit = names.size();
				names.push_back(name);
			} else {
				r_g.groups_dropped++;
				continue;
			}
		}
		bits |= uint8_t(1u << bit);
	}
	return bits;
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
		return image_from_texture(tex);
	}
	const Color c = std->get_albedo();
	r_name = vformat("albedo_%08x", c.to_rgba32());
	Ref<Image> solid = Image::create_empty(64, 64, false, Image::FORMAT_RGBA8);
	solid->fill(c);
	return solid;
}

static bool material_has_extra_maps(const Ref<Material> &p_mat) {
	const Ref<BaseMaterial3D> std = p_mat;
	if (std.is_null()) {
		return false;
	}
	return std->get_texture(BaseMaterial3D::TEXTURE_NORMAL).is_valid() ||
			std->get_texture(BaseMaterial3D::TEXTURE_METALLIC).is_valid() ||
			std->get_texture(BaseMaterial3D::TEXTURE_EMISSION).is_valid() ||
			std->get_texture(BaseMaterial3D::TEXTURE_ROUGHNESS).is_valid();
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

static void emit_tris(const PackedVector3Array &verts, const PackedVector2Array &uvs, const PackedInt32Array &idx, uint8_t p_tex, uint8_t p_node, Vector<CookTri> &r_tris) {
	if (idx.size() >= 3) {
		for (int i = 0; i + 2 < idx.size(); i += 3) {
			CookTri t;
			t.tex_slot = p_tex;
			t.node_id = p_node;
			for (int k = 0; k < 3; k++) {
				const int vi = idx[i + k];
				if (vi < 0 || vi >= verts.size()) {
					continue;
				}
				t.v[k] = verts[vi];
				t.uv[k] = (vi < uvs.size()) ? uvs[vi] : Vector2(0, 0);
			}
			r_tris.push_back(t);
		}
		return;
	}
	for (int i = 0; i + 2 < verts.size(); i += 3) {
		CookTri t;
		t.tex_slot = p_tex;
		t.node_id = p_node;
		t.v[0] = verts[i];
		t.v[1] = verts[i + 1];
		t.v[2] = verts[i + 2];
		t.uv[0] = (i < uvs.size()) ? uvs[i] : Vector2(0, 0);
		t.uv[1] = (i + 1 < uvs.size()) ? uvs[i + 1] : Vector2(1, 0);
		t.uv[2] = (i + 2 < uvs.size()) ? uvs[i + 2] : Vector2(0, 1);
		r_tris.push_back(t);
	}
}

static void emit_mesh_resource(SceneGather &r_g, const Ref<Mesh> &p_mesh, const Transform3D &p_xf, int p_my, Node *p_owner) {
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
		if (material_has_extra_maps(mat)) {
			r_g.extra_pbr++;
		}
		String tex_name;
		const Ref<Image> img = image_from_material(mat, tex_name);
		if (img.is_null() && !r_g.images.is_empty()) {
			r_g.no_albedo_surfaces++;
		}
		const uint8_t slot = register_image(r_g, img, tex_name);
		emit_tris(verts, uvs, idx, slot, uint8_t(p_my), r_g.tris);
	}
	r_g.nodes.write[p_my].tri_hi = uint16_t(MIN(r_g.tris.size(), 65535));
}

static void emit_importer_mesh(SceneGather &r_g, const Ref<ImporterMesh> &p_im, const Transform3D &p_xf, int p_my) {
	if (p_im.is_null()) {
		return;
	}
	const Ref<ArrayMesh> mesh = p_im->get_mesh();
	emit_mesh_resource(r_g, mesh, p_xf, p_my, nullptr);
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

static uint8_t hit_kind_from_name(const String &p_name, bool p_monitoring) {
	const String n = p_name.to_lower();
	if (n.contains("hurt")) {
		return 1;
	}
	if (n.contains("hit") && !n.contains("hurt")) {
		return 2;
	}
	return p_monitoring ? 0 : 0;
}

static void emit_hit_3d(SceneGather &r_g, Area3D *p_area, int p_node_id) {
	if (!p_area || p_node_id < 0) {
		return;
	}
	CollisionShape3D *cs = nullptr;
	for (int i = 0; i < p_area->get_child_count(); i++) {
		if (CollisionShape3D *c = Object::cast_to<CollisionShape3D>(p_area->get_child(i))) {
			cs = c;
			break;
		}
	}
	CollisionPolygon3D *cp = nullptr;
	if (!cs) {
		for (int i = 0; i < p_area->get_child_count(); i++) {
			if (CollisionPolygon3D *c = Object::cast_to<CollisionPolygon3D>(p_area->get_child(i))) {
				cp = c;
				break;
			}
		}
	}
	if (!cs && !cp) {
		r_g.uncooked_types.insert("Area3D(no CollisionShape3D)");
		return;
	}
	Vector3 half;
	Transform3D xf;
	if (cs) {
		const Ref<Shape3D> shape = cs->get_shape();
		if (const Ref<BoxShape3D> box = shape; box.is_valid()) {
			half = box->get_size() * 0.5f;
		} else if (const Ref<SphereShape3D> sph = shape; sph.is_valid()) {
			const float r = sph->get_radius();
			half = Vector3(r, r, r);
		} else if (const Ref<CapsuleShape3D> cap = shape; cap.is_valid()) {
			const float r = cap->get_radius();
			const float h = cap->get_height() * 0.5f;
			half = Vector3(r, MAX(h, r), r);
		} else {
			r_g.uncooked_types.insert(shape.is_valid() ? shape->get_class() : "Area3D(empty shape)");
			return;
		}
		xf = world_xform_3d(cs);
	} else {
		const Vector<Vector2> poly = cp->get_polygon();
		if (poly.is_empty()) {
			r_g.uncooked_types.insert("Area3D(empty polygon)");
			return;
		}
		Vector2 mn = poly[0];
		Vector2 mx = poly[0];
		for (int i = 1; i < poly.size(); i++) {
			mn.x = MIN(mn.x, poly[i].x);
			mn.y = MIN(mn.y, poly[i].y);
			mx.x = MAX(mx.x, poly[i].x);
			mx.y = MAX(mx.y, poly[i].y);
		}
		const float depth = MAX(cp->get_depth() * 0.5f, 1.0f);
		half = Vector3((mx.x - mn.x) * 0.5f, (mx.y - mn.y) * 0.5f, depth);
		xf = world_xform_3d(cp);
		xf.origin += xf.basis.xform(Vector3((mn.x + mx.x) * 0.5f, (mn.y + mx.y) * 0.5f, 0));
	}
	if (r_g.hits.size() >= kMaxAABBs) {
		r_g.aabb_dropped++;
		return;
	}
	const Vector3 c = xf.origin;
	const Vector3 ext = xf.basis.xform(half).abs();
	SceneGather::HitRow h;
	h.node_id = p_node_id;
	h.dim = 3;
	h.kind = hit_kind_from_name(p_area->get_name(), p_area->is_monitoring());
	h.flags = p_area->is_monitoring() ? 1 : 0;
	{
		const uint32_t layer = p_area->get_collision_layer() & 0xFFu;
		h.layer = uint8_t(layer ? layer : 1);
	}
	h.mn = c - ext;
	h.mx = c + ext;
	r_g.hits.push_back(h);
}

static void emit_hit_2d(SceneGather &r_g, Area2D *p_area, int p_node_id) {
	if (!p_area || p_node_id < 0) {
		return;
	}
	CollisionShape2D *cs = nullptr;
	for (int i = 0; i < p_area->get_child_count(); i++) {
		if (CollisionShape2D *c = Object::cast_to<CollisionShape2D>(p_area->get_child(i))) {
			cs = c;
			break;
		}
	}
	CollisionPolygon2D *cp = nullptr;
	if (!cs) {
		for (int i = 0; i < p_area->get_child_count(); i++) {
			if (CollisionPolygon2D *c = Object::cast_to<CollisionPolygon2D>(p_area->get_child(i))) {
				cp = c;
				break;
			}
		}
	}
	if (!cs && !cp) {
		r_g.uncooked_types.insert("Area2D(no CollisionShape2D)");
		return;
	}
	Vector2 half;
	Vector2 c;
	if (cs) {
		const Ref<Shape2D> shape = cs->get_shape();
		if (const Ref<RectangleShape2D> rect = shape; rect.is_valid()) {
			half = rect->get_size() * 0.5f;
		} else if (const Ref<CircleShape2D> cir = shape; cir.is_valid()) {
			const float r = cir->get_radius();
			half = Vector2(r, r);
		} else if (const Ref<CapsuleShape2D> cap = shape; cap.is_valid()) {
			const float r = cap->get_radius();
			const float h = cap->get_height() * 0.5f;
			half = Vector2(r, MAX(h, r));
		} else {
			r_g.uncooked_types.insert(shape.is_valid() ? shape->get_class() : "Area2D(empty shape)");
			return;
		}
		const Transform2D xf = world_xform_2d(cs);
		c = xf.get_origin();
	} else {
		const Vector<Vector2> poly = cp->get_polygon();
		if (poly.is_empty()) {
			r_g.uncooked_types.insert("Area2D(empty polygon)");
			return;
		}
		Vector2 mn = poly[0];
		Vector2 mx = poly[0];
		for (int i = 1; i < poly.size(); i++) {
			mn.x = MIN(mn.x, poly[i].x);
			mn.y = MIN(mn.y, poly[i].y);
			mx.x = MAX(mx.x, poly[i].x);
			mx.y = MAX(mx.y, poly[i].y);
		}
		half = (mx - mn) * 0.5f;
		c = world_xform_2d(cp).get_origin() + (mn + mx) * 0.5f;
	}
	if (r_g.hits.size() >= kMaxAABBs) {
		r_g.aabb_dropped++;
		return;
	}
	SceneGather::HitRow h;
	h.node_id = p_node_id;
	h.dim = 2;
	h.kind = hit_kind_from_name(p_area->get_name(), p_area->is_monitoring());
	h.flags = p_area->is_monitoring() ? 1 : 0;
	{
		const uint32_t layer = p_area->get_collision_layer() & 0xFFu;
		h.layer = uint8_t(layer ? layer : 1);
	}
	h.mn = Vector3(c.x - half.x, c.y - half.y, 0);
	h.mx = Vector3(c.x + half.x, c.y + half.y, 0);
	r_g.hits.push_back(h);
}

static void emit_body_aabb(SceneGather &r_g, Node *p_body, int p_node_id) {
	if (!p_body || p_node_id < 0) {
		return;
	}
	if (Area3D *a3 = Object::cast_to<Area3D>(p_body)) {
		emit_hit_3d(r_g, a3, p_node_id);
		return;
	}
	if (Area2D *a2 = Object::cast_to<Area2D>(p_body)) {
		emit_hit_2d(r_g, a2, p_node_id);
		return;
	}
	CollisionShape3D *cs3 = nullptr;
	CollisionPolygon3D *cp3 = nullptr;
	CollisionShape2D *cs2 = nullptr;
	CollisionPolygon2D *cp2 = nullptr;
	for (int i = 0; i < p_body->get_child_count(); i++) {
		Node *ch = p_body->get_child(i);
		if (!cs3) {
			cs3 = Object::cast_to<CollisionShape3D>(ch);
		}
		if (!cp3) {
			cp3 = Object::cast_to<CollisionPolygon3D>(ch);
		}
		if (!cs2) {
			cs2 = Object::cast_to<CollisionShape2D>(ch);
		}
		if (!cp2) {
			cp2 = Object::cast_to<CollisionPolygon2D>(ch);
		}
	}
	if (r_g.hits.size() >= kMaxAABBs) {
		r_g.aabb_dropped++;
		return;
	}
	SceneGather::HitRow h;
	h.node_id = p_node_id;
	h.kind = 1;
	h.flags = 1;
	h.layer = 1;
	if (cs3 || cp3) {
		Vector3 half(8, 8, 8);
		Transform3D xf;
		if (cs3) {
			const Ref<Shape3D> shape = cs3->get_shape();
			if (const Ref<BoxShape3D> box = shape; box.is_valid()) {
				half = box->get_size() * 0.5f;
			} else if (const Ref<SphereShape3D> sph = shape; sph.is_valid()) {
				const float r = sph->get_radius();
				half = Vector3(r, r, r);
			} else if (const Ref<CapsuleShape3D> cap = shape; cap.is_valid()) {
				const float r = cap->get_radius();
				const float ht = cap->get_height() * 0.5f;
				half = Vector3(r, MAX(ht, r), r);
			}
			xf = world_xform_3d(cs3);
		} else {
			const Vector<Vector2> poly = cp3->get_polygon();
			if (!poly.is_empty()) {
				Vector2 mn = poly[0];
				Vector2 mx = poly[0];
				for (int i = 1; i < poly.size(); i++) {
					mn.x = MIN(mn.x, poly[i].x);
					mn.y = MIN(mn.y, poly[i].y);
					mx.x = MAX(mx.x, poly[i].x);
					mx.y = MAX(mx.y, poly[i].y);
				}
				half = Vector3((mx.x - mn.x) * 0.5f, (mx.y - mn.y) * 0.5f, MAX(cp3->get_depth() * 0.5f, 1.0f));
			}
			xf = world_xform_3d(cp3);
		}
		const Vector3 ext = xf.basis.xform(half).abs();
		h.dim = 3;
		h.mn = xf.origin - ext;
		h.mx = xf.origin + ext;
		r_g.hits.push_back(h);
		r_g.had_body = true;
		return;
	}
	if (cs2 || cp2) {
		Vector2 half(8, 8);
		Vector2 c;
		if (cs2) {
			const Ref<Shape2D> shape = cs2->get_shape();
			if (const Ref<RectangleShape2D> rect = shape; rect.is_valid()) {
				half = rect->get_size() * 0.5f;
			} else if (const Ref<CircleShape2D> cir = shape; cir.is_valid()) {
				const float r = cir->get_radius();
				half = Vector2(r, r);
			} else if (const Ref<CapsuleShape2D> cap = shape; cap.is_valid()) {
				const float r = cap->get_radius();
				half = Vector2(r, MAX(cap->get_height() * 0.5f, r));
			}
			c = world_xform_2d(cs2).get_origin();
		} else {
			const Vector<Vector2> poly = cp2->get_polygon();
			if (!poly.is_empty()) {
				Vector2 mn = poly[0];
				Vector2 mx = poly[0];
				for (int i = 1; i < poly.size(); i++) {
					mn.x = MIN(mn.x, poly[i].x);
					mn.y = MIN(mn.y, poly[i].y);
					mx.x = MAX(mx.x, poly[i].x);
					mx.y = MAX(mx.y, poly[i].y);
				}
				half = (mx - mn) * 0.5f;
				c = world_xform_2d(cp2).get_origin() + (mn + mx) * 0.5f;
			} else {
				c = world_xform_2d(cp2).get_origin();
			}
		}
		h.dim = 2;
		h.mn = Vector3(c.x - half.x, c.y - half.y, 0);
		h.mx = Vector3(c.x + half.x, c.y + half.y, 0);
		r_g.hits.push_back(h);
		r_g.had_body = true;
		return;
	}
	r_g.uncooked_types.insert(p_body->get_class() + String("(no CollisionShape)"));
}

static Rect2 control_screen_rect(Control *p_c) {
	Rect2 r(p_c->get_position(), p_c->get_size());
	for (Node *n = p_c->get_parent(); n; n = n->get_parent()) {
		if (Control *pc = Object::cast_to<Control>(n)) {
			r.position += pc->get_position();
		} else if (Node2D *n2 = Object::cast_to<Node2D>(n)) {
			r.position += n2->get_position();
		}
	}
	if (r.size.x < 1) {
		r.size.x = 16;
	}
	if (r.size.y < 1) {
		r.size.y = 16;
	}
	return r;
}

static void try_wav(SceneGather &r_g, const Ref<AudioStream> &p_stream, const String &p_name = String()) {
	if (p_stream.is_null()) {
		return;
	}
	const Ref<AudioStreamWAV> wav = p_stream;
	if (wav.is_valid() && (wav->get_format() == AudioStreamWAV::FORMAT_8_BITS || wav->get_format() == AudioStreamWAV::FORMAT_16_BITS)) {
		if (r_g.wav.is_null()) {
			r_g.wav = wav;
		} else if (r_g.extra_wavs.size() < kMaxVagClips - 1) {
			r_g.extra_wavs.push_back(wav);
			r_g.extra_wav_names.push_back(p_name.is_empty() ? vformat("sfx%d", r_g.extra_wavs.size()) : p_name);
			r_g.extra_wav++;
		} else {
			r_g.extra_wav++;
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

static uint8_t pack_rgb(const Color &c) {
	const int r = CLAMP(int(c.r * 7.0f), 0, 7);
	const int g = CLAMP(int(c.g * 7.0f), 0, 7);
	const int b = CLAMP(int(c.b * 3.0f), 0, 3);
	return uint8_t((r << 5) | (g << 2) | b);
}

static bool hud_kind_for(Node *p_node, uint8_t &r_kind) {
	if (Object::cast_to<ColorRect>(p_node)) {
		r_kind = 0;
		return true;
	}
	if (Object::cast_to<TextureRect>(p_node) || p_node->is_class("NinePatchRect")) {
		r_kind = 1;
		return true;
	}
	if (Object::cast_to<Label>(p_node) || p_node->is_class("RichTextLabel")) {
		r_kind = 2;
		return true;
	}
	if (Object::cast_to<TextureButton>(p_node)) {
		r_kind = 4;
		return true;
	}
	if (Object::cast_to<CheckBox>(p_node)) {
		r_kind = 5;
		return true;
	}
	if (Object::cast_to<CheckButton>(p_node)) {
		r_kind = 6;
		return true;
	}
	if (Object::cast_to<OptionButton>(p_node)) {
		r_kind = 7;
		return true;
	}
	if (Object::cast_to<LinkButton>(p_node)) {
		r_kind = 8;
		return true;
	}
	if (Object::cast_to<Button>(p_node)) {
		r_kind = 3;
		return true;
	}
	if (Object::cast_to<ProgressBar>(p_node)) {
		r_kind = 10;
		return true;
	}
	if (Object::cast_to<HSlider>(p_node)) {
		r_kind = 11;
		return true;
	}
	if (Object::cast_to<VSlider>(p_node)) {
		r_kind = 12;
		return true;
	}
	if (Object::cast_to<Panel>(p_node) || Object::cast_to<PanelContainer>(p_node)) {
		r_kind = 9;
		return true;
	}
	return false;
}

static void emit_hud(Control *p_c, SceneGather &r_g, int p_node_id) {
	uint8_t kind = 0;
	if (!hud_kind_for(p_c, kind)) {
		return;
	}
	if (r_g.hud.size() >= kMaxHud) {
		r_g.hud_dropped++;
		return;
	}
	SceneGather::HudRow row;
	const Rect2 r = control_screen_rect(p_c);
	row.x = int16_t(CLAMP(int(r.position.x), -2048, 2047));
	row.y = int16_t(CLAMP(int(r.position.y), -2048, 2047));
	row.w = int16_t(CLAMP(int(r.size.x), 1, 1024));
	row.h = int16_t(CLAMP(int(r.size.y), 1, 1024));
	row.kind = kind;
	row.node_id = uint8_t(CLAMP(p_node_id, 0, 255));
	row.flags = p_c->is_visible() ? 1 : 0;
	if (BaseButton *bb = Object::cast_to<BaseButton>(p_c)) {
		if (bb->is_disabled()) {
			row.flags |= 2;
		}
		if (bb->is_toggle_mode() || Object::cast_to<CheckBox>(p_c) || Object::cast_to<CheckButton>(p_c)) {
			row.flags |= 4;
		}
		if (bb->is_pressed()) {
			row.flags |= 8;
		}
	}
	if (Label *lb = Object::cast_to<Label>(p_c)) {
		row.text = lb->get_text();
		if (p_c->has_meta("ps1_line") && r_g.text_lines.size() < kMaxTextLines) {
			SceneGather::TextLine tl;
			tl.name = String(p_c->get_meta("ps1_line"));
			if (tl.name.is_empty()) {
				tl.name = p_c->get_name();
			}
			tl.text = lb->get_text();
			r_g.text_lines.push_back(tl);
		}
	} else if (Button *bt = Object::cast_to<Button>(p_c)) {
		row.text = bt->get_text();
	} else if (LinkButton *lk = Object::cast_to<LinkButton>(p_c)) {
		row.text = lk->get_text();
	}
	if (ColorRect *cr = Object::cast_to<ColorRect>(p_c)) {
		row.rgb = pack_rgb(cr->get_color());
	}
	if (TextureRect *tr = Object::cast_to<TextureRect>(p_c)) {
		String tn;
		row.tex = register_image(r_g, image_from_texture(tr->get_texture()), tr->get_texture().is_valid() ? tr->get_texture()->get_path() : tn);
	}
	if (TextureButton *tb = Object::cast_to<TextureButton>(p_c)) {
		const Ref<Texture2D> tex = tb->get_texture_normal();
		row.tex = register_image(r_g, image_from_texture(tex), tex.is_valid() ? tex->get_path() : String());
	}
	if (Range *rg = Object::cast_to<Range>(p_c)) {
		row.value = int16_t(CLAMP(int(rg->get_value()), -32767, 32767));
		row.vmin = int16_t(CLAMP(int(rg->get_min()), -32767, 32767));
		row.vmax = int16_t(CLAMP(int(rg->get_max()), -32767, 32767));
	}
	if (OptionButton *ob = Object::cast_to<OptionButton>(p_c)) {
		row.value = int16_t(ob->get_selected());
		const int n = MIN(ob->get_item_count(), 8);
		for (int i = 0; i < n; i++) {
			row.items.push_back(ob->get_item_text(i));
		}
		if (row.text.is_empty() && row.value >= 0 && row.value < row.items.size()) {
			row.text = row.items[row.value];
		}
	}
	r_g.hud.push_back(row);
}

static uint8_t register_tileset_atlas(SceneGather &r_g, const Ref<TileSet> &p_ts) {
	if (p_ts.is_null()) {
		return 0;
	}
	const Size2i tsz = p_ts->get_tile_size();
	r_g.tile_w = uint8_t(CLAMP(tsz.x, 1, 255));
	r_g.tile_h = uint8_t(CLAMP(tsz.y, 1, 255));
	for (int i = 0; i < p_ts->get_source_count(); i++) {
		const Ref<TileSetSource> src = p_ts->get_source(p_ts->get_source_id(i));
		const Ref<TileSetAtlasSource> atlas = src;
		if (atlas.is_valid() && atlas->get_texture().is_valid()) {
			return register_image(r_g, image_from_texture(atlas->get_texture()), atlas->get_texture()->get_path());
		}
	}
	return 0;
}

static uint8_t tile_data_solid(TileData *p_td, const Ref<TileSet> &p_ts) {
	if (!p_td) {
		return 0;
	}
	uint8_t flags = 0;
	if (p_td->has_custom_data("solid") && bool(p_td->get_custom_data("solid"))) {
		flags |= 1;
	}
	if (p_td->has_custom_data("one_way") && bool(p_td->get_custom_data("one_way"))) {
		flags |= 2;
	}
	if (p_td->has_custom_data("slope")) {
		const int sl = int(p_td->get_custom_data("slope"));
		if (sl < 0) {
			flags |= 4;
		} else if (sl > 0) {
			flags |= 8;
		}
	}
	if (p_ts.is_valid()) {
		const int n = p_ts->get_physics_layers_count();
		for (int i = 0; i < n; i++) {
			const int pc = p_td->get_collision_polygons_count(i);
			if (pc > 0) {
				flags |= 1;
			}
			for (int p = 0; p < pc; p++) {
				if (p_td->is_collision_polygon_one_way(i, p)) {
					flags |= 2;
				}
			}
		}
	}
	return flags;
}

static void emit_tile_cell(SceneGather &r_g, const Vector2 &p_pos, const Vector2i &p_atlas, const Size2i &p_tsz, uint8_t p_tex, int p_node_id, uint8_t p_flags = 0) {
	if (r_g.tiles.size() >= kMaxTiles) {
		r_g.tile_dropped++;
		return;
	}
	SceneGather::TileRow t;
	t.x = int16_t(CLAMP(int(p_pos.x), -2048, 2047));
	t.y = int16_t(CLAMP(int(p_pos.y), -2048, 2047));
	t.u = uint8_t(CLAMP(p_atlas.x * p_tsz.x, 0, 255));
	t.v = uint8_t(CLAMP(p_atlas.y * p_tsz.y, 0, 255));
	t.tex = p_tex;
	t.node_id = uint8_t(CLAMP(p_node_id, 0, 255));
	t.flags = p_flags;
	if (p_flags & 12) {
		r_g.slope_tiles++;
	}
	r_g.tiles.push_back(t);
}

static void walk_scene(Node *p_node, SceneGather &r_g, int p_parent) {
	if (!p_node) {
		return;
	}
	const String cls = p_node->get_class();
	if (!node_cookable(p_node) || type_refused(cls)) {
		r_g.uncooked_types.insert(cls);
	}
	if (is_catalog_only(p_node)) {
		if (p_node->is_class("Path2D") || p_node->is_class("Path3D")) {
			cook_polyline(r_g, p_node);
		} else if (p_node->is_class("NavigationRegion2D") || p_node->is_class("NavigationRegion3D")) {
			Vector<Vector3> pts;
			const Variant nm = p_node->get("navigation_mesh");
			Object *mesh = nm.get_type() == Variant::OBJECT ? nm.get_validated_object() : nullptr;
			if (!mesh) {
				const Variant np = p_node->get("navigation_polygon");
				mesh = np.get_type() == Variant::OBJECT ? np.get_validated_object() : nullptr;
			}
			if (mesh && mesh->has_method("get_vertices")) {
				const Variant v = mesh->call("get_vertices");
				if (v.get_type() == Variant::PACKED_VECTOR3_ARRAY) {
					const PackedVector3Array a = v;
					for (int i = 0; i < a.size(); i++) {
						pts.push_back(a[i]);
					}
				} else if (v.get_type() == Variant::PACKED_VECTOR2_ARRAY) {
					const PackedVector2Array a = v;
					for (int i = 0; i < a.size(); i++) {
						pts.push_back(Vector3(a[i].x, a[i].y, 0));
					}
				}
			}
			cook_nav_verts(r_g, pts, -1);
		} else if (p_node->is_class("NavigationLink2D") || p_node->is_class("NavigationLink3D")) {
			Vector3 origin;
			if (Node3D *n3 = Object::cast_to<Node3D>(p_node)) {
				origin = n3->get_global_position();
			} else if (Node2D *n2 = Object::cast_to<Node2D>(p_node)) {
				origin = Vector3(n2->get_global_position().x, n2->get_global_position().y, 0);
			}
			Vector3 sa = origin;
			Vector3 sb = origin;
			const Variant a = p_node->get("start_position");
			const Variant b = p_node->get("end_position");
			if (a.get_type() == Variant::VECTOR3) {
				sa = origin + Vector3(a);
			} else if (a.get_type() == Variant::VECTOR2) {
				const Vector2 v = a;
				sa = origin + Vector3(v.x, v.y, 0);
			}
			if (b.get_type() == Variant::VECTOR3) {
				sb = origin + Vector3(b);
			} else if (b.get_type() == Variant::VECTOR2) {
				const Vector2 v = b;
				sb = origin + Vector3(v.x, v.y, 0);
			}
			Vector<Vector3> pts;
			pts.push_back(sa);
			pts.push_back(sb);
			cook_nav_verts(r_g, pts, -1);
		} else if (p_node->is_class("NavigationObstacle2D") || p_node->is_class("NavigationObstacle3D")) {
			SceneGather::NavObs o;
			if (Node3D *n3 = Object::cast_to<Node3D>(p_node)) {
				o.pos = n3->get_global_position();
			} else if (Node2D *n2 = Object::cast_to<Node2D>(p_node)) {
				o.pos = Vector3(n2->get_global_position().x, n2->get_global_position().y, 0);
			}
			o.radius = MAX(1.0f, float(p_node->get("radius")));
			r_g.nav_obs.push_back(o);
		} else if (p_node->is_class("CanvasLayer")) {
			r_g.canvas_layer_count++;
			const Variant off = p_node->get("offset");
			if (off.get_type() == Variant::VECTOR2) {
				const Vector2 o = off;
				r_g.canvas_ox = int16_t(CLAMP(int(o.x), -32768, 32767));
				r_g.canvas_oy = int16_t(CLAMP(int(o.y), -32768, 32767));
			}
		}
		for (int i = 0; i < p_node->get_child_count(); i++) {
			walk_scene(p_node->get_child(i), r_g, p_parent);
		}
		return;
	}
	int my = -1;
	if (r_g.nodes.size() < kMaxNodes) {
		SceneGather::CookedNode cn;
		cn.parent = p_parent;
		cn.name = p_node->get_name();
		cn.flags = 1;
		if (Object::cast_to<MeshInstance3D>(p_node)) {
			cn.type = 2;
		} else if (Object::cast_to<Sprite2D>(p_node) || Object::cast_to<AnimatedSprite2D>(p_node)) {
			cn.type = 3;
		} else if (Object::cast_to<Area3D>(p_node) || Object::cast_to<Area2D>(p_node)) {
			cn.type = 10;
		} else if (Object::cast_to<Camera3D>(p_node) || Object::cast_to<Camera2D>(p_node)) {
			cn.type = 4;
		} else if (is_physics_body(p_node)) {
			cn.type = 11;
		} else if (Object::cast_to<GPUParticles3D>(p_node) || Object::cast_to<GPUParticles2D>(p_node) ||
				Object::cast_to<CPUParticles3D>(p_node) || Object::cast_to<CPUParticles2D>(p_node)) {
			cn.type = 12;
		} else if (Object::cast_to<AnimationPlayer>(p_node)) {
			cn.type = 7;
		} else if (Object::cast_to<AudioStreamPlayer>(p_node) || Object::cast_to<AudioStreamPlayer3D>(p_node) ||
				Object::cast_to<AudioStreamPlayer2D>(p_node)) {
			cn.type = 8;
		} else if (Object::cast_to<VideoStreamPlayer>(p_node)) {
			cn.type = 9;
		} else if (Object::cast_to<Control>(p_node)) {
			cn.type = 5;
		} else if (Object::cast_to<TileMap>(p_node) || Object::cast_to<TileMapLayer>(p_node)) {
			cn.type = 6;
		} else if (Object::cast_to<Node2D>(p_node) || Object::cast_to<Node3D>(p_node)) {
			cn.type = 1;
		}
		if (Node3D *n3 = Object::cast_to<Node3D>(p_node)) {
			const Transform3D xf = n3->get_transform();
			cn.pos = xf.origin;
			cn.euler = xf.basis.get_euler();
			if (!n3->is_visible()) {
				cn.flags = 0;
			}
		} else if (Node2D *n2 = Object::cast_to<Node2D>(p_node)) {
			cn.pos = Vector3(n2->get_position().x, n2->get_position().y, 0);
			if (!n2->is_visible()) {
				cn.flags = 0;
			}
		} else if (Control *ctl = Object::cast_to<Control>(p_node)) {
			cn.pos = Vector3(ctl->get_position().x, ctl->get_position().y, 0);
			if (!ctl->is_visible()) {
				cn.flags = 0;
			}
		}
		if (p_node->get_script()) {
			const Ref<Script> sc = p_node->get_script();
			if (sc.is_valid() && !sc->get_path().is_empty()) {
				cn.script_path = sc->get_path();
				cn.flags |= 2;
			}
		}
		cn.group = cook_group_bits(p_node, r_g);
		if (ParallaxLayer *pl = Object::cast_to<ParallaxLayer>(p_node)) {
			const Vector2 sc = pl->get_motion_scale();
			cn.scroll = uint8_t(CLAMP(int(sc.x * 128.0f), 1, 255));
		} else if (Parallax2D *p2 = Object::cast_to<Parallax2D>(p_node)) {
			const Vector2 sc = p2->get_scroll_scale();
			cn.scroll = uint8_t(CLAMP(int(sc.x * 128.0f), 1, 255));
		}
		if (p_node->is_class("Node2D") && bool(p_node->get("y_sort_enabled"))) {
			cn.ysort = 1;
		}
		if (p_node->is_class("YSort")) {
			cn.ysort = 1;
		}
		if (p_node->is_class("PathFollow2D") || p_node->is_class("PathFollow3D")) {
			cn.kind = 1;
			cn.path_id = uint8_t(r_g.paths.is_empty() ? 0 : r_g.paths.size() - 1);
			r_g.had_path = true;
		} else if (p_node->is_class("RayCast2D") || p_node->is_class("RayCast3D")) {
			cn.kind = 2;
			if (r_g.ray_count < 8) {
				r_g.ray_count++;
			}
		} else if (p_node->is_class("ShapeCast2D") || p_node->is_class("ShapeCast3D")) {
			cn.kind = 3;
			if (r_g.ray_count < 8) {
				r_g.ray_count++;
			}
		} else if (p_node->is_class("VisibleOnScreenEnabler2D") || p_node->is_class("VisibleOnScreenEnabler3D")) {
			cn.kind = 5;
		} else if (p_node->is_class("VisibleOnScreenNotifier2D") || p_node->is_class("VisibleOnScreenNotifier3D")) {
			cn.kind = 4;
		} else if (p_node->is_class("RemoteTransform2D") || p_node->is_class("RemoteTransform3D")) {
			cn.kind = 6;
			const Variant rp = p_node->get("remote_path");
			if (rp.get_type() == Variant::NODE_PATH) {
				Node *tgt = p_node->get_node_or_null(rp);
				const int ti = find_node_ptr(r_g, tgt);
				cn.path_id = uint8_t(ti >= 0 ? ti : 0);
			}
		} else if (p_node->is_class("Timer")) {
			cn.kind = 7;
			if (r_g.timer_count < 8) {
				cn.path_id = uint8_t(r_g.timer_count);
				r_g.timer_count++;
			} else {
				r_g.uncooked_types.insert("Timer(9th dropped)");
			}
		} else if (p_node->is_class("NavigationAgent2D") || p_node->is_class("NavigationAgent3D")) {
			cn.kind = 8;
		} else if (p_node->is_class("SpringArm3D")) {
			cn.kind = 9;
			const float sl = float(p_node->get("spring_length"));
			cn.path_id = uint8_t(CLAMP(int(sl / 4.0f), 1, 255));
		} else if (p_node->is_class("LookAtModifier3D")) {
			cn.kind = 10;
		}
		cn.tri_lo = uint16_t(r_g.tris.size());
		cn.tri_hi = cn.tri_lo;
		my = r_g.nodes.size();
		r_g.nodes.push_back(cn);
		r_g.node_ptrs.push_back(p_node);
	} else {
		r_g.node_dropped++;
	}
	MeshInstance3D *mi = Object::cast_to<MeshInstance3D>(p_node);
	if (mi && mi->get_mesh().is_valid()) {
		if (my < 0) {
			r_g.mesh_on_dropped++;
		} else {
			emit_mesh_resource(r_g, mi->get_mesh(), Transform3D(), my, mi);
		}
	}
	if (ImporterMeshInstance3D *imi = Object::cast_to<ImporterMeshInstance3D>(p_node)) {
		if (my < 0) {
			r_g.mesh_on_dropped++;
		} else {
			emit_importer_mesh(r_g, imi->get_mesh(), Transform3D(), my);
			if (my >= 0) {
				r_g.nodes.write[my].type = 2;
			}
		}
	}
	if (p_node->is_class("CSGShape3D")) {
		r_g.had_csg = true;
		if (my >= 0) {
			const Variant meshes_v = p_node->call("get_meshes");
			if (meshes_v.get_type() == Variant::ARRAY) {
				const Array meshes = meshes_v;
				for (int i = 0; i + 1 < meshes.size(); i += 2) {
					const Transform3D xf = meshes[i];
					const Ref<Mesh> mesh = meshes[i + 1];
					emit_mesh_resource(r_g, mesh, xf, my, nullptr);
				}
			}
			r_g.nodes.write[my].type = 2;
		}
	}
	if (p_node->is_class("GridMap")) {
		r_g.had_gridmap = true;
		if (my >= 0) {
			const Variant cells_v = p_node->call("get_used_cells");
			const Variant lib_v = p_node->get("mesh_library");
			if (cells_v.get_type() == Variant::ARRAY && lib_v.get_type() == Variant::OBJECT) {
				const Array cells = cells_v;
				Object *lib = lib_v;
				const Vector3 cs = p_node->get("cell_size");
				for (int i = 0; i < cells.size(); i++) {
					const Vector3i cell = cells[i];
					const int item = int(p_node->call("get_cell_item", cell));
					if (item < 0 || !lib) {
						continue;
					}
					const Ref<Mesh> mesh = lib->call("get_item_mesh", item);
					Transform3D xf;
					xf.origin = Vector3((cell.x + 0.5f) * cs.x, (cell.y + 0.5f) * cs.y, (cell.z + 0.5f) * cs.z);
					emit_mesh_resource(r_g, mesh, xf, my, nullptr);
				}
			}
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
		if (r_g.cameras.size() < kMaxCameras) {
			SceneGather::CamRow row;
			row.node_id = my;
			row.name = cam->get_name();
			const Transform3D xf = world_xform_3d(cam);
			row.pos = xf.origin;
			row.euler = xf.basis.get_euler();
			row.is_default = cam->is_current();
			r_g.cameras.push_back(row);
		} else {
			r_g.cam_dropped++;
		}
	}
	if (Camera2D *c2 = Object::cast_to<Camera2D>(p_node)) {
		r_g.camera_count++;
		if (r_g.cameras.size() < kMaxCameras) {
			SceneGather::CamRow row;
			row.node_id = my;
			row.name = c2->get_name();
			row.pos = Vector3(c2->get_position().x, c2->get_position().y, 0);
			row.euler = Vector3();
			row.is_default = c2->is_current();
			row.dim = 2;
			const int drag = c2->is_position_smoothing_enabled() ? int(c2->get_position_smoothing_speed()) : 255;
			row.drag = uint8_t(CLAMP(drag, 1, 255));
			row.dead = 0;
			row.lim_l = int16_t(CLAMP(c2->get_limit(SIDE_LEFT), -32768, 32767));
			row.lim_t = int16_t(CLAMP(c2->get_limit(SIDE_TOP), -32768, 32767));
			row.lim_r = int16_t(CLAMP(c2->get_limit(SIDE_RIGHT), -32768, 32767));
			row.lim_b = int16_t(CLAMP(c2->get_limit(SIDE_BOTTOM), -32768, 32767));
			r_g.cameras.push_back(row);
		} else {
			r_g.cam_dropped++;
		}
	}
	if (is_physics_body(p_node)) {
		emit_body_aabb(r_g, p_node, my);
	}
	if (Object::cast_to<GPUParticles3D>(p_node) || Object::cast_to<GPUParticles2D>(p_node) ||
			Object::cast_to<CPUParticles3D>(p_node) || Object::cast_to<CPUParticles2D>(p_node)) {
		r_g.stream_count++;
		if (r_g.stream_count > 4) {
			r_g.stream_dropped++;
		}
	}
	if (Area3D *a3 = Object::cast_to<Area3D>(p_node)) {
		emit_hit_3d(r_g, a3, my);
	}
	if (Area2D *a2 = Object::cast_to<Area2D>(p_node)) {
		emit_hit_2d(r_g, a2, my);
	}
	if (DirectionalLight3D *dl = Object::cast_to<DirectionalLight3D>(p_node)) {
		const Transform3D xf = world_xform_3d(dl);
		const Vector3 dir = -xf.basis.get_column(Vector3::AXIS_Z);
		Color c = dl->get_color();
		c *= float(dl->get_param(Light3D::PARAM_ENERGY));
		add_light(r_g, dir, c);
	} else if (OmniLight3D *ol = Object::cast_to<OmniLight3D>(p_node)) {
		r_g.had_omni_or_spot = true;
		const Transform3D xf = world_xform_3d(ol);
		Color c = ol->get_color();
		c *= float(ol->get_param(Light3D::PARAM_ENERGY));
		add_light(r_g, xf.origin, c);
	} else if (SpotLight3D *sl = Object::cast_to<SpotLight3D>(p_node)) {
		r_g.had_omni_or_spot = true;
		const Transform3D xf = world_xform_3d(sl);
		const Vector3 dir = -xf.basis.get_column(Vector3::AXIS_Z);
		Color c = sl->get_color();
		c *= float(sl->get_param(Light3D::PARAM_ENERGY));
		add_light(r_g, dir, c);
	} else if (Light3D *gl = Object::cast_to<Light3D>(p_node)) {
		r_g.light_count++;
		r_g.had_generic_light = true;
		const Transform3D xf = world_xform_3d(gl);
		const Vector3 dir = -xf.basis.get_column(Vector3::AXIS_Z);
		Color c = gl->get_color();
		c *= float(gl->get_param(Light3D::PARAM_ENERGY));
		add_light(r_g, dir, c);
	}
	if (AudioStreamPlayer *ap = Object::cast_to<AudioStreamPlayer>(p_node)) {
		try_wav(r_g, ap->get_stream(), ap->get_name());
	}
	if (AudioStreamPlayer3D *ap3 = Object::cast_to<AudioStreamPlayer3D>(p_node)) {
		try_wav(r_g, ap3->get_stream(), ap3->get_name());
	}
	if (AudioStreamPlayer2D *ap2 = Object::cast_to<AudioStreamPlayer2D>(p_node)) {
		try_wav(r_g, ap2->get_stream(), ap2->get_name());
	}
	if (Light2D *l2 = Object::cast_to<Light2D>(p_node)) {
		Color c = l2->get_color();
		c *= float(l2->get_energy());
		Vector3 dir(0, -1, -1);
		if (Node2D *n2 = Object::cast_to<Node2D>(p_node)) {
			const Vector2 p = n2->get_position();
			dir = Vector3(p.x, -p.y, -1);
		}
		add_light(r_g, dir, c);
	}
	if (WorldEnvironment *we = Object::cast_to<WorldEnvironment>(p_node)) {
		r_g.had_world_env = true;
		const Ref<Environment> env = we->get_environment();
		if (env.is_valid() && env->is_fog_enabled()) {
			r_g.fog_from_env = true;
			r_g.fog_on = 1;
			r_g.fog_start = 0;
			r_g.fog_end = 2048;
			const Color fc = env->get_fog_light_color();
			r_g.fog_r = uint8_t(CLAMP(int(fc.r * 255.0f), 0, 255));
			r_g.fog_g = uint8_t(CLAMP(int(fc.g * 255.0f), 0, 255));
			r_g.fog_b = uint8_t(CLAMP(int(fc.b * 255.0f), 0, 255));
		}
	}
	if (MeshInstance2D *mi2 = Object::cast_to<MeshInstance2D>(p_node)) {
		r_g.had_draw2d = true;
		if (my >= 0 && mi2->get_mesh().is_valid()) {
			emit_mesh_resource(r_g, mi2->get_mesh(), Transform3D(), my, nullptr);
		}
	}
	if (Polygon2D *poly = Object::cast_to<Polygon2D>(p_node)) {
		r_g.had_draw2d = true;
		if (my >= 0) {
			const Vector<Vector2> pts = poly->get_polygon();
			const Vector<Vector2> uvs = poly->get_uv();
			if (pts.size() >= 3) {
				PackedVector3Array verts;
				PackedVector2Array uv3;
				PackedInt32Array idx;
				for (int i = 0; i < pts.size(); i++) {
					verts.push_back(Vector3(pts[i].x, pts[i].y, 0));
					uv3.push_back(i < uvs.size() ? uvs[i] : Vector2(float(i & 1), float((i >> 1) & 1)));
				}
				for (int i = 1; i + 1 < pts.size(); i++) {
					idx.push_back(0);
					idx.push_back(i);
					idx.push_back(i + 1);
				}
				String tex_name;
				const Ref<Image> img = image_from_texture(poly->get_texture());
				const uint8_t slot = register_image(r_g, img, tex_name);
				emit_tris(verts, uv3, idx, slot, uint8_t(my), r_g.tris);
				r_g.nodes.write[my].tri_hi = uint16_t(MIN(r_g.tris.size(), 65535));
			}
		}
	}
	if (Line2D *line = Object::cast_to<Line2D>(p_node)) {
		r_g.had_draw2d = true;
		if (my >= 0) {
			const Vector<Vector2> pts = line->get_points();
			const float w = MAX(line->get_width() * 0.5f, 1.0f);
			if (pts.size() >= 2) {
				PackedVector3Array verts;
				PackedVector2Array uvs;
				PackedInt32Array idx;
				for (int i = 0; i + 1 < pts.size(); i++) {
					const Vector2 a = pts[i];
					const Vector2 b = pts[i + 1];
					Vector2 d = b - a;
					if (d.length_squared() < 0.0001f) {
						continue;
					}
					Vector2 n = d.orthogonal().normalized() * w;
					const int base = verts.size();
					verts.push_back(Vector3(a.x - n.x, a.y - n.y, 0));
					verts.push_back(Vector3(a.x + n.x, a.y + n.y, 0));
					verts.push_back(Vector3(b.x + n.x, b.y + n.y, 0));
					verts.push_back(Vector3(b.x - n.x, b.y - n.y, 0));
					uvs.push_back(Vector2(0, 0));
					uvs.push_back(Vector2(1, 0));
					uvs.push_back(Vector2(1, 1));
					uvs.push_back(Vector2(0, 1));
					idx.push_back(base);
					idx.push_back(base + 1);
					idx.push_back(base + 2);
					idx.push_back(base);
					idx.push_back(base + 2);
					idx.push_back(base + 3);
				}
				String tex_name;
				const uint8_t slot = register_image(r_g, image_from_texture(line->get_texture()), tex_name);
				emit_tris(verts, uvs, idx, slot, uint8_t(my), r_g.tris);
				r_g.nodes.write[my].tri_hi = uint16_t(MIN(r_g.tris.size(), 65535));
			}
		}
	}
	if (Label3D *lb = Object::cast_to<Label3D>(p_node)) {
		r_g.had_label3d = true;
		if (my >= 0) {
			r_g.uncooked_types.insert("Label3D(no texture; type-1 node)");
			(void)lb;
		}
	}
	if (MultiMeshInstance3D *mmi = Object::cast_to<MultiMeshInstance3D>(p_node)) {
		r_g.had_multimesh = true;
		const Ref<MultiMesh> mm = mmi->get_multimesh();
		if (my >= 0 && mm.is_valid() && mm->get_mesh().is_valid()) {
			const int n = mm->get_instance_count();
			for (int i = 0; i < n; i++) {
				if (r_g.tris.size() >= kMaxTris) {
					break;
				}
				emit_mesh_resource(r_g, mm->get_mesh(), mm->get_instance_transform(i), my, nullptr);
			}
		}
	}
	if (MultiMeshInstance2D *mmi2 = Object::cast_to<MultiMeshInstance2D>(p_node)) {
		r_g.had_multimesh = true;
		const Ref<MultiMesh> mm = mmi2->get_multimesh();
		if (my >= 0 && mm.is_valid() && mm->get_mesh().is_valid()) {
			const int n = mm->get_instance_count();
			for (int i = 0; i < n; i++) {
				if (r_g.tris.size() >= kMaxTris) {
					break;
				}
				emit_mesh_resource(r_g, mm->get_mesh(), mm->get_instance_transform(i), my, nullptr);
			}
		}
	}
	if (Sprite2D *spr = Object::cast_to<Sprite2D>(p_node)) {
		r_g.sprite_count++;
		if (r_g.sprites.size() + r_g.anim_sprites.size() + r_g.sprites3d.size() < kMaxSprites) {
			if (my >= 0) {
				r_g.nodes.write[my].sprite_index = int16_t(r_g.sprites.size() + r_g.anim_sprites.size() + r_g.sprites3d.size());
			}
			r_g.sprites.push_back(spr);
		}
	}
	if (AnimatedSprite2D *as = Object::cast_to<AnimatedSprite2D>(p_node)) {
		r_g.sprite_count++;
		if (r_g.sprites.size() + r_g.anim_sprites.size() + r_g.sprites3d.size() < kMaxSprites) {
			if (my >= 0) {
				r_g.nodes.write[my].sprite_index = int16_t(r_g.sprites.size() + r_g.anim_sprites.size() + r_g.sprites3d.size());
			}
			r_g.anim_sprites.push_back(as);
		}
	}
	if (Sprite3D *s3 = Object::cast_to<Sprite3D>(p_node)) {
		r_g.sprite_count++;
		if (r_g.sprites.size() + r_g.anim_sprites.size() + r_g.sprites3d.size() < kMaxSprites) {
			if (my >= 0) {
				r_g.nodes.write[my].sprite_index = int16_t(r_g.sprites.size() + r_g.anim_sprites.size() + r_g.sprites3d.size());
				r_g.nodes.write[my].type = 3;
			}
			r_g.sprites3d.push_back(s3);
		}
	}
	if (VideoStreamPlayer *vp = Object::cast_to<VideoStreamPlayer>(p_node)) {
		if (!r_g.video) {
			r_g.video = vp;
		} else {
			r_g.extra_video++;
		}
	}
	if (p_node->get_script()) {
		const Ref<Script> sc = p_node->get_script();
		if (sc.is_valid() && !sc->get_path().is_empty()) {
			r_g.attached_scripts.push_back(sc->get_path());
		}
	}
	if (AnimationPlayer *ap = Object::cast_to<AnimationPlayer>(p_node)) {
		r_g.animation_player_count++;
		r_g.anim_players.push_back(ap);
	}
	if (Object::cast_to<Skeleton3D>(p_node) || p_node->is_class("SkeletonModifier3D") || p_node->is_class("Skin")) {
		r_g.had_skeleton = true;
	}
	if (MeshInstance3D *skin_mi = Object::cast_to<MeshInstance3D>(p_node)) {
		if (skin_mi->get_skin().is_valid()) {
			r_g.had_skeleton = true;
		}
	}
	if (p_node->is_class("AnimationTree")) {
		r_g.had_anim_tree = true;
	}
	if (Object::cast_to<CanvasLayer>(p_node)) {
		r_g.canvas_layer_count++;
		const Variant off = p_node->get("offset");
		if (off.get_type() == Variant::VECTOR2) {
			const Vector2 o = off;
			r_g.canvas_ox = int16_t(CLAMP(int(o.x), -32768, 32767));
			r_g.canvas_oy = int16_t(CLAMP(int(o.y), -32768, 32767));
		}
	}
	if ((Object::cast_to<Marker2D>(p_node) || Object::cast_to<Marker3D>(p_node) || p_node->is_in_group("waypoint") || p_node->is_in_group("waypoints")) && my >= 0) {
		if (r_g.ways.size() < 32) {
			SceneGather::WayRow w;
			w.node = uint8_t(my);
			w.path_id = 0;
			r_g.ways.push_back(w);
		} else {
			r_g.ways_dropped++;
		}
	}
	if ((p_node->is_in_group("nav") || p_node->is_in_group("navmesh")) && my >= 0) {
		Vector<Vector3> pts;
		pts.push_back(r_g.nodes[my].pos);
		cook_nav_verts(r_g, pts, my);
	}
	if (Control *ctl = Object::cast_to<Control>(p_node)) {
		r_g.had_control = true;
		emit_hud(ctl, r_g, my >= 0 ? my : 0);
	}
	if (TileMapLayer *layer = Object::cast_to<TileMapLayer>(p_node)) {
		r_g.tilemap_count++;
		const Ref<TileSet> ts = layer->get_tile_set();
		const uint8_t tex = register_tileset_atlas(r_g, ts);
		const Transform2D xf = world_xform_2d(layer);
		const Size2i tsz = ts.is_valid() ? ts->get_tile_size() : Size2i(16, 16);
		const TypedArray<Vector2i> cells = layer->get_used_cells();
		for (int i = 0; i < cells.size(); i++) {
			const Vector2i cell = cells[i];
			emit_tile_cell(r_g, xf.xform(Vector2(cell.x * tsz.x, cell.y * tsz.y)), layer->get_cell_atlas_coords(cell), tsz, tex, my >= 0 ? my : 0, tile_data_solid(layer->get_cell_tile_data(cell), ts));
		}
	} else if (TileMap *tm = Object::cast_to<TileMap>(p_node)) {
		r_g.tilemap_count++;
		const Ref<TileSet> ts = tm->get_tileset();
		const uint8_t tex = register_tileset_atlas(r_g, ts);
		const Transform2D xf = world_xform_2d(tm);
		const Size2i tsz = ts.is_valid() ? ts->get_tile_size() : Size2i(16, 16);
		for (int layer = 0; layer < tm->get_layers_count(); layer++) {
			const TypedArray<Vector2i> cells = tm->get_used_cells(layer);
			for (int i = 0; i < cells.size(); i++) {
				const Vector2i cell = cells[i];
				emit_tile_cell(r_g, xf.xform(Vector2(cell.x * tsz.x, cell.y * tsz.y)), tm->get_cell_atlas_coords(layer, cell), tsz, tex, my >= 0 ? my : 0, tile_data_solid(tm->get_cell_tile_data(layer, cell), ts));
			}
		}
	}
	for (int i = 0; i < p_node->get_child_count(); i++) {
		walk_scene(p_node->get_child(i), r_g, my >= 0 ? my : p_parent);
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

static void write_empty_sprites(Vector<uint8_t> &spr) {
	spr.push_back(0);
	spr.push_back(0);
	spr.push_back(0);
	spr.push_back(0);
}

struct SpriteCook {
	Vector2 pos;
	Vector2 size;
	Vector2 uv;
	uint8_t tex_slot = 0;
	uint8_t frame = 0;
	uint8_t nframes = 1;
	uint8_t fps = 0;
	uint8_t billboard = 0;
};

static void write_scene_sprites(const Vector<SpriteCook> &p_sprites, Vector<uint8_t> &spr) {
	auto u16 = [&](uint16_t v) {
		spr.push_back(uint8_t(v & 0xff));
		spr.push_back(uint8_t(v >> 8));
	};
	auto i16 = [&](int16_t v) { u16(uint16_t(v)); };
	u16(uint16_t(p_sprites.size()));
	u16(uint16_t(kCookABI));
	for (int i = 0; i < p_sprites.size(); i++) {
		i16(int16_t(CLAMP(int(p_sprites[i].pos.x), -1024, 1024)));
		i16(int16_t(CLAMP(int(p_sprites[i].pos.y), -1024, 1024)));
		const int w = CLAMP(int(p_sprites[i].size.x), 1, 255);
		const int h = CLAMP(int(p_sprites[i].size.y), 1, 255);
		u16(uint16_t(w));
		u16(uint16_t(h));
		spr.push_back(uint8_t(CLAMP(int(p_sprites[i].uv.x), 0, 255)));
		spr.push_back(uint8_t(CLAMP(int(p_sprites[i].uv.y), 0, 255)));
		u16(uint16_t(p_sprites[i].tex_slot & 15));
		spr.push_back(p_sprites[i].frame);
		spr.push_back(MAX(uint8_t(1), p_sprites[i].nframes));
		spr.push_back(p_sprites[i].fps);
		spr.push_back(p_sprites[i].billboard);
	}
}

static int16_t clamp_i16(float v) {
	return int16_t(CLAMP(int(v), -32767, 32767));
}

static void write_node_table(const SceneGather &p_g, float p_scale, Vector<uint8_t> &o) {
	auto u16 = [&](uint16_t v) {
		o.push_back(uint8_t(v & 0xff));
		o.push_back(uint8_t(v >> 8));
	};
	auto i16 = [&](int16_t v) { u16(uint16_t(v)); };
	o.push_back('N');
	o.push_back('O');
	o.push_back('D');
	o.push_back('E');
	u16(uint16_t(kCookABI));
	u16(uint16_t(p_g.nodes.size()));
	const float s = p_scale > 0.0f ? p_scale : 1.0f;
	for (int i = 0; i < p_g.nodes.size(); i++) {
		const SceneGather::CookedNode &n = p_g.nodes[i];
		i16(int16_t(n.parent));
		const CharString utf = n.name.utf8();
		o.push_back(uint8_t(CLAMP(utf.length(), 0, 255)));
		for (int c = 0; c < utf.length() && c < 255; c++) {
			o.push_back(uint8_t(utf[c]));
		}
		o.push_back(n.type);
		o.push_back(n.flags);
		i16(clamp_i16(n.pos.x * s));
		i16(clamp_i16(-n.pos.y * s));
		i16(clamp_i16(n.pos.z * s));
		i16(clamp_i16(n.euler.x * 4096.0f / float(Math::TAU)));
		i16(clamp_i16(n.euler.y * 4096.0f / float(Math::TAU)));
		i16(clamp_i16(n.euler.z * 4096.0f / float(Math::TAU)));
		uint16_t lo = n.tri_lo;
		uint16_t hi = n.tri_hi;
		if (lo >= uint16_t(kMaxTris)) {
			lo = 0;
			hi = 0;
		} else if (hi > uint16_t(kMaxTris)) {
			hi = uint16_t(kMaxTris);
		}
		u16(lo);
		u16(hi);
		i16(n.sprite_index);
		i16(n.script_index);
		o.push_back(n.group);
	}
	const Vector<String> &names = p_g.shared_groups ? *p_g.shared_groups : p_g.group_names;
	for (int g = 0; g < 8; g++) {
		CharString utf;
		if (g < names.size()) {
			utf = names[g].utf8();
		}
		for (int c = 0; c < 16; c++) {
			o.push_back(c < utf.length() ? uint8_t(utf[c]) : 0);
		}
	}
	for (int i = 0; i < 128; i++) {
		o.push_back(i < p_g.nodes.size() ? p_g.nodes[i].scroll : 0);
	}
	o.push_back(p_g.fog_on);
	o.push_back(0);
	u16(uint16_t(CLAMP(p_g.fog_start, 0, 65535)));
	u16(uint16_t(CLAMP(p_g.fog_end, 0, 65535)));
	o.push_back(p_g.fog_r);
	o.push_back(p_g.fog_g);
	o.push_back(p_g.fog_b);
	o.push_back(0);
	for (int i = 0; i < 128; i++) {
		o.push_back(i < p_g.nodes.size() ? p_g.nodes[i].ysort : 0);
	}
	for (int i = 0; i < 128; i++) {
		o.push_back(i < p_g.nodes.size() ? p_g.nodes[i].kind : 0);
	}
	for (int i = 0; i < 128; i++) {
		o.push_back(i < p_g.nodes.size() ? p_g.nodes[i].path_id : 0);
	}
	i16(p_g.canvas_ox);
	i16(p_g.canvas_oy);
	o.push_back(0);
	o.push_back(0);
	o.push_back(0);
	o.push_back(0);
}

static void write_hud_table(const Vector<SceneGather::HudRow> &p_hud, Vector<uint8_t> &o) {
	auto u16 = [&](uint16_t v) {
		o.push_back(uint8_t(v & 0xff));
		o.push_back(uint8_t(v >> 8));
	};
	auto i16 = [&](int16_t v) { u16(uint16_t(v)); };
	o.push_back('H');
	o.push_back('U');
	o.push_back('D');
	o.push_back('0');
	u16(uint16_t(kCookABI));
	u16(uint16_t(p_hud.size()));
	for (int i = 0; i < p_hud.size(); i++) {
		const SceneGather::HudRow &h = p_hud[i];
		i16(h.x);
		i16(h.y);
		i16(h.w);
		i16(h.h);
		o.push_back(h.kind);
		o.push_back(h.tex);
		o.push_back(h.rgb);
		o.push_back(h.node_id);
		o.push_back(h.flags);
		i16(h.value);
		i16(h.vmin);
		i16(h.vmax);
		const CharString utf = h.text.utf8();
		const int n = MIN(utf.length(), kMaxHudText);
		o.push_back(uint8_t(n));
		for (int c = 0; c < n; c++) {
			o.push_back(uint8_t(utf[c]));
		}
		const int ni = MIN(h.items.size(), 8);
		o.push_back(uint8_t(ni));
		for (int it = 0; it < ni; it++) {
			const CharString is = h.items[it].utf8();
			const int il = MIN(is.length(), 16);
			o.push_back(uint8_t(il));
			for (int c = 0; c < il; c++) {
				o.push_back(uint8_t(is[c]));
			}
		}
	}
}

static void write_tile_table(const SceneGather &p_g, Vector<uint8_t> &o) {
	auto u16 = [&](uint16_t v) {
		o.push_back(uint8_t(v & 0xff));
		o.push_back(uint8_t(v >> 8));
	};
	auto i16 = [&](int16_t v) { u16(uint16_t(v)); };
	o.push_back('T');
	o.push_back('I');
	o.push_back('L');
	o.push_back('E');
	u16(uint16_t(kCookABI));
	u16(uint16_t(p_g.tiles.size()));
	o.push_back(p_g.tile_w);
	o.push_back(p_g.tile_h);
	for (int i = 0; i < p_g.tiles.size(); i++) {
		const SceneGather::TileRow &t = p_g.tiles[i];
		i16(t.x);
		i16(t.y);
		o.push_back(t.u);
		o.push_back(t.v);
		o.push_back(t.tex);
		o.push_back(t.node_id);
		o.push_back(t.flags);
	}
}

struct ScenePackCook {
	String path;
	uint16_t node_lo = 0, node_hi = 0;
	uint16_t tri_lo = 0, tri_hi = 0;
	uint16_t hud_lo = 0, hud_hi = 0;
	uint16_t tile_lo = 0, tile_hi = 0;
	bool has_cam = false;
	Vector3 cam_pos;
	Vector3 cam_euler;
	uint16_t node_count = 0, tri_count = 0, hud_count = 0, tile_count = 0, tim_count = 0;
	uint32_t ram_bytes = 0, node_bytes = 0, mesh_bytes = 0, hud_bytes = 0, tile_bytes = 0;
};

struct AnimKeyCook {
	uint16_t t_ms = 0;
	uint8_t node_id = 0;
	uint8_t flags = 1;
	Vector3 pos;
	Vector3 euler;
};

struct AnimClipCook {
	String name;
	bool loop = false;
	Vector<AnimKeyCook> keys;
};

static int node_index_of(const SceneGather &p_g, const Node *p_node) {
	for (int i = 0; i < p_g.node_ptrs.size(); i++) {
		if (p_g.node_ptrs[i] == p_node) {
			return i;
		}
	}
	return -1;
}

static void merge_anim_key(AnimClipCook &r_clip, const AnimKeyCook &p_key) {
	for (int i = 0; i < r_clip.keys.size(); i++) {
		if (r_clip.keys[i].t_ms == p_key.t_ms && r_clip.keys[i].node_id == p_key.node_id) {
			AnimKeyCook &k = r_clip.keys.write[i];
			if (p_key.flags & 2) {
				k.pos = p_key.pos;
			}
			if (p_key.flags & 4) {
				k.euler = p_key.euler;
			}
			if (p_key.flags & 8) {
				k.flags = uint8_t((k.flags & ~uint8_t(1)) | (p_key.flags & 1));
			}
			k.flags |= uint8_t(p_key.flags & 14);
			return;
		}
	}
	if (r_clip.keys.size() < kMaxAnimKeys) {
		r_clip.keys.push_back(p_key);
	}
}

static void cook_anim_clips(SceneGather &p_g, Vector<AnimClipCook> &r_clips, bool &r_had_skip) {
	for (int i = 0; i < p_g.anim_players.size() && r_clips.size() < kMaxAnimClips; i++) {
		AnimationPlayer *ap = p_g.anim_players[i];
		if (!ap) {
			continue;
		}
		List<StringName> names;
		ap->get_animation_list(&names);
		Node *rel = ap->get_node_or_null(ap->get_root_node());
		if (!rel) {
			rel = ap->get_parent() ? ap->get_parent() : ap;
		}
		for (const List<StringName>::Element *E = names.front(); E && r_clips.size() < kMaxAnimClips; E = E->next()) {
			const Ref<Animation> anim = ap->get_animation(E->get());
			if (anim.is_null()) {
				continue;
			}
			AnimClipCook clip;
			clip.name = String(E->get());
			clip.loop = anim->get_loop_mode() != Animation::LOOP_NONE;
			for (int t = 0; t < anim->get_track_count(); t++) {
				const Animation::TrackType tt = anim->track_get_type(t);
				if (tt != Animation::TYPE_POSITION_3D && tt != Animation::TYPE_ROTATION_3D && tt != Animation::TYPE_VALUE) {
					r_had_skip = true;
					continue;
				}
				const NodePath np = anim->track_get_path(t);
				Node *target = rel ? rel->get_node_or_null(np) : nullptr;
				if (!target) {
					target = ap->get_node_or_null(np);
				}
				const int nid = node_index_of(p_g, target);
				if (nid < 0) {
					continue;
				}
				const String sub = np.get_concatenated_subnames();
				const bool is_vis = sub == "visible" || sub.ends_with(":visible");
				const bool is_pos = tt == Animation::TYPE_POSITION_3D || sub == "position" || sub.ends_with(":position");
				const bool is_rot = tt == Animation::TYPE_ROTATION_3D || sub == "rotation" || sub.ends_with(":rotation");
				if (tt == Animation::TYPE_VALUE && !is_vis && !is_pos && !is_rot) {
					r_had_skip = true;
					continue;
				}
				const Vector3 rest_pos = nid < p_g.nodes.size() ? p_g.nodes[nid].pos : Vector3();
				const Vector3 rest_eu = nid < p_g.nodes.size() ? p_g.nodes[nid].euler : Vector3();
				for (int k = 0; k < anim->track_get_key_count(t); k++) {
					AnimKeyCook key;
					key.t_ms = uint16_t(CLAMP(int(anim->track_get_key_time(t, k) * 1000.0), 0, 65535));
					key.node_id = uint8_t(CLAMP(nid, 0, 255));
					key.pos = rest_pos;
					key.euler = rest_eu;
					key.flags = 1;
					const Variant val = anim->track_get_key_value(t, k);
					if (is_pos && val.get_type() == Variant::VECTOR3) {
						key.pos = val;
						key.flags |= 2;
					} else if (is_rot) {
						if (val.get_type() == Variant::QUATERNION) {
							key.euler = Basis(Quaternion(val)).get_euler();
							key.flags |= 4;
						} else if (val.get_type() == Variant::VECTOR3) {
							key.euler = val;
							key.flags |= 4;
						}
					} else if (is_vis) {
						key.flags = uint8_t((key.flags & ~uint8_t(1)) | (bool(val) ? 1 : 0));
						key.flags |= 8;
					}
					merge_anim_key(clip, key);
				}
			}
			if (!clip.keys.is_empty()) {
				r_clips.push_back(clip);
			}
		}
	}
}

static void write_scene_packs(const Vector<ScenePackCook> &p_packs, float p_scale, Vector<uint8_t> &o) {
	auto u16 = [&](uint16_t v) {
		o.push_back(uint8_t(v & 0xff));
		o.push_back(uint8_t(v >> 8));
	};
	auto i16 = [&](int16_t v) { u16(uint16_t(v)); };
	auto u32 = [&](uint32_t v) {
		o.push_back(uint8_t(v & 0xff));
		o.push_back(uint8_t((v >> 8) & 0xff));
		o.push_back(uint8_t((v >> 16) & 0xff));
		o.push_back(uint8_t((v >> 24) & 0xff));
	};
	auto append_str = [&](const String &s) {
		const CharString utf = s.utf8();
		o.push_back(uint8_t(CLAMP(utf.length(), 0, 255)));
		for (int i = 0; i < utf.length() && i < 255; i++) {
			o.push_back(uint8_t(utf[i]));
		}
	};
	o.push_back('S');
	o.push_back('C');
	o.push_back('E');
	o.push_back('N');
	u16(uint16_t(kCookABI));
	u16(uint16_t(p_packs.size()));
	const float s = p_scale > 0.0f ? p_scale : 1.0f;
	for (int i = 0; i < p_packs.size(); i++) {
		const ScenePackCook &p = p_packs[i];
		append_str(p.path);
		u16(p.node_lo);
		u16(p.node_hi);
		u16(p.tri_lo);
		u16(p.tri_hi);
		u16(p.hud_lo);
		u16(p.hud_hi);
		u16(p.tile_lo);
		u16(p.tile_hi);
		o.push_back(p.has_cam ? 1 : 0);
		i16(clamp_i16(-p.cam_pos.x * s));
		i16(clamp_i16(p.cam_pos.y * s));
		i16(clamp_i16(p.cam_pos.z * s));
		i16(clamp_i16(p.cam_euler.x * 4096.0f / float(Math::TAU)));
		i16(clamp_i16(p.cam_euler.y * 4096.0f / float(Math::TAU)));
		i16(clamp_i16(p.cam_euler.z * 4096.0f / float(Math::TAU)));
		u16(p.node_count);
		u16(p.tri_count);
		u16(p.hud_count);
		u16(p.tile_count);
		u16(p.tim_count);
		u32(p.ram_bytes);
		u32(p.node_bytes);
		u32(p.mesh_bytes);
		u32(p.hud_bytes);
		u32(p.tile_bytes);
	}
}

static void write_anim_clips(const Vector<AnimClipCook> &p_clips, float p_scale, Vector<uint8_t> &o) {
	auto u16 = [&](uint16_t v) {
		o.push_back(uint8_t(v & 0xff));
		o.push_back(uint8_t(v >> 8));
	};
	auto i16 = [&](int16_t v) { u16(uint16_t(v)); };
	o.push_back('A');
	o.push_back('N');
	o.push_back('I');
	o.push_back('M');
	u16(uint16_t(kCookABI));
	u16(uint16_t(p_clips.size()));
	const float s = p_scale > 0.0f ? p_scale : 1.0f;
	for (int i = 0; i < p_clips.size(); i++) {
		const AnimClipCook &c = p_clips[i];
		const CharString utf = c.name.utf8();
		o.push_back(uint8_t(CLAMP(utf.length(), 0, 255)));
		for (int k = 0; k < utf.length() && k < 255; k++) {
			o.push_back(uint8_t(utf[k]));
		}
		const int nk = MIN(c.keys.size(), kMaxAnimKeys);
		o.push_back(uint8_t(nk));
		for (int k = 0; k < nk; k++) {
			const AnimKeyCook &key = c.keys[k];
			u16(key.t_ms);
			o.push_back(key.node_id);
			o.push_back(uint8_t((key.flags & 1) | (c.loop ? 2 : 0)));
			i16(clamp_i16(key.pos.x * s));
			i16(clamp_i16(-key.pos.y * s));
			i16(clamp_i16(key.pos.z * s));
			i16(clamp_i16(key.euler.x * 4096.0f / float(Math::TAU)));
			i16(clamp_i16(key.euler.y * 4096.0f / float(Math::TAU)));
			i16(clamp_i16(key.euler.z * 4096.0f / float(Math::TAU)));
		}
	}
}

static void mark_default_cameras(Vector<SceneGather::CamRow> &r_cams) {
	bool any = false;
	for (int i = 0; i < r_cams.size(); i++) {
		if (r_cams[i].is_default) {
			any = true;
			break;
		}
	}
	if (!any && !r_cams.is_empty()) {
		r_cams.write[0].is_default = true;
	}
}

static void write_cam_table(const Vector<SceneGather::CamRow> &p_cams, float p_scale, Vector<uint8_t> &o) {
	auto u16 = [&](uint16_t v) {
		o.push_back(uint8_t(v & 0xff));
		o.push_back(uint8_t(v >> 8));
	};
	auto i16 = [&](int16_t v) { u16(uint16_t(v)); };
	o.push_back('C');
	o.push_back('A');
	o.push_back('M');
	o.push_back('0');
	u16(uint16_t(kCookABI));
	u16(uint16_t(p_cams.size()));
	const float s = p_scale > 0.0f ? p_scale : 1.0f;
	for (int i = 0; i < p_cams.size(); i++) {
		const SceneGather::CamRow &c = p_cams[i];
		i16(int16_t(c.node_id));
		const CharString utf = c.name.utf8();
		o.push_back(uint8_t(CLAMP(utf.length(), 0, 255)));
		for (int k = 0; k < utf.length() && k < 255; k++) {
			o.push_back(uint8_t(utf[k]));
		}
		o.push_back(c.is_default ? 1 : 0);
		i16(clamp_i16(-c.pos.x * s));
		i16(clamp_i16(c.pos.y * s));
		i16(clamp_i16(c.pos.z * s));
		i16(clamp_i16(c.euler.x * 4096.0f / float(Math::TAU)));
		i16(clamp_i16(c.euler.y * 4096.0f / float(Math::TAU)));
		i16(clamp_i16(c.euler.z * 4096.0f / float(Math::TAU)));
		o.push_back(c.dim);
		o.push_back(c.drag);
		o.push_back(c.dead);
		i16(c.lim_l);
		i16(c.lim_t);
		i16(c.lim_r);
		i16(c.lim_b);
	}
}

static void write_text_bank(const Vector<SceneGather::TextLine> &p_lines, Vector<uint8_t> &o) {
	auto u16 = [&](uint16_t v) {
		o.push_back(uint8_t(v & 0xff));
		o.push_back(uint8_t(v >> 8));
	};
	o.push_back('T');
	o.push_back('X');
	o.push_back('T');
	o.push_back('0');
	u16(uint16_t(kCookABI));
	const int n = MIN(p_lines.size(), kMaxTextLines);
	u16(uint16_t(n));
	for (int i = 0; i < n; i++) {
		const CharString nm = p_lines[i].name.utf8();
		const CharString tx = p_lines[i].text.utf8();
		const int nl = CLAMP(nm.length(), 0, 31);
		const int tl = CLAMP(tx.length(), 0, 63);
		o.push_back(uint8_t(nl));
		for (int c = 0; c < nl; c++) {
			o.push_back(uint8_t(nm[c]));
		}
		o.push_back(uint8_t(tl));
		for (int c = 0; c < tl; c++) {
			o.push_back(uint8_t(tx[c]));
		}
	}
}

static void ingest_text_file(SceneGather &r_g, const String &p_path) {
	if (r_g.text_lines.size() >= kMaxTextLines) {
		return;
	}
	const String src = FileAccess::get_file_as_string(p_path);
	const Vector<String> rows = src.split("\n");
	for (int i = 0; i < rows.size() && r_g.text_lines.size() < kMaxTextLines; i++) {
		const String row = rows[i].strip_edges();
		if (row.is_empty() || row.begins_with("#")) {
			continue;
		}
		const int eq = row.find("=");
		if (eq <= 0) {
			continue;
		}
		SceneGather::TextLine tl;
		tl.name = row.substr(0, eq).strip_edges();
		tl.text = row.substr(eq + 1);
		if (!tl.name.is_empty()) {
			r_g.text_lines.push_back(tl);
		}
	}
}

static void scan_text_files(SceneGather &r_g) {
	if (FileAccess::exists("res://ps1_text.txt")) {
		ingest_text_file(r_g, "res://ps1_text.txt");
	}
	Ref<DirAccess> da = DirAccess::open("res://text");
	if (da.is_null()) {
		return;
	}
	da->list_dir_begin();
	for (String n = da->get_next(); !n.is_empty(); n = da->get_next()) {
		if (!n.ends_with(".txt")) {
			continue;
		}
		ingest_text_file(r_g, String("res://text").path_join(n));
	}
}

static void write_sfx_bank(const Vector<Ref<AudioStreamWAV>> &p_wavs, const Vector<String> &p_names, int p_budget, Vector<uint8_t> &r_out) {
	r_out.push_back('S');
	r_out.push_back('F');
	r_out.push_back('X');
	r_out.push_back('0');
	r_out.push_back(uint8_t(kCookABI & 0xff));
	r_out.push_back(uint8_t(kCookABI >> 8));
	r_out.push_back(uint8_t(p_wavs.size() & 0xff));
	r_out.push_back(uint8_t(p_wavs.size() >> 8));
	Vector<VagBlob> clips;
	int budget = p_budget;
	for (int i = 0; i < p_wavs.size(); i++) {
		Vector<int16_t> pcm;
		int rate = 22050;
		pcm_from_wav(p_wavs[i], pcm, rate);
		VagBlob one;
		const bool loop = p_wavs[i].is_valid() && p_wavs[i]->get_loop_mode() != AudioStreamWAV::LOOP_DISABLED;
		if (write_vag_pcm(pcm, rate, MAX(64, budget), one, loop) == OK) {
			clips.push_back(one);
			budget -= int(one.bytes.size());
		} else {
			clips.push_back(VagBlob());
		}
	}
	uint32_t off = uint32_t(8 + clips.size() * 28);
	for (int i = 0; i < clips.size(); i++) {
		const CharString nm = (i < p_names.size() ? p_names[i] : String()).utf8();
		for (int c = 0; c < 16; c++) {
			r_out.push_back(c < nm.length() ? uint8_t(nm[c]) : 0);
		}
		const uint32_t sz = uint32_t(clips[i].bytes.size());
		r_out.push_back(uint8_t(off & 0xff));
		r_out.push_back(uint8_t((off >> 8) & 0xff));
		r_out.push_back(uint8_t((off >> 16) & 0xff));
		r_out.push_back(uint8_t((off >> 24) & 0xff));
		r_out.push_back(uint8_t(sz & 0xff));
		r_out.push_back(uint8_t((sz >> 8) & 0xff));
		r_out.push_back(uint8_t((sz >> 16) & 0xff));
		r_out.push_back(uint8_t((sz >> 24) & 0xff));
		r_out.push_back(uint8_t(clips[i].sample_rate & 0xff));
		r_out.push_back(uint8_t((clips[i].sample_rate >> 8) & 0xff));
		r_out.push_back(0);
		r_out.push_back(0);
		off += sz;
	}
	for (int i = 0; i < clips.size(); i++) {
		r_out.append_array(clips[i].bytes);
	}
}

static void write_hit_table(const Vector<SceneGather::HitRow> &p_hits, float p_scale, Vector<uint8_t> &o) {
	auto u16 = [&](uint16_t v) {
		o.push_back(uint8_t(v & 0xff));
		o.push_back(uint8_t(v >> 8));
	};
	auto i16 = [&](int16_t v) { u16(uint16_t(v)); };
	o.push_back('H');
	o.push_back('I');
	o.push_back('T');
	o.push_back('0');
	u16(uint16_t(kCookABI));
	u16(uint16_t(p_hits.size()));
	const float s = p_scale > 0.0f ? p_scale : 1.0f;
	for (int i = 0; i < p_hits.size(); i++) {
		const SceneGather::HitRow &h = p_hits[i];
		i16(int16_t(h.node_id));
		o.push_back(h.dim);
		o.push_back(h.kind);
		o.push_back(h.flags);
		o.push_back(h.layer ? h.layer : 1);
		i16(clamp_i16(h.mn.x * s));
		i16(clamp_i16(-h.mx.y * s));
		i16(clamp_i16(h.mn.z * s));
		i16(clamp_i16(h.mx.x * s));
		i16(clamp_i16(-h.mn.y * s));
		i16(clamp_i16(h.mx.z * s));
	}
}

static void fill_nav_from_hits(SceneGather &r_g) {
	if (r_g.nav_verts.is_empty()) {
	for (int i = 0; i < r_g.hits.size() && r_g.nav_verts.size() < 32; i++) {
		if (!(r_g.hits[i].flags & 1)) {
			continue;
		}
		SceneGather::NavVert v;
		v.pos = (r_g.hits[i].mn + r_g.hits[i].mx) * 0.5f;
		v.node = r_g.hits[i].node_id;
		r_g.nav_verts.push_back(v);
	}
	for (int i = 0; i + 1 < r_g.nav_verts.size() && r_g.nav_edges.size() < 48; i++) {
		SceneGather::NavEdge e;
		e.a = uint8_t(i);
		e.b = uint8_t(i + 1);
		r_g.nav_edges.push_back(e);
	}
	}
	if (!r_g.nav_obs.is_empty() && !r_g.nav_verts.is_empty()) {
		Vector<SceneGather::NavVert> kept;
		Vector<int> map;
		map.resize(r_g.nav_verts.size());
		for (int i = 0; i < r_g.nav_verts.size(); i++) {
			map.write[i] = -1;
			bool drop = false;
			for (int o = 0; o < r_g.nav_obs.size(); o++) {
				if (r_g.nav_verts[i].pos.distance_to(r_g.nav_obs[o].pos) <= r_g.nav_obs[o].radius) {
					drop = true;
					break;
				}
			}
			if (!drop) {
				map.write[i] = kept.size();
				kept.push_back(r_g.nav_verts[i]);
			}
		}
		Vector<SceneGather::NavEdge> ek;
		for (int i = 0; i < r_g.nav_edges.size(); i++) {
			const int a = r_g.nav_edges[i].a < map.size() ? map[r_g.nav_edges[i].a] : -1;
			const int b = r_g.nav_edges[i].b < map.size() ? map[r_g.nav_edges[i].b] : -1;
			if (a >= 0 && b >= 0) {
				SceneGather::NavEdge e;
				e.a = uint8_t(a);
				e.b = uint8_t(b);
				ek.push_back(e);
			}
		}
		r_g.nav_verts = kept;
		r_g.nav_edges = ek;
	}
	if (!r_g.nav_verts.is_empty()) {
		r_g.had_nav = true;
	}
}

static void write_nav_table(const SceneGather &p_g, float p_scale, Vector<uint8_t> &o) {
	auto u16 = [&](uint16_t v) {
		o.push_back(uint8_t(v & 0xff));
		o.push_back(uint8_t(v >> 8));
	};
	auto i16 = [&](int16_t v) { u16(uint16_t(v)); };
	o.push_back('N');
	o.push_back('A');
	o.push_back('V');
	o.push_back('0');
	u16(uint16_t(kCookABI));
	const int nv = MIN(p_g.nav_verts.size(), 32);
	const int ne = MIN(p_g.nav_edges.size(), 48);
	o.push_back(uint8_t(nv));
	o.push_back(uint8_t(ne));
	const float s = p_scale > 0.0f ? p_scale : 1.0f;
	for (int i = 0; i < nv; i++) {
		i16(clamp_i16(p_g.nav_verts[i].pos.x * s));
		i16(clamp_i16(-p_g.nav_verts[i].pos.y * s));
		i16(clamp_i16(p_g.nav_verts[i].pos.z * s));
		i16(int16_t(p_g.nav_verts[i].node));
	}
	for (int i = 0; i < ne; i++) {
		o.push_back(p_g.nav_edges[i].a);
		o.push_back(p_g.nav_edges[i].b);
	}
}

static void write_path_table(const SceneGather &p_g, float p_scale, Vector<uint8_t> &o) {
	auto u16 = [&](uint16_t v) {
		o.push_back(uint8_t(v & 0xff));
		o.push_back(uint8_t(v >> 8));
	};
	auto i16 = [&](int16_t v) { u16(uint16_t(v)); };
	o.push_back('P');
	o.push_back('A');
	o.push_back('T');
	o.push_back('H');
	u16(uint16_t(kCookABI));
	o.push_back(uint8_t(MIN(p_g.paths.size(), 8)));
	o.push_back(0);
	const float s = p_scale > 0.0f ? p_scale : 1.0f;
	const int n = MIN(p_g.paths.size(), 8);
	for (int i = 0; i < n; i++) {
		const SceneGather::PathRow &p = p_g.paths[i];
		o.push_back(p.id);
		o.push_back(p.npts);
		o.push_back(p.closed);
		o.push_back(0);
		for (int k = 0; k < 16; k++) {
			if (k < p.npts) {
				i16(clamp_i16(p.pts[k].x * s));
				i16(clamp_i16(-p.pts[k].y * s));
				i16(clamp_i16(p.pts[k].z * s));
			} else {
				i16(0);
				i16(0);
				i16(0);
			}
		}
	}
}

static void write_way_table(const SceneGather &p_g, Vector<uint8_t> &o) {
	auto u16 = [&](uint16_t v) {
		o.push_back(uint8_t(v & 0xff));
		o.push_back(uint8_t(v >> 8));
	};
	o.push_back('W');
	o.push_back('A');
	o.push_back('Y');
	o.push_back('0');
	u16(uint16_t(kCookABI));
	o.push_back(uint8_t(MIN(p_g.ways.size(), 32)));
	o.push_back(0);
	for (int i = 0; i < 32; i++) {
		if (i < p_g.ways.size()) {
			o.push_back(p_g.ways[i].node);
			o.push_back(p_g.ways[i].path_id);
			o.push_back(0);
			o.push_back(0);
		} else {
			o.push_back(0);
			o.push_back(0);
			o.push_back(0);
			o.push_back(0);
		}
	}
}

static void scan_script_scenes(Vector<String> &r_paths) {
	Vector<String> stack;
	stack.push_back("res://");
	while (!stack.is_empty()) {
		const String dir = stack[stack.size() - 1];
		stack.resize(stack.size() - 1);
		Ref<DirAccess> da = DirAccess::open(dir);
		if (da.is_null()) {
			continue;
		}
		da->list_dir_begin();
		for (String n = da->get_next(); !n.is_empty(); n = da->get_next()) {
			if (n == "." || n == ".." || n.begins_with(".")) {
				continue;
			}
			const String path = dir.path_join(n);
			if (da->current_is_dir()) {
				if (n == "export" || n == ".godot" || n == "addons" || n == "tests" || n == "tools") {
					continue;
				}
				stack.push_back(path);
				continue;
			}
			if (!(n.ends_with(".gd") || n.ends_with(".luau"))) {
				continue;
			}
			if (n == "run_tests.gd" || n.begins_with("test_")) {
				continue;
			}
			const String src = FileAccess::get_file_as_string(path);
			String why;
			if (source_is_rejected(src, why)) {
				continue;
			}
			collect_literal_scenes(src, r_paths);
		}
	}
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

	r_out.warnings.push_back("24 literal packs; extra packs are ISO/pcdrv files. Several may be resident if they fit 2 MiB + 128 nodes + 8192 tris + 16 TIM.");
	r_out.warnings.push_back("PS1.load_scene/unload_scene/can_load_scene/can_instantiate + PS1.get_free_ram/get_static_memory_usage; over-budget load or instantiate is refused. PS1.* is PS1 export only (OS.has_feature(\"ps1\")); not registered on a normal game.");
	r_out.warnings.push_back("NPCs should instantiate from a few resident templates; each copy spends node/HUD/TILE slots + row RAM. Do not give each NPC its own pack.");
	r_out.warnings.push_back("instantiate fails (nil) when free nodes/HUD/tiles or 2 MiB would overflow; queue_free or PS1.unload_scene first. Mesh/TIM are shared with the loaded template. PS1.* is not available on a non-PS1 export.");
	r_out.warnings.push_back("change_scene loads a pack if it fits, then makes it active (camera/_ready). Other residents stay drawn.");
	r_out.warnings.push_back("extra packs stream from ISO (ps1/make_iso); EXE embed is pack 0 only.");
	r_out.warnings.push_back("create_timer has 8 slots; poll is_timer_done or connect timeout.");
	r_out.warnings.push_back("VideoStreamPlayer.play() replays the boot STR (blocking; START skips). PS1.play_fmv streams pack STR then frees; is_playing is true only during play.");
	r_out.warnings.push_back("loop flag is ADPCM 0x02/0x03 on the single embedded WAV; stop keys off voice 0.");
	r_out.warnings.push_back("PAL display uses 50 Hz delta; NTSC stays 60.");
	r_out.warnings.push_back("connect is first-slice (timeout/pressed/animation_finished, 16 slots); other signals are ignored.");
	r_out.warnings.push_back("InputMap: 32 actions in INP1; Input.is_action_* ORs pad 0+1; analog enter via PAD_CMD_*; DualShock sticks via get_vector/get_joy_axis/PS1.get_stick; get_action_strength; set_stick_deadzone; rumble motors poll; no L3/R3, no DS2 pressure.");
	r_out.warnings.push_back("8 cameras; Camera2D in the catalog (XY follow, drag, limits); not a 9th camera. attach/look/orbit/set_camera_transform; shake_camera; world_to_screen; set_camera_scale is GTE FOV approx.");
	r_out.warnings.push_back("AABB + ray + move_and_slide + move_and_drive + tile solids + one-way; HIT 8-bit layer + query mask; ray skips caller AABB; is_on_floor/wall/ceiling; raycast_point; per-node get_velocity; AABB drive, not VehicleWheel / Godot physics.");
	r_out.warnings.push_back("ANIM/SPRITE/HIT/CAM/SFX/TXT/MUSIC/STR stream; extra-pack TPAK LoadImage on load_scene (fail VRAM → false); instantiate copies hit+sprite, shares anim/mesh.");
	r_out.warnings.push_back("get_node walks the cooked tree; FileAccess user:// is the 24 KiB payload; not CARD BIOS. Memcard is Sony MC/SC wrap of user:// via pcdrv SAVE.MCD.");
	r_out.warnings.push_back("can_load_* is a real budget/slice check; missing slice → false. is_*_loaded is the resident flag.");
	r_out.warnings.push_back("2D/3D Godot scenes cook to AABB/mesh/sprite/HUD/cam/light while they fit 2 MiB; CharacterBody velocity + argless move_and_slide; AABB slide, not Jolt / Viewport / NavigationServer / skin.");
	r_out.warnings.push_back("ABI 20: get_forward/right/up, translate_local, move_and_fly (6DoF AABB, not RigidBody); per-node is_on_floor; apply_gravity; not coyote.");
	r_out.warnings.push_back("raycast_group + get_ray_point/node; 32 shots are not nodes; orbit clip; y-sort; wrap_position; set_cull_dist is draw cull.");
	r_out.warnings.push_back("follow_node; 32 marker waypoints; 8 paths x 16 points; PathFollow tick; not Curve3D.");
	r_out.warnings.push_back("32-vert NAV graph; nav_next one hop; not NavigationServer / get_nav_path.");
	r_out.warnings.push_back("8 linear tweens; create_tween allowlisted; not SceneTreeTween tracks.");
	r_out.warnings.push_back("RayCast/ShapeCast (8); VisibleOnScreen AABB; RemoteTransform; SpringArm ray; Timer nodes = 8 slots.");
	r_out.warnings.push_back("unload extra packs and slices; pack 0 stays in the EXE; shots/cull/nav/path/tweens keep the 128/2 MiB walls.");
	r_out.warnings.push_back("Camera / body / area / particles / light / audio / sprite methods live on those nodes; PS1.* is an alias. Instantiate a template to spawn; not a hidden global pool.");
	r_out.warnings.push_back("8 group bits + get_in_group(i); not get_nodes_in_group Array.");
	r_out.warnings.push_back("Slide inherits floor AABB velocity; get_floor_node / get_floor_normal; optional 45° tile slope.");
	r_out.warnings.push_back("find_nearest; runtime billboard; ui_square/ui_triangle/ui_l2/ui_r2/ui_back (Select). ui_select stays Start.");
	r_out.warnings.push_back("24 KiB poke/peek + wrap/unwrap persist; not CARD BIOS. No ConfigFile/JSON class, no DexDrive.");
	r_out.warnings.push_back("VM sprites draw frame UV + flip_h + flip_v; Sprite3D POLY_FT4; 32 sprites; 16 unlit particles (billboard/flipbook/one-segment trail/point/vertex); 4 streams; not GPUParticles / tube; fog/fade; set_light writes GTE.");
	r_out.warnings.push_back("16 clips / 64 keys; ANIM loop/seek/speed; Skeleton3D is bind pose; no AnimationTree.");
	r_out.warnings.push_back("HUD text 64 chars; TXT bank 32x64; str(float)/set_hud_text; set_paused skips _process; per-node set_process.");
	r_out.warnings.push_back("get_position / get_global_position walks parents; 3D look_at + 2D look_at; atan2/length/normalized/dot/cross; OS.has_feature(\"ps1\") allowlisted; PS1.* export only.");
	r_out.warnings.push_back("SFX bank 16 clips / voices 1-7 + set_music_volume; pack-0 VAG plus streamable MUSIC; CD-ROM XA/STR stream via CdSearchFile + CdlReadS when a disc is present; no disc → play_xa false, MUSIC VAG; official Luau is tape-only.");

	SceneGather gathered;
	gathered.shared_groups = &gathered.group_names;
	String src_name;
	Vector<Node *> live_roots;
	Vector<ScenePackCook> packs;
	if (ResourceLoader::exists(main_scene)) {
		Ref<PackedScene> ps = ResourceLoader::load(main_scene);
		if (ps.is_valid()) {
			Node *root = ps->instantiate();
			live_roots.push_back(root);
			walk_scene(root, gathered, -1);
			scan_text_files(gathered);
			if (!gathered.tris.is_empty()) {
				src_name = main_scene;
			}
			ScenePackCook main_pack;
			main_pack.path = main_scene;
			main_pack.node_hi = uint16_t(gathered.nodes.size());
			main_pack.tri_hi = uint16_t(gathered.tris.size());
			main_pack.hud_hi = uint16_t(gathered.hud.size());
			main_pack.tile_hi = uint16_t(gathered.tiles.size());
			main_pack.node_count = main_pack.node_hi;
			main_pack.tri_count = main_pack.tri_hi;
			main_pack.hud_count = main_pack.hud_hi;
			main_pack.tile_count = main_pack.tile_hi;
			if (Camera3D *cam = gathered.cam_current ? gathered.cam_current : gathered.cam_any) {
				main_pack.has_cam = true;
				const Transform3D xf = world_xform_3d(cam);
				main_pack.cam_pos = xf.origin;
				main_pack.cam_euler = xf.basis.get_euler();
			}
			packs.push_back(main_pack);
		}
	} else {
		r_out.errors.push_back("main_scene does not exist: " + main_scene);
		r_out.report = r_out.errors[0];
		return FAILED;
	}

	Vector<String> extra_scenes;
	scan_script_scenes(extra_scenes);
	Vector<SceneGather> extra_gathers;
	for (int i = 0; i < extra_scenes.size(); i++) {
		const String path = extra_scenes[i];
		if (path == main_scene) {
			continue;
		}
		if (!ResourceLoader::exists(path)) {
			r_out.errors.push_back("change_scene/load path not cooked: " + path);
			r_out.report = r_out.errors[r_out.errors.size() - 1];
			for (int r = 0; r < live_roots.size(); r++) {
				memdelete(live_roots[r]);
			}
			return ERR_UNCONFIGURED;
		}
		if (packs.size() >= kMaxScenePacks) {
			r_out.warnings.push_back("kept first 24 scene packs.");
			break;
		}
		Ref<PackedScene> extra = ResourceLoader::load(path);
		if (extra.is_null()) {
			r_out.errors.push_back("change_scene/load path not cooked: " + path);
			r_out.report = r_out.errors[r_out.errors.size() - 1];
			for (int r = 0; r < live_roots.size(); r++) {
				memdelete(live_roots[r]);
			}
			return ERR_UNCONFIGURED;
		}
		Node *er = extra->instantiate();
		live_roots.push_back(er);
		SceneGather extra_g;
		extra_g.shared_groups = &gathered.group_names;
		walk_scene(er, extra_g, -1);
		ScenePackCook pack;
		pack.path = path;
		pack.node_count = uint16_t(extra_g.nodes.size());
		pack.tri_count = uint16_t(MIN(extra_g.tris.size(), kMaxTris));
		pack.hud_count = uint16_t(extra_g.hud.size());
		pack.tile_count = uint16_t(extra_g.tiles.size());
		pack.tim_count = uint16_t(MIN(extra_g.images.size(), kMaxTims));
		if (Camera3D *cam = extra_g.cam_current ? extra_g.cam_current : extra_g.cam_any) {
			pack.has_cam = true;
			const Transform3D xf = world_xform_3d(cam);
			pack.cam_pos = xf.origin;
			pack.cam_euler = xf.basis.get_euler();
		}
		if (extra_g.node_dropped > 0) {
			r_out.warnings.push_back(vformat("kept first %d of %d nodes in %s.", kMaxNodes, kMaxNodes + extra_g.node_dropped, path));
		}
		if (extra_g.tris.size() > kMaxTris) {
			r_out.warnings.push_back(vformat("kept first %d of %d triangles in %s.", kMaxTris, extra_g.tris.size(), path));
		}
		if (extra_g.hud_dropped > 0) {
			r_out.warnings.push_back(vformat("kept first %d of %d HUD widgets in %s.", kMaxHud, kMaxHud + extra_g.hud_dropped, path));
		}
		if (extra_g.tile_dropped > 0) {
			r_out.warnings.push_back(vformat("kept first %d of %d tiles in %s.", kMaxTiles, kMaxTiles + extra_g.tile_dropped, path));
		}
		if (extra_g.cam_dropped > 0) {
			r_out.warnings.push_back(vformat("kept first %d cameras in %s.", kMaxCameras, path));
		}
		if (extra_g.aabb_dropped > 0) {
			r_out.warnings.push_back(vformat("kept first %d AABBs in %s.", kMaxAABBs, path));
		}
		gathered.groups_dropped += extra_g.groups_dropped;
		gathered.slope_tiles += extra_g.slope_tiles;
		gathered.had_body = gathered.had_body || extra_g.had_body;
		gathered.had_csg = gathered.had_csg || extra_g.had_csg;
		gathered.had_gridmap = gathered.had_gridmap || extra_g.had_gridmap;
		gathered.had_multimesh = gathered.had_multimesh || extra_g.had_multimesh;
		gathered.had_draw2d = gathered.had_draw2d || extra_g.had_draw2d;
		gathered.had_label3d = gathered.had_label3d || extra_g.had_label3d;
		gathered.had_world_env = gathered.had_world_env || extra_g.had_world_env;
		extra_gathers.push_back(extra_g);
		packs.push_back(pack);
	}

	Vector<AnimClipCook> anim_clips;
	cook_anim_clips(gathered, anim_clips, gathered.had_anim_skip);
	Vector<Vector<AnimClipCook>> extra_anims;
	for (int e = 0; e < extra_gathers.size(); e++) {
		Vector<AnimClipCook> clips;
		bool skip = false;
		cook_anim_clips(extra_gathers.write[e], clips, skip);
		extra_anims.push_back(clips);
	}

	Vector<CookTri> src_tris = gathered.tris;
	Vector<Ref<Image>> images = gathered.images;
	Vector<String> image_names = gathered.image_names;
	bool have_fmv = false;
	{
		Ref<DirAccess> fmv_peek = DirAccess::open("res://fmv");
		if (fmv_peek.is_valid()) {
			fmv_peek->list_dir_begin();
			for (String n = fmv_peek->get_next(); !n.is_empty(); n = fmv_peek->get_next()) {
				if (n.ends_with(".png") || n.ends_with(".jpg")) {
					have_fmv = true;
					break;
				}
			}
		}
	}
	if (src_tris.is_empty()) {
		if (gathered.sprites.is_empty() && !gathered.video && !have_fmv && gathered.hud.is_empty() && gathered.tiles.is_empty()) {
			r_out.errors.push_back("main_scene has no MeshInstance3D, Sprite2D, FMV, HUD, or tiles; not substituting Industrial.");
			r_out.report = r_out.errors[0];
			for (int r = 0; r < live_roots.size(); r++) {
				memdelete(live_roots[r]);
			}
			return ERR_UNCONFIGURED;
		}
		r_out.infos.push_back("main_scene has no MeshInstance3D; guest is sprite/FMV/HUD/tile-only.");
		if (src_name.is_empty()) {
			src_name = main_scene;
		}
	} else if (images.is_empty()) {
		r_out.warnings.push_back("no albedo on cooked meshes; used Industrial Concrete / orange fallback.");
		const String png = "res://assets/Industrial_exterior_v2_Concrete.png";
		Ref<Image> from_png = try_load_png(png);
		if (from_png.is_valid()) {
			images.push_back(from_png);
			image_names.push_back(png);
			gathered.images.push_back(from_png);
			gathered.image_names.push_back(png);
		} else {
			Ref<Image> fallback = Image::create_empty(64, 64, false, Image::FORMAT_RGBA8);
			fallback->fill(Color(0.85f, 0.55f, 0.15f));
			images.push_back(fallback);
			image_names.push_back("res://generated/fallback.png");
			gathered.images.push_back(fallback);
			gathered.image_names.push_back("res://generated/fallback.png");
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
		sc.nframes = 1;
		sprite_cooks.push_back(sc);
	}
	for (int i = 0; i < gathered.anim_sprites.size(); i++) {
		AnimatedSprite2D *as = gathered.anim_sprites[i];
		SpriteCook sc;
		sc.pos = world_xform_2d(as).get_origin();
		sc.size = Vector2(16, 16);
		sc.uv = Vector2(0, 0);
		sc.nframes = 1;
		const Ref<SpriteFrames> frames = as->get_sprite_frames();
		if (frames.is_valid()) {
			const StringName anim = as->get_animation();
			const int nf = frames->get_frame_count(anim);
			sc.nframes = uint8_t(CLAMP(nf, 1, 16));
			sc.fps = uint8_t(CLAMP(int(frames->get_animation_speed(anim)), 0, 60));
			if (nf > 0) {
				const Ref<Texture2D> tex = frames->get_frame_texture(anim, 0);
				if (tex.is_valid()) {
					sc.size = tex->get_size();
					sc.tex_slot = register_image(gathered, image_from_texture(tex), tex->get_path());
				}
			}
			if (nf > 16) {
				r_out.warnings.push_back("AnimatedSprite2D frames; kept first 16.");
			}
		}
		sprite_cooks.push_back(sc);
	}
	for (int i = 0; i < gathered.sprites3d.size(); i++) {
		Sprite3D *s3 = gathered.sprites3d[i];
		SpriteCook sc;
		const Vector3 p = world_xform_3d(s3).origin;
		sc.pos = Vector2(p.x, -p.y);
		sc.size = Vector2(16, 16);
		sc.billboard = 1;
		sc.nframes = 1;
		if (s3->get_texture().is_valid()) {
			sc.size = s3->get_texture()->get_size();
			sc.tex_slot = register_image(gathered, image_from_texture(s3->get_texture()), s3->get_texture()->get_path());
		}
		sprite_cooks.push_back(sc);
	}
	Vector<Vector<SpriteCook>> extra_sprite_cooks;
	for (int e = 0; e < extra_gathers.size(); e++) {
		Vector<SpriteCook> esc;
		SceneGather &eg = extra_gathers.write[e];
		for (int i = 0; i < eg.sprites.size(); i++) {
			Sprite2D *s = eg.sprites[i];
			SpriteCook sc;
			const Rect2 r = s->get_rect();
			sc.pos = world_xform_2d(s).xform(r.position);
			sc.size = r.size;
			if (s->is_region_enabled()) {
				sc.uv = s->get_region_rect().position;
			}
			sc.tex_slot = register_image(eg, image_from_texture(s->get_texture()), s->get_texture().is_valid() ? s->get_texture()->get_path() : String());
			sc.nframes = 1;
			esc.push_back(sc);
		}
		for (int i = 0; i < eg.anim_sprites.size(); i++) {
			AnimatedSprite2D *as = eg.anim_sprites[i];
			SpriteCook sc;
			sc.pos = world_xform_2d(as).get_origin();
			sc.size = Vector2(16, 16);
			sc.nframes = 1;
			const Ref<SpriteFrames> frames = as->get_sprite_frames();
			if (frames.is_valid()) {
				const StringName anim = as->get_animation();
				sc.nframes = uint8_t(CLAMP(frames->get_frame_count(anim), 1, 16));
				sc.fps = uint8_t(CLAMP(int(frames->get_animation_speed(anim)), 0, 60));
				if (frames->get_frame_count(anim) > 0) {
					const Ref<Texture2D> tex = frames->get_frame_texture(anim, 0);
					if (tex.is_valid()) {
						sc.size = tex->get_size();
						sc.tex_slot = register_image(eg, image_from_texture(tex), tex->get_path());
					}
				}
			}
			esc.push_back(sc);
		}
		for (int i = 0; i < eg.sprites3d.size(); i++) {
			Sprite3D *s3 = eg.sprites3d[i];
			SpriteCook sc;
			const Vector3 p = world_xform_3d(s3).origin;
			sc.pos = Vector2(p.x, -p.y);
			sc.size = Vector2(16, 16);
			sc.billboard = 1;
			sc.nframes = 1;
			if (s3->get_texture().is_valid()) {
				sc.size = s3->get_texture()->get_size();
				sc.tex_slot = register_image(eg, image_from_texture(s3->get_texture()), s3->get_texture()->get_path());
			}
			esc.push_back(sc);
		}
		extra_sprite_cooks.push_back(esc);
	}
	if (!sprite_cooks.is_empty() || !gathered.hud.is_empty() || !gathered.tiles.is_empty()) {
		// #region agent log
		{
			FILE *logf = fopen("D:/projects/ps1_blazium/debug-9aa74d.log", "a");
			if (logf) {
				fprintf(logf, "{\"sessionId\":\"9aa74d\",\"runId\":\"cook\",\"hypothesisId\":\"A\",\"location\":\"cooker.cpp:images-reassign-before\",\"message\":\"HUD/sprite path about to copy gathered.images\",\"data\":{\"local_images\":%d,\"gathered_images\":%d,\"hud\":%d,\"sprites\":%d,\"tris\":%d},\"timestamp\":%lld}\n",
						int(images.size()), int(gathered.images.size()), int(gathered.hud.size()), int(sprite_cooks.size()), int(src_tris.size()), (long long)time(nullptr) * 1000);
				fclose(logf);
			}
		}
		// #endregion
		images = gathered.images;
		image_names = gathered.image_names;
		if (images.is_empty() && !src_tris.is_empty()) {
			Ref<Image> fallback = Image::create_empty(64, 64, false, Image::FORMAT_RGBA8);
			fallback->fill(Color(0.85f, 0.55f, 0.15f));
			images.push_back(fallback);
			image_names.push_back("res://generated/fallback.png");
		}
		// #region agent log
		{
			FILE *logf = fopen("D:/projects/ps1_blazium/debug-9aa74d.log", "a");
			if (logf) {
				fprintf(logf, "{\"sessionId\":\"9aa74d\",\"runId\":\"cook\",\"hypothesisId\":\"A\",\"location\":\"cooker.cpp:images-reassign-after\",\"message\":\"images after HUD/sprite copy\",\"data\":{\"local_images\":%d,\"gathered_images\":%d},\"timestamp\":%lld}\n",
						int(images.size()), int(gathered.images.size()), (long long)time(nullptr) * 1000);
				fclose(logf);
			}
		}
		// #endregion
	}
	Vector<CookLight> lights = gathered.lights;
	for (int r = 0; r < live_roots.size(); r++) {
		memdelete(live_roots[r]);
	}
	live_roots.clear();
	gathered.node_ptrs.clear();
	gathered.anim_players.clear();
	for (int e = 0; e < extra_gathers.size(); e++) {
		extra_gathers.write[e].node_ptrs.clear();
		extra_gathers.write[e].anim_players.clear();
		extra_gathers.write[e].sprites.clear();
		extra_gathers.write[e].anim_sprites.clear();
		mark_default_cameras(extra_gathers.write[e].cameras);
	}
	mark_default_cameras(gathered.cameras);

	if (src_tris.size() > kMaxTris) {
		r_out.warnings.push_back(vformat("kept first %d of %d triangles (dropped %d). Reduce mesh density.", kMaxTris, src_tris.size(), src_tris.size() - kMaxTris));
	}
	if (!gathered.ignored_textures.is_empty()) {
		r_out.warnings.push_back(vformat("used first %d albedo textures, ignored: ", kMaxTims) + String(", ").join(gathered.ignored_textures));
	}
	if (gathered.sprite_count > kMaxSprites) {
		r_out.warnings.push_back(vformat("kept first %d of %d Sprite2D nodes.", kMaxSprites, gathered.sprite_count));
	}
	if (gathered.light_count > kMaxLights) {
		r_out.warnings.push_back(vformat("kept first %d of %d lights (GTE has 3).", kMaxLights, gathered.light_count));
	}
	if (gathered.cam_dropped > 0) {
		r_out.warnings.push_back(vformat("kept first %d of %d cameras.", kMaxCameras, kMaxCameras + gathered.cam_dropped));
	}
	if (gathered.aabb_dropped > 0) {
		r_out.warnings.push_back(vformat("kept first %d AABBs.", kMaxAABBs));
	}
	if (gathered.animation_player_count > 0) {
		r_out.warnings.push_back("AnimationPlayer cooks position/rotation/visible tracks (16 clips, 64 keys). Method/audio/bezier/blend/AnimationTree are skipped.");
	}
	if (gathered.had_anim_skip || gathered.had_anim_tree) {
		r_out.warnings.push_back("Animation method/audio/bezier/blend/AnimationTree tracks are skipped.");
	}
	if (gathered.had_skeleton) {
		r_out.warnings.push_back("Skeleton3D skinning is not applied; mesh is bind pose. AnimationPlayer node xform tracks still play.");
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
	if (gathered.node_dropped > 0) {
		r_out.warnings.push_back(vformat("kept first %d of %d nodes (dropped %d).", kMaxNodes, kMaxNodes + gathered.node_dropped, gathered.node_dropped));
	}
	if (gathered.mesh_on_dropped > 0) {
		r_out.warnings.push_back("MeshInstance3D on dropped node (kept first 128).");
	}
	if (gathered.no_albedo_surfaces > 0) {
		r_out.warnings.push_back("no albedo on node/surface; using TIM slot 0");
	}
	if (gathered.extra_pbr > 0) {
		r_out.warnings.push_back("only albedo is cooked; ignored metallic/roughness/normal/emission maps.");
	}
	if (gathered.had_generic_light) {
		r_out.warnings.push_back("generic Light3D cooked as a directional GTE light.");
	}
	if (gathered.stream_dropped > 0) {
		r_out.warnings.push_back(vformat("kept first 4 particle streams (dropped %d).", gathered.stream_dropped));
	}
	if (gathered.had_control) {
		r_out.warnings.push_back("Control signals are not cooked; poll pressed/button_pressed/value in _process.");
		r_out.warnings.push_back("Control theme/StyleBox/mouse are not cooked; pad-focus only.");
	}
	if (gathered.canvas_layer_count > 0) {
		r_out.warnings.push_back("CanvasLayer offset is one i16 xy; not a layer stack.");
	}
	if (gathered.hud_dropped > 0) {
		r_out.warnings.push_back(vformat("kept first %d of %d HUD widgets.", kMaxHud, kMaxHud + gathered.hud_dropped));
	}
	for (int i = 0; i < gathered.hud.size(); i++) {
		if (gathered.hud[i].text.length() > kMaxHudText) {
			r_out.warnings.push_back("HUD text truncated to 64 characters.");
			break;
		}
	}
	if (gathered.tilemap_count > 0) {
		r_out.warnings.push_back("TileMap navigation/terrain/animated tiles are not cooked; physics-layer or custom_data solid/one_way is a TILE flag.");
	}
	if (gathered.tile_dropped > 0) {
		r_out.warnings.push_back(vformat("kept first %d of %d tiles.", kMaxTiles, kMaxTiles + gathered.tile_dropped));
	}
	if (gathered.extra_wav > 0) {
		r_out.warnings.push_back(vformat("kept first AudioStreamWAV; ignored %d extra WAV stream(s).", gathered.extra_wav));
	}
	if (gathered.extra_video > 0) {
		r_out.warnings.push_back(vformat("kept first VideoStreamPlayer; ignored %d extra player(s).", gathered.extra_video));
	}
	if (gathered.had_omni_or_spot) {
		r_out.warnings.push_back("OmniLight3D/SpotLight3D cooked as directional GTE lights.");
	}
	if (gathered.groups_dropped > 0) {
		r_out.warnings.push_back("kept first 8 group names; extra groups are cook warnings.");
	}
	if (gathered.slope_tiles > 0) {
		r_out.warnings.push_back("tile custom_data slope cooks 45° flags 4/8; axis-aligned slide, not Godot slope meshes.");
	}
	if (gathered.had_body) {
		r_out.warnings.push_back("CharacterBody/Static/Rigid/Animatable cook as AABB + move_and_slide; not Jolt / RigidBody integrate.");
	}
	if (gathered.had_csg || gathered.had_gridmap) {
		r_out.warnings.push_back("CSG/GridMap bake into the mesh catalog while they fit 8192 tris.");
	}
	if (gathered.had_multimesh) {
		r_out.warnings.push_back("MultiMesh instances emit until 8192 tris, then drop.");
	}
	if (gathered.had_draw2d) {
		r_out.warnings.push_back("MeshInstance2D/Polygon2D/Line2D cook as catalog tris; long lines drop.");
	}
	if (gathered.had_label3d) {
		r_out.warnings.push_back("Label3D is a type-1 node unless a texture exists.");
	}
	if (gathered.had_world_env) {
		r_out.warnings.push_back("WorldEnvironment fog seeds set_fog; no sky cubemap.");
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
		const int col = i % 4;
		const int row = i / 4;
		int dest_x = 640 + 64 * col;
		int dest_y = 64 * row;
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
	// #region agent log
	{
		FILE *logf = fopen("D:/projects/ps1_blazium/debug-9aa74d.log", "a");
		if (logf) {
			fprintf(logf, "{\"sessionId\":\"9aa74d\",\"runId\":\"cook\",\"hypothesisId\":\"A\",\"location\":\"cooker.cpp:tim-pack\",\"message\":\"TIM pack result\",\"data\":{\"images\":%d,\"tims\":%d,\"tris\":%d,\"hud\":%d},\"timestamp\":%lld}\n",
					int(images.size()), int(tims.size()), int(src_tris.size()), int(gathered.hud.size()), (long long)time(nullptr) * 1000);
			fclose(logf);
		}
	}
	// #endregion
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
		const bool loop = gathered.wav->get_loop_mode() != AudioStreamWAV::LOOP_DISABLED;
		const Error verr = write_vag_pcm(pcm, rate, kSpuLimit, vag, loop);
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
	if (!vag.bytes.is_empty()) {
		r_out.music_path = p_export_dir.path_join("MUSIC00.bin");
		Ref<FileAccess> mf = FileAccess::open(r_out.music_path, FileAccess::WRITE);
		if (mf.is_valid()) {
			mf->store_buffer(vag.bytes);
		}
	}
	if (!gathered.extra_wavs.is_empty()) {
		Vector<uint8_t> sfxb;
		write_sfx_bank(gathered.extra_wavs, gathered.extra_wav_names, kSpuLimit - int(vag.bytes.size()), sfxb);
		r_out.sfx_bank_path = p_export_dir.path_join("SFX00.bin");
		Ref<FileAccess> sf = FileAccess::open(r_out.sfx_bank_path, FileAccess::WRITE);
		if (sf.is_valid()) {
			sf->store_buffer(sfxb);
		}
	}

	r_out.sprite_path = p_export_dir.path_join("SPRITE00.bin");
	{
		Vector<uint8_t> spr;
		if (!sprite_cooks.is_empty()) {
			write_scene_sprites(sprite_cooks, spr);
		} else {
			write_empty_sprites(spr);
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
	ir.attached = gathered.attached_scripts;
	for (int i = 0; i < gathered.nodes.size(); i++) {
		if (!gathered.nodes[i].script_path.is_empty()) {
			ir.path_owners[gathered.nodes[i].script_path] = uint16_t(i);
		}
	}
	for (int e = 0; e < extra_gathers.size(); e++) {
		for (int i = 0; i < extra_gathers[e].nodes.size(); i++) {
			const String &sp = extra_gathers[e].nodes[i].script_path;
			if (!sp.is_empty() && !ir.path_owners.has(sp)) {
				ir.path_owners[sp] = 0xffff;
			}
		}
	}
	const Error serr = cook_scripts(p_export_dir, ir);
	if (serr != OK) {
		r_out.errors.push_back("Script cook failed.");
		return serr;
	}
	for (int i = 0; i < ir.warnings.size(); i++) {
		r_out.warnings.push_back(ir.warnings[i]);
	}
	if (ir.luau_count > 0) {
		r_out.warnings.push_back("official Luau bytecode is not interpreted; first-slice tape only.");
	}
	{
		bool had_script_row = false;
		for (int i = 0; i < gathered.nodes.size(); i++) {
			if (!gathered.nodes[i].script_path.is_empty()) {
				had_script_row = true;
				break;
			}
		}
		if (had_script_row) {
			r_out.warnings.push_back("GDBC owner binds templates; instances use NODE.script.");
		}
	}
	for (int i = 0; i < ir.extra_scenes.size(); i++) {
		const String path = ir.extra_scenes[i];
		bool found = false;
		for (int p = 0; p < packs.size(); p++) {
			if (packs[p].path == path) {
				found = true;
				break;
			}
		}
		if (!found) {
			r_out.errors.push_back("change_scene/load path not cooked: " + path);
			r_out.report = r_out.errors[r_out.errors.size() - 1];
			return ERR_UNCONFIGURED;
		}
	}
	for (int i = 0; i < ir.accepted.size(); i++) {
		const String src = FileAccess::get_file_as_string(ir.accepted[i]);
		if (src.contains(".connect(")) {
			r_out.warnings.push_back("connect is first-slice (timeout/pressed/animation_finished, 16 slots); other signals are ignored.");
			break;
		}
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
		const float s = mesh.scale > 0.0f ? mesh.scale : 1.0f;
		ir.pos_x = clamp_i16(-cam_cook.pos.x * s);
		ir.pos_y = clamp_i16(cam_cook.pos.y * s);
		ir.pos_z = clamp_i16(cam_cook.pos.z * s);
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

	auto bind_scripts = [&](SceneGather &g) {
		for (int i = 0; i < g.nodes.size(); i++) {
			g.nodes.write[i].script_index = -1;
			const String &sp = g.nodes[i].script_path;
			if (sp.is_empty()) {
				continue;
			}
			for (int j = 0; j < ir.cooked_gd.size(); j++) {
				if (ir.cooked_gd[j] == sp) {
					g.nodes.write[i].script_index = int16_t(j);
					break;
				}
			}
		}
	};
	bind_scripts(gathered);
	for (int e = 0; e < extra_gathers.size(); e++) {
		bind_scripts(extra_gathers.write[e]);
	}
	{
		Vector<uint8_t> nodeb;
		write_node_table(gathered, mesh.scale, nodeb);
		r_out.node_path = p_export_dir.path_join("NODE00.bin");
		Ref<FileAccess> nd = FileAccess::open(r_out.node_path, FileAccess::WRITE);
		ERR_FAIL_COND_V(nd.is_null(), ERR_CANT_CREATE);
		nd->store_buffer(nodeb);
		if (!packs.is_empty()) {
			packs.write[0].node_bytes = uint32_t(nodeb.size());
			packs.write[0].mesh_bytes = uint32_t(mesh.bytes.size());
			packs.write[0].tim_count = uint16_t(tims.size());
		}
	}
	{
		Vector<uint8_t> hudb;
		write_hud_table(gathered.hud, hudb);
		r_out.hud_path = p_export_dir.path_join("HUD00.bin");
		Ref<FileAccess> hf = FileAccess::open(r_out.hud_path, FileAccess::WRITE);
		ERR_FAIL_COND_V(hf.is_null(), ERR_CANT_CREATE);
		hf->store_buffer(hudb);
		if (!packs.is_empty()) {
			packs.write[0].hud_bytes = uint32_t(hudb.size());
		}
	}
	{
		Vector<uint8_t> tileb;
		write_tile_table(gathered, tileb);
		r_out.tile_path = p_export_dir.path_join("TILE00.bin");
		Ref<FileAccess> tf = FileAccess::open(r_out.tile_path, FileAccess::WRITE);
		ERR_FAIL_COND_V(tf.is_null(), ERR_CANT_CREATE);
		tf->store_buffer(tileb);
		if (!packs.is_empty()) {
			packs.write[0].tile_bytes = uint32_t(tileb.size());
			packs.write[0].ram_bytes = packs[0].node_bytes + packs[0].mesh_bytes + packs[0].hud_bytes + packs[0].tile_bytes + uint32_t(packed.bytes.size());
		}
	}
	for (int e = 0; e < extra_gathers.size(); e++) {
		const int pack_i = e + 1;
		if (pack_i >= packs.size()) {
			break;
		}
		SceneGather &eg = extra_gathers.write[e];
		const String nn = vformat("%02d", pack_i);
		const String pdir = p_export_dir.path_join("packs").path_join(nn);
		Ref<DirAccess> pda = DirAccess::create(DirAccess::ACCESS_FILESYSTEM);
		pda->make_dir_recursive(pdir);
		Vector<uint8_t> nodeb;
		write_node_table(eg, mesh.scale, nodeb);
		const String nodep = pdir.path_join("NODE" + nn + ".bin");
		Ref<FileAccess> nf = FileAccess::open(nodep, FileAccess::WRITE);
		ERR_FAIL_COND_V(nf.is_null(), ERR_CANT_CREATE);
		nf->store_buffer(nodeb);
		r_out.extra_pack_files.push_back(nodep);
		Vector<uint8_t> hudb;
		write_hud_table(eg.hud, hudb);
		const String hudp = pdir.path_join("HUD" + nn + ".bin");
		Ref<FileAccess> hf = FileAccess::open(hudp, FileAccess::WRITE);
		ERR_FAIL_COND_V(hf.is_null(), ERR_CANT_CREATE);
		hf->store_buffer(hudb);
		r_out.extra_pack_files.push_back(hudp);
		Vector<uint8_t> tileb;
		write_tile_table(eg, tileb);
		const String tilep = pdir.path_join("TILE" + nn + ".bin");
		Ref<FileAccess> tf = FileAccess::open(tilep, FileAccess::WRITE);
		ERR_FAIL_COND_V(tf.is_null(), ERR_CANT_CREATE);
		tf->store_buffer(tileb);
		r_out.extra_pack_files.push_back(tilep);
		MeshBlob emesh;
		if (!eg.tris.is_empty()) {
			write_mesh_svector(eg.tris, kMaxTris, emesh);
			const String meshp = pdir.path_join("MESH" + nn + ".bin");
			Ref<FileAccess> mf = FileAccess::open(meshp, FileAccess::WRITE);
			ERR_FAIL_COND_V(mf.is_null(), ERR_CANT_CREATE);
			mf->store_buffer(emesh.bytes);
			r_out.extra_pack_files.push_back(meshp);
		}
		uint32_t tpak_bytes = 0;
		if (!eg.images.is_empty()) {
			Vector<TimBlob> etim;
			for (int t = 0; t < eg.images.size() && t < kMaxTims; t++) {
				TimBlob one;
				if (write_tim4(eg.images[t], one) == OK) {
					etim.push_back(one);
				}
			}
			if (!etim.is_empty()) {
				TimBlob etpak;
				if (write_tpak(etim, etpak) == OK) {
					const String timp = pdir.path_join("TPAK" + nn + ".bin");
					Ref<FileAccess> tmf = FileAccess::open(timp, FileAccess::WRITE);
					if (tmf.is_valid()) {
						tmf->store_buffer(etpak.bytes);
						tpak_bytes = uint32_t(etpak.bytes.size());
						r_out.extra_pack_files.push_back(timp);
					}
				}
			}
		}
		auto write_extra_blob = [&](const String &kind, const Vector<uint8_t> &bytes) {
			if (bytes.is_empty()) {
				return;
			}
			const String path = pdir.path_join(kind + nn + ".bin");
			Ref<FileAccess> f = FileAccess::open(path, FileAccess::WRITE);
			if (f.is_valid()) {
				f->store_buffer(bytes);
				r_out.extra_pack_files.push_back(path);
			}
		};
		if (e < extra_anims.size() && !extra_anims[e].is_empty()) {
			Vector<uint8_t> animb;
			write_anim_clips(extra_anims[e], mesh.scale, animb);
			write_extra_blob("ANIM", animb);
		}
		if (e < extra_sprite_cooks.size() && !extra_sprite_cooks[e].is_empty()) {
			Vector<uint8_t> sprb;
			write_scene_sprites(extra_sprite_cooks[e], sprb);
			write_extra_blob("SPRITE", sprb);
		}
		{
			Vector<uint8_t> hitb;
			write_hit_table(eg.hits, mesh.scale, hitb);
			write_extra_blob("HIT", hitb);
		}
		{
			Vector<uint8_t> camb;
			write_cam_table(eg.cameras, mesh.scale, camb);
			write_extra_blob("CAM", camb);
		}
		{
			fill_nav_from_hits(eg);
			Vector<uint8_t> navb;
			write_nav_table(eg, mesh.scale, navb);
			write_extra_blob("NAV", navb);
			Vector<uint8_t> pathb;
			write_path_table(eg, mesh.scale, pathb);
			write_extra_blob("PATH", pathb);
			Vector<uint8_t> wayb;
			write_way_table(eg, wayb);
			write_extra_blob("WAY", wayb);
		}
		if (!eg.extra_wavs.is_empty()) {
			Vector<uint8_t> sfxb;
			write_sfx_bank(eg.extra_wavs, eg.extra_wav_names, kSpuLimit, sfxb);
			write_extra_blob("SFX", sfxb);
		}
		if (eg.wav.is_valid()) {
			Vector<int16_t> pcm;
			int rate = 22050;
			pcm_from_wav(eg.wav, pcm, rate);
			VagBlob mv;
			if (write_vag_pcm(pcm, rate, kSpuLimit, mv, eg.wav->get_loop_mode() != AudioStreamWAV::LOOP_DISABLED) == OK) {
				write_extra_blob("MUSIC", mv.bytes);
			}
		}
		if (!eg.text_lines.is_empty()) {
			Vector<uint8_t> txtb;
			write_text_bank(eg.text_lines, txtb);
			write_extra_blob("TXT", txtb);
		}
		packs.write[pack_i].node_bytes = uint32_t(nodeb.size());
		packs.write[pack_i].hud_bytes = uint32_t(hudb.size());
		packs.write[pack_i].tile_bytes = uint32_t(tileb.size());
		packs.write[pack_i].mesh_bytes = uint32_t(emesh.bytes.size());
		packs.write[pack_i].ram_bytes = packs[pack_i].node_bytes + packs[pack_i].mesh_bytes + packs[pack_i].hud_bytes + packs[pack_i].tile_bytes + tpak_bytes;
	}
	r_out.pack_count = packs.size();
	{
		Vector<uint8_t> scenb;
		write_scene_packs(packs, mesh.scale, scenb);
		r_out.scene_path = p_export_dir.path_join("SCENE00.bin");
		Ref<FileAccess> sf = FileAccess::open(r_out.scene_path, FileAccess::WRITE);
		ERR_FAIL_COND_V(sf.is_null(), ERR_CANT_CREATE);
		sf->store_buffer(scenb);
	}
	if (!anim_clips.is_empty()) {
		Vector<uint8_t> animb;
		write_anim_clips(anim_clips, mesh.scale, animb);
		r_out.anim_path = p_export_dir.path_join("ANIM00.bin");
		Ref<FileAccess> af = FileAccess::open(r_out.anim_path, FileAccess::WRITE);
		ERR_FAIL_COND_V(af.is_null(), ERR_CANT_CREATE);
		af->store_buffer(animb);
	}
	{
		Vector<uint8_t> camb;
		write_cam_table(gathered.cameras, mesh.scale, camb);
		r_out.cam_path = p_export_dir.path_join("CAM00.bin");
		Ref<FileAccess> cf = FileAccess::open(r_out.cam_path, FileAccess::WRITE);
		ERR_FAIL_COND_V(cf.is_null(), ERR_CANT_CREATE);
		cf->store_buffer(camb);
	}
	{
		Vector<uint8_t> hitb;
		write_hit_table(gathered.hits, mesh.scale, hitb);
		r_out.hit_path = p_export_dir.path_join("HIT00.bin");
		Ref<FileAccess> hf = FileAccess::open(r_out.hit_path, FileAccess::WRITE);
		ERR_FAIL_COND_V(hf.is_null(), ERR_CANT_CREATE);
		hf->store_buffer(hitb);
	}
	fill_nav_from_hits(gathered);
	{
		Vector<uint8_t> navb;
		write_nav_table(gathered, mesh.scale, navb);
		r_out.nav_path = p_export_dir.path_join("NAV00.bin");
		Ref<FileAccess> nv = FileAccess::open(r_out.nav_path, FileAccess::WRITE);
		if (nv.is_valid()) {
			nv->store_buffer(navb);
		}
	}
	{
		Vector<uint8_t> pathb;
		write_path_table(gathered, mesh.scale, pathb);
		r_out.path_path = p_export_dir.path_join("PATH00.bin");
		Ref<FileAccess> pf = FileAccess::open(r_out.path_path, FileAccess::WRITE);
		if (pf.is_valid()) {
			pf->store_buffer(pathb);
		}
	}
	{
		Vector<uint8_t> wayb;
		write_way_table(gathered, wayb);
		r_out.way_path = p_export_dir.path_join("WAY00.bin");
		Ref<FileAccess> wf = FileAccess::open(r_out.way_path, FileAccess::WRITE);
		if (wf.is_valid()) {
			wf->store_buffer(wayb);
		}
	}
	if (!gathered.text_lines.is_empty()) {
		Vector<uint8_t> txtb;
		write_text_bank(gathered.text_lines, txtb);
		r_out.txt_path = p_export_dir.path_join("TXT00.bin");
		Ref<FileAccess> xf = FileAccess::open(r_out.txt_path, FileAccess::WRITE);
		if (xf.is_valid()) {
			xf->store_buffer(txtb);
		}
	}
	Ref<FileAccess> nf = FileAccess::open(r_out.names_path, FileAccess::WRITE);
	ERR_FAIL_COND_V(nf.is_null(), ERR_CANT_CREATE);
	nf->store_line(src_name + " -> TEX00.TIM MESH00.bin SFX00.VAG SPRITE00.bin FMV00.MAP SCRIPT.IR SCRIPT.GD.BC SCRIPT.LU.BC NODE00.bin HUD00.bin TILE00.bin SCENE00.bin ANIM00.bin CAM00.bin HIT00.bin TXT00.bin SFX00.bin MUSIC00.bin NAV00.bin PATH00.bin WAY00.bin");

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
	if (!r_out.node_path.is_empty() && FileAccess::exists(r_out.node_path)) {
		extra_blob += FileAccess::get_file_as_bytes(r_out.node_path).size();
	}
	if (!r_out.hud_path.is_empty() && FileAccess::exists(r_out.hud_path)) {
		extra_blob += FileAccess::get_file_as_bytes(r_out.hud_path).size();
	}
	if (!r_out.tile_path.is_empty() && FileAccess::exists(r_out.tile_path)) {
		extra_blob += FileAccess::get_file_as_bytes(r_out.tile_path).size();
	}
	if (!r_out.scene_path.is_empty() && FileAccess::exists(r_out.scene_path)) {
		extra_blob += FileAccess::get_file_as_bytes(r_out.scene_path).size();
	}
	if (!r_out.anim_path.is_empty() && FileAccess::exists(r_out.anim_path)) {
		extra_blob += FileAccess::get_file_as_bytes(r_out.anim_path).size();
	}
	const int ram_blob = packed.bytes.size() + mesh.bytes.size() + vag.bytes.size() + extra_blob;
	r_out.over_budget = packed.vram_bytes > kVramLimit || ram_blob > kRamLimit || vag.bytes.size() > kSpuLimit;

	r_out.infos.push_back(vformat("guest_abi=%d nodes=%d hud=%d tiles=%d packs=%d src=%s tims=%d tris=%d cooked=%d lights=%d sprites=%d display=%s %dx%d gd=%d luau=%d",
			kCookABI, gathered.nodes.size(), gathered.hud.size(), gathered.tiles.size(), packs.size(), src_name, tims.size(), src_tris.size(), mesh.tri_count, lights.size(), sprite_cooks.size(),
			p_opts.region == 1 ? "PAL" : "NTSC", display_w, display_h, ir.gd_count, ir.luau_count));

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
