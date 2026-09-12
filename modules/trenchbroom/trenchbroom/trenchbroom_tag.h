/**************************************************************************/
/*  trenchbroom_tag.h                                                     */
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

#include "core/io/resource.h"
#include "core/variant/typed_array.h"

class TrenchbroomTag : public Resource {
	GDCLASS(TrenchbroomTag, Resource);

public:
	enum TagMatchType {
		TAG_MATCH_TEXTURE,
		TAG_MATCH_CLASSNAME,
	};

protected:
	static void _bind_methods();

	String tag_name;
	TypedArray<String> tag_attributes;
	TagMatchType tag_match_type = TAG_MATCH_TEXTURE;
	String tag_pattern;
	String texture_name;

public:
	void set_tag_name(const String &p_name) { tag_name = p_name; }
	String get_tag_name() const { return tag_name; }

	void set_tag_attributes(const TypedArray<String> &p_attributes) { tag_attributes = p_attributes; }
	TypedArray<String> get_tag_attributes() const { return tag_attributes; }

	void set_tag_match_type(TagMatchType p_type) { tag_match_type = p_type; }
	TagMatchType get_tag_match_type() const { return tag_match_type; }

	void set_tag_pattern(const String &p_pattern) { tag_pattern = p_pattern; }
	String get_tag_pattern() const { return tag_pattern; }

	void set_texture_name(const String &p_name) { texture_name = p_name; }
	String get_texture_name() const { return texture_name; }

	static String get_match_key(TagMatchType p_type);
};

VARIANT_ENUM_CAST(TrenchbroomTag::TagMatchType);
