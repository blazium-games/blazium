/**************************************************************************/
/*  autowork_test.h                                                       */
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

#include "autowork_doubler.h"
#include "autowork_logger.h"
#include "autowork_signal_watcher.h"
#include "autowork_spy.h"
#include "autowork_stub_params.h"
#include "autowork_stubber.h"
#include "core/object/gdvirtual.gen.inc"
#include "scene/main/node.h"

class AutoworkTest : public Node {
	GDCLASS(AutoworkTest, Node);

	int _frames_left = 0;
	double _time_left = 0.0;
	Signal _signal = Signal();

	void _count_frames() {
		_frames_left--;
		if (_frames_left < 0) {
			_frames_left = 0;
			emit_signal("frameout");
		}
	}

	void _check_callable(Callable p_callable, bool p_wait_for_true) {
		if (p_callable.is_null()) {
			return;
		}
		SceneTree *tree = SceneTree::get_singleton();
		ERR_FAIL_NULL_MSG(tree, "SceneTree is invalid, this is a bug.");
		_time_left -= tree->get_physics_process_time();
		bool return_value = p_callable.call().booleanize() == p_wait_for_true;
		if (return_value || _time_left <= 0.0) {
			_time_left = 0.0;
			emit_signal("timeout", return_value);
		}
	}

	void _check_signal_timeout() {
		if (_signal.is_null()) {
			return;
		}
		SceneTree *tree = SceneTree::get_singleton();
		ERR_FAIL_NULL_MSG(tree, "SceneTree is invalid, this is a bug.");
		_time_left -= tree->get_physics_process_time();
		if (_time_left < 0.0) {
			_signal.disconnect(callable_mp(this, &AutoworkTest::_signal_callback));
			_signal = Signal();
			_time_left = 0.0;
			emit_signal("timeout", false);
		}
	}

	template <typename... VarArgs>
	void _signal_callback(VarArgs... p_args) {
		_signal = Signal();
		_time_left = 0.0;
		emit_signal("timeout", true);
	}

	Signal _emit_deferred(const String &p_signal_name) {
		if (p_signal_name == "timeout") {
			call_deferred("emit_signal", p_signal_name, false);
		} else {
			call_deferred("emit_signal", p_signal_name);
		}
		return Signal(this, p_signal_name);
	}

	Signal _wait_frames(int p_frames, const String &p_msg, bool p_physics_frames);
	Signal _wait_callable(const Callable &p_callable, double p_max_wait, const String &p_msg, bool p_wait_for_true);

protected:
	static void _bind_methods();

	Ref<AutoworkLogger> logger;
	Ref<AutoworkSignalWatcher> signal_watcher;
	Ref<AutoworkDoubler> doubler;
	Ref<AutoworkSpy> spy;
	Ref<AutoworkStubber> stubber;

	bool _has_parameters = false;
	Array _parameters;
	int _current_parameter_index = 0;

	Vector<Object *> _to_free;
	Vector<Node *> _to_queue_free;

public:
	enum SimulateMethod {
		SIMULATE_BOTH,
		SIMULATE_PROCESS,
		SIMULATE_PHYSICS
	};

	AutoworkTest();
	~AutoworkTest();

	Variant use_parameters(const Array &p_params);
	bool has_parameters() const;
	int get_parameter_count() const;
	int get_current_parameter_index() const;
	void set_current_parameter_index(int p_index);
	void clear_parameters();

	void autofree(Object *p_object);
	void autoqfree(Node *p_node);
	Node *add_child_autofree(Node *p_child);
	Node *add_child_autoqfree(Node *p_child);
	void free_all();

	void set_logger(Ref<AutoworkLogger> p_logger);

	void watch_signals(Object *p_object);
	void watch_signal(Object *p_object, const StringName &p_signal);

	void print_log(const String &p_text);
	Variant get_logger() { return logger; }
	int get_test_count();

	void set_doubler(Ref<AutoworkDoubler> p_doubler);
	void set_spy(Ref<AutoworkSpy> p_spy);
	void set_stubber(Ref<AutoworkStubber> p_stubber);

