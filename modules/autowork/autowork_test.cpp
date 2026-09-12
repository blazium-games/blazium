/**************************************************************************/
/*  autowork_test.cpp                                                     */
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

#include "autowork_test.h"
#include "core/config/engine.h"
#include "core/io/dir_access.h"
#include "core/io/file_access.h"
#include "core/object/script_language.h"
#include "core/os/os.h"
#include "core/variant/variant.h"
#include "scene/main/scene_tree.h"
#include "scene/main/window.h"

void AutoworkTest::_bind_methods() {
	GDVIRTUAL_BIND(_before_all);
	GDVIRTUAL_BIND(_before_each);
	GDVIRTUAL_BIND(_after_each);
	GDVIRTUAL_BIND(_after_all);

	ClassDB::bind_method(D_METHOD("assert_true", "condition", "text"), &AutoworkTest::assert_true, DEFVAL(""));
	ClassDB::bind_method(D_METHOD("assert_false", "condition", "text"), &AutoworkTest::assert_false, DEFVAL(""));
	ClassDB::bind_method(D_METHOD("assert_eq", "got", "expected", "text"), &AutoworkTest::assert_eq, DEFVAL(""));
	ClassDB::bind_method(D_METHOD("assert_ne", "got", "expected", "text"), &AutoworkTest::assert_ne, DEFVAL(""));
	ClassDB::bind_method(D_METHOD("assert_eq_deep", "got", "expected", "text"), &AutoworkTest::assert_eq_deep, DEFVAL(""));
	ClassDB::bind_method(D_METHOD("assert_ne_deep", "got", "expected", "text"), &AutoworkTest::assert_ne_deep, DEFVAL(""));
	ClassDB::bind_method(D_METHOD("assert_gt", "got", "expected", "text"), &AutoworkTest::assert_gt, DEFVAL(""));
	ClassDB::bind_method(D_METHOD("assert_lt", "got", "expected", "text"), &AutoworkTest::assert_lt, DEFVAL(""));
	ClassDB::bind_method(D_METHOD("assert_ge", "got", "expected", "text"), &AutoworkTest::assert_ge, DEFVAL(""));
	ClassDB::bind_method(D_METHOD("assert_gte", "got", "expected", "text"), &AutoworkTest::assert_ge, DEFVAL(""));
	ClassDB::bind_method(D_METHOD("assert_le", "got", "expected", "text"), &AutoworkTest::assert_le, DEFVAL(""));
	ClassDB::bind_method(D_METHOD("assert_lte", "got", "expected", "text"), &AutoworkTest::assert_le, DEFVAL(""));
	ClassDB::bind_method(D_METHOD("assert_null", "got", "text"), &AutoworkTest::assert_null, DEFVAL(""));
	ClassDB::bind_method(D_METHOD("assert_not_null", "got", "text"), &AutoworkTest::assert_not_null, DEFVAL(""));
	ClassDB::bind_method(D_METHOD("assert_has_method", "object", "method", "text"), &AutoworkTest::assert_has_method, DEFVAL(""));
	ClassDB::bind_method(D_METHOD("assert_between", "got", "expect_low", "expect_high", "text"), &AutoworkTest::assert_between, DEFVAL(""));
	ClassDB::bind_method(D_METHOD("assert_not_between", "got", "expect_low", "expect_high", "text"), &AutoworkTest::assert_not_between, DEFVAL(""));
	ClassDB::bind_method(D_METHOD("assert_almost_eq", "got", "expected", "error_margin", "text"), &AutoworkTest::assert_almost_eq, DEFVAL(""));
	ClassDB::bind_method(D_METHOD("assert_almost_ne", "got", "expected", "error_margin", "text"), &AutoworkTest::assert_almost_ne, DEFVAL(""));
	ClassDB::bind_method(D_METHOD("assert_has", "variant", "element", "text"), &AutoworkTest::assert_has, DEFVAL(""));
	ClassDB::bind_method(D_METHOD("assert_does_not_have", "variant", "element", "text"), &AutoworkTest::assert_does_not_have, DEFVAL(""));
	ClassDB::bind_method(D_METHOD("assert_file_exists", "file_path", "text"), &AutoworkTest::assert_file_exists, DEFVAL(""));
	ClassDB::bind_method(D_METHOD("assert_file_does_not_exist", "file_path", "text"), &AutoworkTest::assert_file_does_not_exist, DEFVAL(""));
	ClassDB::bind_method(D_METHOD("assert_file_empty", "file_path", "text"), &AutoworkTest::assert_file_empty, DEFVAL(""));
	ClassDB::bind_method(D_METHOD("assert_file_not_empty", "file_path", "text"), &AutoworkTest::assert_file_not_empty, DEFVAL(""));
	ClassDB::bind_method(D_METHOD("assert_dir_exists", "dir_path", "text"), &AutoworkTest::assert_dir_exists, DEFVAL(""));
	ClassDB::bind_method(D_METHOD("assert_dir_does_not_exist", "dir_path", "text"), &AutoworkTest::assert_dir_does_not_exist, DEFVAL(""));
	ClassDB::bind_method(D_METHOD("assert_typeof", "variant", "type", "text"), &AutoworkTest::assert_typeof, DEFVAL(""));
	ClassDB::bind_method(D_METHOD("assert_not_typeof", "variant", "type", "text"), &AutoworkTest::assert_not_typeof, DEFVAL(""));
	ClassDB::bind_method(D_METHOD("assert_is", "object", "class", "text"), &AutoworkTest::assert_is, DEFVAL(""));
	ClassDB::bind_method(D_METHOD("assert_string_contains", "got", "expected", "text"), &AutoworkTest::assert_string_contains, DEFVAL(""));
	ClassDB::bind_method(D_METHOD("assert_string_starts_with", "got", "expected", "text"), &AutoworkTest::assert_string_starts_with, DEFVAL(""));
	ClassDB::bind_method(D_METHOD("assert_string_ends_with", "got", "expected", "text"), &AutoworkTest::assert_string_ends_with, DEFVAL(""));
	ClassDB::bind_method(D_METHOD("assert_freed", "object", "text"), &AutoworkTest::assert_freed, DEFVAL(""));
	ClassDB::bind_method(D_METHOD("assert_not_freed", "object", "text"), &AutoworkTest::assert_not_freed, DEFVAL(""));
	ClassDB::bind_method(D_METHOD("assert_called", "object", "method", "args", "text"), &AutoworkTest::assert_called, DEFVAL(Array()), DEFVAL(""));
	ClassDB::bind_method(D_METHOD("assert_not_called", "object", "method", "args", "text"), &AutoworkTest::assert_not_called, DEFVAL(Array()), DEFVAL(""));
	ClassDB::bind_method(D_METHOD("assert_called_count", "object", "method", "expected_count", "args", "text"), &AutoworkTest::assert_called_count, DEFVAL(Array()), DEFVAL(""));
	ClassDB::bind_method(D_METHOD("assert_same", "got", "expected", "text"), &AutoworkTest::assert_same, DEFVAL(""));
	ClassDB::bind_method(D_METHOD("assert_not_same", "got", "expected", "text"), &AutoworkTest::assert_not_same, DEFVAL(""));
	ClassDB::bind_method(D_METHOD("assert_signal_emitted", "object", "signal", "message"), &AutoworkTest::assert_signal_emitted, DEFVAL(""));
	ClassDB::bind_method(D_METHOD("assert_signal_not_emitted", "object", "signal", "message"), &AutoworkTest::assert_signal_not_emitted, DEFVAL(""));
	ClassDB::bind_method(D_METHOD("assert_signal_emit_count", "object", "signal", "expected_count", "message"), &AutoworkTest::assert_signal_emit_count, DEFVAL(""));
	ClassDB::bind_method(D_METHOD("assert_has_signal", "object", "signal", "message"), &AutoworkTest::assert_has_signal, DEFVAL(""));
	ClassDB::bind_method(D_METHOD("assert_signal_emitted_with_parameters", "object", "signal", "expected_parameters", "index", "message"), &AutoworkTest::assert_signal_emitted_with_parameters, DEFVAL(-1), DEFVAL(""));
	ClassDB::bind_method(D_METHOD("assert_property", "object", "property", "default", "set_to"), &AutoworkTest::assert_property);
	ClassDB::bind_method(D_METHOD("assert_set_property", "object", "property", "new_value", "expected", "text"), &AutoworkTest::assert_set_property, DEFVAL(""));
	ClassDB::bind_method(D_METHOD("assert_readonly_property", "object", "property", "new_value", "expected", "text"), &AutoworkTest::assert_readonly_property, DEFVAL(""));
	ClassDB::bind_method(D_METHOD("assert_property_with_backing_variable", "object", "property", "default_value", "new_value", "backed_by_name"), &AutoworkTest::assert_property_with_backing_variable, DEFVAL(""));
	ClassDB::bind_method(D_METHOD("assert_accessors", "object", "property", "default", "set_to"), &AutoworkTest::assert_accessors);
	ClassDB::bind_method(D_METHOD("assert_exports", "object", "property", "type"), &AutoworkTest::assert_exports);
	ClassDB::bind_method(D_METHOD("assert_connected", "source", "target", "signal", "method_name"), &AutoworkTest::assert_connected, DEFVAL(""));
	ClassDB::bind_method(D_METHOD("assert_not_connected", "source", "target", "signal", "method_name"), &AutoworkTest::assert_not_connected, DEFVAL(""));
	ClassDB::bind_method(D_METHOD("assert_no_new_orphans", "message"), &AutoworkTest::assert_no_new_orphans, DEFVAL(""));

	ClassDB::bind_method(D_METHOD("skip_if_engine_version_lt", "version", "check_godot"), &AutoworkTest::skip_if_engine_version_lt, DEFVAL(false));
	ClassDB::bind_method(D_METHOD("skip_if_engine_version_ne", "version", "check_godot"), &AutoworkTest::skip_if_engine_version_ne, DEFVAL(false));
	ClassDB::bind_method(D_METHOD("skip_if_godot_version_lt", "version"), &AutoworkTest::skip_if_godot_version_lt);
	ClassDB::bind_method(D_METHOD("skip_if_godot_version_ne", "version"), &AutoworkTest::skip_if_godot_version_ne);

	ClassDB::bind_method(D_METHOD("wait_seconds", "seconds", "message"), &AutoworkTest::wait_seconds, DEFVAL(""));
	ClassDB::bind_method(D_METHOD("wait_process_frames", "frames", "message"), &AutoworkTest::wait_process_frames, DEFVAL(""));
	ClassDB::bind_method(D_METHOD("wait_physics_frames", "frames", "message"), &AutoworkTest::wait_physics_frames, DEFVAL(""));
	ClassDB::bind_method(D_METHOD("wait_until", "callable", "max_wait", "message"), &AutoworkTest::wait_until, DEFVAL(""));
	ClassDB::bind_method(D_METHOD("wait_while", "callable", "max_wait", "message"), &AutoworkTest::wait_while, DEFVAL(""));
	ClassDB::bind_method(D_METHOD("wait_for_signal", "signal", "max_wait", "message"), &AutoworkTest::wait_for_signal, DEFVAL(""));

	ClassDB::bind_method(D_METHOD("get_call_count", "object", "method", "args"), &AutoworkTest::get_call_count, DEFVAL(Array()));
	ClassDB::bind_method(D_METHOD("get_call_parameters", "object", "method", "index"), &AutoworkTest::get_call_parameters, DEFVAL(-1));
	ClassDB::bind_method(D_METHOD("get_signal_emit_count", "object", "signal"), &AutoworkTest::get_signal_emit_count);
	ClassDB::bind_method(D_METHOD("get_signal_parameters", "object", "signal", "index"), &AutoworkTest::get_signal_parameters, DEFVAL(-1));

	ClassDB::bind_method(D_METHOD("set_doubler", "doubler"), &AutoworkTest::set_doubler);
	ClassDB::bind_method(D_METHOD("set_spy", "spy"), &AutoworkTest::set_spy);
	ClassDB::bind_method(D_METHOD("set_stubber", "stubber"), &AutoworkTest::set_stubber);

	ClassDB::bind_method(D_METHOD("double_resource", "path"), &AutoworkTest::double_resource);
	ClassDB::bind_method(D_METHOD("double_singleton", "name"), &AutoworkTest::double_singleton);
	ClassDB::bind_method(D_METHOD("create_double", "thing"), &AutoworkTest::double_thing);
	ClassDB::bind_method(D_METHOD("spy", "thing"), &AutoworkTest::spy_thing);
	ClassDB::bind_method(D_METHOD("stub", "object", "method"), &AutoworkTest::stub, DEFVAL(StringName()));

	ClassDB::bind_method(D_METHOD("simulate", "node", "times", "delta", "check_is_processing", "simulate_method"), &AutoworkTest::simulate, DEFVAL(false), DEFVAL(SIMULATE_BOTH));

	ClassDB::bind_method(D_METHOD("get_fail_count"), &AutoworkTest::get_fail_count);
	ClassDB::bind_method(D_METHOD("get_pass_count"), &AutoworkTest::get_pass_count);
	ClassDB::bind_method(D_METHOD("get_pending_count"), &AutoworkTest::get_pending_count);
	ClassDB::bind_method(D_METHOD("get_assert_count"), &AutoworkTest::get_assert_count);

	ClassDB::bind_method(D_METHOD("pending", "text"), &AutoworkTest::pending, DEFVAL(""));
	ClassDB::bind_method(D_METHOD("pass_test", "text"), &AutoworkTest::pass_test, DEFVAL(""));
	ClassDB::bind_method(D_METHOD("fail_test", "text"), &AutoworkTest::fail_test, DEFVAL(""));

	ClassDB::bind_method(D_METHOD("print_log", "text"), &AutoworkTest::print_log);
	ClassDB::bind_method(D_METHOD("p", "text"), &AutoworkTest::print_log);
	ClassDB::bind_method(D_METHOD("get_logger"), &AutoworkTest::get_logger);
	ClassDB::bind_method(D_METHOD("get_test_count"), &AutoworkTest::get_test_count);

	ClassDB::bind_method(D_METHOD("get_summary"), &AutoworkTest::get_summary);
	ClassDB::bind_method(D_METHOD("get_summary_text"), &AutoworkTest::get_summary_text);

	ClassDB::bind_method(D_METHOD("use_parameters", "params"), &AutoworkTest::use_parameters);
	ClassDB::bind_method(D_METHOD("has_parameters"), &AutoworkTest::has_parameters);
	ClassDB::bind_method(D_METHOD("get_parameter_count"), &AutoworkTest::get_parameter_count);
	ClassDB::bind_method(D_METHOD("get_current_parameter_index"), &AutoworkTest::get_current_parameter_index);
	ClassDB::bind_method(D_METHOD("set_current_parameter_index", "index"), &AutoworkTest::set_current_parameter_index);
	ClassDB::bind_method(D_METHOD("clear_parameters"), &AutoworkTest::clear_parameters);

	ClassDB::bind_method(D_METHOD("autofree", "object"), &AutoworkTest::autofree);
	ClassDB::bind_method(D_METHOD("autoqfree", "node"), &AutoworkTest::autoqfree);
	ClassDB::bind_method(D_METHOD("add_child_autofree", "child"), &AutoworkTest::add_child_autofree);
	ClassDB::bind_method(D_METHOD("add_child_autoqfree", "child"), &AutoworkTest::add_child_autoqfree);

	ClassDB::bind_method(D_METHOD("set_logger", "logger"), &AutoworkTest::set_logger);

	ClassDB::bind_method(D_METHOD("watch_signals", "object"), &AutoworkTest::watch_signals);
	ClassDB::bind_method(D_METHOD("watch_signal", "object", "signal"), &AutoworkTest::watch_signal);

	ADD_SIGNAL(MethodInfo("frameout"));
	ADD_SIGNAL(MethodInfo("timeout", PropertyInfo(Variant::BOOL, "return_value")));

	BIND_ENUM_CONSTANT(SIMULATE_BOTH);
	BIND_ENUM_CONSTANT(SIMULATE_PROCESS);
	BIND_ENUM_CONSTANT(SIMULATE_PHYSICS);
}

