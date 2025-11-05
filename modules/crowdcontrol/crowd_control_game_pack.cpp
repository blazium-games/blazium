/**************************************************************************/
/*  crowd_control_game_pack.cpp                                           */
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

#include "crowd_control_game_pack.h"

#include "core/io/file_access.h"
#include "core/io/json.h"

void CrowdControlGamePack::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_pack_meta", "meta"), &CrowdControlGamePack::set_pack_meta);
	ClassDB::bind_method(D_METHOD("get_pack_meta"), &CrowdControlGamePack::get_pack_meta);

	ClassDB::bind_method(D_METHOD("set_effects", "effects"), &CrowdControlGamePack::set_effects);
	ClassDB::bind_method(D_METHOD("get_effects"), &CrowdControlGamePack::get_effects);

	ClassDB::bind_method(D_METHOD("add_effect", "effect"), &CrowdControlGamePack::add_effect);
	ClassDB::bind_method(D_METHOD("remove_effect", "effect_id"), &CrowdControlGamePack::remove_effect);
	ClassDB::bind_method(D_METHOD("get_effect", "effect_id"), &CrowdControlGamePack::get_effect);

	ClassDB::bind_method(D_METHOD("export_to_json", "path"), &CrowdControlGamePack::export_to_json);
	ClassDB::bind_method(D_METHOD("to_json"), &CrowdControlGamePack::to_json);

	ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "pack_meta", PROPERTY_HINT_RESOURCE_TYPE, "CrowdControlGamePackMeta"), "set_pack_meta", "get_pack_meta");
	ADD_PROPERTY(PropertyInfo(Variant::ARRAY, "effects", PROPERTY_HINT_ARRAY_TYPE, "CrowdControlEffect"), "set_effects", "get_effects");
}

void CrowdControlGamePack::set_pack_meta(const Ref<CrowdControlGamePackMeta> &p_meta) {
	pack_meta = p_meta;
}

Ref<CrowdControlGamePackMeta> CrowdControlGamePack::get_pack_meta() const {
	return pack_meta;
}

void CrowdControlGamePack::set_effects(const TypedArray<CrowdControlEffect> &p_effects) {
	effects = p_effects;
}

TypedArray<CrowdControlEffect> CrowdControlGamePack::get_effects() const {
	return effects;
}

void CrowdControlGamePack::add_effect(const Ref<CrowdControlEffect> &p_effect) {
	ERR_FAIL_COND(p_effect.is_null());
	effects.push_back(p_effect);
}

void CrowdControlGamePack::remove_effect(const String &p_effect_id) {
	for (int i = 0; i < effects.size(); i++) {
		Ref<CrowdControlEffect> effect = effects[i];
		if (effect.is_valid() && effect->get_effect_id() == p_effect_id) {
			effects.remove_at(i);
			return;
		}
	}
}

Ref<CrowdControlEffect> CrowdControlGamePack::get_effect(const String &p_effect_id) const {
	for (int i = 0; i < effects.size(); i++) {
		Ref<CrowdControlEffect> effect = effects[i];
		if (effect.is_valid() && effect->get_effect_id() == p_effect_id) {
			return effect;
		}
	}
	return Ref<CrowdControlEffect>();
}

Error CrowdControlGamePack::export_to_json(const String &p_path) {
	ERR_FAIL_COND_V(pack_meta.is_null(), ERR_UNCONFIGURED);

	// Convert to JSON dictionary
	Dictionary pack_dict = to_json();

	// Convert to JSON string with pretty printing
	String json_string = JSON::stringify(pack_dict, "\t", false);

	// Write to file
	Ref<FileAccess> file = FileAccess::open(p_path, FileAccess::WRITE);
	ERR_FAIL_COND_V_MSG(file.is_null(), ERR_FILE_CANT_WRITE, vformat("Cannot open file for writing: %s", p_path));

	file->store_string(json_string);
	file->close();

	return OK;
}

Dictionary CrowdControlGamePack::to_json() const {
	Dictionary result;

	// Add metadata
	if (pack_meta.is_valid()) {
		result["meta"] = pack_meta->to_json();
	}

	// Add effects under "effects" -> "game" structure
	Dictionary effects_dict;
	Dictionary game_effects;

	for (int i = 0; i < effects.size(); i++) {
		Ref<CrowdControlEffect> effect = effects[i];
		if (effect.is_valid()) {
			String effect_id = effect->get_effect_id();
			if (!effect_id.is_empty()) {
				game_effects[effect_id] = effect->to_json();
			} else {
				WARN_PRINT(vformat("Effect at index %d has no effect_id and will be skipped", i));
			}
		}
	}

	effects_dict["game"] = game_effects;
	result["effects"] = effects_dict;

	return result;
}

CrowdControlGamePack::CrowdControlGamePack() {
}

CrowdControlGamePack::~CrowdControlGamePack() {
}
