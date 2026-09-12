/**************************************************************************/
/*  data.h                                                                */
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
#include "core/templates/local_vector.h"
#include "core/variant/dictionary.h"
#include "scene/3d/mesh_instance_3d.h"
#include "scene/3d/occluder_instance_3d.h"
#include "scene/3d/physics/collision_shape_3d.h"
#include "scene/resources/3d/concave_polygon_shape_3d.h"
#include "scene/resources/3d/shape_3d.h"
#include "scene/resources/mesh.h"

class BlaziumFGDEntityClass;
struct GroupData;

struct DispInfoData {
	int power = 2;
	real_t elevation = 0.0;
	PackedFloat32Array distances;
	PackedVector3Array disp_normals;
	PackedVector3Array offsets;
	PackedVector3Array offset_normals;
	PackedFloat32Array alphas;
	PackedInt32Array allowed_verts;
	Vector3 startposition = Vector3(0, 0, 0);
	bool valid = false;

	int get_grid_size() const;
	bool has_distance_grid() const;
	bool is_vertex_allowed(int p_index) const;
};

struct OverlayData {
	String material;
	Vector3 origin;
	Vector3 basis_u;
	Vector3 basis_v;
	Vector3 basis_normal;
	Vector2 uv0 = Vector2(0, 1);
	Vector2 uv1 = Vector2(0, 0);
	Vector2 uv2 = Vector2(1, 0);
	Vector2 uv3 = Vector2(1, 1);
};

struct FaceData {
	PackedVector3Array vertices;
	PackedInt32Array indices;
	PackedVector3Array normals;
	PackedFloat32Array tangents;
	String texture;
	Transform2D uv;
	PackedVector3Array uv_axes;
	Plane plane;
	DispInfoData disp;

	Vector3 get_centroid() const;
	Vector3 get_basis() const;
	void wind();
	void index_vertices();
};

struct BrushData {
	LocalVector<Plane> planes;
	LocalVector<FaceData> faces;
	bool origin = false;
	bool cordon = false;
};

struct PatchData {
	String texture;
	PackedInt32Array size;
	PackedVector3Array points;
	PackedVector2Array uvs;
};

struct GroupData {
	enum GroupType {
		GROUP_TYPE_GROUP,
		GROUP_TYPE_LAYER,
	};

	GroupType type = GROUP_TYPE_GROUP;
	int id = 0;
	String name;
	int parent_id = -1;
	GroupData *parent = nullptr;
	Node3D *node = nullptr;
	bool omit = false;
};

struct EntityData {
	Dictionary properties;
	LocalVector<BrushData> brushes;
	LocalVector<PatchData> patches;
	LocalVector<OverlayData> overlays;
	GroupData *group = nullptr;
	Ref<BlaziumFGDEntityClass> definition;
	Ref<ArrayMesh> mesh;
	MeshInstance3D *mesh_instance = nullptr;
	Dictionary mesh_metadata;
	LocalVector<Ref<Shape3D>> shapes;
	LocalVector<CollisionShape3D *> collision_shapes;
	OccluderInstance3D *occluder_instance = nullptr;
	Vector3 origin;

	bool is_visual() const;
	bool is_gi_enabled() const;
	bool is_collision_convex() const;
	bool is_collision_concave() const;
	bool is_smooth_shaded(const String &p_smoothing_property = "_phong") const;
	real_t get_smoothing_angle(const String &p_smoothing_angle_property = "_phong_angle") const;
};

struct VertexGroupData {
	LocalVector<FaceData *> faces;
	LocalVector<int> face_indices;
};

struct ParseData {
	LocalVector<EntityData> entities;
	LocalVector<GroupData> groups;
};
