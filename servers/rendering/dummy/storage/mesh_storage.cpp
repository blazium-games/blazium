/**************************************************************************/
/*  mesh_storage.cpp                                                      */
/**************************************************************************/
/*                         This file is part of:                          */
/*                             GODOT ENGINE                               */
/*                        https://godotengine.org                         */
/**************************************************************************/
/* Copyright (c) 2014-present Godot Engine contributors (see AUTHORS.md). */
/* Copyright (c) 2007-2014 Juan Linietsky, Ariel Manzur.                  */
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

#include "mesh_storage.h"

using namespace RendererDummy;

MeshStorage *MeshStorage::singleton = nullptr;

MeshStorage::MeshStorage() {
	singleton = this;
}

MeshStorage::~MeshStorage() {
	singleton = nullptr;
}

RID MeshStorage::mesh_allocate() {
	return mesh_owner.allocate_rid();
}

void MeshStorage::mesh_initialize(RID p_rid) {
	mesh_owner.initialize_rid(p_rid, DummyMesh());
}

void MeshStorage::mesh_free(RID p_rid) {
	DummyMesh *mesh = mesh_owner.get_or_null(p_rid);
	ERR_FAIL_NULL(mesh);
	mesh->dependency.deleted_notify(p_rid);
	mesh_owner.free(p_rid);
}

void MeshStorage::mesh_surface_remove(RID p_mesh, int p_surface) {
	DummyMesh *m = mesh_owner.get_or_null(p_mesh);
	ERR_FAIL_NULL(m);
	m->dependency.changed_notify(Dependency::DEPENDENCY_CHANGED_MESH);
	m->surfaces.remove_at(p_surface);
}

void MeshStorage::mesh_clear(RID p_mesh) {
	DummyMesh *m = mesh_owner.get_or_null(p_mesh);
	ERR_FAIL_NULL(m);

	m->surfaces.clear();
}

RID MeshStorage::_multimesh_allocate() {
	return multimesh_owner.allocate_rid();
}

void MeshStorage::_multimesh_initialize(RID p_rid) {
	multimesh_owner.initialize_rid(p_rid, DummyMultiMesh());
}

void MeshStorage::_multimesh_free(RID p_rid) {
	DummyMultiMesh *multimesh = multimesh_owner.get_or_null(p_rid);
	ERR_FAIL_NULL(multimesh);

	multimesh_owner.free(p_rid);
}

void MeshStorage::_multimesh_set_buffer(RID p_multimesh, const Vector<float> &p_buffer) {
	DummyMultiMesh *multimesh = multimesh_owner.get_or_null(p_multimesh);
	ERR_FAIL_NULL(multimesh);
	multimesh->buffer.resize(p_buffer.size());
	float *cache_data = multimesh->buffer.ptrw();
	memcpy(cache_data, p_buffer.ptr(), p_buffer.size() * sizeof(float));
}

Vector<float> MeshStorage::_multimesh_get_buffer(RID p_multimesh) const {
	DummyMultiMesh *multimesh = multimesh_owner.get_or_null(p_multimesh);
	ERR_FAIL_NULL_V(multimesh, Vector<float>());

	return multimesh->buffer;
}

void MeshStorage::_multimesh_allocate_data(RID p_multimesh, int p_instances, RS::MultimeshTransformFormat p_transform_format, bool p_use_colors, bool p_use_custom_data) {
	DummyMultiMesh *multimesh = multimesh_owner.get_or_null(p_multimesh);
	ERR_FAIL_NULL(multimesh);
	multimesh->instances = MAX(p_instances, 0);
	multimesh->xform_format = p_transform_format;
	multimesh->uses_colors = p_use_colors;
	multimesh->uses_custom_data = p_use_custom_data;
	multimesh->stride = (p_transform_format == RS::MULTIMESH_TRANSFORM_2D ? 8 : 12) + (p_use_colors ? 4 : 0) + (p_use_custom_data ? 4 : 0);
	multimesh->buffer.resize(multimesh->instances * multimesh->stride);
	multimesh->buffer.fill(0.0f);
	// identity transforms, as the real servers start with
	float *w = multimesh->buffer.ptrw();
	for (int i = 0; i < multimesh->instances; i++) {
		float *d = w + i * multimesh->stride;
		if (p_transform_format == RS::MULTIMESH_TRANSFORM_2D) {
			d[0] = 1.0f;
			d[5] = 1.0f;
		} else {
			d[0] = 1.0f;
			d[5] = 1.0f;
			d[10] = 1.0f;
		}
	}
}

