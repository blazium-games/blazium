/**************************************************************************/
/*  string_cache.cpp                                                      */
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

#include "string_cache.h"

#include "core/os/spin_lock.h"

using namespace luau_module;

struct CacheSlot {
	StringName str_name;
	CharString char_str;
};

constexpr size_t CACHE_MASK = CACHE_SIZE - 1;
static_assert(CACHE_MASK <= INT16_MAX);

static_assert(sizeof(CacheSlot) <= 16);

static CacheSlot *cache = nullptr;
static SpinLock *cache_lock = nullptr;

void luau_module::initialize_string_cache() {
	cache = memnew_arr(CacheSlot, CACHE_SIZE);
	cache_lock = memnew(SpinLock);
}

void luau_module::uninitialize_string_cache() {
	if (cache_lock != nullptr) {
		cache_lock->lock();
	}

	if (cache != nullptr) {
		memdelete_arr(cache);
		cache = nullptr;
	}

	if (cache_lock != nullptr) {
		cache_lock->unlock();

		memdelete(cache_lock);
		cache_lock = nullptr;
	}
}

static unsigned slot_for_string_name(const StringName &p_str_name) {
	return static_cast<unsigned>(p_str_name.hash()) & CACHE_MASK;
}

int16_t luau_module::create_atom(const char *p_str, size_t p_len) {
	ERR_FAIL_COND_V_MSG(p_len == 0, -1, "Cannot create atom for empty string.");

	StringName str_name(String::utf8(p_str, p_len));

	unsigned atom = slot_for_string_name(str_name);
	bool slot_filled;

	cache_lock->lock();
	{
		StringName &slot = cache[atom].str_name;
		if (slot.is_empty()) {
			slot = std::move(str_name);
			slot_filled = true;
		} else if (slot == str_name) {
			slot_filled = true;
		} else {
			slot_filled = false;
		}
	}
	cache_lock->unlock();

	if (slot_filled) {
		return static_cast<int16_t>(atom);
	} else {
		return -1;
	}
}

StringName luau_module::string_name_for_atom(int p_atom) {
	ERR_FAIL_COND_V_MSG(p_atom >= static_cast<int>(CACHE_SIZE), StringName(), "Invalid atom index.");

	if (p_atom < 0) {
		return StringName();
	}

	cache_lock->lock();

	const StringName &str_name = cache[p_atom].str_name;
	cache_lock->unlock();

	return str_name;
}

CharString luau_module::char_string(const StringName &p_str_name) {
	ERR_FAIL_COND_V_MSG(p_str_name.is_empty(), CharString(), "Cannot cache CharString for empty StringName.");

	unsigned atom = slot_for_string_name(p_str_name);
	bool different_string_in_slot = false;
	CharString char_str;

	cache_lock->lock();
	{
		CacheSlot &slot = cache[atom];
		if (slot.str_name.is_empty()) {
			slot.str_name = p_str_name;
		} else if (slot.str_name == p_str_name) {
			char_str = slot.char_str;
		} else {
			different_string_in_slot = true;
		}
	}
	cache_lock->unlock();

	if (char_str.size() > 0) {
		return char_str;
	}

	char_str = String(p_str_name).utf8();
	if (!different_string_in_slot) {
		cache_lock->lock();
		cache[atom].char_str = char_str;
		cache_lock->unlock();
	}

	return char_str;
}
