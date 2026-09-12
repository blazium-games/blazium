/**************************************************************************/
/*  resource_saver_luau.cpp                                               */
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

#include "resource_saver_luau.h"

#include "luau.h"
#include "luau_bytecode_format.h"
#include "luau_compile_result.h"
#include "luau_script.h"
#include "luau_script_language.h"

#include "core/error/error_macros.h"
#include "core/io/file_access.h"
#include "core/object/script_language.h"

#ifdef TOOLS_ENABLED
#include "editor/editor_settings.h"
#include "editor/luau_formatter.h"
#endif

Error ResourceFormatSaverLuau::save(const Ref<Resource> &p_resource, const String &p_path, uint32_t p_flags) {
	Ref<LuauScript> luau_script = p_resource;
	ERR_FAIL_COND_V(luau_script.is_null(), ERR_INVALID_PARAMETER);

	Error err;
	Ref<FileAccess> file = FileAccess::open(p_path, FileAccess::WRITE, &err);
	ERR_FAIL_COND_V_MSG(err != OK, err, vformat("Cannot save Luau script '%s'.", p_path));

	if (p_path.ends_with(".luauc")) {
		String source = luau_script->get_source_code();
#ifdef TOOLS_ENABLED
		if (EDITOR_GET("luau/editor/bytecode_minify_on_save")) {
			source = LuauFormatter::minify_source(source);
		}
#endif
		const luau_module::LuauCompileResult compile_result = luau_module::Luau::compile_with_diagnostics(source);
		ERR_FAIL_COND_V_MSG(compile_result.is_error() || compile_result.bytecode.is_empty(), ERR_CANT_CREATE,
				vformat("Cannot compile Luau script '%s': %s", p_path, compile_result.error_message));

		luau_module::LuauBytecodeWriteOptions opts;
#ifdef TOOLS_ENABLED
		opts.compress = EDITOR_GET("luau/editor/bytecode_compress_on_save");
		opts.encrypt = EDITOR_GET("luau/editor/bytecode_encrypt_on_save");
#endif
		const Vector<uint8_t> wrapped = luau_module::LuauBytecodeFormat::wrap_for_write(compile_result.bytecode, opts);
		file->store_buffer(wrapped);
	} else {
		file->store_string(luau_script->get_source_code());
	}

	if (file->get_error() != OK && file->get_error() != ERR_FILE_EOF) {
		return ERR_CANT_CREATE;
	}

	if (ScriptServer::is_reload_scripts_on_save_enabled()) {
		LuauScriptLanguage::get_singleton()->reload_tool_script(luau_script, true);
	}

	return OK;
}

void ResourceFormatSaverLuau::get_recognized_extensions(const Ref<Resource> &p_resource, List<String> *p_extensions) const {
	if (Object::cast_to<LuauScript>(*p_resource)) {
		p_extensions->push_back("luau");
		p_extensions->push_back("lua");
		p_extensions->push_back("luauc");
	}
}

bool ResourceFormatSaverLuau::recognize(const Ref<Resource> &p_resource) const {
	return Object::cast_to<LuauScript>(*p_resource) != nullptr;
}
