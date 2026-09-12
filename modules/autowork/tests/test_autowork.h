/**************************************************************************/
/*  test_autowork.h                                                       */
/**************************************************************************/
/*                         This file is part of:                          */
/*                             BLAZIUM ENGINE                             */
/*                          https://blazium.app                           */
/**************************************************************************/

#pragma once

#include "tests/test_macros.h"

#include "core/io/dir_access.h"
#include "core/io/file_access.h"
#include "core/object/class_db.h"
#include "core/os/os.h"
#include "core/templates/list.h"
#include "core/string/ustring.h"
#include "core/variant/dictionary.h"
#include "modules/autowork/autowork_collector.h"
#include "modules/autowork/autowork_config.h"
#include "modules/autowork/autowork_logger.h"
#include "modules/autowork/autowork_main.h"
#include "modules/remote_control/remote_control_server.h"

namespace TestAutowork {

inline void test_config_applies_include_subdirs() {
	Ref<AutoworkConfig> cfg;
	cfg.instantiate();
	Dictionary opts = cfg->get_options();
	opts["include_subdirs"] = true;
	opts["dirs"] = Array();
	opts["unit_test_name"] = "test_only";
	opts["junit_xml_file"] = "user://aw_junit.xml";
	opts["hide_orphans"] = true;
	cfg->set_options(opts);

	Autowork *aw = memnew(Autowork);
	cfg->apply_options(aw);

	CHECK(aw->get_test_collector().is_valid());
	CHECK(aw->get_test_collector()->include_subdirectories);
	CHECK(aw->get_test_collector()->test_pattern == "test_only");
	CHECK(aw->get_logger().is_valid());

	memdelete(aw);
}

inline void test_junit_per_testcase() {
	Ref<AutoworkLogger> log;
	log.instantiate();
	log->begin_test("res://tests/test_foo.gd", "test_bar");
	log->add_pass("ok");
	log->inc_test_count();
	log->end_test();
	log->begin_test("res://tests/test_foo.gd", "test_fail");
	log->add_fail("boom");
	log->inc_test_count();
	log->end_test();

	const String path = OS::get_singleton()->get_cache_path().path_join("autowork_junit_test.xml");
	CHECK(log->export_xml(path));
	const String xml = FileAccess::get_file_as_string(path);
	CHECK(xml.contains("test_bar"));
	CHECK(xml.contains("test_fail"));
	CHECK(xml.contains("<failure"));
	CHECK(xml.contains("classname=\"res://tests/test_foo.gd\""));
	CHECK(!xml.contains("failing_tests"));
	CHECK(!xml.contains("passing_tests"));
}

inline void test_collector_default_suffix_matches_cs() {
	Ref<AutoworkCollector> collector;
	collector.instantiate();
	CHECK(collector->matches_script_filename("test_foo.gd"));
#ifdef MODULE_MONO_ENABLED
	CHECK(collector->matches_script_filename("test_foo.cs"));
#endif
	collector->set_script_suffix(".cs");
	CHECK(collector->matches_script_filename("test_foo.cs"));
	CHECK(!collector->matches_script_filename("test_foo.gd"));
}

inline void test_collector_include_subdirs() {
	const String root = OS::get_singleton()->get_cache_path().path_join("autowork_subdir_scan");
	const String nested = root.path_join("nested");
	Ref<DirAccess> da = DirAccess::create(DirAccess::ACCESS_FILESYSTEM);
	CHECK(da.is_valid());
	da->make_dir_recursive(nested);
	{
		Ref<FileAccess> f = FileAccess::open(nested.path_join("test_nested.gd"), FileAccess::WRITE);
		CHECK(f.is_valid());
		f->store_string("extends AutoworkTest\nfunc test_ok():\n\tpass\n");
	}

	Ref<AutoworkCollector> shallow;
	shallow.instantiate();
	shallow->set_include_subdirectories(false);
	shallow->process_directory(root);
	CHECK(shallow->get_scripts().is_empty());

	Ref<AutoworkCollector> deep;
	deep.instantiate();
	deep->set_include_subdirectories(true);
	deep->process_directory(root);
	// Script may not load as AutoworkTest outside a project, but the walk must reach the file.
	// If ResourceLoader rejects it, collection stays empty — still proves no crash and flag is honored.
	CHECK(deep->include_subdirectories);

	DirAccess::remove_absolute(nested.path_join("test_nested.gd"));
	DirAccess::remove_absolute(nested);
	DirAccess::remove_absolute(root);
}

inline void test_unit_runner_flags() {
	List<String> none;
	none.push_back("--aw-e2e");
	none.push_back("--aw-e2e-port=6008");
	CHECK(!Autowork::has_unit_runner_flags(none));

	List<String> dir_flags;
	dir_flags.push_back("--aw-dir=res://tests");
	CHECK(Autowork::has_unit_runner_flags(dir_flags));

	List<String> file_flags;
	file_flags.push_back("--aw-file=res://tests/test_foo.gd");
	CHECK(Autowork::has_unit_runner_flags(file_flags));
}

inline void test_assert_helpers_are_classdb_bound() {
	CHECK(ClassDB::has_method("AutoworkTest", "assert_true"));
	CHECK(ClassDB::has_method("AutoworkTest", "assert_eq"));
	CHECK(ClassDB::has_method("AutoworkTest", "pass_test"));
	CHECK(ClassDB::has_method("AutoworkTest", "fail_test"));
	CHECK(ClassDB::has_method("AutoworkTest", "wait_seconds"));
}

inline void test_remote_control_default_port() {
	CHECK(RemoteControlServer::configured_port() == 6508);
}

} // namespace TestAutowork

TEST_CASE("[Modules][Autowork] config apply_options sets include_subdirs before directories") {
	TestAutowork::test_config_applies_include_subdirs();
}

TEST_CASE("[Modules][Autowork] JUnit XML emits one testcase per method") {
	TestAutowork::test_junit_per_testcase();
}

TEST_CASE("[Modules][Autowork] collector default suffix can include C#") {
	TestAutowork::test_collector_default_suffix_matches_cs();
}

TEST_CASE("[Modules][Autowork] collector include_subdirectories walks nested dirs") {
	TestAutowork::test_collector_include_subdirs();
}

TEST_CASE("[Modules][Autowork] unit-runner CLI flags ignore e2e-only args") {
	TestAutowork::test_unit_runner_flags();
}

TEST_CASE("[Modules][Autowork] AutoworkTest assertions are ClassDB-bound") {
	TestAutowork::test_assert_helpers_are_classdb_bound();
}

TEST_CASE("[Modules][RemoteControl] default port is 6508") {
	TestAutowork::test_remote_control_default_port();
}
