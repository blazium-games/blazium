/**************************************************************************/
/*  dddbrowser_font.cpp                                                   */
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

#include "dddbrowser_font.h"

void DDDBrowserFont::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_asset_id", "id"), &DDDBrowserFont::set_asset_id);
	ClassDB::bind_method(D_METHOD("get_asset_id"), &DDDBrowserFont::get_asset_id);
	ClassDB::bind_method(D_METHOD("set_source_path", "path"), &DDDBrowserFont::set_source_path);
	ClassDB::bind_method(D_METHOD("get_source_path"), &DDDBrowserFont::get_source_path);
	ClassDB::bind_method(D_METHOD("set_size", "size"), &DDDBrowserFont::set_size);
	ClassDB::bind_method(D_METHOD("get_size"), &DDDBrowserFont::get_size);
	ClassDB::bind_method(D_METHOD("set_style", "style"), &DDDBrowserFont::set_style);
	ClassDB::bind_method(D_METHOD("get_style"), &DDDBrowserFont::get_style);

	ADD_PROPERTY(PropertyInfo(Variant::STRING, "asset_id"), "set_asset_id", "get_asset_id");
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "source_path", PROPERTY_HINT_FILE, "*.ttf,*.otf"), "set_source_path", "get_source_path");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "size", PROPERTY_HINT_RANGE, "1,512,0.1"), "set_size", "get_size");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "style", PROPERTY_HINT_ENUM, "Normal,Bold,Italic"), "set_style", "get_style");

	BIND_ENUM_CONSTANT(STYLE_NORMAL);
	BIND_ENUM_CONSTANT(STYLE_BOLD);
	BIND_ENUM_CONSTANT(STYLE_ITALIC);
}

void DDDBrowserFont::set_asset_id(const String &p_id) {
	asset_id = p_id;
}
String DDDBrowserFont::get_asset_id() const {
	return asset_id;
}
void DDDBrowserFont::set_source_path(const String &p_path) {
	source_path = p_path;
}
String DDDBrowserFont::get_source_path() const {
	return source_path;
}
void DDDBrowserFont::set_size(float p_size) {
	size = p_size;
}
float DDDBrowserFont::get_size() const {
	return size;
}
void DDDBrowserFont::set_style(Style p_style) {
	style = p_style;
}
DDDBrowserFont::Style DDDBrowserFont::get_style() const {
	return style;
}

String DDDBrowserFont::style_string() const {
	switch (style) {
		case STYLE_BOLD:
			return "bold";
		case STYLE_ITALIC:
			return "italic";
		default:
			return "normal";
	}
}
