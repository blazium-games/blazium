/**************************************************************************/
/*  trenchbroom_tag.cpp                                                   */
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

#include "trenchbroom_tag.h"

#include "core/error/error_macros.h"
#include "core/object/class_db.h"

void TrenchbroomTag::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_tag_name", "tag_name"), &TrenchbroomTag::set_tag_name);
	ClassDB::bind_method(D_METHOD("get_tag_name"), &TrenchbroomTag::get_tag_name);
	ClassDB::bind_method(D_METHOD("set_tag_attributes", "tag_attributes"), &TrenchbroomTag::set_tag_attributes);
	ClassDB::bind_method(D_METHOD("get_tag_attributes"), &TrenchbroomTag::get_tag_attributes);
	ClassDB::bind_method(D_METHOD("set_tag_match_type", "tag_match_type"), &TrenchbroomTag::set_tag_match_type);
	ClassDB::bind_method(D_METHOD("get_tag_match_type"), &TrenchbroomTag::get_tag_match_type);
	ClassDB::bind_method(D_METHOD("set_tag_pattern", "tag_pattern"), &TrenchbroomTag::set_tag_pattern);
	ClassDB::bind_method(D_METHOD("get_tag_pattern"), &TrenchbroomTag::get_tag_pattern);
	ClassDB::bind_method(D_METHOD("set_texture_name", "texture_name"), &TrenchbroomTag::set_texture_name);
	ClassDB::bind_method(D_METHOD("get_texture_name"), &TrenchbroomTag::get_texture_name);
	ClassDB::bind_static_method("TrenchbroomTag", D_METHOD("get_match_key", "tag_match_type"), &TrenchbroomTag::get_match_key);

	BIND_ENUM_CONSTANT(TAG_MATCH_TEXTURE);
	BIND_ENUM_CONSTANT(TAG_MATCH_CLASSNAME);

	ADD_PROPERTY(PropertyInfo(Variant::STRING, "tag_name"), "set_tag_name", "get_tag_name");
	ADD_PROPERTY(PropertyInfo(Variant::PACKED_STRING_ARRAY, "tag_attributes"), "set_tag_attributes", "get_tag_attributes");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "tag_match_type", PROPERTY_HINT_ENUM, "Texture,Classname"), "set_tag_match_type", "get_tag_match_type");
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "tag_pattern"), "set_tag_pattern", "get_tag_pattern");
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "texture_name"), "set_texture_name", "get_texture_name");
}

String TrenchbroomTag::get_match_key(TagMatchType p_type) {
	switch (p_type) {
		case TAG_MATCH_TEXTURE:
			return "material";
		case TAG_MATCH_CLASSNAME:
			return "classname";
		default:
			ERR_PRINT(vformat("Tag match type %s is not valid", (int)p_type));
			return "ERROR";
	}
}