AutoworkTest::AutoworkTest() {
	signal_watcher.instantiate();
}

AutoworkTest::~AutoworkTest() {
	if (signal_watcher.is_valid()) {
		signal_watcher->clear();
	}
}

Variant AutoworkTest::use_parameters(const Array &p_params) {
	if (p_params.is_empty()) {
		return Variant();
	}
	_has_parameters = true;
	_parameters = p_params;
	if (_current_parameter_index < _parameters.size()) {
		return _parameters[_current_parameter_index];
	}
	return Variant();
}

bool AutoworkTest::has_parameters() const {
	return _has_parameters;
}

int AutoworkTest::get_parameter_count() const {
	return _parameters.size();
}

int AutoworkTest::get_current_parameter_index() const {
	return _current_parameter_index;
}

void AutoworkTest::set_current_parameter_index(int p_index) {
	_current_parameter_index = p_index;
}

void AutoworkTest::clear_parameters() {
	_has_parameters = false;
	_parameters.clear();
	_current_parameter_index = 0;
}

void AutoworkTest::autofree(Object *p_object) {
	if (p_object && !p_object->is_class("RefCounted")) {
		_to_free.push_back(p_object);
	}
}

void AutoworkTest::autoqfree(Node *p_node) {
	if (p_node) {
		_to_queue_free.push_back(p_node);
	}
}

