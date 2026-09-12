/**************************************************************************/
/*  crowd_control_effect_parameter.cpp                                    */
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

#include "crowd_control_effect_parameter.h"

void CrowdControlEffectParameter::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_parameter_type", "type"), &CrowdControlEffectParameter::set_parameter_type);
	ClassDB::bind_method(D_METHOD("get_parameter_type"), &CrowdControlEffectParameter::get_parameter_type);

	ClassDB::bind_method(D_METHOD("set_parameter_name", "name"), &CrowdControlEffectParameter::set_parameter_name);
	ClassDB::bind_method(D_METHOD("get_parameter_name"), &CrowdControlEffectParameter::get_parameter_name);

	ClassDB::bind_method(D_METHOD("set_options", "options"), &CrowdControlEffectParameter::set_options);
	ClassDB::bind_method(D_METHOD("get_options"), &CrowdControlEffectParameter::get_options);

	ClassDB::bind_method(D_METHOD("to_json"), &CrowdControlEffectParameter::to_json);

	ADD_PROPERTY(PropertyInfo(Variant::STRING, "parameter_type"), "set_parameter_type", "get_parameter_type");
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "parameter_name"), "set_parameter_name", "get_parameter_name");
	ADD_PROPERTY(PropertyInfo(Variant::DICTIONARY, "options"), "set_options", "get_options");
}

void CrowdControlEffectParameter::set_parameter_type(const String &p_type) {
	parameter_type = p_type;
}

String CrowdControlEffectParameter::get_parameter_type() const {
	return parameter_type;
}

void CrowdControlEffectParameter::set_parameter_name(const String &p_name) {
	parameter_name = p_name;
}

String CrowdControlEffectParameter::get_parameter_name() const {
	return parameter_name;
}

void CrowdControlEffectParameter::set_options(const Dictionary &p_options) {
	options = p_options;
}

Dictionary CrowdControlEffectParameter::get_options() const {
	return options;
}

Dictionary CrowdControlEffectParameter::to_json() const {
	Dictionary result;
	result["type"] = parameter_type;
	result["name"] = parameter_name;

	// Convert options to proper format
	Dictionary formatted_options;
	Array keys = options.keys();
	for (int i = 0; i < keys.size(); i++) {
		String key = keys[i];
		Dictionary option_data;
		option_data["name"] = options[key];
		formatted_options[key] = option_data;
	}
	result["options"] = formatted_options;

	return result;
}

CrowdControlEffectParameter::CrowdControlEffectParameter() {
}

CrowdControlEffectParameter::~CrowdControlEffectParameter() {
}
