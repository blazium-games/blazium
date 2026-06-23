/**************************************************************************/
/*  autowork_main.cpp                                                     */
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

#include "autowork_main.h"
#include "autowork_doubler.h"
#include "autowork_hook_script.h"
#include "autowork_logger.h"
#include "autowork_signal_watcher.h"
#include "autowork_spy.h"
#include "autowork_stubber.h"
#include "core/config/engine.h"
#include "core/io/file_access.h"
#include "core/io/json.h"
#include "core/io/resource_loader.h"
#include "core/object/object.h"
#include "core/object/script_instance.h"
#include "core/object/script_language.h"
#include "core/os/os.h"
#include "modules/gdscript/gdscript.h"
#include "scene/main/scene_tree.h"

void Autowork::_bind_methods() {
	ClassDB::bind_method(D_METHOD("add_directory", "path", "prefix", "suffix"), &Autowork::add_directory, DEFVAL(""), DEFVAL(""));
	ClassDB::bind_method(D_METHOD("add_script", "path"), &Autowork::add_script);
	ClassDB::bind_method(D_METHOD("set_test", "test_name"), &Autowork::set_test);
	ClassDB::bind_method(D_METHOD("run_tests"), &Autowork::run_tests);

	ClassDB::bind_method(D_METHOD("get_test_count"), &Autowork::get_test_count);
	ClassDB::bind_method(D_METHOD("get_assert_count"), &Autowork::get_assert_count);
	ClassDB::bind_method(D_METHOD("get_pass_count"), &Autowork::get_pass_count);
	ClassDB::bind_method(D_METHOD("get_fail_count"), &Autowork::get_fail_count);
	ClassDB::bind_method(D_METHOD("get_pending_count"), &Autowork::get_pending_count);
	ClassDB::bind_method(D_METHOD("get_test_script_count"), &Autowork::get_test_script_count);

	ClassDB::bind_method(D_METHOD("get_test_collector"), &Autowork::get_test_collector);
	ClassDB::bind_method(D_METHOD("get_logger"), &Autowork::get_logger);
	ClassDB::bind_method(D_METHOD("get_doubler"), &Autowork::get_doubler);
	ClassDB::bind_method(D_METHOD("get_spy"), &Autowork::get_spy);
	ClassDB::bind_method(D_METHOD("get_stubber"), &Autowork::get_stubber);
}

Autowork::Autowork() {
	collector.instantiate();
	logger.instantiate();
	doubler.instantiate();
	spy.instantiate();
	stubber.instantiate();
	doubler->set_spy(spy);
	doubler->set_stubber(stubber);
}

Autowork::~Autowork() {
}

void Autowork::add_directory(const String &p_path, const String &p_prefix, const String &p_suffix) {
	if (collector.is_valid()) {
		if (!p_prefix.is_empty()) {
			collector->script_prefix = p_prefix;
		}
		if (!p_suffix.is_empty()) {
			collector->script_suffix = p_suffix;
		}
		collector->process_directory(p_path);
	}
}

void Autowork::add_script(const String &p_path) {
	if (collector.is_valid()) {
		collector->add_script(p_path);
	}
}

void Autowork::set_test(const String &p_test_name) {
	if (collector.is_valid()) {
		collector->test_pattern = p_test_name;
	}
}

void Autowork::_restore_editor_scripting_if_needed() {
#ifdef TOOLS_ENABLED
	if (restore_editor_scripting) {
		ScriptServer::set_scripting_enabled(false);
		restore_editor_scripting = false;
	}
#endif
}

