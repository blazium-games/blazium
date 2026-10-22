/**************************************************************************/
/*  test_http_server.h                                                    */
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

#include "modules/httpserver/http_server.h"
#include "tests/test_macros.h"

#include <cstring>

TEST_CASE("[HTTPServer] too many header bytes are rejected") {
	CHECK(HTTPServer::accept_another_client(0));
	CHECK_FALSE(HTTPServer::accept_another_client(HTTPServer::MAX_CONCURRENT_CLIENTS));

	const char *ok = "GET / HTTP/1.1\r\nHost: localhost\r\n\r\n";
	const int ok_len = (int)strlen(ok);
	CHECK(HTTPServer::find_header_terminator((const uint8_t *)ok, ok_len, 3, HTTPServer::MAX_HEADER_BYTES) == ok_len);

	Vector<uint8_t> huge;
	huge.resize(HTTPServer::MAX_HEADER_BYTES + 32);
	for (int i = 0; i < huge.size(); i++) {
		huge.write[i] = 'A';
	}
	CHECK(HTTPServer::find_header_terminator(huge.ptr(), huge.size(), 3, HTTPServer::MAX_HEADER_BYTES) == -2);

	// A later chunk must not rescan from the start: the terminator sits in the overlap.
	Vector<uint8_t> split;
	split.resize(ok_len);
	memcpy(split.ptrw(), ok, ok_len);
	const int from = MAX(3, ok_len - 3);
	CHECK(HTTPServer::find_header_terminator(split.ptr(), split.size(), from, HTTPServer::MAX_HEADER_BYTES) == ok_len);
}