	Variant double_resource(const String &p_path);
	Variant double_singleton(const String &p_name);

	Variant double_thing(const Variant &p_thing) {
		if (p_thing.get_type() == Variant::STRING) {
			return double_resource(p_thing);
		}
		return double_singleton("");
	}
	Variant spy_thing(const Variant &p_thing) {
		if (spy.is_valid() && p_thing.get_type() == Variant::OBJECT) {
			Object *obj = p_thing;
			obj->set("__autowork_spy", spy);
		}
		return p_thing;
	}

	Ref<AutoworkStubParams> stub(const Variant &p_object, const StringName &p_method = StringName());

	void simulate(Node *p_node, int p_times, double p_delta, bool check_is_processing = false, SimulateMethod p_simulate_method = SIMULATE_BOTH);

	GDVIRTUAL0(_before_all)
	GDVIRTUAL0(_before_each)
	GDVIRTUAL0(_after_each)
	GDVIRTUAL0(_after_all)

	bool assert_true(bool p_condition, const String &p_text = "");
	bool assert_false(bool p_condition, const String &p_text = "");
	bool assert_eq(const Variant &p_got, const Variant &p_expected, const String &p_text = "");
	bool assert_ne(const Variant &p_got, const Variant &p_expected, const String &p_text = "");
	bool assert_eq_deep(const Variant &p_got, const Variant &p_expected, const String &p_text = "");
	bool assert_ne_deep(const Variant &p_got, const Variant &p_expected, const String &p_text = "");
	bool assert_gt(const Variant &p_got, const Variant &p_expected, const String &p_text = "");
	bool assert_lt(const Variant &p_got, const Variant &p_expected, const String &p_text = "");
	bool assert_ge(const Variant &p_got, const Variant &p_expected, const String &p_text = "");
	bool assert_le(const Variant &p_got, const Variant &p_expected, const String &p_text = "");
	bool assert_null(const Variant &p_got, const String &p_text = "");
	bool assert_not_null(const Variant &p_got, const String &p_text = "");
	bool assert_has_method(Object *p_object, const StringName &p_method, const String &p_text = "");
	bool assert_between(const Variant &p_got, const Variant &p_expect_low, const Variant &p_expect_high, const String &p_text = "");
	bool assert_not_between(const Variant &p_got, const Variant &p_expect_low, const Variant &p_expect_high, const String &p_text = "");
	bool assert_almost_eq(const Variant &p_got, const Variant &p_expected, const Variant &p_error_margin, const String &p_text = "");
	bool assert_almost_ne(const Variant &p_got, const Variant &p_expected, const Variant &p_error_margin, const String &p_text = "");
	bool assert_has(const Variant &p_variant, const Variant &p_element, const String &p_text = "");
	bool assert_does_not_have(const Variant &p_variant, const Variant &p_element, const String &p_text = "");
	bool assert_file_exists(const String &p_file_path, const String &p_text = "");
	bool assert_file_does_not_exist(const String &p_file_path, const String &p_text = "");
	bool assert_file_empty(const String &p_file_path, const String &p_text = "");
	bool assert_file_not_empty(const String &p_file_path, const String &p_text = "");
	bool assert_dir_exists(const String &p_dir_path, const String &p_text = "");
	bool assert_dir_does_not_exist(const String &p_dir_path, const String &p_text = "");
	bool assert_typeof(const Variant &p_variant, Variant::Type p_type, const String &p_text = "");
	bool assert_not_typeof(const Variant &p_variant, Variant::Type p_type, const String &p_text = "");
	bool assert_is(Object *p_object, const String &p_class, const String &p_text = "");
	bool assert_string_contains(const String &p_got, const String &p_expected, const String &p_text = "");
	bool assert_string_starts_with(const String &p_got, const String &p_expected, const String &p_text = "");
	bool assert_string_ends_with(const String &p_got, const String &p_expected, const String &p_text = "");
	bool assert_freed(Object *p_object, const String &p_text = "");
	bool assert_not_freed(Object *p_object, const String &p_text = "");
	bool assert_called(Object *p_object, const StringName &p_method, const Array &p_args = Array(), const String &p_text = "");
	bool assert_not_called(Object *p_object, const StringName &p_method, const Array &p_args = Array(), const String &p_text = "");
	bool assert_called_count(Object *p_object, const StringName &p_method, int p_expected_count, const Array &p_args = Array(), const String &p_text = "");
	bool assert_same(const Variant &p_got, const Variant &p_expected, const String &p_text = "");
	bool assert_not_same(const Variant &p_got, const Variant &p_expected, const String &p_text = "");
	bool assert_signal_emitted(Object *p_object, const StringName &p_signal, const String &p_text = "");
	bool assert_signal_not_emitted(Object *p_object, const StringName &p_signal, const String &p_text = "");
	bool assert_signal_emit_count(Object *p_object, const StringName &p_signal, int p_expected_count, const String &p_text = "");
	bool assert_has_signal(Object *p_object, const StringName &p_signal, const String &p_text = "");
	bool assert_signal_emitted_with_parameters(Object *p_object, const StringName &p_signal, const Array &p_expected_parameters, int p_index = -1, const String &p_text = "");
	bool assert_property(Object *p_object, const String &p_property, const Variant &p_default, const Variant &p_set_to);
	bool assert_set_property(Object *p_object, const String &p_property, const Variant &p_new_value, const Variant &p_expected, const String &p_text = "");
	bool assert_readonly_property(Object *p_object, const String &p_property, const Variant &p_new_value, const Variant &p_expected, const String &p_text = "");
	bool assert_property_with_backing_variable(Object *p_object, const String &p_property, const Variant &p_default_value, const Variant &p_new_value, const String &p_backed_by_name = "");
	bool assert_accessors(Object *p_object, const StringName &p_property, const Variant &p_default, const Variant &p_set_to);
	bool assert_exports(Object *p_object, const StringName &p_property, Variant::Type p_type);
	bool assert_connected(Object *p_source, Object *p_target, const StringName &p_signal, const StringName &p_method_name = "");
	bool assert_not_connected(Object *p_source, Object *p_target, const StringName &p_signal, const StringName &p_method_name = "");
	bool assert_no_new_orphans(const String &p_text = "");

