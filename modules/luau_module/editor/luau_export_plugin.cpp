/**************************************************************************/
/*  luau_export_plugin.cpp                                                */
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

#ifdef TOOLS_ENABLED

#include "editor/luau_export_plugin.h"

#include "editor/luau_formatter.h"
#include "luau.h"
#include "luau_bytecode_format.h"

#include "core/io/compression.h"
#include "core/io/file_access.h"

using namespace luau_module;

void EditorExportLuau::_get_export_options(const Ref<EditorExportPlatform> &p_export_platform, List<EditorExportPlatform::ExportOption> *r_options) const {
	ERR_FAIL_NULL(r_options);
	const bool is_web = p_export_platform.is_valid() && p_export_platform->get_os_name() == "Web";
	r_options->push_back(EditorExportPlatform::ExportOption(PropertyInfo(Variant::BOOL, "luau/compile_bytecode"), true));
	r_options->push_back(EditorExportPlatform::ExportOption(PropertyInfo(Variant::BOOL, "luau/strip_source"), is_web));
	r_options->push_back(EditorExportPlatform::ExportOption(PropertyInfo(Variant::BOOL, "luau/minify"), false));
	r_options->push_back(EditorExportPlatform::ExportOption(PropertyInfo(Variant::BOOL, "luau/tree_shake"), false));
	r_options->push_back(EditorExportPlatform::ExportOption(
			PropertyInfo(Variant::BOOL, "luau/encrypt", PROPERTY_HINT_NONE, "", is_web ? PROPERTY_USAGE_NONE : PROPERTY_USAGE_DEFAULT),
			false));
}

void EditorExportLuau::_export_begin(const HashSet<String> &p_features, bool p_debug, const String &p_path, int p_flags) {
	(void)p_features;
	(void)p_debug;
	(void)p_path;
	(void)p_flags;

	script_mode = DEFAULT_SCRIPT_MODE;
	compile_bytecode = bool(get_option("luau/compile_bytecode"));
	strip_source = bool(get_option("luau/strip_source"));
	minify_source = bool(get_option("luau/minify"));
	tree_shake_source = bool(get_option("luau/tree_shake"));
	encrypt_bytecode = bool(get_option("luau/encrypt"));

	const Ref<EditorExportPreset> &preset = get_export_preset();
	if (preset.is_valid()) {
		script_mode = preset->get_script_export_mode();
	}

	const Ref<EditorExportPlatform> platform = get_export_platform();
	const bool is_web = platform.is_valid() && platform->get_os_name() == "Web";
	if (is_web) {
		compile_bytecode = true;
		encrypt_bytecode = false;
		if (script_mode == EditorExportPreset::MODE_SCRIPT_TEXT) {
			WARN_PRINT("Luau web export requires compiled bytecode; forcing bytecode export.");
		}
	}

	if (script_mode == EditorExportPreset::MODE_SCRIPT_TEXT && !is_web) {
		compile_bytecode = false;
	}
}

void EditorExportLuau::_export_file(const String &p_path, const String &p_type, const HashSet<String> &p_features) {
	(void)p_type;
	(void)p_features;

	const String extension = p_path.get_extension();
	if (extension != "luau" && extension != "lua") {
		return;
	}

	const Vector<uint8_t> file = FileAccess::get_file_as_bytes(p_path);
	if (file.is_empty()) {
		return;
	}

	String source;
	source.parse_utf8(reinterpret_cast<const char *>(file.ptr()), file.size());
	if (source.is_empty()) {
		return;
	}

	if (tree_shake_source) {
		source = LuauFormatter::tree_shake_source(source);
	}
	if (minify_source) {
		source = LuauFormatter::minify_source(source);
	}

	if (!compile_bytecode) {
		if (minify_source || tree_shake_source) {
			const Vector<uint8_t> transformed = source.to_utf8_buffer();
			add_file(p_path, transformed, false);
		}
		return;
	}

	const Ref<EditorExportPlatform> platform = get_export_platform();
	const bool is_web = platform.is_valid() && platform->get_os_name() == "Web";
	if (is_web && source.contains("--!native")) {
		WARN_PRINT_ONCE("Luau: --!native is ignored on web export (CodeGen/JIT unavailable on wasm).");
	}

	const PackedByteArray bytecode = Luau::compile(source);
	if (bytecode.is_empty()) {
		return;
	}

	Vector<uint8_t> export_data;
	const bool use_compression = script_mode == EditorExportPreset::MODE_SCRIPT_BINARY_TOKENS_COMPRESSED;
	luau_module::LuauBytecodeWriteOptions opts;
	opts.compress = use_compression;
	opts.encrypt = encrypt_bytecode;
	export_data = LuauBytecodeFormat::wrap_for_write(bytecode, opts);

	if (export_data.is_empty()) {
		return;
	}

	add_file(p_path.get_basename() + ".luauc", export_data, true);

	if (strip_source) {
		skip();
	}
}

#endif
