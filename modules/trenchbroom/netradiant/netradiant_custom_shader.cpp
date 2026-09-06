/**************************************************************************/
/*  netradiant_custom_shader.cpp                                          */
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

#include "netradiant_custom_shader.h"

#include "core/object/class_db.h"

void NetRadiantCustomShader::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_texture_path", "texture_path"), &NetRadiantCustomShader::set_texture_path);
	ClassDB::bind_method(D_METHOD("get_texture_path"), &NetRadiantCustomShader::get_texture_path);
	ClassDB::bind_method(D_METHOD("set_shader_attributes", "shader_attributes"), &NetRadiantCustomShader::set_shader_attributes);
	ClassDB::bind_method(D_METHOD("get_shader_attributes"), &NetRadiantCustomShader::get_shader_attributes);

	ADD_PROPERTY(PropertyInfo(Variant::STRING, "texture_path"), "set_texture_path", "get_texture_path");
	ADD_PROPERTY(PropertyInfo(Variant::PACKED_STRING_ARRAY, "shader_attributes"), "set_shader_attributes", "get_shader_attributes");
}
