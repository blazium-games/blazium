/**************************************************************************/
/*  os_ps3.cpp                                                            */
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

#include "os_ps3.h"

#include "display_server_ps3.h"
#include "platform/ps3/ps3_boot_splash.h"
#include "ps3_wrapper.h"

#include "core/config/project_settings.h"
#include "drivers/unix/dir_access_unix.h"
#include "drivers/unix/file_access_unix.h"
#include "main/main.h"
#include "servers/audio_server.h"

#include <unistd.h>

#ifdef PS3_ENABLED
#include <sysutil/sysutil.h>
#endif

OS_PS3 *OS_PS3::get_singleton() {
	return static_cast<OS_PS3 *>(OS::get_singleton());
}

String OS_PS3::_get_game_dir() const {
	const String exec_path = get_executable_path();
	if (!exec_path.is_empty()) {
		return exec_path.get_base_dir();
	}
	return "/dev_hdd0/game/BLAZ00001";
}

OS_PS3::OS_PS3() {
	AudioDriverManager::add_driver(&audio_driver);
}

OS_PS3::~OS_PS3() {
}

#include "core/string/print_string.h"

void OS_PS3::initialize() {
	print_line("PS3: OS initialize begin");
	OS_Unix::initialize_core();
	DisplayServerPS3::register_ps3_driver();
	print_line("PS3: OS initialize end");
}

void OS_PS3::set_main_loop(MainLoop *p_main_loop) {
	main_loop = p_main_loop;
}

MainLoop *OS_PS3::get_main_loop() const {
	return main_loop;
}

void OS_PS3::delete_main_loop() {
	if (main_loop) {
		memdelete(main_loop);
	}
	main_loop = nullptr;
}

void OS_PS3::finalize() {
	delete_main_loop();
	if (joypad) {
		memdelete(joypad);
		joypad = nullptr;
	}
}

bool OS_PS3::main_loop_iterate() {
#ifdef PS3_ENABLED
	sysUtilCheckCallback();
#endif

	if (joypad) {
		joypad->process();
	}

	audio_driver.mix_audio();

	DisplayServer::get_singleton()->process_events();

	return Main::iteration();
}

void OS_PS3::delay_usec(uint32_t p_usec) const {
	ps3_boot_splash_refresh_if_due();
	OS_Unix::delay_usec(p_usec);
}

bool OS_PS3::_check_internal_feature_support(const String &p_feature) {
	if (p_feature == "systemdirs") {
		return true;
	}
	return false;
}

void OS_PS3::initialize_joypads() {
	if (!joypad) {
		joypad = memnew(JoypadPS3);
	}
}

String OS_PS3::get_name() const {
	return "PS3";
}

String OS_PS3::get_distribution_name() const {
	return "PSL1GHT";
}

String OS_PS3::get_version() const {
	return "1.0";
}

String OS_PS3::get_version_alias() const {
	return "PS3";
}

String OS_PS3::get_executable_path() const {
	
	return _get_game_dir().path_join("EBOOT.BIN");
}

String OS_PS3::get_resource_dir() const {
	return _get_game_dir().path_join("USRDIR");
}

String OS_PS3::get_user_data_dir() const {
	return "/dev_hdd0/home/00000001/blazium";
}

String OS_PS3::get_data_path() const {
	return "/dev_hdd0/home/00000001/blazium";
}

String OS_PS3::get_cache_path() const {
	return "/dev_hdd0/tmp/blazium";
}

String OS_PS3::get_config_path() const {
	return "/dev_hdd0/home/00000001/blazium";
}

Error OS_PS3::execute(const String &p_path, const List<String> &p_arguments, String *r_pipe, int *r_exitcode, bool read_stderr, Mutex *p_pipe_mutex, bool p_open_console) {
	return ERR_UNAVAILABLE;
}

Error OS_PS3::create_process(const String &p_path, const List<String> &p_arguments, ProcessID *r_child_id, bool p_open_console) {
	return ERR_UNAVAILABLE;
}

Error OS_PS3::kill(const ProcessID &p_pid) {
	return ERR_UNAVAILABLE;
}

bool OS_PS3::is_process_running(const ProcessID &p_pid) const {
	return false;
}

int OS_PS3::get_processor_count() const {
	return 2;
}