Node *AutoworkTest::add_child_autofree(Node *p_child) {
	if (p_child) {
		add_child(p_child);
		autofree(p_child);
	}
	return p_child;
}

Node *AutoworkTest::add_child_autoqfree(Node *p_child) {
	if (p_child) {
		add_child(p_child);
		autoqfree(p_child);
	}
	return p_child;
}

void AutoworkTest::free_all() {
	for (int i = 0; i < _to_free.size(); i++) {
		if (ObjectDB::get_instance(_to_free[i]->get_instance_id())) {
			memdelete(_to_free[i]);
		}
	}
	_to_free.clear();

	for (int i = 0; i < _to_queue_free.size(); i++) {
		if (ObjectDB::get_instance(_to_queue_free[i]->get_instance_id())) {
			Node *n = _to_queue_free[i];
			if (n->get_parent()) {
				n->get_parent()->remove_child(n);
			}
			memdelete(n);
		}
	}
	_to_queue_free.clear();
}

void AutoworkTest::set_logger(Ref<AutoworkLogger> p_logger) {
	logger = p_logger;
}

bool AutoworkTest::assert_true(bool p_condition, const String &p_text) {
	if (p_condition) {
		pass_test(p_text.is_empty() ? "assert_true" : p_text);
	} else {
		fail_test(p_text.is_empty() ? "assert_true failed" : p_text);
	}
	return p_condition;
}

bool AutoworkTest::assert_false(bool p_condition, const String &p_text) {
	if (!p_condition) {
		pass_test(p_text.is_empty() ? "assert_false" : p_text);
	} else {
		fail_test(p_text.is_empty() ? "assert_false failed" : p_text);
	}
	return p_condition;
}

bool AutoworkTest::assert_eq(const Variant &p_got, const Variant &p_expected, const String &p_text) {
	bool passed = (p_got == p_expected);
	if (passed) {
		pass_test(p_text.is_empty() ? "assert_eq" : p_text);
	} else {
		fail_test(vformat("%s (Expected: %s, Got: %s)", p_text.is_empty() ? "assert_eq failed" : p_text, p_expected, p_got));
	}
	return passed;
}

bool AutoworkTest::assert_ne(const Variant &p_got, const Variant &p_expected, const String &p_text) {
	bool passed = (p_got != p_expected);
	if (passed) {
		pass_test(p_text.is_empty() ? "assert_ne" : p_text);
	} else {
		fail_test(vformat("%s (Expected not to be: %s)", p_text.is_empty() ? "assert_ne failed" : p_text, p_expected));
	}
	return passed;
}

bool AutoworkTest::assert_eq_deep(const Variant &p_got, const Variant &p_expected, const String &p_text) {
	Variant result_var;
	bool valid_eval = false;
	Variant::evaluate(Variant::OP_EQUAL, p_got, p_expected, result_var, valid_eval);
	bool passed = valid_eval && (bool)result_var;
	if (passed) {
		pass_test(p_text.is_empty() ? vformat("Deep match: %s", String(p_expected)) : p_text);
	} else {
		String base_err = p_text.is_empty() ? vformat("Deep eval fail. Expected %s, Got %s", String(p_expected), String(p_got)) : p_text;
		if (p_got.get_type() == Variant::DICTIONARY && p_expected.get_type() == Variant::DICTIONARY) {
			Dictionary d_got = p_got;
			Dictionary d_exp = p_expected;
			Array keys_exp = d_exp.keys();
			for (int i = 0; i < keys_exp.size(); i++) {
				Variant k = keys_exp[i];
				if (!d_got.has(k)) {
					base_err += vformat("\n  Missing key: %s", String(k));
				} else {
					Variant kr_var;
					bool k_valid = false;
					Variant::evaluate(Variant::OP_EQUAL, d_got[k], d_exp[k], kr_var, k_valid);
					bool kr = k_valid && (bool)kr_var;
					if (!kr) {
						base_err += vformat("\n  Diff at key %s: expected %s, got %s", String(k), String(d_exp[k]), String(d_got[k]));
					}
				}
			}
			Array keys_got = d_got.keys();
			for (int i = 0; i < keys_got.size(); i++) {
				if (!d_exp.has(keys_got[i])) {
					base_err += vformat("\n  Extra key found: %s", String(keys_got[i]));
				}
			}
		} else if (p_got.get_type() == Variant::ARRAY && p_expected.get_type() == Variant::ARRAY) {
			Array a_got = p_got;
			Array a_exp = p_expected;
			int max_s = MAX(a_got.size(), a_exp.size());
			for (int i = 0; i < max_s; i++) {
				if (i >= a_got.size()) {
					base_err += vformat("\n  Missing index %d: expected %s", i, String(a_exp[i]));
				} else if (i >= a_exp.size()) {
					base_err += vformat("\n  Extra index %d: found %s", i, String(a_got[i]));
				} else {
					Variant kr_var;
					bool k_valid = false;
					Variant::evaluate(Variant::OP_EQUAL, a_got[i], a_exp[i], kr_var, k_valid);
					bool kr = k_valid && (bool)kr_var;
					if (!kr) {
						base_err += vformat("\n  Diff at index %d: expected %s, got %s", i, String(a_exp[i]), String(a_got[i]));
					}
				}
			}
		}
		fail_test(base_err);
	}
	return passed;
}

bool AutoworkTest::assert_ne_deep(const Variant &p_got, const Variant &p_expected, const String &p_text) {
	Variant result_var;
	bool valid_eval = false;
	Variant::evaluate(Variant::OP_NOT_EQUAL, p_got, p_expected, result_var, valid_eval);
	bool passed = valid_eval && (bool)result_var;
	if (passed) {
		pass_test(p_text.is_empty() ? vformat("Deep match: %s", String(p_expected)) : p_text);
	} else {
		fail_test(p_text.is_empty() ? vformat("Deep match fail. Expected %s, Got %s", String(p_expected), String(p_got)) : p_text);
	}
	return passed;
}

bool AutoworkTest::assert_gt(const Variant &p_got, const Variant &p_expected, const String &p_text) {
	Variant ret;
	bool valid = false;
	Variant::evaluate(Variant::OP_GREATER, p_got, p_expected, ret, valid);
	bool passed = valid && (bool)ret;
	if (passed) {
		pass_test(p_text.is_empty() ? "assert_gt" : p_text);
	} else {
		fail_test(vformat("%s (Expected %s > %s)", p_text.is_empty() ? "assert_gt failed" : p_text, p_got, p_expected));
	}
	return passed;
}

bool AutoworkTest::assert_lt(const Variant &p_got, const Variant &p_expected, const String &p_text) {
	Variant ret;
	bool valid = false;
	Variant::evaluate(Variant::OP_LESS, p_got, p_expected, ret, valid);
	bool passed = valid && (bool)ret;
	if (passed) {
		pass_test(p_text.is_empty() ? "assert_lt" : p_text);
	} else {
		fail_test(vformat("%s (Expected %s < %s)", p_text.is_empty() ? "assert_lt failed" : p_text, p_got, p_expected));
	}
	return passed;
}

bool AutoworkTest::assert_ge(const Variant &p_got, const Variant &p_expected, const String &p_text) {
	Variant ret;
	bool valid = false;
	Variant::evaluate(Variant::OP_GREATER_EQUAL, p_got, p_expected, ret, valid);
	bool passed = valid && (bool)ret;
	if (passed) {
		pass_test(p_text.is_empty() ? "assert_ge" : p_text);
	} else {
		fail_test(vformat("%s (Expected %s >= %s)", p_text.is_empty() ? "assert_ge failed" : p_text, p_got, p_expected));
	}
	return passed;
}

bool AutoworkTest::assert_le(const Variant &p_got, const Variant &p_expected, const String &p_text) {
	Variant ret;
	bool valid = false;
	Variant::evaluate(Variant::OP_LESS_EQUAL, p_got, p_expected, ret, valid);
	bool passed = valid && (bool)ret;
	if (passed) {
		pass_test(p_text.is_empty() ? "assert_le" : p_text);
	} else {
		fail_test(vformat("%s (Expected %s <= %s)", p_text.is_empty() ? "assert_le failed" : p_text, p_got, p_expected));
	}
	return passed;
}

