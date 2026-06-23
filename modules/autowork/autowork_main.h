/**************************************************************************/
/*  autowork_main.h                                                       */
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

#include "autowork_collector.h"
#include "autowork_doubler.h"
#include "autowork_logger.h"
#include "autowork_spy.h"
#include "autowork_stubber.h"
#include "autowork_test.h"
#include "scene/main/node.h"

class Autowork : public Node {
	GDCLASS(Autowork, Node);

	AutoworkTest *_get_test_instance(Dictionary script_info);
	void _on_test_over();
	void _restore_editor_scripting_if_needed();

#ifdef TOOLS_ENABLED
	bool restore_editor_scripting = false;
#endif

protected:
	static void _bind_methods();

	Ref<AutoworkCollector> collector;
	Ref<AutoworkLogger> logger;
	Ref<AutoworkDoubler> doubler;
	Ref<AutoworkSpy> spy;
	Ref<AutoworkStubber> stubber;

public:
	Autowork();
	~Autowork();

	void add_directory(const String &p_path, const String &p_prefix = "", const String &p_suffix = "");
	void add_script(const String &p_path);
	void set_test(const String &p_test_name);
	void run_tests();

	int get_test_count() { return logger.is_valid() ? logger->get_test_count() : 0; }
	int get_assert_count() { return logger.is_valid() ? (logger->get_passes() + logger->get_fails()) : 0; }
	int get_pass_count() { return logger.is_valid() ? logger->get_passes() : 0; }
	int get_fail_count() { return logger.is_valid() ? logger->get_fails() : 0; }
	int get_pending_count() { return logger.is_valid() ? logger->get_warnings() : 0; }
	int get_test_script_count() { return collector.is_valid() ? collector->get_scripts().size() : 0; }

	Ref<AutoworkCollector> get_test_collector() const { return collector; }
	Ref<AutoworkLogger> get_logger() const { return logger; }
	Ref<AutoworkDoubler> get_doubler() const { return doubler; }
	Ref<AutoworkSpy> get_spy() const { return spy; }
	Ref<AutoworkStubber> get_stubber() const { return stubber; }
};
