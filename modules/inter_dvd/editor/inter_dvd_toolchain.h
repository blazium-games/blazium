/**************************************************************************/
/*  inter_dvd_toolchain.h                                                 */
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

#ifdef TOOLS_ENABLED

#include "core/error/error_list.h"
#include "core/string/ustring.h"
#include "core/templates/list.h"
#include "core/templates/vector.h"

class InterDVDToolchain {
public:
	static String discover_binary();
	static Error ensure_ready(String *r_error = nullptr);
	static Error run_ffmpeg(const List<String> &p_args, String *r_pipe = nullptr, int *r_code = nullptr);
	static Error run_ffprobe(const List<String> &p_args, String *r_pipe = nullptr, int *r_code = nullptr);
	static Error write_iso(const String &p_work_dir, const String &p_iso_path, const String &p_meta_path, String *r_pipe = nullptr);
	static Vector<String> iso_args(const String &p_work_dir, const String &p_iso_path, const String &p_meta_path);

private:
	static Error run_tool(const String &p_name, const List<String> &p_args, String *r_pipe, int *r_code);
};

#endif
