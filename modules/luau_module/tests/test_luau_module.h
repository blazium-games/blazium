/**************************************************************************/
/*  test_luau_module.h                                                    */
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

#include "tests/test_macros.h"

#include "modules/luau_module/luau.h"
#include "modules/luau_module/luau_bytecode_format.h"
#include "modules/luau_module/luau_class_info.h"
#include "modules/luau_module/luau_codegen.h"
#include "modules/luau_module/luau_compile_result.h"

namespace TestLuauModule {

TEST_CASE("[Modules][LuauModule] compile simple expression") {
	PackedByteArray bytecode = luau_module::Luau::compile("return 1 + 1");
	CHECK(bytecode.size() > 0);
}

TEST_CASE("[Modules][LuauModule] compile error diagnostics") {
	luau_module::LuauCompileResult result = luau_module::Luau::compile_with_diagnostics("local x =\nreturn 1");
	CHECK(result.is_error());
	CHECK(result.error_line >= 1);
	CHECK(!result.error_message.is_empty());
}

TEST_CASE("[Modules][LuauModule] bytecode format round-trip") {
	PackedByteArray raw = luau_module::Luau::compile("return 1");
	CHECK(raw.size() > 0);
	Vector<uint8_t> wrapped = luau_module::LuauBytecodeFormat::wrap_raw(raw);
	CHECK(wrapped.size() > raw.size());
	PackedByteArray unwrapped = luau_module::LuauBytecodeFormat::unwrap(wrapped);
	CHECK(unwrapped.size() == raw.size());
}

TEST_CASE("[Modules][LuauModule] native directive detection") {
	CHECK(luau_module::LuauCodegen::source_requests_native("--!native\nreturn 1"));
	(void)luau_module::LuauCodegen::is_enabled();
}

TEST_CASE("[Modules][LuauModule] wrap_for_write compressed round-trip") {
	PackedByteArray raw = luau_module::Luau::compile("return 1 + 1");
	CHECK(raw.size() > 0);

	luau_module::LuauBytecodeWriteOptions opts;
	opts.compress = true;
	Vector<uint8_t> wrapped = luau_module::LuauBytecodeFormat::wrap_for_write(raw, opts);
	CHECK(wrapped.size() > 4);

	Vector<uint8_t> decrypted = luau_module::LuauBytecodeFormat::decrypt_export_data(wrapped);
	PackedByteArray unwrapped = luau_module::LuauBytecodeFormat::unwrap(decrypted);
	CHECK(unwrapped.size() == raw.size());
}

TEST_CASE("[Modules][LuauModule] parse global class metadata from source") {
	const String source = R"(
--- @class LuauFixtureTableDsl
--- @extends Node
local TableDslNode = {
	extends = "Node",
	class_name = "LuauFixtureTableDsl",
	tool = true,
}
return TableDslNode
)";

	LuauClassInfo info;
	LuauClassInfo::parse_global_class_metadata_from_source(source, &info);
	CHECK(info.class_name == StringName("LuauFixtureTableDsl"));
	CHECK(info.extends == "Node");
	CHECK(info.tool);
}

} //namespace TestLuauModule
