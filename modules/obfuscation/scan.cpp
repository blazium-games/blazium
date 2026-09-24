/**************************************************************************/
/*  scan.cpp                                                              */
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

#include "scan.h"

#include "core/object/class_db.h"

void ObfuscationScan::add_mark(const String &p_path, int p_kind, bool p_matched, int p_bits) {
	Dictionary d;
	d["path"] = p_path;
	d["kind"] = p_kind;
	d["matched"] = p_matched;
	d["bits"] = p_bits;
	marks.push_back(d);
	total++;
	if (p_matched) {
		matched++;
	}
}

void ObfuscationScan::finalize_score() {
	score = total > 0 ? (float)matched / (float)total : 0.0f;
}

void ObfuscationScan::_bind_methods() {
	ClassDB::bind_method(D_METHOD("get_seal"), &ObfuscationScan::get_seal);
	ClassDB::bind_method(D_METHOD("get_signature_valid"), &ObfuscationScan::get_signature_valid);
	ClassDB::bind_method(D_METHOD("get_marks"), &ObfuscationScan::get_marks);
	ClassDB::bind_method(D_METHOD("get_matched"), &ObfuscationScan::get_matched);
	ClassDB::bind_method(D_METHOD("get_total"), &ObfuscationScan::get_total);
	ClassDB::bind_method(D_METHOD("get_score"), &ObfuscationScan::get_score);
	ClassDB::bind_method(D_METHOD("get_owner_id"), &ObfuscationScan::get_owner_id);
	ClassDB::bind_method(D_METHOD("get_project_id"), &ObfuscationScan::get_project_id);
	ClassDB::bind_method(D_METHOD("get_reconstructed_copyright"), &ObfuscationScan::get_reconstructed_copyright);

	ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "seal", PROPERTY_HINT_RESOURCE_TYPE, "ObfuscationClaim"), "", "get_seal");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "signature_valid"), "", "get_signature_valid");
	ADD_PROPERTY(PropertyInfo(Variant::ARRAY, "marks"), "", "get_marks");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "matched"), "", "get_matched");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "total"), "", "get_total");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "score"), "", "get_score");
	ADD_PROPERTY(PropertyInfo(Variant::PACKED_BYTE_ARRAY, "owner_id"), "", "get_owner_id");
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "project_id"), "", "get_project_id");
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "reconstructed_copyright"), "", "get_reconstructed_copyright");
}
