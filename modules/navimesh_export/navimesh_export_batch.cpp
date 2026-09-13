/**************************************************************************/
/*  navimesh_export_batch.cpp                                             */
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

#ifdef TOOLS_ENABLED

#include "navimesh_export_batch.h"

#include "navimesh_export_cmdline.h"
#include "navimesh_exporter.h"

#include "core/os/os.h"
#include "scene/main/scene_tree.h"

int NavimeshExportBatch::run_and_quit() {
	const NavimeshExportCliOptions options = NavimeshExportCmdline::parse_os_args();
	NavimeshExporter *exporter = NavimeshExporter::get_singleton();
	if (!exporter) {
		if (OS::get_singleton()) {
			OS::get_singleton()->set_exit_code(EXIT_FAILURE);
		}
		if (SceneTree::get_singleton()) {
			SceneTree::get_singleton()->quit(EXIT_FAILURE);
		}
		return EXIT_FAILURE;
	}

	const Dictionary result = exporter->export_project(options.scenes, options.output, options.mode, options.format, options.dimension);
	const Array exported = result.get("exported", Array());
	const Array skipped = result.get("skipped", Array());
	print_line(vformat("Navimesh export finished: %d exported, %d skipped.", exported.size(), skipped.size()));
	const PackedStringArray paths = result.get("paths", PackedStringArray());
	for (int i = 0; i < paths.size(); i++) {
		print_line(vformat("  wrote %s", paths[i]));
	}

	const int code = bool(result.get("ok", false)) ? EXIT_SUCCESS : EXIT_FAILURE;
	if (OS::get_singleton()) {
		OS::get_singleton()->set_exit_code(code);
	}
	if (SceneTree::get_singleton()) {
		SceneTree::get_singleton()->quit(code);
	}
	return code;
}

#endif
