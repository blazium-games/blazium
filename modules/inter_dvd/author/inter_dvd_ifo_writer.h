/**************************************************************************/
/*  inter_dvd_ifo_writer.h                                                */
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

#include "core/error/error_list.h"
#include "core/io/resource.h"
#include "core/object/object.h"
#include "core/object/ref_counted.h"
#include "core/string/ustring.h"
#include "core/templates/vector.h"
#include "core/variant/callable.h"
#include "inter_dvd_project.h"

class InterDVDExportProgress : public RefCounted {
	GDCLASS(InterDVDExportProgress, RefCounted);

	int step = 0;
	int total = 0;
	String label;
	uint64_t started_usec = 0;
	Callable notify;

protected:
	static void _bind_methods();

public:
	void begin(int p_total);
	void report(const String &p_label);
	void set_notify(const Callable &p_notify) { notify = p_notify; }
	Callable get_notify() const { return notify; }
	int get_step() const { return step; }
	int get_total() const { return total; }
	String get_label() const { return label; }
	uint64_t get_started_usec() const { return started_usec; }
	static String format_clock(int64_t p_usec);
	String format_line() const;
};

class InterDVDIfoWriter : public Object {
	GDCLASS(InterDVDIfoWriter, Object);

protected:
	static void _bind_methods();

public:
	static constexpr int SECTOR_SIZE = 2048;

	static Error write_video_ts(const String &p_root, const Ref<InterDVDProject> &p_project, bool p_allow_dummy_vob, const String &p_ffmpeg, String *r_error = nullptr, bool p_auto_find_ffmpeg = true, const Ref<InterDVDExportProgress> &p_progress = Ref<InterDVDExportProgress>());
	static Error write_video_ts_bind(const String &p_root, const Ref<InterDVDProject> &p_project, bool p_allow_dummy_vob = false, const String &p_ffmpeg = String(), bool p_auto_find_ffmpeg = true, const Ref<InterDVDExportProgress> &p_progress = Ref<InterDVDExportProgress>());
	static Vector<String> toolchain_iso_args(const String &p_work_dir, const String &p_iso_path, const String &p_meta_path);
	static Error write_disc_meta(const String &p_path, const Ref<InterDVDProject> &p_project);
	static Error copy_extras(const String &p_work_dir, const Ref<InterDVDProject> &p_project, String *r_error = nullptr);
	static Error copy_extras_bind(const String &p_work_dir, const Ref<InterDVDProject> &p_project);
};
