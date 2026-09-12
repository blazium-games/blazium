/**************************************************************************/
/*  test_justamcp_asset_tags_tools.cpp                                    */
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

#include "test_justamcp_asset_tags_tools.h"
#include "../tools/justamcp_asset_tags_tools.h"
#include "tests/test_macros.h"

void test_justamcp_asset_tags_elicitation() {
	JustAMCPAssetTagsTools tools;

	Dictionary update_args;
	update_args["tag_name"] = "Test.Tag";
	update_args["comment"] = "note";
	Dictionary update_pending = tools.tags_update_comment(update_args);
	CHECK(update_pending.get("elicitation_required", false));

	update_args["confirmed"] = true;
	Dictionary update_confirmed = tools.tags_update_comment(update_args);
	CHECK(!update_confirmed.get("elicitation_required", false));

	Dictionary add_args;
	add_args["path"] = "res://test.tscn";
	Array tags;
	tags.push_back("Test.Tag");
	add_args["tags"] = tags;
	Dictionary add_pending = tools.tags_add_to_asset(add_args);
	CHECK(add_pending.get("elicitation_required", false));

	add_args["confirmed"] = true;
	Dictionary add_confirmed = tools.tags_add_to_asset(add_args);
	CHECK(!add_confirmed.get("elicitation_required", false));

	Dictionary remove_args;
	remove_args["path"] = "res://test.tscn";
	remove_args["tags"] = tags;
	Dictionary remove_pending = tools.tags_remove_from_asset(remove_args);
	CHECK(remove_pending.get("elicitation_required", false));

	remove_args["confirmed"] = true;
	Dictionary remove_confirmed = tools.tags_remove_from_asset(remove_args);
	CHECK(!remove_confirmed.get("elicitation_required", false));

	Dictionary batch_args;
	Array assignments;
	Dictionary item;
	item["path"] = "res://test.tscn";
	item["tags"] = tags;
	assignments.push_back(item);
	batch_args["assignments"] = assignments;
	Dictionary batch_pending = tools.tags_batch_set_on_assets(batch_args);
	CHECK(batch_pending.get("elicitation_required", false));

	batch_args["confirmed"] = true;
	Dictionary batch_confirmed = tools.tags_batch_set_on_assets(batch_args);
	CHECK(!batch_confirmed.get("elicitation_required", false));
}

#endif
