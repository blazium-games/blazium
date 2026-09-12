/**************************************************************************/
/*  dddbrowser_picturebox.cpp                                             */
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

#include "dddbrowser_picturebox.h"

void DDDBrowserPicturebox::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_texture_path", "path"), &DDDBrowserPicturebox::set_texture_path);
	ClassDB::bind_method(D_METHOD("get_texture_path"), &DDDBrowserPicturebox::get_texture_path);
	ClassDB::bind_method(D_METHOD("set_width", "width"), &DDDBrowserPicturebox::set_width);
	ClassDB::bind_method(D_METHOD("get_width"), &DDDBrowserPicturebox::get_width);
	ClassDB::bind_method(D_METHOD("set_height", "height"), &DDDBrowserPicturebox::set_height);
	ClassDB::bind_method(D_METHOD("get_height"), &DDDBrowserPicturebox::get_height);

	ADD_PROPERTY(PropertyInfo(Variant::STRING, "texture_path", PROPERTY_HINT_FILE, "*.png,*.jpg,*.jpeg,*.tga"), "set_texture_path", "get_texture_path");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "width", PROPERTY_HINT_RANGE, "0.01,10000,0.01"), "set_width", "get_width");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "height", PROPERTY_HINT_RANGE, "0.01,10000,0.01"), "set_height", "get_height");
}

void DDDBrowserPicturebox::set_texture_path(const String &p_path) {
	texture_path = p_path;
}
String DDDBrowserPicturebox::get_texture_path() const {
	return texture_path;
}
void DDDBrowserPicturebox::set_width(float p_width) {
	width = p_width;
}
float DDDBrowserPicturebox::get_width() const {
	return width;
}
void DDDBrowserPicturebox::set_height(float p_height) {
	height = p_height;
}
float DDDBrowserPicturebox::get_height() const {
	return height;
}