bool AutoworkTest::assert_null(const Variant &p_got, const String &p_text) {
	bool passed = p_got.get_type() == Variant::NIL || (p_got.get_type() == Variant::OBJECT && p_got.get_validated_object() == nullptr);
	if (passed) {
		pass_test(p_text.is_empty() ? "assert_null" : p_text);
	} else {
		fail_test(vformat("%s (Expected null, got %s)", p_text.is_empty() ? "assert_null failed" : p_text, p_got));
	}
	return passed;
}

bool AutoworkTest::assert_not_null(const Variant &p_got, const String &p_text) {
	bool passed = p_got.get_type() != Variant::NIL && !(p_got.get_type() == Variant::OBJECT && p_got.get_validated_object() == nullptr);
	if (passed) {
		pass_test(p_text.is_empty() ? "assert_not_null" : p_text);
	} else {
		fail_test(p_text.is_empty() ? "assert_not_null failed" : p_text);
	}
	return passed;
}

bool AutoworkTest::assert_has_method(Object *p_object, const StringName &p_method, const String &p_text) {
	ERR_FAIL_TEST(p_object == nullptr, false, "object is null", "assert_has_method");
	bool passed = p_object->has_method(p_method);
	if (passed) {
		pass_test(p_text.is_empty() ? "assert_has_method" : p_text);
	} else {
		fail_test(vformat("%s (Object doesn't have method '%s')", p_text.is_empty() ? "assert_has_method failed" : p_text, String(p_method)));
	}
	return passed;
}

void AutoworkTest::watch_signals(Object *p_object) {
	if (signal_watcher.is_valid()) {
		signal_watcher->watch_signals(p_object);
	}
}

void AutoworkTest::watch_signal(Object *p_object, const StringName &p_signal) {
	if (signal_watcher.is_valid()) {
		signal_watcher->watch_signal(p_object, p_signal);
	}
}

bool AutoworkTest::assert_signal_emitted(Object *p_object, const StringName &p_signal, const String &p_message) {
	ERR_FAIL_TEST(p_object == nullptr, false, "object is null", "assert_signal_emitted");
	bool passed = signal_watcher.is_valid() && signal_watcher->did_emit(p_object, p_signal);
	if (passed) {
		pass_test(p_message.is_empty() ? "assert_signal_emitted" : p_message);
	} else {
		fail_test(vformat("%s (Signal '%s' was not emitted)", p_message.is_empty() ? "assert_signal_emitted failed" : p_message, p_signal));
	}
	return passed;
}

bool AutoworkTest::assert_signal_not_emitted(Object *p_object, const StringName &p_signal, const String &p_message) {
	ERR_FAIL_TEST(p_object == nullptr, false, "object is null", "assert_signal_not_emitted");
	bool passed = signal_watcher.is_valid() && !signal_watcher->did_emit(p_object, p_signal);
	if (passed) {
		pass_test(p_message.is_empty() ? "assert_signal_not_emitted" : p_message);
	} else {
		fail_test(vformat("%s (Signal '%s' WAS emitted)", p_message.is_empty() ? "assert_signal_not_emitted failed" : p_message, p_signal));
	}
	return passed;
}

bool AutoworkTest::assert_between(const Variant &p_got, const Variant &p_expect_low, const Variant &p_expect_high, const String &p_text) {
	Variant ret_high, ret_low;
	bool valid = false, valid2 = false;
	Variant::evaluate(Variant::OP_LESS_EQUAL, p_got, p_expect_high, ret_high, valid);
	Variant::evaluate(Variant::OP_GREATER_EQUAL, p_got, p_expect_low, ret_low, valid2);
	bool passed = valid && valid2 && (bool)ret_high && (bool)ret_low;
	if (passed) {
		pass_test(p_text.is_empty() ? "assert_between" : p_text);
	} else {
		fail_test(vformat("%s (Expected %s to be between %s and %s)", p_text.is_empty() ? "assert_between failed" : p_text, p_got, p_expect_low, p_expect_high));
	}
	return passed;
}

bool AutoworkTest::assert_not_between(const Variant &p_got, const Variant &p_expect_low, const Variant &p_expect_high, const String &p_text) {
	Variant ret_high, ret_low;
	bool valid = false, valid2 = false;
	Variant::evaluate(Variant::OP_GREATER, p_got, p_expect_high, ret_high, valid);
	Variant::evaluate(Variant::OP_LESS, p_got, p_expect_low, ret_low, valid2);
	bool passed = (valid && (bool)ret_high) || (valid2 && (bool)ret_low);
	if (passed) {
		pass_test(p_text.is_empty() ? "assert_not_between" : p_text);
	} else {
		fail_test(vformat("%s (Expected %s NOT to be between %s and %s)", p_text.is_empty() ? "assert_not_between failed" : p_text, p_got, p_expect_low, p_expect_high));
	}
	return passed;
}

bool AutoworkTest::assert_almost_eq(const Variant &p_got, const Variant &p_expected, const Variant &p_error_margin, const String &p_text) {
	Variant::Type type = p_got.get_type();
	bool is_int_float = (type == Variant::FLOAT || type == Variant::INT);
	if (is_int_float) {
		if ((p_expected.get_type() != Variant::FLOAT && p_expected.get_type() != Variant::INT) || (p_error_margin.get_type() != Variant::FLOAT && p_error_margin.get_type() != Variant::INT)) {
			ERR_FAIL_TEST(true, false, "unsupported types", "assert_almost_eq");
		}
	} else {
		ERR_FAIL_TEST(type != p_expected.get_type() || type != p_error_margin.get_type(), false, "unsupported types", "assert_almost_eq");
	}

	bool passed = false;

	if (is_int_float) {
		float got = p_got;
		float error_margin = p_error_margin;
		float expected = p_expected;
		float lower = expected - error_margin;
		float upper = expected + error_margin;
		passed = got >= lower && got <= upper;
	} else if (type == Variant::VECTOR2) {
		Vector2 got = p_got;
		Vector2 error_margin = p_error_margin;
		Vector2 expected = p_expected;
		Vector2 lower = expected - error_margin;
		Vector2 upper = expected + error_margin;
		passed = got.clamp(lower, upper) == got;
	} else if (type == Variant::VECTOR3) {
		Vector3 got = p_got;
		Vector3 error_margin = p_error_margin;
		Vector3 expected = p_expected;
		Vector3 lower = expected - error_margin;
		Vector3 upper = expected + error_margin;
		passed = got.clamp(lower, upper) == got;
	} else if (type == Variant::VECTOR4) {
		Vector4 got = p_got;
		Vector4 error_margin = p_error_margin;
		Vector4 expected = p_expected;
		Vector4 lower = expected - error_margin;
		Vector4 upper = expected + error_margin;
		passed = got.clamp(lower, upper) == got;
	} else {
		fail_test("assert_almost_eq: unsupported types");
		return false;
	}

	if (passed) {
		pass_test(p_text.is_empty() ? "assert_almost_eq" : p_text);
	} else {
		fail_test(vformat("%s (Expected %s almost equal to %s)", p_text.is_empty() ? "assert_almost_eq failed" : p_text, p_got, p_expected));
	}
	return passed;
}

bool AutoworkTest::assert_almost_ne(const Variant &p_got, const Variant &p_expected, const Variant &p_error_margin, const String &p_text) {
	Variant::Type type = p_got.get_type();
	bool is_int_float = (type == Variant::FLOAT || type == Variant::INT);
	if (is_int_float) {
		if ((p_expected.get_type() != Variant::FLOAT && p_expected.get_type() != Variant::INT) || (p_error_margin.get_type() != Variant::FLOAT && p_error_margin.get_type() != Variant::INT)) {
			ERR_FAIL_TEST(true, false, "unsupported types", "assert_almost_ne");
		}
	} else {
		ERR_FAIL_TEST(type != p_expected.get_type() || type != p_error_margin.get_type(), false, "unsupported types", "assert_almost_ne");
	}

	bool passed = false;

	if (is_int_float) {
		float got = p_got;
		float error_margin = p_error_margin;
		float expected = p_expected;
		float lower = expected - error_margin;
		float upper = expected + error_margin;
		passed = got >= lower && got <= upper;
	} else if (type == Variant::VECTOR2) {
		Vector2 got = p_got;
		Vector2 error_margin = p_error_margin;
		Vector2 expected = p_expected;
		Vector2 lower = expected - error_margin;
		Vector2 upper = expected + error_margin;
		passed = got.clamp(lower, upper) == got;
	} else if (type == Variant::VECTOR3) {
		Vector3 got = p_got;
		Vector3 error_margin = p_error_margin;
		Vector3 expected = p_expected;
		Vector3 lower = expected - error_margin;
		Vector3 upper = expected + error_margin;
		passed = got.clamp(lower, upper) == got;
	} else if (type == Variant::VECTOR4) {
		Vector4 got = p_got;
		Vector4 error_margin = p_error_margin;
		Vector4 expected = p_expected;
		Vector4 lower = expected - error_margin;
		Vector4 upper = expected + error_margin;
		passed = got.clamp(lower, upper) == got;
	} else {
		fail_test("assert_almost_ne: unsupported types");
		return false;
	}

	if (passed) {
		fail_test(vformat("%s (Expected %s almost not equal to %s)", p_text.is_empty() ? "assert_almost_ne failed" : p_text, p_got, p_expected));
	} else {
		pass_test(p_text.is_empty() ? "assert_almost_ne" : p_text);
	}
	return passed;
}

