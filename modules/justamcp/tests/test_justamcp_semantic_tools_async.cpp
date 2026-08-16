/**************************************************************************/
/*  test_justamcp_semantic_tools_async.cpp                                */
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

#ifdef TOOLS_ENABLED

#include "test_justamcp_semantic_tools_async.h"

#include "../tools/justamcp_semantic_search_tools.h"

#include "modules/modules_enabled.gen.h"

#ifdef MODULE_SEMANTICSEARCH_ENABLED
#include "modules/semanticsearch/semantic_asset_index.h"
#include "modules/semanticsearch/semantic_async_search_worker.h"
#endif

#include "core/io/file_access.h"
#include "tests/test_macros.h"

void test_justamcp_semantic_search_enqueue_poll() {
#ifdef MODULE_SEMANTICSEARCH_ENABLED
	SemanticAsyncSearchWorker *worker = SemanticAsyncSearchWorker::get_singleton();
	CHECK(worker != nullptr);
	SemanticAssetIndex *index = SemanticAssetIndex::get_singleton();
	CHECK(index != nullptr);
	const String test_path = "res://async_search.tscn";
	Ref<FileAccess> async_file = FileAccess::open(test_path, FileAccess::WRITE);
	CHECK(async_file.is_valid());
	async_file.unref();
	CHECK(index->upsert_entry(test_path) == OK);
	JustAMCPSemanticSearchTools tools;
	Dictionary args;
	args["query"] = "async";
	args["limit"] = 5;
	Dictionary enqueue = tools.execute_tool("semantic_search_enqueue", args);
	CHECK(enqueue.get("ok", false));
	const String job_id = String(enqueue.get("job_id", ""));
	CHECK(!job_id.is_empty());
	Dictionary poll_args;
	poll_args["job_id"] = job_id;
	Dictionary poll = tools.execute_tool("semantic_search_poll", poll_args);
	CHECK(poll.get("ok", false));
#else
	TEST_FAIL_COND(true, "MODULE_SEMANTICSEARCH_ENABLED is required for semantic async search tests");
#endif
}

void test_justamcp_semantic_search_cancel() {
#ifdef MODULE_SEMANTICSEARCH_ENABLED
	CHECK(SemanticAsyncSearchWorker::get_singleton() != nullptr);
	JustAMCPSemanticSearchTools tools;
	Dictionary args;
	args["query"] = "cancel";
	args["limit"] = 5;
	Dictionary enqueue = tools.execute_tool("semantic_search_enqueue", args);
	CHECK(enqueue.get("ok", false));
	const String job_id = String(enqueue.get("job_id", ""));
	Dictionary cancel_args;
	cancel_args["job_id"] = job_id;
	Dictionary cancel = tools.execute_tool("semantic_search_cancel", cancel_args);
	CHECK(cancel.get("ok", false));
#else
	TEST_FAIL_COND(true, "MODULE_SEMANTICSEARCH_ENABLED is required for semantic async search tests");
#endif
}

#endif

#endif