int MeshStorage::_multimesh_get_instance_count(RID p_multimesh) const {
	DummyMultiMesh *multimesh = multimesh_owner.get_or_null(p_multimesh);
	ERR_FAIL_NULL_V(multimesh, 0);
	return multimesh->instances;
}

void MeshStorage::_multimesh_set_mesh(RID p_multimesh, RID p_mesh) {
	DummyMultiMesh *multimesh = multimesh_owner.get_or_null(p_multimesh);
	ERR_FAIL_NULL(multimesh);
	multimesh->mesh = p_mesh;
}

RID MeshStorage::_multimesh_get_mesh(RID p_multimesh) const {
	DummyMultiMesh *multimesh = multimesh_owner.get_or_null(p_multimesh);
	return multimesh ? multimesh->mesh : RID();
}

void MeshStorage::_multimesh_instance_set_transform(RID p_multimesh, int p_index, const Transform3D &p_transform) {
	DummyMultiMesh *multimesh = multimesh_owner.get_or_null(p_multimesh);
	ERR_FAIL_NULL(multimesh);
	ERR_FAIL_INDEX(p_index, multimesh->instances);
	ERR_FAIL_COND(multimesh->xform_format != RS::MULTIMESH_TRANSFORM_3D);
	float *d = multimesh->buffer.ptrw() + p_index * multimesh->stride;
	for (int r = 0; r < 3; r++) {
		d[r * 4 + 0] = p_transform.basis.rows[r][0];
		d[r * 4 + 1] = p_transform.basis.rows[r][1];
		d[r * 4 + 2] = p_transform.basis.rows[r][2];
		d[r * 4 + 3] = p_transform.origin[r];
	}
}

void MeshStorage::_multimesh_instance_set_transform_2d(RID p_multimesh, int p_index, const Transform2D &p_transform) {
	DummyMultiMesh *multimesh = multimesh_owner.get_or_null(p_multimesh);
	ERR_FAIL_NULL(multimesh);
	ERR_FAIL_INDEX(p_index, multimesh->instances);
	ERR_FAIL_COND(multimesh->xform_format != RS::MULTIMESH_TRANSFORM_2D);
	float *d = multimesh->buffer.ptrw() + p_index * multimesh->stride;
	d[0] = p_transform.columns[0][0];
	d[1] = p_transform.columns[1][0];
	d[2] = 0.0f;
	d[3] = p_transform.columns[2][0];
	d[4] = p_transform.columns[0][1];
	d[5] = p_transform.columns[1][1];
	d[6] = 0.0f;
	d[7] = p_transform.columns[2][1];
}

void MeshStorage::_multimesh_instance_set_color(RID p_multimesh, int p_index, const Color &p_color) {
	DummyMultiMesh *multimesh = multimesh_owner.get_or_null(p_multimesh);
	ERR_FAIL_NULL(multimesh);
	ERR_FAIL_INDEX(p_index, multimesh->instances);
	ERR_FAIL_COND(!multimesh->uses_colors);
	float *d = multimesh->buffer.ptrw() + p_index * multimesh->stride + (multimesh->xform_format == RS::MULTIMESH_TRANSFORM_2D ? 8 : 12);
	d[0] = p_color.r;
	d[1] = p_color.g;
	d[2] = p_color.b;
	d[3] = p_color.a;
}

void MeshStorage::_multimesh_instance_set_custom_data(RID p_multimesh, int p_index, const Color &p_color) {
	DummyMultiMesh *multimesh = multimesh_owner.get_or_null(p_multimesh);
	ERR_FAIL_NULL(multimesh);
	ERR_FAIL_INDEX(p_index, multimesh->instances);
	ERR_FAIL_COND(!multimesh->uses_custom_data);
	float *d = multimesh->buffer.ptrw() + p_index * multimesh->stride + (multimesh->xform_format == RS::MULTIMESH_TRANSFORM_2D ? 8 : 12) + (multimesh->uses_colors ? 4 : 0);
	d[0] = p_color.r;
	d[1] = p_color.g;
	d[2] = p_color.b;
	d[3] = p_color.a;
}