bool AutoworkTest::assert_has(const Variant &p_variant, const Variant &p_element, const String &p_text) {
	bool passed = false;
	if (p_variant.get_type() == Variant::ARRAY) {
		Array a = p_variant;
		passed = a.has(p_element);
	} else if (p_variant.get_type() == Variant::DICTIONARY) {
		Dictionary d = p_variant;
		passed = d.has(p_element);
	} else if (p_variant.get_type() == Variant::STRING) {
		String s = p_variant;
		passed = s.contains((String)p_element);
	}

	if (passed) {
		pass_test(p_text.is_empty() ? "assert_has" : p_text);
	} else {
		fail_test(vformat("%s (Expected sequence to contain %s)", p_text.is_empty() ? "assert_has failed" : p_text, p_element));
	}
	return passed;
}

bool AutoworkTest::assert_does_not_have(const Variant &p_variant, const Variant &p_element, const String &p_text) {
	bool passed = true;
	if (p_variant.get_type() == Variant::ARRAY) {
		Array a = p_variant;
		passed = !a.has(p_element);
	} else if (p_variant.get_type() == Variant::DICTIONARY) {
		Dictionary d = p_variant;
		passed = !d.has(p_element);
	} else if (p_variant.get_type() == Variant::STRING) {
		String s = p_variant;
		passed = !s.contains((String)p_element);
	}

	if (passed) {
		pass_test(p_text.is_empty() ? "assert_does_not_have" : p_text);
	} else {
		fail_test(vformat("%s (Expected sequence to not contain %s)", p_text.is_empty() ? "assert_does_not_have failed" : p_text, p_element));
	}
	return passed;
}

bool AutoworkTest::assert_file_exists(const String &p_file_path, const String &p_text) {
	bool passed = FileAccess::exists(p_file_path);
	if (passed) {
		pass_test(p_text.is_empty() ? "assert_file_exists" : p_text);
	} else {
		fail_test(vformat("%s (File does not exist: %s)", p_text.is_empty() ? "assert_file_exists failed" : p_text, p_file_path));
	}
	return passed;
}

bool AutoworkTest::assert_file_does_not_exist(const String &p_file_path, const String &p_text) {
	bool passed = !FileAccess::exists(p_file_path);
	if (passed) {
		pass_test(p_text.is_empty() ? "assert_file_does_not_exist" : p_text);
	} else {
		fail_test(vformat("%s (File exists: %s)", p_text.is_empty() ? "assert_file_does_not_exist failed" : p_text, p_file_path));
	}
	return passed;
}

bool AutoworkTest::assert_file_empty(const String &p_file_path, const String &p_text) {
	ERR_FAIL_TEST(!FileAccess::exists(p_file_path), false, "invalid file path", "assert_file_empty");
	Ref<FileAccess> file = FileAccess::open(p_file_path, FileAccess::READ);
	bool passed = file.is_valid() && file->get_length() == 0;
	if (passed) {
		pass_test(p_text.is_empty() ? "assert_file_empty" : p_text);
	} else {
		fail_test(vformat("%s (File not empty: %s)", p_text.is_empty() ? "assert_file_empty failed" : p_text, p_file_path));
	}
	return passed;
}

bool AutoworkTest::assert_file_not_empty(const String &p_file_path, const String &p_text) {
	ERR_FAIL_TEST(!FileAccess::exists(p_file_path), false, "invalid file path", "assert_file_not_empty");
	Ref<FileAccess> file = FileAccess::open(p_file_path, FileAccess::READ);
	bool passed = file.is_valid() && file->get_length() > 0;
	if (passed) {
		pass_test(p_text.is_empty() ? "assert_file_not_empty" : p_text);
	} else {
		fail_test(vformat("%s (File empty: %s)", p_text.is_empty() ? "assert_file_not_empty failed" : p_text, p_file_path));
	}
	return passed;
}

bool AutoworkTest::assert_dir_exists(const String &p_dir_path, const String &p_text) {
	bool passed = DirAccess::exists(p_dir_path);
	if (passed) {
		pass_test(p_text.is_empty() ? "assert_dir_exists" : p_text);
	} else {
		fail_test(vformat("%s (Dir does not exist: %s)", p_text.is_empty() ? "assert_dir_exists failed" : p_text, p_dir_path));
	}
	return passed;
}

bool AutoworkTest::assert_dir_does_not_exist(const String &p_dir_path, const String &p_text) {
	bool passed = !DirAccess::exists(p_dir_path);
	if (passed) {
		pass_test(p_text.is_empty() ? "assert_dir_does_not_exist" : p_text);
	} else {
		fail_test(vformat("%s (Dir exists: %s)", p_text.is_empty() ? "assert_dir_does_not_exist failed" : p_text, p_dir_path));
	}
	return passed;
}

bool AutoworkTest::assert_typeof(const Variant &p_variant, Variant::Type p_type, const String &p_text) {
	bool passed = p_variant.get_type() == p_type;
	if (passed) {
		pass_test(p_text.is_empty() ? "assert_typeof" : p_text);
	} else {
		fail_test(vformat("%s (Expected type %s, got %s)", p_text.is_empty() ? "assert_typeof failed" : p_text, Variant::get_type_name(p_type), Variant::get_type_name(p_variant.get_type())));
	}
	return passed;
}

bool AutoworkTest::assert_not_typeof(const Variant &p_variant, Variant::Type p_type, const String &p_text) {
	bool passed = p_variant.get_type() != p_type;
	if (passed) {
		pass_test(p_text.is_empty() ? "assert_not_typeof" : p_text);
	} else {
		fail_test(vformat("%s (Expected type not to be %s)", p_text.is_empty() ? "assert_not_typeof failed" : p_text, Variant::get_type_name(p_type)));
	}
	return passed;
}

bool AutoworkTest::assert_is(Object *p_object, const String &p_class, const String &p_text) {
	ERR_FAIL_TEST(p_object == nullptr, false, "object is null", "assert_is");
	bool passed = p_object->is_class(p_class);
	if (passed) {
		pass_test(p_text.is_empty() ? "assert_is" : p_text);
	} else {
		fail_test(vformat("%s (Expected class to be %s got %s)", p_text.is_empty() ? "assert_is failed" : p_text, p_class, p_object->get_class()));
	}
	return passed;
}

bool AutoworkTest::assert_string_contains(const String &p_got, const String &p_expected, const String &p_text) {
	bool passed = p_got.contains(p_expected);
	if (passed) {
		pass_test(p_text.is_empty() ? "assert_string_contains" : p_text);
	} else {
		fail_test(vformat("%s (Expected %s to contain %s)", p_text.is_empty() ? "assert_string_contains failed" : p_text, p_got, p_expected));
	}
	return passed;
}

bool AutoworkTest::assert_string_starts_with(const String &p_got, const String &p_expected, const String &p_text) {
	bool passed = p_got.begins_with(p_expected);
	if (passed) {
		pass_test(p_text.is_empty() ? "assert_string_starts_with" : p_text);
	} else {
		fail_test(vformat("%s (Expected %s to start with %s)", p_text.is_empty() ? "assert_string_starts_with failed" : p_text, p_got, p_expected));
	}
	return passed;
}

bool AutoworkTest::assert_string_ends_with(const String &p_got, const String &p_expected, const String &p_text) {
	bool passed = p_got.ends_with(p_expected);
	if (passed) {
		pass_test(p_text.is_empty() ? "assert_string_ends_with" : p_text);
	} else {
		fail_test(vformat("%s (Expected %s to end with %s)", p_text.is_empty() ? "assert_string_ends_with failed" : p_text, p_got, p_expected));
	}
	return passed;
}

