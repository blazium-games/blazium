/**************************************************************************/
/*  ps3_singleton.cpp                                                     */
/**************************************************************************/
/*                         This file is part of:                          */
/*                             BLAZIUM ENGINE                             */
/*                          https://blazium.app                           */
/**************************************************************************/

#include "ps3_singleton.h"

#include "core/config/engine.h"
#include "core/object/class_db.h"
#include "core/os/os.h"

#ifdef PS3_ENABLED
#include <sys/process.h>
#endif

PlayStation3 *PlayStation3::singleton = nullptr;

PlayStation3 *PlayStation3::get_singleton() {
	return singleton;
}

void PlayStation3::_bind_methods() {
	ClassDB::bind_method(D_METHOD("request_exit"), &PlayStation3::request_exit);
	ClassDB::bind_method(D_METHOD("get_platform_name"), &PlayStation3::get_platform_name);
	ClassDB::bind_method(D_METHOD("get_memory_available"), &PlayStation3::get_memory_available);
	ClassDB::bind_method(D_METHOD("get_title_id"), &PlayStation3::get_title_id);
	ClassDB::bind_method(D_METHOD("get_content_path"), &PlayStation3::get_content_path);
}

void PlayStation3::request_exit() {
#ifdef PS3_ENABLED
	sysProcessExit(0);
#endif
}

String PlayStation3::get_platform_name() const {
	return "PlayStation 3";
}

int PlayStation3::get_memory_available() const {
#ifdef PS3_ENABLED
	// Typical LV2 user memory available to homebrew; exact query varies by firmware.
	return 227 * 1024 * 1024;
#else
	return 0;
#endif
}

String PlayStation3::get_title_id() const {
	const String exec_path = OS::get_singleton()->get_executable_path();
	if (!exec_path.is_empty()) {
		return exec_path.get_base_dir().get_file();
	}
	return "BLAZ00001";
}

String PlayStation3::get_content_path() const {
	return OS::get_singleton()->get_resource_dir();
}

PlayStation3::PlayStation3() {
	singleton = this;
}

PlayStation3::~PlayStation3() {
	if (singleton == this) {
		singleton = nullptr;
	}
}
