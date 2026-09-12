/**************************************************************************/
/*  autowork_vscode_debugger.cpp                                          */
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

#include "autowork_vscode_debugger.h"
#include "modules/autowork/autowork_main.h"

#ifdef DEBUG_ENABLED
#include "core/debugger/engine_debugger.h"

Error AutoworkVSCodeDebugger::_capture(void *p_user, const String &p_msg, const Array &p_args, bool &r_captured) {
	AutoworkVSCodeDebugger *debugger = (AutoworkVSCodeDebugger *)p_user;
	if (p_msg.begins_with("dap:run_test") && debugger->runner) {
		// Similar to gut_vscode_debugger
		Autowork *main_runner = Object::cast_to<Autowork>(debugger->runner);
		if (main_runner) {
			String script = p_args.size() > 0 ? p_args[0] : "";
			String test = p_args.size() > 1 ? p_args[1] : "";
			if (!script.is_empty()) {
				main_runner->add_script(script);
			}
			if (!test.is_empty()) {
				main_runner->set_test(test);
			}
			main_runner->run_tests();
		}
		r_captured = true;
		return OK;
	}
	r_captured = false;
	return OK;
}
#endif

void AutoworkVSCodeDebugger::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_runner", "runner"), &AutoworkVSCodeDebugger::set_runner);
	ClassDB::bind_method(D_METHOD("setup"), &AutoworkVSCodeDebugger::setup);
}

AutoworkVSCodeDebugger::AutoworkVSCodeDebugger() {}
AutoworkVSCodeDebugger::~AutoworkVSCodeDebugger() {}

void AutoworkVSCodeDebugger::set_runner(Object *p_runner) {
	runner = p_runner;
}

void AutoworkVSCodeDebugger::setup() {
#ifdef DEBUG_ENABLED
	EngineDebugger::register_message_capture("dap", EngineDebugger::Capture(this, _capture));
#endif
}