void Autowork::run_tests() {
	ERR_FAIL_COND_MSG(collector.is_null(), "collector is null");
	ERR_FAIL_COND_MSG(logger.is_null(), "logger is null");
	OS *os = OS::get_singleton();
	ERR_FAIL_NULL_MSG(os, "OS singleton is null");

#ifdef TOOLS_ENABLED
	restore_editor_scripting = false;
	if (Engine::get_singleton()->is_editor_hint() && !ScriptServer::is_scripting_enabled()) {
		ScriptServer::set_scripting_enabled(true);
		restore_editor_scripting = true;
	}
#endif

	bool dir_overriden = false;
	String junit_path;
	String json_path;
	String pre_run_path;
	String post_run_path;

	String config_file = "";
	if (FileAccess::exists("res://.autoworkconfig.json")) {
		config_file = "res://.autoworkconfig.json";
	} else if (FileAccess::exists("res://.gutconfig.json")) {
		config_file = "res://.gutconfig.json";
	}

	if (!config_file.is_empty()) {
		Ref<FileAccess> f = FileAccess::open(config_file, FileAccess::READ);
		if (f.is_valid()) {
			String json_text = f->get_as_text();
			f->close();

			Ref<JSON> json;
			json.instantiate();

			Error err = json->parse(json_text);
			if (err == OK && json->get_data().get_type() == Variant::DICTIONARY) {
				Dictionary dict = json->get_data();

				if (dict.has("include_subdirs")) {
					collector->include_subdirectories = dict["include_subdirs"];
				}
				if (dict.has("prefix")) {
					collector->script_prefix = dict["prefix"];
				}
				if (dict.has("suffix")) {
					collector->script_suffix = dict["suffix"];
				}
				if (dict.has("inner_class")) {
					collector->inner_class_pattern = dict["inner_class"];
				}
				if (dict.has("unit_test_name")) {
					collector->test_pattern = dict["unit_test_name"];
				}

				if (dict.has("pre_run_script")) {
					pre_run_path = dict["pre_run_script"];
				}
				if (dict.has("post_run_script")) {
					post_run_path = dict["post_run_script"];
				}
				if (dict.has("junit_xml_file")) {
					junit_path = dict["junit_xml_file"];
				}
				if (dict.has("hide_orphans")) {
					logger->set_hide_orphans(dict["hide_orphans"]);
				}

				if (dict.has("dirs") && dict["dirs"].get_type() == Variant::ARRAY) {
					Array dirs = dict["dirs"];
					for (int i = 0; i < dirs.size(); i++) {
						collector->process_directory(dirs[i]);
						dir_overriden = true;
					}
				}

				if (dict.has("tests") && dict["tests"].get_type() == Variant::ARRAY) {
					Array tests = dict["tests"];
					for (int i = 0; i < tests.size(); i++) {
						collector->add_script(tests[i]);
					}
				}
			}
		}
	}

	for (const String &arg : os->get_cmdline_args()) {
		if (arg.begins_with("--aw-dir=") || arg.begins_with("-gdir=")) {
			if (!dir_overriden) {
				collector->clear();
				dir_overriden = true;
			}
			collector->process_directory(arg.get_slicec('=', 1));
		} else if (arg.begins_with("--aw-test=") || arg.begins_with("-gunit_test_name=")) {
			collector->test_pattern = arg.get_slicec('=', 1);
		} else if (arg.begins_with("--aw-select=") || arg.begins_with("-gselect=")) {
			collector->script_pattern = arg.get_slicec('=', 1);
		} else if (arg.begins_with("--aw-file=") || arg.begins_with("-gtest=")) {
			collector->add_script(arg.get_slicec('=', 1));
		} else if (arg.begins_with("--aw-prefix=") || arg.begins_with("-gprefix=")) {
			collector->script_prefix = arg.get_slicec('=', 1);
		} else if (arg.begins_with("--aw-suffix=") || arg.begins_with("-gsuffix=")) {
			collector->script_suffix = arg.get_slicec('=', 1);
		} else if (arg.begins_with("--aw-inner-class=") || arg.begins_with("-ginner_class=")) {
			collector->inner_class_pattern = arg.get_slicec('=', 1);
		} else if (arg.begins_with("--aw-include-subdirs") || arg.begins_with("-ginclude_subdirs")) {
			collector->include_subdirectories = true;
		} else if (arg.begins_with("--aw-junit=") || arg.begins_with("-gjunit_xml_file=")) {
			junit_path = arg.get_slicec('=', 1);
		} else if (arg.begins_with("--aw-json=")) {
			json_path = arg.get_slicec('=', 1);
		} else if (arg.begins_with("--aw-pre-run=") || arg.begins_with("-gpre_run_script=")) {
			pre_run_path = arg.get_slicec('=', 1);
		} else if (arg.begins_with("--aw-post-run=") || arg.begins_with("-gpost_run_script=")) {
			post_run_path = arg.get_slicec('=', 1);
		} else if (arg.begins_with("--aw-hide-orphans") || arg.begins_with("-ghide_orphans")) {
			logger->set_hide_orphans(true);
		}
	}

	if (!pre_run_path.is_empty()) {
		Ref<Script> pre_s = ResourceLoader::load(pre_run_path);
		if (pre_s.is_valid()) {
			Ref<AutoworkHookScript> hook;
			hook.instantiate();
			hook->set_script(pre_s);
			hook->call("_run");
			if (hook->should_abort()) {
				print_line("Autowork Main: Aborting tests due to pre-run script abort() call.");
				_restore_editor_scripting_if_needed();
				return;
			}
		}
	}

	Ref<GDScript> gd_proxy_runner;
	gd_proxy_runner.instantiate();
	String code = R"(
extends Object
static func __run_tests__(
	scripts: Array,

	get_test_instance: Callable,
	on_test_over: Callable,

	begin_test: Callable,
	inc_test_count: Callable,
	inc_orphans: Callable,
	end_test: Callable,
):
	for script: Dictionary in scripts:
		print("Running Script: %s" % script["path"])

		var test_instance: Node = get_test_instance.call(script)
		if test_instance == null:
			continue

		if (test_instance.has_method("_before_all")):
			await test_instance.call("_before_all")

		for test: String in script["tests"]:
			print("\t- %s" % test)

			var is_parameterized: bool = false
			var param_index: int = 0

			if (test_instance.has_method("clear_parameters")):
				test_instance.call("clear_parameters")

			if (is_parameterized and test_instance.has_method("set_current_parameter_index")):
				test_instance.call("set_current_parameter_index", param_index)

			while (true):
				var ds: String = ' (param %d)' % param_index if is_parameterized else ""
				begin_test.call(script["path"], test + ds)
				if (!is_parameterized or param_index == 0):
					inc_test_count.call()

				var objects_before: int = int(Performance.get_monitor(Performance.OBJECT_ORPHAN_NODE_COUNT))

				if (test_instance.has_method("_before_each")):
					await test_instance.call("_before_each")

				await test_instance.call(test)

				if (test_instance.has_method("_after_each")):
					await test_instance.call("_after_each")

				if (test_instance.has_method("free_all")):
					test_instance.call("free_all")

				var objects_after: int = int(Performance.get_monitor(Performance.OBJECT_ORPHAN_NODE_COUNT))
				if (objects_after > objects_before):
					inc_orphans.call(objects_after - objects_before)

				end_test.call()

				if (test_instance.has_method("has_parameters") and test_instance.call("has_parameters")):
					is_parameterized = true
					param_index += 1
					if (param_index >= test_instance.call("get_parameter_count")):
						break
				else:
					break

		if (test_instance.has_method("_after_all")):
			await test_instance.call("_after_all")

		test_instance.queue_free()

	on_test_over.call()
)";
	gd_proxy_runner->set_source_code(code);
	Error err = gd_proxy_runner->reload();
	if (err != OK) {
		_restore_editor_scripting_if_needed();
		ERR_FAIL_MSG("Error initializing proxy script");
	}

	Array scripts = collector->get_scripts();
	Callable get_test_instance = callable_mp(this, &Autowork::_get_test_instance);
	Callable on_test_over = callable_mp(this, &Autowork::_on_test_over);

	AutoworkLogger *logger_ptr = logger.ptr();
	Callable begin_test = callable_mp(logger_ptr, &AutoworkLogger::begin_test);
	Callable inc_test_count = callable_mp(logger_ptr, &AutoworkLogger::inc_test_count);
	Callable inc_orphans = callable_mp(logger_ptr, &AutoworkLogger::inc_orphans);
	Callable end_test = callable_mp(logger_ptr, &AutoworkLogger::end_test);

	gd_proxy_runner->call("__run_tests__", scripts, get_test_instance, on_test_over, begin_test, inc_test_count, inc_orphans, end_test);
}

