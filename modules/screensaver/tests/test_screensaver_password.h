/**************************************************************************/
/*  test_screensaver_password.h                                           */
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

#include "modules/screensaver/screensaver_password.h"
#include "tests/test_macros.h"

TEST_CASE("[Modules][Screensaver] hash verify set clear") {
	ScreensaverPassword::reset_for_tests();
	CHECK_FALSE(ScreensaverPassword::has_password());

	const String salt = ScreensaverPassword::generate_salt();
	const String hash = ScreensaverPassword::hash_password("secret", salt);
	CHECK_FALSE(hash.is_empty());
	CHECK(hash != "secret");
	CHECK(ScreensaverPassword::hash_password("secret", salt) == hash);
	CHECK(ScreensaverPassword::hash_password("other", salt) != hash);

	CHECK(ScreensaverPassword::set_password("", "secret") == OK);
	CHECK(ScreensaverPassword::has_password());
	CHECK(ScreensaverPassword::verify("secret"));
	CHECK_FALSE(ScreensaverPassword::verify("wrong"));
	CHECK_FALSE(ScreensaverPassword::get_stored_hash_for_tests().contains("secret"));
	CHECK_FALSE(ScreensaverPassword::get_stored_salt_for_tests().contains("secret"));

	CHECK(ScreensaverPassword::set_password("wrong", "next") == ERR_INVALID_PARAMETER);
	CHECK(ScreensaverPassword::verify("secret"));

	CHECK(ScreensaverPassword::set_password("secret", "next") == OK);
	CHECK(ScreensaverPassword::verify("next"));

	CHECK(ScreensaverPassword::clear_password("wrong") == ERR_INVALID_PARAMETER);
	CHECK(ScreensaverPassword::clear_password("next") == OK);
	CHECK_FALSE(ScreensaverPassword::has_password());
}

TEST_CASE("[Modules][Screensaver] password_enabled persist in test store") {
	ScreensaverPassword::reset_for_tests();
	CHECK_FALSE(ScreensaverPassword::has_password_enabled_override());
	CHECK_FALSE(ScreensaverPassword::has_enabled_override_for_tests());

	CHECK(ScreensaverPassword::set_password_enabled(true) == OK);
	CHECK(ScreensaverPassword::has_password_enabled_override());
	CHECK(ScreensaverPassword::get_password_enabled());
	CHECK(ScreensaverPassword::get_enabled_for_tests());

	CHECK(ScreensaverPassword::set_password_enabled(false) == OK);
	CHECK(ScreensaverPassword::has_password_enabled_override());
	CHECK_FALSE(ScreensaverPassword::get_password_enabled());
	CHECK_FALSE(ScreensaverPassword::get_enabled_for_tests());

	ScreensaverPassword::set_enabled_for_tests(true);
	CHECK(ScreensaverPassword::get_password_enabled());

	CHECK(ScreensaverPassword::set_password("", "secret") == OK);
	CHECK(ScreensaverPassword::has_password());
	CHECK(ScreensaverPassword::get_password_enabled());
	CHECK(ScreensaverPassword::clear_password("secret") == OK);
	CHECK_FALSE(ScreensaverPassword::has_password());
	CHECK(ScreensaverPassword::has_password_enabled_override());
	CHECK(ScreensaverPassword::get_password_enabled());
}