	int get_call_count(Object *p_object, const StringName &p_method, const Array &p_args = Array());
	Array get_call_parameters(Object *p_object, const StringName &p_method, int p_index = -1);
	int get_signal_emit_count(Object *p_object, const StringName &p_signal);
	Array get_signal_parameters(Object *p_object, const StringName &p_signal, int p_index = -1);

	Signal wait_seconds(double p_seconds, const String &p_msg = "");
	Signal wait_process_frames(int p_frames, const String &p_msg = "");
	Signal wait_physics_frames(int p_frames, const String &p_msg = "");
	Signal wait_until(const Callable &p_callable, double p_max_wait, const String &p_msg = "");
	Signal wait_while(const Callable &p_callable, double p_max_wait, const String &p_msg = "");
	Signal wait_for_signal(const Signal &p_signal, double p_max_wait, const String &p_msg = "");

	int get_fail_count();
	int get_pass_count();
	int get_pending_count();
	int get_assert_count();

	Dictionary get_summary();
	String get_summary_text();

	bool skip_if_engine_version_lt(const String &p_version, const bool p_check_godot = false);
	bool skip_if_engine_version_ne(const String &p_version, const bool p_check_godot = false);
	bool skip_if_godot_version_lt(const String &p_version);
	bool skip_if_godot_version_ne(const String &p_version);

	void pending(const String &p_text = "");
	void pass_test(const String &p_text = "");
	void fail_test(const String &p_text = "");

#define ERR_FAIL_TEST(m_cond, m_retval, m_msg, m_assert_name)       \
	if (unlikely(m_cond)) {                                         \
		ERR_PRINT(m_msg);                                           \
		fail_test(vformat("%s failed (%s)", m_assert_name, m_msg)); \
		return m_retval;                                            \
	} else                                                          \
		((void)0)
};

VARIANT_ENUM_CAST(AutoworkTest::SimulateMethod);
