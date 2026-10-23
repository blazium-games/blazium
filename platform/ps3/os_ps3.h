/**************************************************************************/
/*  os_ps3.h                                                              */
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

#include "crash_handler_ps3.h"
#include "drivers/audio/audio_driver_ps3.h"
#include "joypad_ps3.h"

#include "core/input/input.h"
#include "drivers/unix/os_unix.h"

class OS_PS3 : public OS_Unix {
	MainLoop *main_loop = nullptr;
	JoypadPS3 *joypad = nullptr;
	AudioDriverPS3 audio_driver;
	CrashHandler crash_handler;

	String _get_game_dir() const;

protected:
	void initialize() override;
	void set_main_loop(MainLoop *p_main_loop) override;
	void delete_main_loop() override;
	void finalize() override;
	bool _check_internal_feature_support(const String &p_feature) override;

	MainLoop *get_main_loop() const override;

public:
	static OS_PS3 *get_singleton();

	void initialize_joypads() override;

	bool main_loop_iterate();

	String get_name() const override;
	String get_distribution_name() const override;
	String get_version() const override;
	String get_version_alias() const override;

	String get_executable_path() const override;
	String get_resource_dir() const override;
	String get_user_data_dir() const override;
	String get_data_path() const override;
	String get_cache_path() const override;
	String get_config_path() const override;

	Error execute(const String &p_path, const List<String> &p_arguments, String *r_pipe = nullptr, int *r_exitcode = nullptr, bool read_stderr = false, Mutex *p_pipe_mutex = nullptr, bool p_open_console = false) override;
	Error create_process(const String &p_path, const List<String> &p_arguments, ProcessID *r_child_id = nullptr, bool p_open_console = false) override;
	Error kill(const ProcessID &p_pid) override;
	bool is_process_running(const ProcessID &p_pid) const override;

	int get_processor_count() const override;

	void delay_usec(uint32_t p_usec) const override;

	OS_PS3();
	~OS_PS3();
};
