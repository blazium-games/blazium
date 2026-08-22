/**************************************************************************/
/*  justamcp_pagination.cpp                                               */
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

#include "justamcp_pagination.h"

#include "core/crypto/crypto_core.h"
#include "core/io/marshalls.h"
#include "tools/justamcp_settings_resolver.h"

static const uint8_t CURSOR_MAGIC_0 = 'J';
static const uint8_t CURSOR_MAGIC_1 = 'M';
static const uint8_t CURSOR_VERSION = 1;
static const int CURSOR_PAYLOAD_SIZE = 7;

int justamcp_pagination_page_size() {
	return MAX(1, JustAMCPSettingsResolver::resolve_int("blazium/justamcp/list_page_size", 50));
}

int justamcp_mcp_log_buffer_size() {
	return MAX(1, JustAMCPSettingsResolver::resolve_int("blazium/justamcp/mcp_log_buffer_size", 500));
}

String justamcp_pagination_cursor_from_uri_suffix(const String &p_suffix) {
	const String trimmed = p_suffix.strip_edges();
	if (trimmed.is_empty() || trimmed == "start") {
		return String();
	}
	return trimmed;
}

bool justamcp_pagination_decode_cursor(const String &p_cursor, int &r_offset) {
	r_offset = 0;
	const String trimmed = p_cursor.strip_edges();
	if (trimmed.is_empty() || trimmed == "start") {
		return true;
	}

	PackedByteArray raw = trimmed.to_utf8_buffer();
	if (raw.is_empty()) {
		return false;
	}

	for (int i = 0; i < raw.size(); i++) {
		if (raw[i] == (uint8_t)'-') {
			raw.write[i] = (uint8_t)'+';
		} else if (raw[i] == (uint8_t)'_') {
			raw.write[i] = (uint8_t)'/';
		}
	}

	size_t out_len = 0;
	const size_t max_len = (raw.size() * 3) / 4 + 4;
	PackedByteArray decoded;
	decoded.resize(max_len);
	const Error err = CryptoCore::b64_decode(decoded.ptrw(), max_len, &out_len, raw.ptr(), raw.size());
	if (err != OK || out_len != CURSOR_PAYLOAD_SIZE) {
		return false;
	}

	if (decoded[0] != CURSOR_MAGIC_0 || decoded[1] != CURSOR_MAGIC_1 || decoded[2] != CURSOR_VERSION) {
		return false;
	}

	const uint32_t offset = decode_uint32(&decoded[3]);
	if (offset > INT_MAX) {
		return false;
	}
	r_offset = int(offset);
	return true;
}

String justamcp_pagination_encode_cursor(int p_offset) {
	if (p_offset < 0) {
		p_offset = 0;
	}

	uint8_t payload[CURSOR_PAYLOAD_SIZE];
	payload[0] = CURSOR_MAGIC_0;
	payload[1] = CURSOR_MAGIC_1;
	payload[2] = CURSOR_VERSION;
	encode_uint32((uint32_t)p_offset, &payload[3]);
	String encoded = CryptoCore::b64_encode_str(payload, CURSOR_PAYLOAD_SIZE);

	return encoded.replace("+", "-").replace("/", "_");
}

static Dictionary _pagination_error_invalid_cursor() {
	Dictionary err;
	err["ok"] = false;
	err["error_code"] = -32602;
	err["error"] = "Invalid pagination cursor.";
	return err;
}

static Dictionary _pagination_slice_at_offset(const Array &p_page_items, int p_offset, int p_total, const String &p_result_key) {
	Dictionary result;
	result["ok"] = true;
	result[p_result_key] = p_page_items;
	if (p_offset + p_page_items.size() < p_total) {
		result["nextCursor"] = justamcp_pagination_encode_cursor(p_offset + p_page_items.size());
	}
	return result;
}

Dictionary justamcp_pagination_slice_array(const Array &p_items, const String &p_cursor, const String &p_result_key) {
	int offset = 0;
	if (!justamcp_pagination_decode_cursor(p_cursor, offset)) {
		return _pagination_error_invalid_cursor();
	}
	if (offset < 0) {
		return _pagination_error_invalid_cursor();
	}
	if (offset >= p_items.size()) {
		return _pagination_slice_at_offset(Array(), offset, p_items.size(), p_result_key);
	}

	const int page_size = justamcp_pagination_page_size();
	Array page;
	for (int i = offset; i < p_items.size() && page.size() < page_size; i++) {
		page.push_back(p_items[i]);
	}
	return _pagination_slice_at_offset(page, offset, p_items.size(), p_result_key);
}

Dictionary justamcp_pagination_slice_strings(const Vector<String> &p_items, const String &p_cursor, const String &p_result_key) {
	int offset = 0;
	if (!justamcp_pagination_decode_cursor(p_cursor, offset)) {
		return _pagination_error_invalid_cursor();
	}
	if (offset < 0) {
		return _pagination_error_invalid_cursor();
	}
	if (offset >= p_items.size()) {
		return _pagination_slice_at_offset(Array(), offset, p_items.size(), p_result_key);
	}

	const int page_size = justamcp_pagination_page_size();
	Array page;
	for (int i = offset; i < p_items.size() && page.size() < page_size; i++) {
		page.push_back(p_items[i]);
	}
	return _pagination_slice_at_offset(page, offset, p_items.size(), p_result_key);
}
