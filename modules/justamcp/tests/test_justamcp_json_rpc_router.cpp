/**************************************************************************/
/*  test_justamcp_json_rpc_router.cpp                                     */
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

#ifdef TESTS_ENABLED

#include "test_justamcp_json_rpc_router.h"
#include "../tools/justamcp_json_rpc_router.h"
#include "tests/test_macros.h"

void test_justamcp_json_rpc_router() {
	Dictionary payload;
	Dictionary params;
	params["name"] = "blazium_search_tools";
	params["arguments"] = Dictionary();
	payload["params"] = params;
	Dictionary validated = JustAMCPJsonRpcRouter::route("tools/call", payload, 1, nullptr, nullptr);
	CHECK(validated.get("handled", false));
	CHECK(String(validated.get("tool_name", "")) == "blazium_search_tools");

	Dictionary bad_payload;
	Dictionary invalid = JustAMCPJsonRpcRouter::route("tools/call", bad_payload, 2, nullptr, nullptr);
	CHECK(invalid.get("handled", false));
	CHECK(invalid.has("error"));

	Dictionary list_result = JustAMCPJsonRpcRouter::route_tools_list("", 3);
	const bool list_has_payload = list_result.has("result") || list_result.has("error");
	CHECK(list_has_payload);

	Dictionary cursor_payload;
	Dictionary cursor_params;
	cursor_params["cursor"] = "abc";
	cursor_payload["params"] = cursor_params;
	CHECK(JustAMCPJsonRpcRouter::extract_list_cursor(cursor_payload) == "abc");

	Dictionary prompts_list = JustAMCPJsonRpcRouter::route_prompts_list("", 4, nullptr);
	const bool prompts_has_payload = prompts_list.has("result") || prompts_list.has("error");
	CHECK(prompts_has_payload);

	Dictionary bad_prompt_payload;
	Dictionary bad_prompt = JustAMCPJsonRpcRouter::route_prompts_get(bad_prompt_payload, 5, nullptr);
	CHECK(bad_prompt.has("error"));

	Dictionary bad_task_payload;
	Dictionary bad_task = JustAMCPJsonRpcRouter::route("tasks/result", bad_task_payload, 6, nullptr, nullptr);
	CHECK(bad_task.get("handled", false));
	CHECK(bad_task.has("error"));
}

#endif
