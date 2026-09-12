/**************************************************************************/
/*  test_justamcp_resource_read_caps.cpp                                  */
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

#include "test_justamcp_resource_read_caps.h"

#ifdef TOOLS_ENABLED

#include "../justamcp_read_limits.h"
#include "../tools/justamcp_scene_tools.h"
#include "core/io/dir_access.h"
#include "core/io/file_access.h"
#include "core/os/os.h"
#include "tests/test_macros.h"

void test_justamcp_scene_file_read_cap_rejects_large_file() {
	const String temp_dir = OS::get_singleton()->get_cache_path().path_join("justamcp_read_cap_test");
	Ref<DirAccess> dir = DirAccess::open(temp_dir);
	if (dir.is_null()) {
		Ref<DirAccess> make_dir = DirAccess::create(DirAccess::ACCESS_FILESYSTEM);
		if (make_dir.is_valid()) {
			make_dir->make_dir_recursive(temp_dir);
		}
	}
	const String scene_path = temp_dir.path_join("oversize.tscn");
	Ref<FileAccess> file = FileAccess::open(scene_path, FileAccess::WRITE);
	TEST_FAIL_COND(file.is_null(), "Failed to create oversize test scene file");
	const int oversize_bytes = JUSTAMCP_MAX_SYNC_READ_BYTES + 1024;
	PackedByteArray chunk;
	chunk.resize(4096);
	for (int i = 0; i < chunk.size(); i++) {
		chunk.write[i] = 'x';
	}
	int written = 0;
	while (written < oversize_bytes) {
		const int to_write = MIN(chunk.size(), oversize_bytes - written);
		file->store_buffer(chunk.ptr(), to_write);
		written += to_write;
	}
	file->close();

	JustAMCPSceneTools tools;
	Dictionary args;
	args["path"] = scene_path;
	const Dictionary result = tools.get_scene_file_content(args);
	CHECK(!bool(result.get("ok", true)));
	CHECK(String(result.get("error", "")).contains("read limit"));

	DirAccess::remove_absolute(scene_path);
}

#else
void test_justamcp_scene_file_read_cap_rejects_large_file() {
	TEST_FAIL_COND(true, "TOOLS_ENABLED is required for resource read cap test");
}
#endif

#endif