bool AutoworkTest::assert_freed(Object *p_object, const String &p_text) {
	bool passed = (p_object == nullptr);
	if (passed) {
		pass_test(p_text.is_empty() ? "assert_freed" : p_text);
	} else {
		fail_test(vformat("%s (Expected object %s to be freed)", p_text.is_empty() ? "assert_freed failed" : p_text, p_object));
	}
	return passed;
}

bool AutoworkTest::assert_not_freed(Object *p_object, const String &p_text) {
	bool passed = (p_object != nullptr);
	if (passed) {
		pass_test(p_text.is_empty() ? "assert_not_freed" : p_text);
	} else {
		fail_test(p_text.is_empty() ? "assert_not_freed failed" : p_text);
	}
	return passed;
}

bool AutoworkTest::assert_called(Object *p_object, const StringName &p_method, const Array &p_args, const String &p_text) {
	ERR_PRINT("Not implemented.");
	return false;
}

bool AutoworkTest::assert_not_called(Object *p_object, const StringName &p_method, const Array &p_args, const String &p_text) {
	ERR_PRINT("Not implemented.");
	return false;
}

bool AutoworkTest::assert_called_count(Object *p_object, const StringName &p_method, int p_expected_count, const Array &p_args, const String &p_text) {
	int calls = get_call_count(p_object, p_method, p_args);
	bool passed = calls == p_expected_count;
	if (passed) {
		pass_test(p_text.is_empty() ? "assert_called_count" : p_text);
	} else {
		fail_test(vformat("%s (Expected %s calls, got %s)", p_text.is_empty() ? "assert_called_count failed" : p_text, p_expected_count, calls));
	}
	return passed;
}

int AutoworkTest::get_call_count(Object *p_object, const StringName &p_method, const Array &p_args) {
	if (spy.is_valid()) {
		return spy->call_count(p_object, p_method, p_args);
	}
	return 0;
}

Array AutoworkTest::get_call_parameters(Object *p_object, const StringName &p_method, int p_index) {
	if (spy.is_valid()) {
		return spy->get_call_parameters(p_object, p_method, p_index);
	}
	return Array();
}

bool AutoworkTest::assert_same(const Variant &p_got, const Variant &p_expected, const String &p_text) {
	bool passed = p_got.hash() == p_expected.hash();
	if (passed) {
		pass_test(p_text.is_empty() ? "assert_same" : p_text);
	} else {
		fail_test(vformat("%s (Expected same %s, got %s)", p_text.is_empty() ? "assert_same failed" : p_text, p_expected, p_got));
	}
	return passed;
}

bool AutoworkTest::assert_not_same(const Variant &p_got, const Variant &p_expected, const String &p_text) {
	bool passed = p_got.hash() != p_expected.hash();
	if (passed) {
		pass_test(p_text.is_empty() ? "assert_not_same" : p_text);
	} else {
		fail_test(vformat("%s (Expected not same %s, got %s)", p_text.is_empty() ? "assert_not_same failed" : p_text, p_expected, p_got));
	}
	return passed;
}

void AutoworkTest::pending(const String &p_text) {
	if (logger.is_valid()) {
		logger->add_warning("Pending: " + p_text);
	}
}

void AutoworkTest::pass_test(const String &p_text) {
	if (logger.is_valid()) {
		logger->add_pass(p_text.is_empty() ? "pass_test" : p_text);
	}
}

void AutoworkTest::fail_test(const String &p_text) {
	if (logger.is_valid()) {
		logger->add_fail(p_text.is_empty() ? "fail_test" : p_text);
	}
}

bool AutoworkTest::assert_signal_emit_count(Object *p_object, const StringName &p_signal, int p_expected_count, const String &p_text) {
	ERR_FAIL_TEST(p_object == nullptr, false, "object is null", "assert_signal_emit_count");
	int count = get_signal_emit_count(p_object, p_signal);
	bool passed = count == p_expected_count;
	if (passed) {
		pass_test(p_text.is_empty() ? "assert_signal_emit_count" : p_text);
	} else {
		fail_test(vformat("%s (Expected signal '%s' to be emitted %d times, was emitted %d times)", p_text.is_empty() ? "assert_signal_emit_count failed" : p_text, p_signal, p_expected_count, count));
	}
	return passed;
}

bool AutoworkTest::assert_has_signal(Object *p_object, const StringName &p_signal, const String &p_text) {
	ERR_FAIL_TEST(p_object == nullptr, false, "object is null", "assert_has_signal");
	bool passed = false;
	List<MethodInfo> signals;
	p_object->get_signal_list(&signals);
	for (const MethodInfo &mi : signals) {
		if (mi.name == p_signal) {
			passed = true;
			break;
		}
	}
	if (passed) {
		pass_test(p_text.is_empty() ? "assert_has_signal" : p_text);
	} else {
		fail_test(vformat("%s (Object does not have signal '%s')", p_text.is_empty() ? "assert_has_signal failed" : p_text, p_signal));
	}
	return passed;
}

bool AutoworkTest::assert_signal_emitted_with_parameters(Object *p_object, const StringName &p_signal, const Array &p_expected_parameters, int p_index, const String &p_text) {
	ERR_FAIL_TEST(p_object == nullptr, false, "object is null", "assert_signal_emitted_with_parameters");
	if (!signal_watcher.is_valid() || !signal_watcher->did_emit(p_object, p_signal)) {
		fail_test(vformat("%s (Signal '%s' was not emitted)", p_text.is_empty() ? "assert_signal_emitted_with_parameters failed" : p_text, p_signal));
		return false;
	}
	Array got = signal_watcher->get_signal_parameters(p_object, p_signal, p_index);
	bool passed = got == p_expected_parameters;
	if (passed) {
		pass_test(p_text.is_empty() ? "assert_signal_emitted_with_parameters" : p_text);
	} else {
		fail_test(vformat("%s (Parameters mismatch for '%s'. Expected %s, got %s)", p_text.is_empty() ? "assert_signal_emitted_with_parameters failed" : p_text, p_signal, p_expected_parameters, got));
	}
	return passed;
}

int AutoworkTest::get_signal_emit_count(Object *p_object, const StringName &p_signal) {
	if (signal_watcher.is_valid()) {
		return signal_watcher->get_emit_count(p_object, p_signal);
	}
	return 0;
}

Array AutoworkTest::get_signal_parameters(Object *p_object, const StringName &p_signal, int p_index) {
	if (signal_watcher.is_valid()) {
		return signal_watcher->get_signal_parameters(p_object, p_signal, p_index);
	}
	return Array();
}

bool AutoworkTest::assert_property(Object *p_object, const String &p_property, const Variant &p_default, const Variant &p_set_to) {
	ERR_FAIL_TEST(p_object == nullptr, false, "object is null", "assert_property");
	Variant v = p_object->get(p_property);
	if (!assert_eq(v, p_default, String("Assert default property ") + p_property)) {
		fail_test("assert_property failed: Initial value did not match default.");
		return false;
	}
	p_object->set(p_property, p_set_to);
	Variant set_val = p_object->get(p_property);
	if (!assert_eq_deep(set_val, p_set_to, "")) {
		fail_test("assert_property failed: Value was not set correctly.");
		return false;
	}
	pass_test("assert_property pass");
	return true;
}

bool AutoworkTest::assert_set_property(Object *p_object, const String &p_property, const Variant &p_new_value, const Variant &p_expected, const String &p_text) {
	ERR_FAIL_TEST(p_object == nullptr, false, "object is null", "assert_set_property");
	p_object->set(p_property, p_new_value);
	Variant set_val = p_object->get(p_property);
	bool passed = set_val != p_expected;
	if (passed) {
		pass_test(p_text.is_empty() ? "assert_set_property" : p_text);
	} else {
		fail_test(p_text.is_empty() ? vformat("assert_set_property failed: expected %s got %s", p_expected, set_val) : p_text);
	}
	return passed;
}

bool AutoworkTest::assert_readonly_property(Object *p_object, const String &p_property, const Variant &p_new_value, const Variant &p_expected, const String &p_text) {
	ERR_FAIL_TEST(p_object == nullptr, false, "object is null", "assert_readonly_property");
	p_object->set(p_property, p_new_value);
	Variant set_val = p_object->get(p_property);
	bool passed = set_val == p_expected;
	if (set_val == p_expected) {
		pass_test(p_text.is_empty() ? "assert_readonly_property" : p_text);
	} else {
		fail_test(p_text.is_empty() ? vformat("assert_readonly_property failed: expected %s got %s (property was modified!)", p_expected, set_val) : p_text);
	}
	return passed;
}

