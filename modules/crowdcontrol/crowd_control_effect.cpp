/**************************************************************************/
/*  crowd_control_effect.cpp                                              */
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

#include "crowd_control_effect.h"

#include "crowd_control_effect_parameter.h"

void CrowdControlEffect::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_effect_id", "id"), &CrowdControlEffect::set_effect_id);
	ClassDB::bind_method(D_METHOD("get_effect_id"), &CrowdControlEffect::get_effect_id);

	ClassDB::bind_method(D_METHOD("set_effect_name", "name"), &CrowdControlEffect::set_effect_name);
	ClassDB::bind_method(D_METHOD("get_effect_name"), &CrowdControlEffect::get_effect_name);

	ClassDB::bind_method(D_METHOD("set_price", "price"), &CrowdControlEffect::set_price);
	ClassDB::bind_method(D_METHOD("get_price"), &CrowdControlEffect::get_price);

	ClassDB::bind_method(D_METHOD("set_description", "description"), &CrowdControlEffect::set_description);
	ClassDB::bind_method(D_METHOD("get_description"), &CrowdControlEffect::get_description);

	ClassDB::bind_method(D_METHOD("set_duration", "duration"), &CrowdControlEffect::set_duration);
	ClassDB::bind_method(D_METHOD("get_duration"), &CrowdControlEffect::get_duration);

	ClassDB::bind_method(D_METHOD("set_quantity_min", "min"), &CrowdControlEffect::set_quantity_min);
	ClassDB::bind_method(D_METHOD("get_quantity_min"), &CrowdControlEffect::get_quantity_min);

	ClassDB::bind_method(D_METHOD("set_quantity_max", "max"), &CrowdControlEffect::set_quantity_max);
	ClassDB::bind_method(D_METHOD("get_quantity_max"), &CrowdControlEffect::get_quantity_max);

	ClassDB::bind_method(D_METHOD("set_note", "note"), &CrowdControlEffect::set_note);
	ClassDB::bind_method(D_METHOD("get_note"), &CrowdControlEffect::get_note);

	ClassDB::bind_method(D_METHOD("set_inactive", "inactive"), &CrowdControlEffect::set_inactive);
	ClassDB::bind_method(D_METHOD("is_inactive"), &CrowdControlEffect::is_inactive);

	ClassDB::bind_method(D_METHOD("set_category", "category"), &CrowdControlEffect::set_category);
	ClassDB::bind_method(D_METHOD("get_category"), &CrowdControlEffect::get_category);

	ClassDB::bind_method(D_METHOD("set_group", "group"), &CrowdControlEffect::set_group);
	ClassDB::bind_method(D_METHOD("get_group"), &CrowdControlEffect::get_group);

	ClassDB::bind_method(D_METHOD("set_parameters", "parameters"), &CrowdControlEffect::set_parameters);
	ClassDB::bind_method(D_METHOD("get_parameters"), &CrowdControlEffect::get_parameters);

	ClassDB::bind_method(D_METHOD("to_json"), &CrowdControlEffect::to_json);

	ADD_PROPERTY(PropertyInfo(Variant::STRING, "effect_id"), "set_effect_id", "get_effect_id");
	ADD_PROPERTY(PropertyInfo(Variant::NIL, "effect_name", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_DEFAULT), "set_effect_name", "get_effect_name");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "price", PROPERTY_HINT_RANGE, "0,100000,1"), "set_price", "get_price");
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "description", PROPERTY_HINT_MULTILINE_TEXT), "set_description", "get_description");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "duration", PROPERTY_HINT_RANGE, "0,180,1,or_greater"), "set_duration", "get_duration");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "quantity_min", PROPERTY_HINT_RANGE, "1,1000,1"), "set_quantity_min", "get_quantity_min");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "quantity_max", PROPERTY_HINT_RANGE, "1,1000,1"), "set_quantity_max", "get_quantity_max");
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "note"), "set_note", "get_note");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "inactive"), "set_inactive", "is_inactive");
	ADD_PROPERTY(PropertyInfo(Variant::PACKED_STRING_ARRAY, "category"), "set_category", "get_category");
	ADD_PROPERTY(PropertyInfo(Variant::PACKED_STRING_ARRAY, "group"), "set_group", "get_group");
	ADD_PROPERTY(PropertyInfo(Variant::DICTIONARY, "parameters"), "set_parameters", "get_parameters");
}

