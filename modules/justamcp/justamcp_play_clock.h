/**************************************************************************/
/*  justamcp_play_clock.h                                                 */
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

#include "core/string/ustring.h"
#include "core/templates/vector.h"
#include "core/variant/dictionary.h"

Dictionary justamcp_validate_play_launch_args(const Dictionary &p_args);
Dictionary justamcp_validate_runtime_step_args(const Dictionary &p_args);
Dictionary justamcp_validate_runtime_step_until_args(const Dictionary &p_args);
Dictionary justamcp_play_clock_snapshot();
void justamcp_note_play_launch_args(const Dictionary &p_args);
void justamcp_note_play_frozen(bool p_frozen);
void justamcp_note_play_time_scale(double p_scale);
void justamcp_prepare_play_clock_environment(const Dictionary &p_args);
void justamcp_clear_play_clock_environment();
bool justamcp_try_play_clock_command(const String &p_command, const Dictionary &p_params, Dictionary &r_result);
bool justamcp_gdscript_source_compiles(const String &p_source, String &r_error);
bool justamcp_script_write_requires_validate(const String &p_path, const Dictionary &p_params);
Dictionary justamcp_guard_gdscript_write(const String &p_path, const String &p_content, const Dictionary &p_params);
Dictionary justamcp_guard_scene_text(const String &p_path, const String &p_previous, const String &p_next);
int justamcp_export_smoke_timeout_ms(const Dictionary &p_args);
String justamcp_normalize_export_smoke_path(const String &p_path);
bool justamcp_export_smoke_path_allowed(const String &p_candidate, const Vector<String> &p_export_directories);
bool justamcp_export_smoke_direct_argv(const String &p_path, String &r_executable, Vector<String> &r_arguments);
