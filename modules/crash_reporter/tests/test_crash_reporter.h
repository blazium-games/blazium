/**************************************************************************/
/*  test_crash_reporter.h                                                 */
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

#include "tests/test_macros.h"

void test_crash_reporter_multipart();
void test_crash_reporter_state_and_scan();
void test_crash_reporter_metadata_json();
void test_crash_reporter_shared_identity();
void test_crash_reporter_dump_apis();

TEST_CASE("[Modules][CrashReporter] multipart body") {
	test_crash_reporter_multipart();
}

TEST_CASE("[Modules][CrashReporter] state files and pending scan") {
	test_crash_reporter_state_and_scan();
}

TEST_CASE("[Modules][CrashReporter] metadata JSON") {
	test_crash_reporter_metadata_json();
}

TEST_CASE("[Modules][CrashReporter] shared app_id and build_id") {
	test_crash_reporter_shared_identity();
}

TEST_CASE("[Modules][CrashReporter] dump APIs") {
	test_crash_reporter_dump_apis();
}
