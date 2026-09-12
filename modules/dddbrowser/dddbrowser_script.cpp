/**************************************************************************/
/*  dddbrowser_script.cpp                                                 */
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

#include "dddbrowser_script.h"

void DDDBrowserScript::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_asset_id", "id"), &DDDBrowserScript::set_asset_id);
	ClassDB::bind_method(D_METHOD("get_asset_id"), &DDDBrowserScript::get_asset_id);
	ClassDB::bind_method(D_METHOD("set_source_path", "path"), &DDDBrowserScript::set_source_path);
	ClassDB::bind_method(D_METHOD("get_source_path"), &DDDBrowserScript::get_source_path);
	ClassDB::bind_method(D_METHOD("set_pin_sha256", "pin"), &DDDBrowserScript::set_pin_sha256);
	ClassDB::bind_method(D_METHOD("get_pin_sha256"), &DDDBrowserScript::get_pin_sha256);

	ADD_PROPERTY(PropertyInfo(Variant::STRING, "asset_id"), "set_asset_id", "get_asset_id");
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "source_path", PROPERTY_HINT_FILE, "*.luau"), "set_source_path", "get_source_path");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "pin_sha256"), "set_pin_sha256", "get_pin_sha256");
}

void DDDBrowserScript::set_asset_id(const String &p_id) {
	asset_id = p_id;
}
String DDDBrowserScript::get_asset_id() const {
	return asset_id;
}
void DDDBrowserScript::set_source_path(const String &p_path) {
	source_path = p_path;
}
String DDDBrowserScript::get_source_path() const {
	return source_path;
}
void DDDBrowserScript::set_pin_sha256(bool p_pin) {
	pin_sha256 = p_pin;
}
bool DDDBrowserScript::get_pin_sha256() const {
	return pin_sha256;
}
