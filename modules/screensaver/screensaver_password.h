/**************************************************************************/
/*  screensaver_password.h                                                */
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

#include "core/string/ustring.h"

class ScreensaverPassword {
public:
	static String hash_password(const String &p_plain, const String &p_salt);
	static String generate_salt();
	static bool has_password();
	static bool verify(const String &p_plain);
	static Error set_password(const String &p_old_plain, const String &p_new_plain);
	static Error clear_password(const String &p_current_plain);

	static bool has_password_enabled_override();
	static bool get_password_enabled();
	static Error set_password_enabled(bool p_enabled);

	static String storage_key();

	static void reset_for_tests();
	static void set_store_for_tests(const String &p_salt, const String &p_hash);
	static String get_stored_hash_for_tests();
	static String get_stored_salt_for_tests();
	static void set_enabled_for_tests(bool p_enabled);
	static bool has_enabled_override_for_tests();
	static bool get_enabled_for_tests();

private:
	static bool _load(String &r_salt, String &r_hash);
	static Error _save(const String &p_salt, const String &p_hash);
	static Error _clear_store();
	static bool _load_enabled(bool &r_enabled);
	static Error _save_enabled(bool p_enabled);

	static bool test_override;
	static String test_salt;
	static String test_hash;
	static bool test_enabled_override;
	static bool test_enabled;
};
