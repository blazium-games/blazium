/**************************************************************************/
/*  inter_dvd_toolchain.cpp                                               */
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

#include "inter_dvd_toolchain.h"

#include "core/io/file_access.h"
#include "core/io/json.h"
#include "core/os/os.h"
#include "editor/editor_settings.h"

namespace {
#ifdef WINDOWS_ENABLED
const char *k_toolchain_exe = "blazium-toolchain.exe";
#else
const char *k_toolchain_exe = "blazium-toolchain";
#endif

bool file_ok(const String &p_path) {
	return !p_path.is_empty() && FileAccess::exists(p_path);
}

String look_path(const String &p_name) {
#ifdef WINDOWS_ENABLED
	const String sep = ";";
#else
	const String sep = ":";
#endif
	const Vector<String> parts = OS::get_singleton()->get_environment("PATH").split(sep, false);
	for (int i = 0; i < parts.size(); i++) {
		const String candidate = parts[i].strip_edges().path_join(p_name);
		if (FileAccess::exists(candidate)) {
			return candidate;
		}
	}
	return String();
}

int exec_toolchain(const List<String> &p_args, String *r_pipe) {
	const String bin = InterDVDToolchain::discover_binary();
	if (bin.is_empty()) {
		if (r_pipe) {
			*r_pipe = "blazium-toolchain not found (export/inter_dvd/toolchain, BLAZIUM_TOOLCHAIN, or PATH).";
		}
		return -1;
	}
	String pipe;
	const int code = OS::get_singleton()->execute(bin, p_args, &pipe);
	if (r_pipe) {
		*r_pipe = pipe;
	}
	return code;
}

bool status_encode_ready(String *r_error) {
	List<String> args;
	args.push_back("--json");
	args.push_back("interdvd");
	args.push_back("status");
	String pipe;
	const int code = exec_toolchain(args, &pipe);
	if (code != 0) {
		if (r_error) {
			*r_error = pipe.is_empty() ? String("interdvd status failed.") : pipe;
		}
		return false;
	}
	JSON json;
	const Error err = json.parse(pipe.strip_edges());
	if (err != OK) {
		if (r_error) {
			*r_error = "interdvd status returned invalid JSON.";
		}
		return false;
	}
	const Dictionary d = json.get_data();
	return bool(d.get("encode_ready", false));
}
} //namespace

String InterDVDToolchain::discover_binary() {
	if (EditorSettings::get_singleton()) {
		const String configured = EDITOR_GET("export/inter_dvd/toolchain");
		if (file_ok(configured)) {
			return configured;
		}
	}
	const String env = OS::get_singleton()->get_environment("BLAZIUM_TOOLCHAIN");
	if (file_ok(env)) {
		return env;
	}
	return look_path(k_toolchain_exe);
}

Error InterDVDToolchain::ensure_ready(String *r_error) {
	if (discover_binary().is_empty()) {
		if (r_error) {
			*r_error = "blazium-toolchain not found. Set export/inter_dvd/toolchain, BLAZIUM_TOOLCHAIN, or put blazium-toolchain on PATH.";
		}
		return ERR_UNCONFIGURED;
	}
	if (status_encode_ready(nullptr)) {
		return OK;
	}
	List<String> args;
	args.push_back("interdvd");
	args.push_back("setup");
	String pipe;
	const int code = exec_toolchain(args, &pipe);
	if (code != 0) {
		if (r_error) {
			*r_error = pipe.is_empty() ? String("interdvd setup failed.") : pipe;
		}
		return FAILED;
	}
	if (!status_encode_ready(r_error)) {
		return ERR_UNCONFIGURED;
	}
	return OK;
}

Error InterDVDToolchain::run_tool(const String &p_name, const List<String> &p_args, String *r_pipe, int *r_code) {
	if (discover_binary().is_empty()) {
		if (r_pipe) {
			*r_pipe = "blazium-toolchain not found.";
		}
		if (r_code) {
			*r_code = 3;
		}
		return ERR_UNCONFIGURED;
	}
	List<String> args;
	args.push_back("interdvd");
	args.push_back(p_name);
	args.push_back("--");
	for (const List<String>::Element *E = p_args.front(); E; E = E->next()) {
		args.push_back(E->get());
	}
	String pipe;
	const int code = exec_toolchain(args, &pipe);
	if (r_pipe) {
		*r_pipe = pipe;
	}
	if (r_code) {
		*r_code = code;
	}
	return code == 0 ? OK : FAILED;
}

Error InterDVDToolchain::run_ffmpeg(const List<String> &p_args, String *r_pipe, int *r_code) {
	return run_tool("ffmpeg", p_args, r_pipe, r_code);
}

Error InterDVDToolchain::run_ffprobe(const List<String> &p_args, String *r_pipe, int *r_code) {
	return run_tool("ffprobe", p_args, r_pipe, r_code);
}

Vector<String> InterDVDToolchain::iso_args(const String &p_work_dir, const String &p_iso_path, const String &p_meta_path) {
	Vector<String> args;
	args.push_back("interdvd");
	args.push_back("iso");
	args.push_back("--dir");
	args.push_back(p_work_dir);
	args.push_back("--out");
	args.push_back(p_iso_path);
	if (!p_meta_path.is_empty()) {
		args.push_back("--meta");
		args.push_back(p_meta_path);
		args.push_back("--write-meta");
		args.push_back(p_meta_path);
	}
	args.push_back("--json");
	return args;
}

Error InterDVDToolchain::write_iso(const String &p_work_dir, const String &p_iso_path, const String &p_meta_path, String *r_pipe) {
	const String bin = discover_binary();
	if (bin.is_empty()) {
		if (r_pipe) {
			*r_pipe = "blazium-toolchain not found.";
		}
		return ERR_UNCONFIGURED;
	}
	const Vector<String> packed = iso_args(p_work_dir, p_iso_path, p_meta_path);
	List<String> args;
	args.push_back("--json");
	for (int i = 0; i < packed.size(); i++) {
		if (packed[i] == "--json") {
			continue;
		}
		args.push_back(packed[i]);
	}
	String pipe;
	const int code = OS::get_singleton()->execute(bin, args, &pipe);
	if (r_pipe) {
		*r_pipe = pipe;
	}
	if (code != 0) {
		return FAILED;
	}
	return FileAccess::exists(p_iso_path) ? OK : FAILED;
}

#endif
