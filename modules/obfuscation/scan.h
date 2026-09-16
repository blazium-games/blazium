/**************************************************************************/
/*  scan.h                                                                */
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

#include "claim.h"

class ObfuscationScan : public Resource {
	GDCLASS(ObfuscationScan, Resource);

	Ref<ObfuscationClaim> seal;
	bool signature_valid = false;
	Array marks;
	int matched = 0;
	int total = 0;
	float score = 0;
	PackedByteArray owner_id;
	String project_id;
	String reconstructed_copyright;

protected:
	static void _bind_methods();

public:
	void set_seal(const Ref<ObfuscationClaim> &p_seal) { seal = p_seal; }
	Ref<ObfuscationClaim> get_seal() const { return seal; }
	void set_signature_valid(bool p_v) { signature_valid = p_v; }
	bool get_signature_valid() const { return signature_valid; }
	void set_marks(const Array &p_marks) { marks = p_marks; }
	Array get_marks() const { return marks; }
	void set_matched(int p_v) { matched = p_v; }
	int get_matched() const { return matched; }
	void set_total(int p_v) { total = p_v; }
	int get_total() const { return total; }
	void set_score(float p_v) { score = p_v; }
	float get_score() const { return score; }
	void set_owner_id(const PackedByteArray &p_id) { owner_id = p_id; }
	PackedByteArray get_owner_id() const { return owner_id; }
	void set_project_id(const String &p_id) { project_id = p_id; }
	String get_project_id() const { return project_id; }
	void set_reconstructed_copyright(const String &p_v) { reconstructed_copyright = p_v; }
	String get_reconstructed_copyright() const { return reconstructed_copyright; }

	void add_mark(const String &p_path, int p_kind, bool p_matched, int p_bits = 0);
	void finalize_score();
};
