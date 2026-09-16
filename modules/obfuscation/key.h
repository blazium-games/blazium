/**************************************************************************/
/*  key.h                                                                 */
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

class ObfuscationKey : public Resource {
	GDCLASS(ObfuscationKey, Resource);

	Ref<CryptoKey> key;
	PackedByteArray hmac_secret;
	String project_id;
	String publisher_id;

protected:
	static void _bind_methods();

public:
	void set_key(const Ref<CryptoKey> &p_key) { key = p_key; }
	Ref<CryptoKey> get_public_key() const { return key; }
	bool has_private() const { return key.is_valid() && !key->is_public_only(); }
	void set_hmac_secret(const PackedByteArray &p_s) { hmac_secret = p_s; }
	PackedByteArray get_hmac_secret() const { return hmac_secret; }
	void set_project_id(const String &p_id) { project_id = p_id; }
	String get_project_id() const { return project_id; }
	void set_publisher_id(const String &p_id) { publisher_id = p_id; }
	String get_publisher_id() const { return publisher_id; }
	Ref<CryptoKey> get_crypto_key() const { return key; }

	virtual void set_path(const String &p_path, bool p_take_over = false) override {
		if (p_path.begins_with("res://") && has_private()) {
			ERR_FAIL_MSG("ObfuscationKey private material cannot be saved under res://.");
		}
		Resource::set_path(p_path, p_take_over);
	}
};
