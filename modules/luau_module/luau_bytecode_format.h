/**************************************************************************/
/*  luau_bytecode_format.h                                                */
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

#include "core/templates/vector.h"
#include "core/variant/variant.h"

namespace luau_module {

struct LuauBytecodeWriteOptions {
	bool compress = false;
	bool encrypt = false;
};

class LuauBytecodeFormat {
public:
	static constexpr char MAGIC_RAW[4] = { 'L', 'U', 'A', 'C' };
	static constexpr char MAGIC_ZSTD[4] = { 'L', 'U', 'Z', 'C' };
	static constexpr char MAGIC_ENCRYPT[4] = { 'L', 'U', 'A', 'E' };

	static Vector<uint8_t> decrypt_export_data(const Vector<uint8_t> &p_data);
	static Vector<uint8_t> encrypt_data(const Vector<uint8_t> &p_data);

	static Vector<uint8_t> wrap_raw(const PackedByteArray &p_bytecode);
	static Vector<uint8_t> wrap_compressed(const PackedByteArray &p_bytecode);
	static Vector<uint8_t> wrap_for_write(const PackedByteArray &p_bytecode, const LuauBytecodeWriteOptions &p_opts);
	static PackedByteArray unwrap(const Vector<uint8_t> &p_file_data, bool *r_was_compressed = nullptr);
};

} //namespace luau_module