AutoworkTest *Autowork::_get_test_instance(Dictionary script_info) {
	Ref<Script> test_script = script_info["script"];

	print_line(vformat("Running Script: %s", script_info["path"]));

	AutoworkTest *test_instance = Object::cast_to<AutoworkTest>(ClassDB::instantiate(test_script->get_instance_base_type()));
	if (!test_instance) {
		return nullptr;
	}

	test_instance->set_script(test_script);
	if (test_instance->get_script_instance() && test_instance->get_script_instance()->is_placeholder()) {
		ERR_PRINT(vformat("Autowork: test script '%s' is a placeholder in editor mode; enable scripting before run_tests().", script_info["path"]));
		memdelete(test_instance);
		return nullptr;
	}
	test_instance->set_name("TestInstance");
	add_child(test_instance);

	if (test_instance->has_method("set_logger") && logger.is_valid()) {
		test_instance->call("set_logger", logger);
	}
	if (test_instance->has_method("set_doubler") && doubler.is_valid()) {
		test_instance->call("set_doubler", doubler);
	}
	if (test_instance->has_method("set_spy") && spy.is_valid()) {
		test_instance->call("set_spy", spy);
	}
	if (test_instance->has_method("set_stubber") && stubber.is_valid()) {
		test_instance->call("set_stubber", stubber);
	}

	return test_instance;
}

void Autowork::_on_test_over() {
	ERR_FAIL_COND_MSG(logger.is_null(), "logger is null");
	_restore_editor_scripting_if_needed();
	logger->print_summary();

	OS *os = OS::get_singleton();
	ERR_FAIL_NULL_MSG(os, "OS singleton is null");

	String junit_p;
	String json_p;
	String post_p;
	for (const String &arg : os->get_cmdline_args()) {
		if (arg.begins_with("--aw-junit=")) {
			junit_p = arg.get_slicec('=', 1);
		}
		if (arg.begins_with("--aw-json=")) {
			json_p = arg.get_slicec('=', 1);
		}
		if (arg.begins_with("--aw-post-run=")) {
			post_p = arg.get_slicec('=', 1);
		}
	}
	if (!post_p.is_empty()) {
		Ref<Script> post_s = ResourceLoader::load(post_p);
		if (post_s.is_valid()) {
			Ref<AutoworkHookScript> hook;
			hook.instantiate();
			hook->set_script(post_s);
			hook->call("_run");
		}
	}
	if (!junit_p.is_empty()) {
		logger->export_xml(junit_p);
	}
	if (!json_p.is_empty()) {
		logger->export_json(json_p);
	}

	if (DisplayServer::get_singleton() && DisplayServer::get_singleton()->get_name() == "headless") {
		SceneTree *tree = SceneTree::get_singleton();
		ERR_FAIL_NULL_MSG(tree, "SceneTree is available.");
		tree->queue_delete(this);
		tree->quit(logger->get_fails());
	}
}
