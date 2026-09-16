/**************************************************************************/
/*  claim.cpp                                                             */
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

#include "claim.h"

void ObfuscationClaim::_bind_methods() {
	ClassDB::bind_method(D_METHOD("get_magic"), &ObfuscationClaim::get_magic);
	ClassDB::bind_method(D_METHOD("get_version"), &ObfuscationClaim::get_version);
	ClassDB::bind_method(D_METHOD("get_owner_id"), &ObfuscationClaim::get_owner_id);
	ClassDB::bind_method(D_METHOD("get_project_id"), &ObfuscationClaim::get_project_id);
	ClassDB::bind_method(D_METHOD("get_issued_unix"), &ObfuscationClaim::get_issued_unix);
	ClassDB::bind_method(D_METHOD("get_flags"), &ObfuscationClaim::get_flags);
	ClassDB::bind_method(D_METHOD("get_signature_valid"), &ObfuscationClaim::get_signature_valid);
	ClassDB::bind_method(D_METHOD("get_public_key"), &ObfuscationClaim::get_public_key);
	ClassDB::bind_method(D_METHOD("get_copyright_hash"), &ObfuscationClaim::get_copyright_hash);

	ADD_PROPERTY(PropertyInfo(Variant::STRING, "magic"), "", "get_magic");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "version"), "", "get_version");
	ADD_PROPERTY(PropertyInfo(Variant::PACKED_BYTE_ARRAY, "owner_id"), "", "get_owner_id");
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "project_id"), "", "get_project_id");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "issued_unix"), "", "get_issued_unix");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "flags"), "", "get_flags");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "signature_valid"), "", "get_signature_valid");
	ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "public_key", PROPERTY_HINT_RESOURCE_TYPE, "CryptoKey"), "", "get_public_key");
	ADD_PROPERTY(PropertyInfo(Variant::PACKED_BYTE_ARRAY, "copyright_hash"), "", "get_copyright_hash");
}
