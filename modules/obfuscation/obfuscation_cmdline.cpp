/**************************************************************************/
/*  obfuscation_cmdline.cpp                                               */
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

#include "obfuscation_cmdline.h"

#include "obfuscation.h"

bool ObfuscationCmdline::verify = false;
String ObfuscationCmdline::key_path;
String ObfuscationCmdline::target_path;
String ObfuscationCmdline::out_path;
Vector<String> ObfuscationCmdline::pending;

void ObfuscationCmdline::reset() {
	verify = false;
	key_path = String();
	target_path = String();
	out_path = String();
	pending.clear();
}

bool ObfuscationCmdline::wants_verify() {
	return verify;
}

bool ObfuscationCmdline::try_consume(const String &p_arg, const String &p_next, bool &r_consumed_next) {
	r_consumed_next = false;
	if (p_arg == "--obfuscation-verify") {
		verify = true;
		if (!p_next.is_empty() && !p_next.begins_with("-")) {
			pending.push_back(p_next);
			r_consumed_next = true;
		}
		return true;
	}
	if (p_arg.begins_with("--obfuscation-verify=")) {
		verify = true;
		pending.push_back(p_arg.get_slice("=", 1));
		return true;
	}
	if (verify && p_arg == "--out" && !p_next.is_empty()) {
		out_path = p_next;
		r_consumed_next = true;
		return true;
	}
	if (verify && p_arg.begins_with("--out=")) {
		out_path = p_arg.get_slice("=", 1);
		return true;
	}
	if (verify && !p_arg.begins_with("-")) {
		pending.push_back(p_arg);
		return true;
	}
	return false;
}

int ObfuscationCmdline::run() {
	if (pending.size() >= 1) {
		key_path = pending[0];
	}
	if (pending.size() >= 2) {
		target_path = pending[1];
	}
	return Obfuscation::run_verify_cli(key_path, target_path, out_path);
}
