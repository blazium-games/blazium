/**************************************************************************/
/*  breakpad_linuxbsd_windows.h                                           */
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

#ifdef USE_BREAKPAD

void initialize_breakpad(bool p_register_handlers);
void disable_breakpad();
void breakpad_set_dump_path(const char *p_utf8_path);
void breakpad_cache_identity(const char *p_app_id, const char *p_app_name, const char *p_app_version, const char *p_engine_version, const char *p_engine_hash, const char *p_os, const char *p_arch, const char *p_build_channel, const char *p_contact_url, const char *p_build_id);
void breakpad_cache_spawn(const char *p_reporter_utf8, bool p_spawn_on_crash);
void breakpad_handle_signal(int p_sig);
void breakpad_handle_exception_pointers(void *p_exinfo);
void breakpad_write_minidump();
const char *breakpad_last_dump_path();

#endif
