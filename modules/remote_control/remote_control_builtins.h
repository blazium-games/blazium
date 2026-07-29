/**************************************************************************/
/*  remote_control_builtins.h                                             */
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

#include "core/variant/dictionary.h"

class RemoteControlRegistry;

void remote_control_register_builtin_commands(RemoteControlRegistry *p_registry);

Dictionary remote_control_cmd_ping(const Dictionary &p_args);
Dictionary remote_control_cmd_get_status(const Dictionary &p_args);
Dictionary remote_control_cmd_list_commands(const Dictionary &p_args);
Dictionary remote_control_cmd_get_project_path(const Dictionary &p_args);
Dictionary remote_control_cmd_get_scene_tree(const Dictionary &p_args);
Dictionary remote_control_cmd_get_node(const Dictionary &p_args);
Dictionary remote_control_cmd_set_property(const Dictionary &p_args);
Dictionary remote_control_cmd_call_method(const Dictionary &p_args);
Dictionary remote_control_cmd_play_main_scene(const Dictionary &p_args);
Dictionary remote_control_cmd_stop_playing(const Dictionary &p_args);
Dictionary remote_control_cmd_save_scene(const Dictionary &p_args);
Dictionary remote_control_cmd_reload_filesystem(const Dictionary &p_args);

Dictionary remote_control_cmd_get_logs(const Dictionary &p_args);
Dictionary remote_control_cmd_get_errors(const Dictionary &p_args);
Dictionary remote_control_cmd_debugger_info(const Dictionary &p_args);
Dictionary remote_control_cmd_debugger_clear(const Dictionary &p_args);
Dictionary remote_control_cmd_debugger_status(const Dictionary &p_args);
Dictionary remote_control_cmd_debugger_stack(const Dictionary &p_args);
Dictionary remote_control_cmd_debugger_list_breakpoints(const Dictionary &p_args);
Dictionary remote_control_cmd_debugger_error_breaks(const Dictionary &p_args);
Dictionary remote_control_cmd_get_failed_run(const Dictionary &p_args);
Dictionary remote_control_cmd_autowork_run(const Dictionary &p_args);
Dictionary remote_control_cmd_autowork_status(const Dictionary &p_args);
Dictionary remote_control_cmd_autowork_results(const Dictionary &p_args);