Transform3D MeshStorage::_multimesh_instance_get_transform(RID p_multimesh, int p_index) const {
	DummyMultiMesh *multimesh = multimesh_owner.get_or_null(p_multimesh);
	ERR_FAIL_NULL_V(multimesh, Transform3D());
	ERR_FAIL_INDEX_V(p_index, multimesh->instances, Transform3D());
	ERR_FAIL_COND_V(multimesh->xform_format != RS::MULTIMESH_TRANSFORM_3D, Transform3D());
	if (multimesh->buffer.size() < (p_index + 1) * multimesh->stride) {
		return Transform3D();
	}
	const float *d = multimesh->buffer.ptr() + p_index * multimesh->stride;
	Transform3D t;
	for (int r = 0; r < 3; r++) {
		t.basis.rows[r] = Vector3(d[r * 4 + 0], d[r * 4 + 1], d[r * 4 + 2]);
		t.origin[r] = d[r * 4 + 3];
	}
	return t;
}

Transform2D MeshStorage::_multimesh_instance_get_transform_2d(RID p_multimesh, int p_index) const {
	DummyMultiMesh *multimesh = multimesh_owner.get_or_null(p_multimesh);
	ERR_FAIL_NULL_V(multimesh, Transform2D());
	ERR_FAIL_INDEX_V(p_index, multimesh->instances, Transform2D());
	ERR_FAIL_COND_V(multimesh->xform_format != RS::MULTIMESH_TRANSFORM_2D, Transform2D());
	if (multimesh->buffer.size() < (p_index + 1) * multimesh->stride) {
		return Transform2D();
	}
	const float *d = multimesh->buffer.ptr() + p_index * multimesh->stride;
	Transform2D t;
	t.columns[0] = Vector2(d[0], d[4]);
	t.columns[1] = Vector2(d[1], d[5]);
	t.columns[2] = Vector2(d[3], d[7]);
	return t;
}

Color MeshStorage::_multimesh_instance_get_color(RID p_multimesh, int p_index) const {
	DummyMultiMesh *multimesh = multimesh_owner.get_or_null(p_multimesh);
	ERR_FAIL_NULL_V(multimesh, Color());
	ERR_FAIL_INDEX_V(p_index, multimesh->instances, Color());
	if (!multimesh->uses_colors || multimesh->buffer.size() < (p_index + 1) * multimesh->stride) {
		return Color();
	}
	const float *d = multimesh->buffer.ptr() + p_index * multimesh->stride + (multimesh->xform_format == RS::MULTIMESH_TRANSFORM_2D ? 8 : 12);
	return Color(d[0], d[1], d[2], d[3]);
}

Color MeshStorage::_multimesh_instance_get_custom_data(RID p_multimesh, int p_index) const {
	DummyMultiMesh *multimesh = multimesh_owner.get_or_null(p_multimesh);
	ERR_FAIL_NULL_V(multimesh, Color());
	ERR_FAIL_INDEX_V(p_index, multimesh->instances, Color());
	if (!multimesh->uses_custom_data || multimesh->buffer.size() < (p_index + 1) * multimesh->stride) {
		return Color();
	}
	const float *d = multimesh->buffer.ptr() + p_index * multimesh->stride + (multimesh->xform_format == RS::MULTIMESH_TRANSFORM_2D ? 8 : 12) + (multimesh->uses_colors ? 4 : 0);
	return Color(d[0], d[1], d[2], d[3]);
}

void MeshStorage::_multimesh_set_visible_instances(RID p_multimesh, int p_visible) {
	DummyMultiMesh *multimesh = multimesh_owner.get_or_null(p_multimesh);
	ERR_FAIL_NULL(multimesh);
	multimesh->visible_instances = p_visible;
}

int MeshStorage::_multimesh_get_visible_instances(RID p_multimesh) const {
	DummyMultiMesh *multimesh = multimesh_owner.get_or_null(p_multimesh);
	ERR_FAIL_NULL_V(multimesh, -1);
	return multimesh->visible_instances;
}