void CrowdControlEffect::set_effect_id(const String &p_id) {
	effect_id = p_id;
}

String CrowdControlEffect::get_effect_id() const {
	return effect_id;
}

void CrowdControlEffect::set_effect_name(const Variant &p_name) {
	effect_name = p_name;
}

Variant CrowdControlEffect::get_effect_name() const {
	return effect_name;
}

void CrowdControlEffect::set_price(int p_price) {
	price = MAX(0, p_price);
}

int CrowdControlEffect::get_price() const {
	return price;
}

void CrowdControlEffect::set_description(const String &p_description) {
	description = p_description;
}

String CrowdControlEffect::get_description() const {
	return description;
}

void CrowdControlEffect::set_duration(int p_duration) {
	duration = MAX(0, p_duration);
}

int CrowdControlEffect::get_duration() const {
	return duration;
}

void CrowdControlEffect::set_quantity_min(int p_min) {
	quantity_min = MAX(1, p_min);
}

int CrowdControlEffect::get_quantity_min() const {
	return quantity_min;
}

void CrowdControlEffect::set_quantity_max(int p_max) {
	quantity_max = MAX(1, p_max);
}

int CrowdControlEffect::get_quantity_max() const {
	return quantity_max;
}

void CrowdControlEffect::set_note(const String &p_note) {
	note = p_note;
}

String CrowdControlEffect::get_note() const {
	return note;
}

void CrowdControlEffect::set_inactive(bool p_inactive) {
	inactive = p_inactive;
}

bool CrowdControlEffect::is_inactive() const {
	return inactive;
}

void CrowdControlEffect::set_category(const PackedStringArray &p_category) {
	category = p_category;
}

PackedStringArray CrowdControlEffect::get_category() const {
	return category;
}

void CrowdControlEffect::set_group(const PackedStringArray &p_group) {
	group = p_group;
}

PackedStringArray CrowdControlEffect::get_group() const {
	return group;
}

void CrowdControlEffect::set_parameters(const Dictionary &p_parameters) {
	parameters = p_parameters;
}

Dictionary CrowdControlEffect::get_parameters() const {
	return parameters;
}

Dictionary CrowdControlEffect::to_json() const {
	Dictionary result;

	// Name (can be String or Dictionary with public/sort)
	result["name"] = effect_name;

	// Price
	result["price"] = price;

	// Description
	if (!description.is_empty()) {
		result["description"] = description;
	}

	// Duration (only if > 0)
	if (duration > 0) {
		Dictionary duration_dict;
		duration_dict["value"] = duration;
		result["duration"] = duration_dict;
	}

	// Quantity (only if different from defaults)
	if (quantity_min != 1 || quantity_max != 1) {
		Dictionary quantity_dict;
		quantity_dict["min"] = quantity_min;
		quantity_dict["max"] = quantity_max;
		result["quantity"] = quantity_dict;
	}

	// Note (optional)
	if (!note.is_empty()) {
		result["note"] = note;
	}

	// Inactive (only if true)
	if (inactive) {
		result["inactive"] = true;
	}

	// Category
	if (category.size() > 0) {
		Array cat_array;
		for (int i = 0; i < category.size(); i++) {
			cat_array.push_back(category[i]);
		}
		result["category"] = cat_array;
	}

	// Group
	if (group.size() > 0) {
		Array group_array;
		for (int i = 0; i < group.size(); i++) {
			group_array.push_back(group[i]);
		}
		result["group"] = group_array;
	}

	// Parameters
	if (parameters.size() > 0) {
		Dictionary params_result;
		Array keys = parameters.keys();
		for (int i = 0; i < keys.size(); i++) {
			String key = keys[i];
			Variant value = parameters[key];

			// If value is a CrowdControlEffectParameter resource, convert it
			if (value.get_type() == Variant::OBJECT) {
				Object *obj = value;
				CrowdControlEffectParameter *param = Object::cast_to<CrowdControlEffectParameter>(obj);
				if (param) {
					params_result[key] = param->to_json();
				} else {
					params_result[key] = value;
				}
			} else {
				params_result[key] = value;
			}
		}
		result["parameters"] = params_result;
	}

	return result;
}

CrowdControlEffect::CrowdControlEffect() {
}

CrowdControlEffect::~CrowdControlEffect() {
}
