/**************************************************************************/
/*  dddbrowser_model.cpp                                                  */
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

#include "dddbrowser_model.h"

void DDDBrowserModel::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_collider_type", "type"), &DDDBrowserModel::set_collider_type);
	ClassDB::bind_method(D_METHOD("get_collider_type"), &DDDBrowserModel::get_collider_type);
	ClassDB::bind_method(D_METHOD("set_collider_box_size", "size"), &DDDBrowserModel::set_collider_box_size);
	ClassDB::bind_method(D_METHOD("get_collider_box_size"), &DDDBrowserModel::get_collider_box_size);
	ClassDB::bind_method(D_METHOD("set_collider_radius", "radius"), &DDDBrowserModel::set_collider_radius);
	ClassDB::bind_method(D_METHOD("get_collider_radius"), &DDDBrowserModel::get_collider_radius);
	ClassDB::bind_method(D_METHOD("set_collider_half_height", "half_height"), &DDDBrowserModel::set_collider_half_height);
	ClassDB::bind_method(D_METHOD("get_collider_half_height"), &DDDBrowserModel::get_collider_half_height);
	ClassDB::bind_method(D_METHOD("set_script_path", "path"), &DDDBrowserModel::set_script_path);
	ClassDB::bind_method(D_METHOD("get_script_path"), &DDDBrowserModel::get_script_path);
	ClassDB::bind_method(D_METHOD("set_script_data", "data"), &DDDBrowserModel::set_script_data);
	ClassDB::bind_method(D_METHOD("get_script_data"), &DDDBrowserModel::get_script_data);
	ClassDB::bind_method(D_METHOD("set_pin_script_sha256", "pin"), &DDDBrowserModel::set_pin_script_sha256);
	ClassDB::bind_method(D_METHOD("get_pin_script_sha256"), &DDDBrowserModel::get_pin_script_sha256);

	ADD_GROUP("Collider", "collider_");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "collider_type", PROPERTY_HINT_ENUM, "None,Box,Sphere,Capsule,Cylinder"), "set_collider_type", "get_collider_type");
	ADD_PROPERTY(PropertyInfo(Variant::VECTOR3, "collider_box_size"), "set_collider_box_size", "get_collider_box_size");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "collider_radius", PROPERTY_HINT_RANGE, "0.01,100,0.01"), "set_collider_radius", "get_collider_radius");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "collider_half_height", PROPERTY_HINT_RANGE, "0.01,100,0.01"), "set_collider_half_height", "get_collider_half_height");
	ADD_GROUP("Script", "script_");
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "script_path", PROPERTY_HINT_FILE, "*.luau"), "set_script_path", "get_script_path");
	ADD_PROPERTY(PropertyInfo(Variant::DICTIONARY, "script_data"), "set_script_data", "get_script_data");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "pin_script_sha256"), "set_pin_script_sha256", "get_pin_script_sha256");

	BIND_ENUM_CONSTANT(COLLIDER_NONE);
	BIND_ENUM_CONSTANT(COLLIDER_BOX);
	BIND_ENUM_CONSTANT(COLLIDER_SPHERE);
	BIND_ENUM_CONSTANT(COLLIDER_CAPSULE);
	BIND_ENUM_CONSTANT(COLLIDER_CYLINDER);
}

void DDDBrowserModel::set_collider_type(ColliderType p_type) {
	collider_type = p_type;
}
DDDBrowserModel::ColliderType DDDBrowserModel::get_collider_type() const {
	return collider_type;
}
void DDDBrowserModel::set_collider_box_size(const Vector3 &p_size) {
	collider_box_size = p_size;
}
Vector3 DDDBrowserModel::get_collider_box_size() const {
	return collider_box_size;
}
void DDDBrowserModel::set_collider_radius(float p_radius) {
	collider_radius = p_radius;
}
float DDDBrowserModel::get_collider_radius() const {
	return collider_radius;
}
void DDDBrowserModel::set_collider_half_height(float p_half_height) {
	collider_half_height = p_half_height;
}
float DDDBrowserModel::get_collider_half_height() const {
	return collider_half_height;
}
void DDDBrowserModel::set_script_path(const String &p_path) {
	script_path = p_path;
}
String DDDBrowserModel::get_script_path() const {
	return script_path;
}
void DDDBrowserModel::set_script_data(const Dictionary &p_data) {
	script_data = p_data;
}
Dictionary DDDBrowserModel::get_script_data() const {
	return script_data;
}
void DDDBrowserModel::set_pin_script_sha256(bool p_pin) {
	pin_script_sha256 = p_pin;
}
bool DDDBrowserModel::get_pin_script_sha256() const {
	return pin_script_sha256;
}

Dictionary DDDBrowserModel::build_collider_dictionary() const {
	Dictionary out;
	switch (collider_type) {
		case COLLIDER_BOX: {
			out["type"] = "box";
			Dictionary box;
			box["size"] = Dictionary();
			Dictionary size;
			size["x"] = MAX(0.01, collider_box_size.x);
			size["y"] = MAX(0.01, collider_box_size.y);
			size["z"] = MAX(0.01, collider_box_size.z);
			box["size"] = size;
			out["box"] = box;
		} break;
		case COLLIDER_SPHERE: {
			out["type"] = "sphere";
			Dictionary sphere;
			sphere["radius"] = MAX(0.01, collider_radius);
			out["sphere"] = sphere;
		} break;
		case COLLIDER_CAPSULE: {
			out["type"] = "capsule";
			Dictionary capsule;
			capsule["halfHeight"] = MAX(0.01, collider_half_height);
			capsule["radius"] = MAX(0.01, collider_radius);
			out["capsule"] = capsule;
		} break;
		case COLLIDER_CYLINDER: {
			out["type"] = "cylinder";
			Dictionary cylinder;
			cylinder["halfHeight"] = MAX(0.01, collider_half_height);
			cylinder["radius"] = MAX(0.01, collider_radius);
			out["cylinder"] = cylinder;
		} break;
		default:
			break;
	}
	return out;
}
