/**************************************************************************/
/*  claim.h                                                               */
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

#include "core/crypto/crypto.h"
#include "core/io/resource.h"

class ObfuscationClaim : public Resource {
	GDCLASS(ObfuscationClaim, Resource);

	String magic;
	int version = 0;
	PackedByteArray owner_id;
	String project_id;
	int64_t issued_unix = 0;
	int flags = 0;
	bool signature_valid = false;
	Ref<CryptoKey> public_key;
	PackedByteArray copyright_hash;

protected:
	static void _bind_methods();

public:
	void set_magic(const String &p_magic) { magic = p_magic; }
	String get_magic() const { return magic; }
	void set_version(int p_version) { version = p_version; }
	int get_version() const { return version; }
	void set_owner_id(const PackedByteArray &p_id) { owner_id = p_id; }
	PackedByteArray get_owner_id() const { return owner_id; }
	void set_project_id(const String &p_id) { project_id = p_id; }
	String get_project_id() const { return project_id; }
	void set_issued_unix(int64_t p_v) { issued_unix = p_v; }
	int64_t get_issued_unix() const { return issued_unix; }
	void set_flags(int p_flags) { flags = p_flags; }
	int get_flags() const { return flags; }
	void set_signature_valid(bool p_v) { signature_valid = p_v; }
	bool get_signature_valid() const { return signature_valid; }
	void set_public_key(const Ref<CryptoKey> &p_key) { public_key = p_key; }
	Ref<CryptoKey> get_public_key() const { return public_key; }
	void set_copyright_hash(const PackedByteArray &p_h) { copyright_hash = p_h; }
	PackedByteArray get_copyright_hash() const { return copyright_hash; }
};
