/**************************************************************************/
/*  autowork_logger.h                                                     */
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

#include "core/object/ref_counted.h"

struct AutoworkTestMethodResult {
	StringName script_name;
	StringName method_name;
	int passes = 0;
	int fails = 0;
	int orphans = 0;
	Vector<String> fail_messages;
};

class AutoworkLogger : public RefCounted {
	GDCLASS(AutoworkLogger, RefCounted);

protected:
	static void _bind_methods();

	int passes = 0;
	int fails = 0;
	int warnings = 0;
	int orphans = 0;
	int test_count = 0;
	bool hide_orphans = false;

	Object *output_ui = nullptr;

	AutoworkTestMethodResult current_test;
	Vector<AutoworkTestMethodResult> test_results;

	String _xml_indent(int p_level);

public:
	AutoworkLogger();
	~AutoworkLogger();

	void set_output_ui(Object *p_ui);
	void print_log(const String &p_msg, bool p_error = false);

	void begin_test(const StringName &p_script, const StringName &p_method);
	void end_test();

	void add_pass(const String &p_message);
	void add_fail(const String &p_message);
	void add_warning(const String &p_message);
	void inc_orphans(int p_count);
	void inc_test_count();

	int get_passes() const { return passes; }
	int get_fails() const { return fails; }
	int get_warnings() const { return warnings; }
	int get_orphans() const { return orphans; }
	int get_test_count() const { return test_count; }

	void set_hide_orphans(bool p_hide) { hide_orphans = p_hide; }

	const Vector<AutoworkTestMethodResult> &get_test_results() const { return test_results; }

	void print_summary();

	bool export_json(const String &p_file_path);
	bool export_xml(const String &p_file_path);
};
