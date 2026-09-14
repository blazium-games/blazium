/**************************************************************************/
/*  trenchbroom_util.h                                                    */
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

#pragma once

#include "core/math/plane.h"
#include "core/math/transform_2d.h"
#include "core/math/vector3.h"
#include "core/os/mutex.h"
#include "core/templates/hash_map.h"
#include "core/templates/hash_set.h"
#include "core/variant/dictionary.h"
#include "scene/resources/mesh.h"

class Material;

class TrenchbroomMapSettings;
class QuakeWadFile;
struct EntityData;
struct FaceData;

class TrenchbroomUtil {
public:
	static constexpr real_t VERTEX_EPSILON = 0.008;

	static String newline();
	static Vector3 op_vec3_sum(const Vector3 &p_lhs, const Vector3 &p_rhs);
	static Vector3 op_vec3_avg(const PackedVector3Array &p_array);
	static Vector3 id_to_opengl(const Vector3 &p_vec);
	static bool is_point_in_convex_hull(const LocalVector<Plane> &p_planes, const Vector3 &p_vertex);

	static Ref<Texture2D> load_texture(const String &p_texture_name, const TypedArray<QuakeWadFile> &p_wad_resources, const TrenchbroomMapSettings *p_map_settings);
	static bool is_skip(const String &p_texture, const TrenchbroomMapSettings *p_map_settings);
	static bool is_clip(const String &p_texture, const TrenchbroomMapSettings *p_map_settings);
	static bool is_origin(const String &p_texture, const TrenchbroomMapSettings *p_map_settings);
	static bool filter_face(const String &p_texture, const TrenchbroomMapSettings *p_map_settings);
	static void build_base_material(const TrenchbroomMapSettings *p_map_settings, Ref<BaseMaterial3D> p_material, const String &p_texture);
	static void collect_texture_names(const LocalVector<EntityData> &p_entity_data, const TrenchbroomMapSettings *p_map_settings, HashSet<String> &r_names);
	static void register_texture_entry(
			const String &p_texture_name,
			const TrenchbroomMapSettings *p_map_settings,
			const TypedArray<QuakeWadFile> &p_wad_resources,
			Dictionary &p_texture_materials,
			Dictionary &p_texture_sizes,
			Mutex &p_mutex,
			LocalVector<Pair<String, Ref<Material>>> *p_materials_to_save = nullptr);
	static Array build_texture_map_parallel(const LocalVector<EntityData> &p_entity_data, const TrenchbroomMapSettings *p_map_settings);
	static Array build_texture_map(const LocalVector<EntityData> &p_entity_data, const TrenchbroomMapSettings *p_map_settings);

	static Vector2 get_valve_uv(const Vector3 &p_vertex, const Vector3 &p_u_axis, const Vector3 &p_v_axis, const Transform2D &p_uv_basis = Transform2D(), const Vector2 &p_texture_size = Vector2(1, 1));
	static Vector2 get_quake_uv(const Vector3 &p_vertex, const Vector3 &p_normal, const Transform2D &p_uv_in = Transform2D(), const Vector2 &p_texture_size = Vector2(1, 1));
	static Vector2 get_face_vertex_uv(const Vector3 &p_vertex, const FaceData &p_face, const Vector2 &p_texture_size);
	static PackedFloat32Array get_valve_tangent(const Vector3 &p_u, const Vector3 &p_v, const Vector3 &p_normal);
	static PackedFloat32Array get_quake_tangent(const Vector3 &p_normal, real_t p_uv_y_scale, real_t p_uv_rotation);
	static PackedFloat32Array get_face_tangent(const FaceData &p_face);

	static Ref<ArrayMesh> smooth_mesh_by_angle(const Ref<ArrayMesh> &p_mesh, real_t p_angle_deg = 89.0);
	static void print_profile_info(const String &p_message, const String &p_signature);

	static Vector3 sample_bezier_curve(const PackedVector3Array &p_controls, real_t p_t);
	static Vector3 sample_bezier_surface(const PackedVector3Array &p_controls, int p_width, int p_height, real_t p_u, real_t p_v);
	static PackedInt32Array get_triangle_indices(int p_width, int p_height);
	static PackedVector3Array elevate_quadratic(const Vector3 &p0, const Vector3 &p1, const Vector3 &p2);
};
