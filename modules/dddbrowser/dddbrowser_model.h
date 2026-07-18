/**************************************************************************/
/*  dddbrowser_model.h                                                    */
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

#include "scene/3d/mesh_instance_3d.h"

class DDDBrowserModel : public MeshInstance3D {
	GDCLASS(DDDBrowserModel, MeshInstance3D);

public:
	enum ColliderType {
		COLLIDER_NONE,
		COLLIDER_BOX,
		COLLIDER_SPHERE,
		COLLIDER_CAPSULE,
		COLLIDER_CYLINDER,
	};

private:
	ColliderType collider_type = COLLIDER_NONE;
	Vector3 collider_box_size = Vector3(1, 1, 1);
	float collider_radius = 0.5f;
	float collider_half_height = 1.0f;
	String script_path;
	Dictionary script_data;
	bool pin_script_sha256 = true;

protected:
	static void _bind_methods();

public:
	void set_collider_type(ColliderType p_type);
	ColliderType get_collider_type() const;
	void set_collider_box_size(const Vector3 &p_size);
	Vector3 get_collider_box_size() const;
	void set_collider_radius(float p_radius);
	float get_collider_radius() const;
	void set_collider_half_height(float p_half_height);
	float get_collider_half_height() const;
	void set_script_path(const String &p_path);
	String get_script_path() const;
	void set_script_data(const Dictionary &p_data);
	Dictionary get_script_data() const;
	void set_pin_script_sha256(bool p_pin);
	bool get_pin_script_sha256() const;

	Dictionary build_collider_dictionary() const;
};

VARIANT_ENUM_CAST(DDDBrowserModel::ColliderType);
