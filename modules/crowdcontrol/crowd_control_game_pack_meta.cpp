/**************************************************************************/
/*  crowd_control_game_pack_meta.cpp                                      */
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

#include "crowd_control_game_pack_meta.h"

void CrowdControlGamePackMeta::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_game_name", "name"), &CrowdControlGamePackMeta::set_game_name);
	ClassDB::bind_method(D_METHOD("get_game_name"), &CrowdControlGamePackMeta::get_game_name);

	ClassDB::bind_method(D_METHOD("set_release_date", "date"), &CrowdControlGamePackMeta::set_release_date);
	ClassDB::bind_method(D_METHOD("get_release_date"), &CrowdControlGamePackMeta::get_release_date);

	ClassDB::bind_method(D_METHOD("set_platform", "platform"), &CrowdControlGamePackMeta::set_platform);
	ClassDB::bind_method(D_METHOD("get_platform"), &CrowdControlGamePackMeta::get_platform);

	ClassDB::bind_method(D_METHOD("set_connector", "connector"), &CrowdControlGamePackMeta::set_connector);
	ClassDB::bind_method(D_METHOD("get_connector"), &CrowdControlGamePackMeta::get_connector);

	ClassDB::bind_method(D_METHOD("set_steam_id", "id"), &CrowdControlGamePackMeta::set_steam_id);
	ClassDB::bind_method(D_METHOD("get_steam_id"), &CrowdControlGamePackMeta::get_steam_id);

	ClassDB::bind_method(D_METHOD("set_description", "description"), &CrowdControlGamePackMeta::set_description);
	ClassDB::bind_method(D_METHOD("get_description"), &CrowdControlGamePackMeta::get_description);

	ClassDB::bind_method(D_METHOD("set_guide_url", "url"), &CrowdControlGamePackMeta::set_guide_url);
	ClassDB::bind_method(D_METHOD("get_guide_url"), &CrowdControlGamePackMeta::get_guide_url);

	ClassDB::bind_method(D_METHOD("to_json"), &CrowdControlGamePackMeta::to_json);

	ADD_PROPERTY(PropertyInfo(Variant::NIL, "game_name", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_DEFAULT), "set_game_name", "get_game_name");
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "release_date"), "set_release_date", "get_release_date");
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "platform"), "set_platform", "get_platform");
	ADD_PROPERTY(PropertyInfo(Variant::PACKED_STRING_ARRAY, "connector"), "set_connector", "get_connector");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "steam_id"), "set_steam_id", "get_steam_id");
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "description", PROPERTY_HINT_MULTILINE_TEXT), "set_description", "get_description");
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "guide_url"), "set_guide_url", "get_guide_url");
}

void CrowdControlGamePackMeta::set_game_name(const Variant &p_name) {
	game_name = p_name;
}

Variant CrowdControlGamePackMeta::get_game_name() const {
	return game_name;
}

void CrowdControlGamePackMeta::set_release_date(const String &p_date) {
	release_date = p_date;
}

String CrowdControlGamePackMeta::get_release_date() const {
	return release_date;
}

void CrowdControlGamePackMeta::set_platform(const String &p_platform) {
	platform = p_platform;
}

String CrowdControlGamePackMeta::get_platform() const {
	return platform;
}

void CrowdControlGamePackMeta::set_connector(const PackedStringArray &p_connector) {
	connector = p_connector;
}

PackedStringArray CrowdControlGamePackMeta::get_connector() const {
	return connector;
}

void CrowdControlGamePackMeta::set_steam_id(int p_id) {
	steam_id = p_id;
}

int CrowdControlGamePackMeta::get_steam_id() const {
	return steam_id;
}

void CrowdControlGamePackMeta::set_description(const String &p_description) {
	description = p_description;
}

String CrowdControlGamePackMeta::get_description() const {
	return description;
}

void CrowdControlGamePackMeta::set_guide_url(const String &p_url) {
	guide_url = p_url;
}

String CrowdControlGamePackMeta::get_guide_url() const {
	return guide_url;
}

Dictionary CrowdControlGamePackMeta::to_json() const {
	Dictionary result;

	// Name (required)
	result["name"] = game_name;

	// Release date (required)
	result["releaseDate"] = release_date;

	// Platform (required)
	result["platform"] = platform;

	// Connector (required)
	if (connector.size() > 0) {
		Array connector_array;
		for (int i = 0; i < connector.size(); i++) {
			connector_array.push_back(connector[i]);
		}
		result["connector"] = connector_array;
	}

	// Steam ID (optional)
	if (steam_id > 0) {
		Dictionary steam_dict;
		steam_dict["id"] = steam_id;
		result["steam"] = steam_dict;
	}

	// Description (optional)
	if (!description.is_empty()) {
		result["description"] = description;
	}

	// Guide URL (optional)
	if (!guide_url.is_empty()) {
		result["guide"] = guide_url;
	}

	return result;
}

CrowdControlGamePackMeta::CrowdControlGamePackMeta() {
	// Set default connector
	connector.push_back("ExternalUnity");
}

CrowdControlGamePackMeta::~CrowdControlGamePackMeta() {
}
