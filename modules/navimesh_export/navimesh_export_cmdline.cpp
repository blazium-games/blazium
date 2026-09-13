/**************************************************************************/
/*  navimesh_export_cmdline.cpp                                           */
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

#include "navimesh_export_cmdline.h"

#include "core/os/os.h"
#include "core/templates/list.h"

static bool _starts_with_eq(const String &p_arg, const String &p_key, String &r_value) {
	const String prefix = p_key + "=";
	if (p_arg.begins_with(prefix)) {
		r_value = p_arg.substr(prefix.length());
		return true;
	}
	return false;
}

bool NavimeshExportCmdline::has_export_cli() {
	return parse_os_args().enabled;
}

NavimeshExportCliOptions NavimeshExportCmdline::parse_os_args() {
	PackedStringArray args;
	if (OS::get_singleton()) {
		const List<String> list = OS::get_singleton()->get_cmdline_args();
		for (const List<String>::Element *E = list.front(); E; E = E->next()) {
			args.push_back(E->get());
		}
	}
	return parse(args);
}

NavimeshExportCliOptions NavimeshExportCmdline::parse(const PackedStringArray &p_args) {
	NavimeshExportCliOptions options;
	for (int i = 0; i < p_args.size(); i++) {
		const String arg = p_args[i];
		String value;
		if (arg == "--export-navmesh") {
			options.enabled = true;
		} else if (_starts_with_eq(arg, "--export-navmesh-scenes", value)) {
			options.scenes = value;
			options.enabled = true;
		} else if (arg == "--export-navmesh-scenes" && i + 1 < p_args.size()) {
			options.scenes = p_args[++i];
			options.enabled = true;
		} else if (_starts_with_eq(arg, "--export-navmesh-output", value)) {
			options.output = value;
			options.enabled = true;
		} else if (arg == "--export-navmesh-output" && i + 1 < p_args.size()) {
			options.output = p_args[++i];
			options.enabled = true;
		} else if (_starts_with_eq(arg, "--export-navmesh-mode", value)) {
			options.mode = NavimeshExporter::mode_from_string(value);
			options.enabled = true;
		} else if (arg == "--export-navmesh-mode" && i + 1 < p_args.size()) {
			options.mode = NavimeshExporter::mode_from_string(p_args[++i]);
			options.enabled = true;
		} else if (_starts_with_eq(arg, "--export-navmesh-format", value)) {
			options.format = NavimeshExporter::format_from_string(value);
			options.enabled = true;
		} else if (arg == "--export-navmesh-format" && i + 1 < p_args.size()) {
			options.format = NavimeshExporter::format_from_string(p_args[++i]);
			options.enabled = true;
		} else if (_starts_with_eq(arg, "--export-navmesh-dimension", value)) {
			options.dimension = NavimeshExporter::dimension_from_string(value);
			options.enabled = true;
		} else if (arg == "--export-navmesh-dimension" && i + 1 < p_args.size()) {
			options.dimension = NavimeshExporter::dimension_from_string(p_args[++i]);
			options.enabled = true;
		}
	}
	return options;
}

#endif
