/**************************************************************************/
/*  test_justamcp_analysis_read_cap.cpp                                   */
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

#include "test_justamcp_analysis_read_cap.h"

#ifdef TESTS_ENABLED

#ifdef TOOLS_ENABLED

#include "../justamcp_read_limits.h"
#include "../tools/justamcp_analysis_tools.h"

#include "core/config/project_settings.h"
#include "core/io/dir_access.h"
#include "core/io/file_access.h"
#include "tests/test_macros.h"

void test_justamcp_analysis_read_cap() {
	const String path = "res://justamcp_oversize_analysis.gd";
	Ref<FileAccess> f = FileAccess::open(path, FileAccess::WRITE);
	CHECK(f.is_valid());
	const int oversize = JUSTAMCP_MAX_SYNC_READ_BYTES + 64;
	PackedByteArray chunk;
	chunk.resize(4096);
	for (int i = 0; i < 4096; i++) {
		chunk.write[i] = 'A';
	}
	int written = 0;
	while (written < oversize) {
		const int n = MIN(4096, oversize - written);
		f->store_buffer(chunk.ptr(), n);
		written += n;
	}
	f->close();

	String text;
	int64_t size = 0;
	Dictionary limit_err;
	const bool ok = justamcp_read_utf8_within_limit(path, JUSTAMCP_MAX_SYNC_READ_BYTES, text, size, limit_err);
	CHECK(!ok);
	CHECK(limit_err.has("max_bytes"));

	JustAMCPAnalysisTools tools;
	Dictionary args;
	args["query"] = "A";
	args["path"] = "res://";
	args["max_results"] = 50;
	Dictionary result = tools.execute_tool("find_script_references", args);
	const bool hit_limit = result.has("max_bytes") || String(result.get("error", "")).contains("read limit");
	CHECK(hit_limit);

	DirAccess::remove_absolute(ProjectSettings::get_singleton()->globalize_path(path));
}

#else
void test_justamcp_analysis_read_cap() {
	TEST_FAIL_COND(true, "TOOLS_ENABLED is required");
}
#endif

#endif