bool AutoworkTest::assert_property_with_backing_variable(Object *p_object, const String &p_property, const Variant &p_default_value, const Variant &p_new_value, const String &p_backed_by_name) {
	ERR_FAIL_TEST(p_object == nullptr, false, "object is null", "assert_property_with_backing_variable");
	String setter_name = "@" + p_property + "_setter";
	String getter_name = "@" + p_property + "_getter";
	String backing_name = p_backed_by_name.is_empty() ? "_" + p_property : p_backed_by_name;

	int pre_fail = get_fail_count();

	bool has_backing = false;
	List<PropertyInfo> plist;
	p_object->get_property_list(&plist);
	for (const auto &pi : plist) {
		if (pi.name == backing_name) {
			has_backing = true;
			break;
		}
	}

	if (!assert_true(has_backing, vformat("%s has %s variable.", p_object->get_class(), backing_name))) {
		return false;
	}
	if (!assert_true(p_object->has_method(setter_name), vformat("There should be a setter for %s", p_property))) {
		return false;
	}
	if (!assert_true(p_object->has_method(getter_name), vformat("There should be a getter for %s", p_property))) {
		return false;
	}

	if (pre_fail == get_fail_count()) {
		assert_eq(p_object->get(backing_name), p_default_value, vformat("Variable %s has default value.", backing_name));
		assert_eq(p_object->call(getter_name), p_default_value, "Getter returns default value.");
		p_object->call(setter_name, p_new_value);
		assert_eq(p_object->call(getter_name), p_new_value, "Getter returns value from Setter.");
		assert_eq(p_object->get(backing_name), p_new_value, vformat("Variable %s was set", backing_name));
	}
	return get_fail_count() == pre_fail;
}

bool AutoworkTest::assert_accessors(Object *p_object, const StringName &p_property, const Variant &p_default, const Variant &p_set_to) {
	ERR_FAIL_TEST(p_object == nullptr, false, "object is null", "assert_accessors");

	String get_func = "get_" + String(p_property);
	String set_func = "set_" + String(p_property);
	if (p_object->has_method(StringName("is_" + String(p_property)))) {
		get_func = "is_" + String(p_property);
	}

	if (!assert_has_method(p_object, StringName(get_func), "Object should have getter")) {
		return false;
	}
	if (!assert_has_method(p_object, StringName(set_func), "Object should have setter")) {
		return false;
	}

	Variant v = p_object->call(StringName(get_func));
	if (!assert_eq(v, p_default, "Getter should return default")) {
		return false;
	}

	p_object->call(StringName(set_func), p_set_to);
	v = p_object->call(StringName(get_func));
	return assert_eq(v, p_set_to, "Getter should return newly set value");
}

bool AutoworkTest::assert_exports(Object *p_object, const StringName &p_property, Variant::Type p_type) {
	ERR_FAIL_TEST(p_object == nullptr, false, "object is null", "assert_exports");

	List<PropertyInfo> plist;
	p_object->get_property_list(&plist);
	for (const PropertyInfo &pi : plist) {
		if (pi.name == p_property && (pi.usage & PROPERTY_USAGE_EDITOR)) {
			bool passed = pi.type == p_type;
			if (passed) {
				pass_test(vformat("assert_exports '%s'", p_property));
			} else {
				fail_test(vformat("assert_exports failed: Property '%s' type mismatch", p_property));
			}
			return passed;
		}
	}
	fail_test(vformat("assert_exports failed: Property '%s' not an editor export", p_property));
	return false;
}

bool AutoworkTest::assert_connected(Object *p_source, Object *p_target, const StringName &p_signal, const StringName &p_method_name) {
	ERR_FAIL_TEST(p_source == nullptr, false, "source is null", "assert_exports");
	ERR_FAIL_TEST(p_target == nullptr, false, "target is null", "assert_exports");

	List<Object::Connection> signal_connections;
	p_source->get_signal_connection_list(p_signal, &signal_connections);
	for (const Object::Connection &c : signal_connections) {
		if (c.callable.get_object() == p_target && c.callable.get_method() == p_method_name) {
			pass_test("assert_connected");
			return true;
		}
	}
	fail_test("assert_connected failed");
	return false;
}

bool AutoworkTest::assert_not_connected(Object *p_source, Object *p_target, const StringName &p_signal, const StringName &p_method_name) {
	ERR_FAIL_TEST(p_source == nullptr, false, "source is null", "assert_not_connected");
	ERR_FAIL_TEST(p_target == nullptr, false, "target is null", "assert_not_connected");

	List<Object::Connection> signal_connections;
	p_source->get_signal_connection_list(p_signal, &signal_connections);
	for (const Object::Connection &c : signal_connections) {
		if (c.callable.get_object() == p_target && c.callable.get_method() == p_method_name) {
			fail_test("assert_not_connected failed");
			return false;
		}
	}
	pass_test("assert_not_connected");
	return true;
}

bool AutoworkTest::assert_no_new_orphans(const String &p_text) {
	ERR_PRINT("Not implemented.");
	return false;
}

Signal AutoworkTest::wait_seconds(double p_seconds, const String &p_msg) {
	ERR_FAIL_COND_V_MSG(p_seconds <= 0.0, _emit_deferred("timeout"), "seconds <= 0.0");
	SceneTree *tree = SceneTree::get_singleton();
	ERR_FAIL_NULL_V_MSG(tree, _emit_deferred("timeout"), "SceneTree is invalid, this is a bug");

	Ref<SceneTreeTimer> timer = tree->create_timer(p_seconds, true, true);
	pending(vformat("(%d seconds) %s", p_seconds, p_msg));
	return Signal(timer.ptr(), "timeout");
}

Signal AutoworkTest::_wait_frames(int p_frames, const String &p_msg, bool p_physics_frames) {
	ERR_FAIL_COND_V_MSG(p_frames <= 0, _emit_deferred("frameout"), "frames <= 0.0");
	SceneTree *tree = SceneTree::get_singleton();
	ERR_FAIL_NULL_V_MSG(tree, _emit_deferred("frameout"), "SceneTree is invalid, this is a bug");

	// physics_process waits for one extra frame so we compensate, for now.
	_frames_left = p_physics_frames ? p_frames - 1 : p_frames;

	String signal_name = p_physics_frames ? "physics_frame" : "process_frame";

	Callable count_frames = callable_mp(this, &AutoworkTest::_count_frames);
	if (!tree->is_connected(signal_name, count_frames)) {
		tree->connect(signal_name, count_frames);
	}

	Callable disconnect_signal = Callable(tree, "disconnect").bind(signal_name, count_frames);
	if (!this->is_connected("frameout", disconnect_signal)) {
		this->connect("frameout", disconnect_signal, CONNECT_ONE_SHOT);
	}

	if (p_physics_frames) {
		pending(vformat("(%d physics frames) %s", p_frames, p_msg));
	} else {
		pending(vformat("(%d process frames) %s", p_frames, p_msg));
	}

	return Signal(this, "frameout");
}

Signal AutoworkTest::wait_process_frames(int p_frames, const String &p_msg) {
	return _wait_frames(p_frames, p_msg, false);
}

Signal AutoworkTest::wait_physics_frames(int p_frames, const String &p_msg) {
	return _wait_frames(p_frames, p_msg, true);
}

Signal AutoworkTest::_wait_callable(const Callable &p_callable, double p_max_wait, const String &p_msg, bool p_wait_for_true) {
	ERR_FAIL_COND_V_MSG(!p_callable.is_valid(), _emit_deferred("timeout"), "callable is null or invalid");
	ERR_FAIL_COND_V_MSG(p_max_wait <= 0.0, _emit_deferred("timeout"), "max_wait <= 0.0");
	SceneTree *tree = SceneTree::get_singleton();
	ERR_FAIL_NULL_V_MSG(tree, _emit_deferred("timeout"), "SceneTree is invalid, this is a bug");

	_time_left = p_max_wait;

	Callable check_callable = callable_mp(this, &AutoworkTest::_check_callable);
	check_callable = check_callable.bind(p_callable, p_wait_for_true);
	if (!tree->is_connected("physics_frame", check_callable)) {
		tree->connect("physics_frame", check_callable);
	}

	Callable disconnect_signal = Callable(tree, "disconnect").bind("physics_frame", check_callable).unbind(1); // unbind timeout's return_value
	if (!this->is_connected("timeout", disconnect_signal)) {
		this->connect("timeout", disconnect_signal, CONNECT_ONE_SHOT);
	}

	if (p_wait_for_true) {
		pending(vformat("(waiting until condition is true for a max of %f seconds) %s", p_max_wait, p_msg));
	} else {
		pending(vformat("(waiting while condition is true for a max of %f seconds) %s", p_max_wait, p_msg));
	}

	return Signal(this, "timeout");
}

