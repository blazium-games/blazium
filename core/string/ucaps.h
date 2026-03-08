/**************************************************************************/
/*  ucaps.h                                                               */
/**************************************************************************/
/*                         This file is part of:                          */
/*                             GODOT ENGINE                               */
/*                        https://godotengine.org                         */
/**************************************************************************/
/* Copyright (c) 2014-present Godot Engine contributors (see AUTHORS.md). */
/* Copyright (c) 2007-2014 Juan Linietsky, Ariel Manzur.                  */
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

#include "core/string/ucaps_data.h"
#include <array>
#include <cstdint>

namespace UcapsInternal {
struct UcapsEntry {
	uint32_t key;
	uint32_t value;
};

template <uint32_t MULTIPLIER, uint32_t BITS, size_t TABLE_LEN, size_t DATA_LEN>
constexpr std::array<UcapsEntry, TABLE_LEN> make_caps_hash_table(const int (&p_data_table)[DATA_LEN][2]) {
	constexpr uint32_t shift = 32 - BITS;
	std::array<UcapsEntry, TABLE_LEN> arr{};
	for (size_t i = 0; i < DATA_LEN; ++i) {
		uint32_t key = p_data_table[i][0];
		uint32_t value = p_data_table[i][1];
		size_t idx = ((key * MULTIPLIER) & 0xFFFFFFFF) >> shift;
		arr[idx] = { key, value };
	}
	return arr;
}

constexpr auto caps_table_lookup = make_caps_hash_table<CAPS_MULTIPLIER, BITS, TABLE_LEN, LTU_LEN>(caps_table);

constexpr auto reverse_caps_table_lookup = make_caps_hash_table<REVERSE_CAPS_MULTIPLIER, BITS, TABLE_LEN, UTL_LEN>(reverse_caps_table);

static constexpr int hash(const uint32_t p_ch, const uint32_t p_multiplier) {
	constexpr uint32_t shift = 32 - BITS;
	const uint32_t idx = ((p_ch * p_multiplier) & 0xFFFFFFFF) >> shift;
	return idx;
}

static constexpr int find_upper(const uint32_t p_ch) {
	const uint32_t idx = hash(p_ch, CAPS_MULTIPLIER);
	const UcapsEntry &entry = caps_table_lookup[idx];
	return (entry.key == p_ch) ? entry.value : p_ch;
}

static constexpr int find_lower(const uint32_t p_ch) {
	const uint32_t idx = hash(p_ch, REVERSE_CAPS_MULTIPLIER);
	const UcapsEntry &entry = reverse_caps_table_lookup[idx];
	return (entry.key == p_ch) ? entry.value : p_ch;
}
} // namespace UcapsInternal

static constexpr int _find_upper(const uint32_t p_ch) {
	return UcapsInternal::find_upper(p_ch);
}

static constexpr int _find_lower(const uint32_t p_ch) {
	return UcapsInternal::find_lower(p_ch);
}
