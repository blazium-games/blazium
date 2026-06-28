/**************************************************************************/
/*  resource_loader_luau.cpp                                              */
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

#include "resource_loader_luau.h"

#include "luau_script.h"

#include "core/error/error_macros.h"

Ref<Resource> ResourceFormatLoaderLuau::load(const String &p_path, const String &p_original_path, Error *r_error, bool p_use_sub_threads, float *r_progress, CacheMode p_cache_mode) {
	Ref<LuauScript> luau_script;
	luau_script.instantiate();

	const String path = p_original_path.is_empty() ? p_path : p_original_path;
	Error err = OK;
	if (path.get_extension().to_lower() == "luauc") {
		err = luau_script->load_bytecode_file(path);
	} else {
		err = luau_script->load_source_code(path);
	}

	if (r_error) {
		*r_error = luau_script.is_valid() && luau_script->is_valid() ? OK : err;
	}
	return luau_script;
}

void ResourceFormatLoaderLuau::get_recognized_extensions(List<String> *p_extensions) const {
	p_extensions->push_back("luau");
	p_extensions->push_back("lua");
	p_extensions->push_back("luauc");
}

bool ResourceFormatLoaderLuau::handles_type(const String &p_type) const {
	return p_type == "Script" || p_type == "LuauScript";
}

String ResourceFormatLoaderLuau::get_resource_type(const String &p_path) const {
	String ext = p_path.get_extension().to_lower();
	if (ext == "luau" || ext == "lua" || ext == "luauc") {
		return "LuauScript";
	}
	return String();
}
