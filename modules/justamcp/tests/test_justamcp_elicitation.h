/**************************************************************************/
/*  test_justamcp_elicitation.h                                           */
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

void test_justamcp_initialize_2025_advertises_elicitation();
void test_justamcp_older_initialize_does_not_break();
void test_justamcp_roots_list_changed_updates_session();
void test_justamcp_elicitation_hold_and_decline();
void test_justamcp_url_elicitation_error_shape();
void test_justamcp_icons_and_invalid_tool_name();

TEST_CASE("[Modules][JustAMCP] 2025-11-25 initialize advertises elicitation") {
	test_justamcp_initialize_2025_advertises_elicitation();
}

TEST_CASE("[Modules][JustAMCP] older initialize does not break without elicitation") {
	test_justamcp_older_initialize_does_not_break();
}

TEST_CASE("[Modules][JustAMCP] roots list_changed updates session storage") {
	test_justamcp_roots_list_changed_updates_session();
}

TEST_CASE("[Modules][JustAMCP] elicitation holds tools/call until decline") {
	test_justamcp_elicitation_hold_and_decline();
}

TEST_CASE("[Modules][JustAMCP] URL elicitation error is -32042") {
	test_justamcp_url_elicitation_error_shape();
}

TEST_CASE("[Modules][JustAMCP] icons attach and invalid tool name is a tool error") {
	test_justamcp_icons_and_invalid_tool_name();
}
