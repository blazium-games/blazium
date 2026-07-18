/**************************************************************************/
/*  test_justamcp_origin_host_strict.cpp                                  */
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

#include "test_justamcp_origin_host_strict.h"

#ifdef TESTS_ENABLED

#include "../justamcp_session_manager.h"
#include "tests/test_macros.h"

void test_justamcp_origin_host_strict() {
	CHECK(MCPSessionManager::is_allowed_origin_string("http://127.0.0.1"));
	CHECK(MCPSessionManager::is_allowed_origin_string("http://127.0.0.1:6506"));
	CHECK(MCPSessionManager::is_allowed_origin_string("http://localhost"));
	CHECK(MCPSessionManager::is_allowed_origin_string("http://localhost:8080"));
	CHECK(MCPSessionManager::is_allowed_origin_string("http://[::1]"));
	CHECK(MCPSessionManager::is_allowed_origin_string("https://127.0.0.1"));

	CHECK(!MCPSessionManager::is_allowed_origin_string("http://127.0.0.1.evil"));
	CHECK(!MCPSessionManager::is_allowed_origin_string("http://localhost.attacker"));
	CHECK(!MCPSessionManager::is_allowed_origin_string("http://evil.com"));
	CHECK(!MCPSessionManager::is_allowed_origin_string("http://127.0.0.1.evil.com"));
	CHECK(!MCPSessionManager::is_allowed_origin_string("ftp://127.0.0.1"));
	CHECK(!MCPSessionManager::is_allowed_origin_string("http://127.0.0.1/path"));
	CHECK(!MCPSessionManager::is_allowed_origin_string("http://user@127.0.0.1"));
}

#endif
