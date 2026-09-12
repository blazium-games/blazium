/**************************************************************************/
/*  luau_web_bridge.cpp                                                   */
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

#ifdef __EMSCRIPTEN__

#include "web/luau_web_bridge.h"

#include "lua_state.h"
#include "luau.h"
#include "luau_compile_result.h"

#include "core/string/ustring.h"
#include <lualib.h>

#ifdef LUAU_MODULE_ANALYSIS_ENABLED
#include "analysis/luau_typecheck.h"
#endif

#include <emscripten/emscripten.h>

using namespace luau_module;

namespace {

String g_execute_result;
String g_check_result;

static bool setup_sandboxed_state(Ref<LuaState> &r_state) {
	r_state.instantiate();
	r_state->open_libs(LuaState::LIB_BASE | LuaState::LIB_STRING | LuaState::LIB_TABLE | LuaState::LIB_MATH);
	r_state->sandbox();
	r_state->sandbox_thread();
	return r_state->is_valid();
}

} //namespace

extern "C" {

EMSCRIPTEN_KEEPALIVE const char *luau_web_execute_script(const char *p_source) {
	g_execute_result = String();

	if (!p_source) {
		g_execute_result = "source is null";
		return g_execute_result.utf8().get_data();
	}

	Ref<LuaState> state;
	if (!setup_sandboxed_state(state)) {
		g_execute_result = "failed to create Luau state";
		return g_execute_result.utf8().get_data();
	}

	const LuaState::Status status = state->do_string(String::utf8(p_source), "@web_playground");
	if (status != luau_module::LuaState::STATUS_OK) {
		if (state->get_top() > 0) {
			g_execute_result = state->to_string_inplace(-1);
			state->pop(1);
		} else {
			g_execute_result = "Luau execution failed";
		}
		return g_execute_result.utf8().get_data();
	}

	state->set_top(0);
	return nullptr;
}

EMSCRIPTEN_KEEPALIVE const char *luau_web_check_script(const char *p_source, int p_use_new_solver) {
	(void)p_use_new_solver;
	g_check_result = String();

	if (!p_source) {
		g_check_result = "source is null";
		return g_check_result.utf8().get_data();
	}

	const String source = String::utf8(p_source);
	const luau_module::LuauCompileResult compile_result = Luau::compile_with_diagnostics(source);
	if (compile_result.is_error() || compile_result.bytecode.is_empty()) {
		g_check_result = compile_result.error_message.is_empty() ? "Luau compilation failed" : compile_result.error_message;
		if (compile_result.error_line > 0) {
			g_check_result = itos(compile_result.error_line) + ": " + g_check_result;
		}
		return g_check_result.utf8().get_data();
	}

#ifdef LUAU_MODULE_ANALYSIS_ENABLED
	List<ScriptLanguage::ScriptError> errors;
	List<ScriptLanguage::Warning> warnings;
	LuauTypecheck::analyze(source, "web_playground", &errors, &warnings);

	for (const ScriptLanguage::ScriptError &err : errors) {
		if (!g_check_result.is_empty()) {
			g_check_result += "\n";
		}
		g_check_result += itos(err.line) + ": " + err.message;
	}

	for (const ScriptLanguage::Warning &warning : warnings) {
		if (!g_check_result.is_empty()) {
			g_check_result += "\n";
		}
		g_check_result += itos(warning.start_line) + ": " + warning.message;
	}
#endif

	return g_check_result.is_empty() ? nullptr : g_check_result.utf8().get_data();
}
}

#endif
