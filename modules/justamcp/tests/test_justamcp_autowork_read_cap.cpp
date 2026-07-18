/**************************************************************************/
/*  test_justamcp_autowork_read_cap.cpp                                   */
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

#include "test_justamcp_autowork_read_cap.h"

#ifdef TESTS_ENABLED

#ifdef TOOLS_ENABLED

#include "../justamcp_read_limits.h"
#include "../tools/resources/justamcp_resource_autowork_results.h"

#include "core/io/dir_access.h"
#include "core/io/file_access.h"
#include "core/os/os.h"
#include "tests/test_macros.h"

void test_justamcp_autowork_read_cap() {
	const String user_dir = OS::get_singleton()->get_user_data_dir();
	DirAccess::make_dir_recursive_absolute(user_dir);
	const String path = user_dir.path_join("autowork_results.json");
	Ref<FileAccess> f = FileAccess::open(path, FileAccess::WRITE);
	REQUIRE(f.is_valid());
	const int oversize = JUSTAMCP_MAX_SYNC_READ_BYTES + 64;
	PackedByteArray chunk;
	chunk.resize(4096);
	for (int i = 0; i < 4096; i++) {
		chunk.write[i] = 'B';
	}
	int written = 0;
	while (written < oversize) {
		const int n = MIN(4096, oversize - written);
		f->store_buffer(chunk.ptr(), n);
		written += n;
	}
	f->close();

	JustAMCPResourceAutoworkResults provider;
	Dictionary result = provider.read_resource(provider.get_uri());
	const bool ok = bool(result.get("ok", true));
	const bool hit_limit = result.has("max_bytes") || String(result.get("error", "")).contains("read limit");
	CHECK(!ok);
	CHECK(hit_limit);

	DirAccess::remove_absolute(path);
}

#else
void test_justamcp_autowork_read_cap() {
	TEST_FAIL_COND(true, "TOOLS_ENABLED is required");
}
#endif

#endif