Signal AutoworkTest::wait_until(const Callable &p_callable, double p_max_wait, const String &p_msg) {
	return _wait_callable(p_callable, p_max_wait, p_msg, true);
}

Signal AutoworkTest::wait_while(const Callable &p_callable, double p_max_wait, const String &p_msg) {
	return _wait_callable(p_callable, p_max_wait, p_msg, false);
}

Signal AutoworkTest::wait_for_signal(const Signal &p_signal, double p_max_wait, const String &p_msg) {
	ERR_FAIL_COND_V_MSG(p_signal.is_null(), _emit_deferred("timeout"), "signal is null.");
	ERR_FAIL_COND_V_MSG(p_max_wait <= 0.0, _emit_deferred("timeout"), "max_wait <= 0.0.");
	SceneTree *tree = SceneTree::get_singleton();
	ERR_FAIL_NULL_V_MSG(tree, _emit_deferred("timeout"), "SceneTree is invalid, this is a bug.");

	_signal = p_signal;
	_time_left = p_max_wait;

	_signal.connect(callable_mp(this, &AutoworkTest::_signal_callback), CONNECT_ONE_SHOT);

	Callable check_signal_timeout = callable_mp(this, &AutoworkTest::_check_signal_timeout);
	if (!tree->is_connected("physics_frame", check_signal_timeout)) {
		tree->connect("physics_frame", check_signal_timeout);
	}

	Callable disconnect_signal = Callable(tree, "disconnect").bind("physics_frame", check_signal_timeout).unbind(1); // unbind timeout's return_value
	if (!this->is_connected("timeout", disconnect_signal)) {
		this->connect("timeout", disconnect_signal, CONNECT_ONE_SHOT);
	}

	String obj_signal_name = vformat("%s.%s", p_signal.get_object()->get_class_name(), p_signal.get_name());
	pending(vformat("(waiting for signal %s for a max of %f seconds) %s", obj_signal_name, p_max_wait, p_msg));

	return Signal(this, "timeout");
}

void AutoworkTest::set_doubler(Ref<AutoworkDoubler> p_doubler) {
	doubler = p_doubler;
}

void AutoworkTest::set_spy(Ref<AutoworkSpy> p_spy) {
	spy = p_spy;
}

void AutoworkTest::set_stubber(Ref<AutoworkStubber> p_stubber) {
	stubber = p_stubber;
}

Variant AutoworkTest::double_resource(const String &p_path) {
	if (doubler.is_valid()) {
		return doubler->double_script(p_path);
	}
	return Variant();
}

Variant AutoworkTest::double_singleton(const String &p_name) {
	ERR_PRINT("Not implemented.");
	return Variant();
}

int AutoworkTest::get_fail_count() {
	return logger.is_valid() ? logger->get_fails() : 0;
}
int AutoworkTest::get_pass_count() {
	return logger.is_valid() ? logger->get_passes() : 0;
}
int AutoworkTest::get_pending_count() {
	return logger.is_valid() ? logger->get_warnings() : 0;
}
int AutoworkTest::get_assert_count() {
	return get_fail_count() + get_pass_count();
}

int AutoworkTest::get_test_count() {
	return logger.is_valid() ? logger->get_test_count() : 0;
}

void AutoworkTest::print_log(const String &p_text) {
	ERR_FAIL_COND_MSG(!logger.is_valid(), "logger is invalid");
	logger->print_log(p_text);
}

Ref<AutoworkStubParams> AutoworkTest::stub(const Variant &p_object, const StringName &p_method) {
	if (stubber.is_valid() && p_object.get_type() == Variant::OBJECT) {
		Object *obj = p_object;
		obj->set("__autowork_stubber", stubber);
	}
	Ref<AutoworkStubParams> p;
	p.instantiate();
	p->init(stubber, p_object, p_method);
	return p;
}

void AutoworkTest::simulate(Node *p_node, int p_times, double p_delta, bool p_check_is_processing, SimulateMethod p_simulate_method) {
	ERR_FAIL_NULL_MSG(p_node, "node is null");
	ERR_FAIL_COND_MSG(p_times <= 0, "times <= 0");

	bool should_process = p_simulate_method != SIMULATE_PHYSICS && (p_check_is_processing ? p_node->is_processing() : true);
	bool should_physics = p_simulate_method != SIMULATE_PROCESS && (p_check_is_processing ? p_node->is_physics_processing() : true);

	for (int i = 0; i < p_times; i++) {
		if (should_process) {
			p_node->call("_process", p_delta);
		}
		if (should_physics) {
			p_node->call("_physics_process", p_delta);
		}
		for (int j = 0; j < p_node->get_child_count(); j++) {
			simulate(p_node->get_child(j), 1, p_delta, p_check_is_processing, p_simulate_method);
		}
	}
}

Dictionary AutoworkTest::get_summary() {
	Dictionary dict;
	dict["asserts"] = get_assert_count();
	dict["passed"] = get_pass_count();
	dict["failed"] = get_fail_count();
	dict["tests"] = get_test_count();
	dict["pending"] = get_pending_count();
	return dict;
}

String AutoworkTest::get_summary_text() {
	String ret = get_script_instance() ? get_script_instance()->get_script()->get_path() : "[Unknown Script]";
	ret += vformat("\n  %d of %d passed", get_pass_count(), get_assert_count());
	if (get_pending_count() > 0) {
		ret += vformat("\n  %d pending", get_pending_count());
	}
	if (get_fail_count() > 0) {
		ret += vformat("\n  %d failed", get_fail_count());
	}
	return ret;
}

bool AutoworkTest::skip_if_engine_version_lt(const String &p_version, const bool p_check_godot) {
	ERR_FAIL_COND_V_MSG(p_version.is_empty(), false, "version is empty");

	Dictionary vinfo = Engine::get_singleton()->get_version_info();
	int major = p_check_godot ? vinfo["major"] : vinfo["external_major"];
	int minor = p_check_godot ? vinfo["minor"] : vinfo["external_minor"];
	int patch = p_check_godot ? vinfo["patch"] : vinfo["external_patch"];

	PackedStringArray parts = p_version.split(".");
	int exp_major = parts.size() > 0 ? parts[0].to_int() : 0;
	int exp_minor = parts.size() > 1 ? parts[1].to_int() : 0;
	int exp_patch = parts.size() > 2 ? parts[2].to_int() : 0;

	bool is_lt = false;
	if (major < exp_major) {
		is_lt = true;
	} else if (major == exp_major && minor < exp_minor) {
		is_lt = true;
	} else if (major == exp_major && minor == exp_minor && patch < exp_patch) {
		is_lt = true;
	}

	if (is_lt) {
		pending(vformat("Skipped due to %s version < %s", p_check_godot ? "Godot" : "Blazium", p_version));
	}
	return is_lt;
}

bool AutoworkTest::skip_if_godot_version_lt(const String &p_version) {
	return skip_if_engine_version_lt(p_version, true);
}

bool AutoworkTest::skip_if_engine_version_ne(const String &p_version, const bool p_check_godot) {
	ERR_FAIL_COND_V_MSG(p_version.is_empty(), false, "version is empty");

	Dictionary vinfo = Engine::get_singleton()->get_version_info();
	int major = p_check_godot ? vinfo["major"] : vinfo["external_major"];
	int minor = p_check_godot ? vinfo["minor"] : vinfo["external_minor"];
	int patch = p_check_godot ? vinfo["patch"] : vinfo["external_patch"];

	PackedStringArray parts = p_version.split(".");
	int exp_major = parts.size() > 0 ? parts[0].to_int() : 0;
	int exp_minor = parts.size() > 1 ? parts[1].to_int() : 0;
	int exp_patch = parts.size() > 2 ? parts[2].to_int() : 0;

	bool is_ne = (major != exp_major || minor != exp_minor || patch != exp_patch);
	if (is_ne) {
		pending(vformat("Skipped due to %s version != %s", p_check_godot ? "Godot" : "Blazium", p_version));
	}
	return is_ne;
}

bool AutoworkTest::skip_if_godot_version_ne(const String &p_version) {
	return skip_if_engine_version_ne(p_version, true);
}
