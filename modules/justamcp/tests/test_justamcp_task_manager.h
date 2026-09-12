/**************************************************************************/
/*  test_justamcp_task_manager.h                                          */
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

void test_justamcp_task_manager_create_complete_cancel();
void test_justamcp_task_manager_ttl_purge();
void test_justamcp_task_manager_max_concurrent();

void test_justamcp_task_manager_result_timeout();
void test_justamcp_task_manager_result_no_wait();

TEST_CASE("[Modules][JustAMCP] task manager create complete cancel") {
	test_justamcp_task_manager_create_complete_cancel();
}

TEST_CASE("[Modules][JustAMCP] task manager ttl purge") {
	test_justamcp_task_manager_ttl_purge();
}

TEST_CASE("[Modules][JustAMCP] task manager max concurrent") {
	test_justamcp_task_manager_max_concurrent();
}

TEST_CASE("[Modules][JustAMCP] task manager result timeout") {
	test_justamcp_task_manager_result_timeout();
}

TEST_CASE("[Modules][JustAMCP] task manager result no wait") {
	test_justamcp_task_manager_result_no_wait();
}
