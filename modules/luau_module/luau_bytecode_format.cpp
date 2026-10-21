/**************************************************************************/
/*  luau_bytecode_format.cpp                                              */
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

#include "luau_bytecode_format.h"

#include "core/io/compression.h"

using namespace luau_module;

namespace {
constexpr int MAX_LUAU_BYTECODE = 16 * 1024 * 1024;

void write_u32(uint8_t *p_dst, uint32_t p_value) {
	p_dst[0] = p_value & 0xff;
	p_dst[1] = (p_value >> 8) & 0xff;
	p_dst[2] = (p_value >> 16) & 0xff;
	p_dst[3] = (p_value >> 24) & 0xff;
}

uint32_t read_u32(const uint8_t *p_src) {
	return (uint32_t)p_src[0] | ((uint32_t)p_src[1] << 8) | ((uint32_t)p_src[2] << 16) | ((uint32_t)p_src[3] << 24);
}
}

Vector<uint8_t> LuauBytecodeFormat::decrypt_export_data(const Vector<uint8_t> &p_data) {
	static const char key[] = "blazium_luau_export_key";
	if (p_data.size() < 4 || memcmp(p_data.ptr(), MAGIC_ENCRYPT, 4) != 0) {
		return p_data;
	}

	Vector<uint8_t> decrypted;
	decrypted.resize(p_data.size() - 4);
	memcpy(decrypted.ptrw(), p_data.ptr() + 4, decrypted.size());
	const int key_len = sizeof(key) - 1;
	for (int i = 0; i < decrypted.size(); i++) {
		decrypted.write[i] ^= key[i % key_len];
	}
	return decrypted;
}

Vector<uint8_t> LuauBytecodeFormat::encrypt_data(const Vector<uint8_t> &p_data) {
	static const char key[] = "blazium_luau_export_key";
	Vector<uint8_t> encrypted;
	encrypted.resize(4 + p_data.size());
	memcpy(encrypted.ptrw(), MAGIC_ENCRYPT, 4);
	memcpy(encrypted.ptrw() + 4, p_data.ptr(), p_data.size());
	const int key_len = sizeof(key) - 1;
	for (int i = 4; i < encrypted.size(); i++) {
		encrypted.write[i] ^= key[i % key_len];
	}
	return encrypted;
}

Vector<uint8_t> LuauBytecodeFormat::wrap_raw(const PackedByteArray &p_bytecode) {
	Vector<uint8_t> out;
	out.resize(4 + p_bytecode.size());
	memcpy(out.ptrw(), MAGIC_RAW, 4);
	memcpy(out.ptrw() + 4, p_bytecode.ptr(), p_bytecode.size());
	return out;
}

Vector<uint8_t> LuauBytecodeFormat::wrap_compressed(const PackedByteArray &p_bytecode) {
	const int max_compressed = Compression::get_max_compressed_buffer_size(p_bytecode.size(), Compression::MODE_ZSTD);
	Vector<uint8_t> compressed;
	compressed.resize(max_compressed);
	const int compressed_size = Compression::compress(compressed.ptrw(), p_bytecode.ptr(), p_bytecode.size(), Compression::MODE_ZSTD);
	if (compressed_size <= 0) {
		return wrap_raw(p_bytecode);
	}
	compressed.resize(compressed_size);
	if (p_bytecode.size() <= 0 || p_bytecode.size() > MAX_LUAU_BYTECODE) {
		return wrap_raw(p_bytecode);
	}

	Vector<uint8_t> out;
	out.resize(8 + compressed_size);
	memcpy(out.ptrw(), MAGIC_ZSTD, 4);
	write_u32(out.ptrw() + 4, (uint32_t)p_bytecode.size());
	memcpy(out.ptrw() + 8, compressed.ptr(), compressed_size);
	return out;
}

Vector<uint8_t> LuauBytecodeFormat::wrap_for_write(const PackedByteArray &p_bytecode, const LuauBytecodeWriteOptions &p_opts) {
	Vector<uint8_t> wrapped = p_opts.compress ? wrap_compressed(p_bytecode) : wrap_raw(p_bytecode);
	if (p_opts.encrypt) {
		wrapped = encrypt_data(wrapped);
	}
	return wrapped;
}

PackedByteArray LuauBytecodeFormat::unwrap(const Vector<uint8_t> &p_file_data, bool *r_was_compressed) {
	PackedByteArray result;
	if (p_file_data.size() < 4) {
		result.resize(p_file_data.size());
		memcpy(result.ptrw(), p_file_data.ptr(), p_file_data.size());
		return result;
	}

	const char *magic = reinterpret_cast<const char *>(p_file_data.ptr());
	const int payload_size = p_file_data.size() - 4;
	const uint8_t *payload = p_file_data.ptr() + 4;

	if (memcmp(magic, MAGIC_RAW, 4) == 0) {
		if (r_was_compressed) {
			*r_was_compressed = false;
		}
		result.resize(payload_size);
		memcpy(result.ptrw(), payload, payload_size);
		return result;
	}

	if (memcmp(magic, MAGIC_ZSTD, 4) == 0) {
		if (r_was_compressed) {
			*r_was_compressed = true;
		}
		if (payload_size < 4) {
			result.clear();
			return result;
		}
		const uint32_t raw_size = read_u32(payload);
		if (raw_size == 0 || raw_size > (uint32_t)MAX_LUAU_BYTECODE) {
			result.clear();
			return result;
		}
		result.resize((int)raw_size);
		const int decompressed_size = Compression::decompress(result.ptrw(), (int)raw_size, payload + 4, payload_size - 4, Compression::MODE_ZSTD);
		if (decompressed_size != (int)raw_size) {
			result.clear();
		}
		return result;
	}

	if (r_was_compressed) {
		*r_was_compressed = false;
	}
	result.resize(p_file_data.size());
	memcpy(result.ptrw(), p_file_data.ptr(), p_file_data.size());
	return result;
}
