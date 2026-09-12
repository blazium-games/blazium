/**************************************************************************/
/*  dddbrowser_textbox.cpp                                                */
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

#include "dddbrowser_textbox.h"

void DDDBrowserTextbox::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_text", "text"), &DDDBrowserTextbox::set_text);
	ClassDB::bind_method(D_METHOD("get_text"), &DDDBrowserTextbox::get_text);
	ClassDB::bind_method(D_METHOD("set_title", "title"), &DDDBrowserTextbox::set_title);
	ClassDB::bind_method(D_METHOD("get_title"), &DDDBrowserTextbox::get_title);
	ClassDB::bind_method(D_METHOD("set_font_asset_id", "id"), &DDDBrowserTextbox::set_font_asset_id);
	ClassDB::bind_method(D_METHOD("get_font_asset_id"), &DDDBrowserTextbox::get_font_asset_id);
	ClassDB::bind_method(D_METHOD("set_font_size", "size"), &DDDBrowserTextbox::set_font_size);
	ClassDB::bind_method(D_METHOD("get_font_size"), &DDDBrowserTextbox::get_font_size);
	ClassDB::bind_method(D_METHOD("set_color", "color"), &DDDBrowserTextbox::set_color);
	ClassDB::bind_method(D_METHOD("get_color"), &DDDBrowserTextbox::get_color);

	ADD_PROPERTY(PropertyInfo(Variant::STRING, "text", PROPERTY_HINT_MULTILINE_TEXT), "set_text", "get_text");
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "title"), "set_title", "get_title");
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "font_asset_id"), "set_font_asset_id", "get_font_asset_id");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "font_size", PROPERTY_HINT_RANGE, "1,512,0.1"), "set_font_size", "get_font_size");
	ADD_PROPERTY(PropertyInfo(Variant::COLOR, "color"), "set_color", "get_color");
}

void DDDBrowserTextbox::set_text(const String &p_text) {
	text = p_text;
}
String DDDBrowserTextbox::get_text() const {
	return text;
}
void DDDBrowserTextbox::set_title(const String &p_title) {
	title = p_title;
}
String DDDBrowserTextbox::get_title() const {
	return title;
}
void DDDBrowserTextbox::set_font_asset_id(const String &p_id) {
	font_asset_id = p_id;
}
String DDDBrowserTextbox::get_font_asset_id() const {
	return font_asset_id;
}
void DDDBrowserTextbox::set_font_size(float p_size) {
	font_size = p_size;
}
float DDDBrowserTextbox::get_font_size() const {
	return font_size;
}
void DDDBrowserTextbox::set_color(const Color &p_color) {
	color = p_color;
}
Color DDDBrowserTextbox::get_color() const {
	return color;
}
