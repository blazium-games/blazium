/**************************************************************************/
/*  test_justamcp_cors_non_localhost_policy.cpp                           */
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

#include "test_justamcp_cors_non_localhost_policy.h"

#ifdef TESTS_ENABLED

#if defined(MODULE_HTTPSERVER_ENABLED)

#include "../justamcp_cors_policy.h"
#include "tests/test_macros.h"

void test_justamcp_cors_non_localhost_policy() {
	CHECK(justamcp_compute_startup_cors_origin(true, false, String()) == "http://127.0.0.1");
	CHECK(justamcp_compute_startup_cors_origin(false, false, String()).is_empty());
	CHECK(justamcp_compute_startup_cors_origin(false, true, String()) == "*");
	CHECK(justamcp_compute_startup_cors_origin(false, false, "https://example.com") == "https://example.com");
}

#else
void test_justamcp_cors_non_localhost_policy() {
	TEST_FAIL_COND(true, "MODULE_HTTPSERVER_ENABLED is required");
}
#endif

#endif
