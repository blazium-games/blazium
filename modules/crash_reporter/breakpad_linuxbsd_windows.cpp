/**************************************************************************/
/*  breakpad_linuxbsd_windows.cpp                                         */
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

#ifdef USE_BREAKPAD

#include "breakpad_linuxbsd_windows.h"

#include <stdio.h>
#include <string.h>
#include <string>

#ifdef WINDOWS_ENABLED
#include "client/windows/handler/exception_handler.h"
#include <windows.h>
#else
#include "client/linux/handler/exception_handler.h"
#include "client/linux/handler/minidump_descriptor.h"
#endif

static google_breakpad::ExceptionHandler *g_handler = nullptr;
static char g_dump_dir[1024] = { 0 };
static char g_last_dump[2048] = { 0 };
static char g_app_id[256] = { 0 };
static char g_app_name[256] = { 0 };
static char g_app_version[128] = { 0 };
static char g_engine_version[256] = { 0 };
static char g_engine_hash[128] = { 0 };
static char g_os[64] = { 0 };
static char g_arch[64] = { 0 };
static char g_build_channel[64] = { 0 };
static char g_contact_url[512] = { 0 };
static char g_reporter_path[1024] = { 0 };
static bool g_spawn_on_crash = false;

static void _copy_cstr(char *p_dst, size_t p_dst_size, const char *p_src) {
	if (!p_dst || p_dst_size == 0) {
		return;
	}
	if (!p_src) {
		p_dst[0] = 0;
		return;
	}
	strncpy(p_dst, p_src, p_dst_size - 1);
	p_dst[p_dst_size - 1] = 0;
}

static void _write_sidecar(const char *p_dump_path) {
	if (!p_dump_path || !p_dump_path[0]) {
		return;
	}
	char json_path[2048];
	_copy_cstr(json_path, sizeof(json_path), p_dump_path);
	size_t n = strlen(json_path);
	if (n >= 4 && (strcmp(json_path + n - 4, ".dmp") == 0 || strcmp(json_path + n - 4, ".DMP") == 0)) {
		json_path[n - 4] = 0;
	}
	strncat(json_path, ".json", sizeof(json_path) - strlen(json_path) - 1);

	const char *slash = strrchr(p_dump_path, '/');
#ifdef WINDOWS_ENABLED
	const char *bslash = strrchr(p_dump_path, '\\');
	if (!slash || (bslash && bslash > slash)) {
		slash = bslash;
	}
#endif
	const char *file = slash ? slash + 1 : p_dump_path;
	char id[256];
	_copy_cstr(id, sizeof(id), file);
	n = strlen(id);
	if (n >= 4 && (strcmp(id + n - 4, ".dmp") == 0 || strcmp(id + n - 4, ".DMP") == 0)) {
		id[n - 4] = 0;
	}

	FILE *f = fopen(json_path, "wb");
	if (!f) {
		return;
	}
	fprintf(f,
			"{\n"
			"\t\"id\": \"%s\",\n"
			"\t\"engine_version\": \"%s\",\n"
			"\t\"engine_hash\": \"%s\",\n"
			"\t\"app_id\": \"%s\",\n"
			"\t\"app_name\": \"%s\",\n"
			"\t\"app_version\": \"%s\",\n"
			"\t\"build_channel\": \"%s\",\n"
			"\t\"contact_url\": \"%s\",\n"
			"\t\"os\": \"%s\",\n"
			"\t\"arch\": \"%s\",\n"
			"\t\"dump_path\": \"%s\"\n"
			"}\n",
			id, g_engine_version, g_engine_hash, g_app_id, g_app_name, g_app_version, g_build_channel, g_contact_url, g_os, g_arch, p_dump_path);
	fclose(f);
}

#ifdef WINDOWS_ENABLED
static void _spawn_reporter_win(const char *p_dump_path) {
	if (!g_spawn_on_crash || !g_reporter_path[0]) {
		return;
	}
	char cmd[4096];
	STARTUPINFOA si;
	PROCESS_INFORMATION pi;
	ZeroMemory(&si, sizeof(si));
	si.cb = sizeof(si);
	ZeroMemory(&pi, sizeof(pi));

	const char *slash = strrchr(p_dump_path, '\\');
	if (!slash) {
		slash = strrchr(p_dump_path, '/');
	}
	const char *file = slash ? slash + 1 : p_dump_path;
	char id[256];
	_copy_cstr(id, sizeof(id), file);
	size_t n = strlen(id);
	if (n >= 4) {
		id[n - 4] = 0;
	}
	snprintf(cmd, sizeof(cmd), "\"%s\" --crash-dir \"%s\" --report-id \"%s\" --app-id \"%s\"",
			g_reporter_path, g_dump_dir, id, g_app_id);
	CreateProcessA(nullptr, cmd, nullptr, nullptr, FALSE, 0, nullptr, nullptr, &si, &pi);
	if (pi.hThread) {
		CloseHandle(pi.hThread);
	}
	if (pi.hProcess) {
		CloseHandle(pi.hProcess);
	}
}

static bool _win_dump_callback(const wchar_t *dump_path, const wchar_t *minidump_id, void *, EXCEPTION_POINTERS *, MDRawAssertionInfo *, bool succeeded) {
	if (!succeeded || !dump_path || !minidump_id) {
		return succeeded;
	}
	char path_utf8[2048];
	char id_utf8[256];
	WideCharToMultiByte(CP_UTF8, 0, dump_path, -1, path_utf8, sizeof(path_utf8), nullptr, nullptr);
	WideCharToMultiByte(CP_UTF8, 0, minidump_id, -1, id_utf8, sizeof(id_utf8), nullptr, nullptr);
	snprintf(g_last_dump, sizeof(g_last_dump), "%s\\%s.dmp", path_utf8, id_utf8);
	_write_sidecar(g_last_dump);
	fprintf(stderr, "Crash dump created at: %s\nPlease attach this file when reporting issues.\n", g_last_dump);
	fflush(stderr);
	_spawn_reporter_win(g_last_dump);
	return succeeded;
}
#else
static bool _linux_dump_callback(const google_breakpad::MinidumpDescriptor &descriptor, void *, bool succeeded) {
	if (!succeeded) {
		return succeeded;
	}
	_copy_cstr(g_last_dump, sizeof(g_last_dump), descriptor.path());
	_write_sidecar(g_last_dump);
	fprintf(stderr, "Crash dump created at: %s\nPlease attach this file when reporting issues.\n", g_last_dump);
	fflush(stderr);
	return succeeded;
}
#endif

void initialize_breakpad(bool p_register_handlers) {
	if (g_handler) {
		return;
	}
	if (!g_dump_dir[0]) {
#ifdef WINDOWS_ENABLED
		char tmp[MAX_PATH];
		if (GetTempPathA(MAX_PATH, tmp) == 0) {
			_copy_cstr(g_dump_dir, sizeof(g_dump_dir), "C:\\temp");
		} else {
			_copy_cstr(g_dump_dir, sizeof(g_dump_dir), tmp);
		}
#else
		_copy_cstr(g_dump_dir, sizeof(g_dump_dir), "/tmp");
#endif
	}

#ifdef WINDOWS_ENABLED
	wchar_t wpath[1024];
	MultiByteToWideChar(CP_UTF8, 0, g_dump_dir, -1, wpath, 1024);
	const int handler_types = p_register_handlers ? google_breakpad::ExceptionHandler::HANDLER_ALL : google_breakpad::ExceptionHandler::HANDLER_NONE;
	g_handler = new google_breakpad::ExceptionHandler(std::wstring(wpath), nullptr, _win_dump_callback, nullptr, handler_types);
#else
	google_breakpad::MinidumpDescriptor descriptor(g_dump_dir);
	g_handler = new google_breakpad::ExceptionHandler(descriptor, nullptr, _linux_dump_callback, nullptr, p_register_handlers, -1);
#endif
}

void disable_breakpad() {
	if (!g_handler) {
		return;
	}
	delete g_handler;
	g_handler = nullptr;
}

void breakpad_set_dump_path(const char *p_utf8_path) {
	_copy_cstr(g_dump_dir, sizeof(g_dump_dir), p_utf8_path);
	if (!g_handler || !p_utf8_path || !p_utf8_path[0]) {
		return;
	}
#ifdef WINDOWS_ENABLED
	wchar_t wpath[1024];
	MultiByteToWideChar(CP_UTF8, 0, p_utf8_path, -1, wpath, 1024);
	g_handler->set_dump_path(std::wstring(wpath));
#else
	g_handler->set_minidump_descriptor(google_breakpad::MinidumpDescriptor(p_utf8_path));
#endif
}

void breakpad_cache_identity(const char *p_app_id, const char *p_app_name, const char *p_app_version, const char *p_engine_version, const char *p_engine_hash, const char *p_os, const char *p_arch, const char *p_build_channel, const char *p_contact_url) {
	_copy_cstr(g_app_id, sizeof(g_app_id), p_app_id);
	_copy_cstr(g_app_name, sizeof(g_app_name), p_app_name);
	_copy_cstr(g_app_version, sizeof(g_app_version), p_app_version);
	_copy_cstr(g_engine_version, sizeof(g_engine_version), p_engine_version);
	_copy_cstr(g_engine_hash, sizeof(g_engine_hash), p_engine_hash);
	_copy_cstr(g_os, sizeof(g_os), p_os);
	_copy_cstr(g_arch, sizeof(g_arch), p_arch);
	_copy_cstr(g_build_channel, sizeof(g_build_channel), p_build_channel);
	_copy_cstr(g_contact_url, sizeof(g_contact_url), p_contact_url);
}

void breakpad_cache_spawn(const char *p_reporter_utf8, bool p_spawn_on_crash) {
	_copy_cstr(g_reporter_path, sizeof(g_reporter_path), p_reporter_utf8);
	g_spawn_on_crash = p_spawn_on_crash;
}

void breakpad_handle_signal(int p_sig) {
#ifndef WINDOWS_ENABLED
	if (g_handler) {
		g_handler->SimulateSignalDelivery(p_sig);
	}
#else
	(void)p_sig;
#endif
}

void breakpad_handle_exception_pointers(void *p_exinfo) {
#ifdef WINDOWS_ENABLED
	if (g_handler && p_exinfo) {
		g_handler->WriteMinidumpForException(static_cast<EXCEPTION_POINTERS *>(p_exinfo));
	}
#else
	(void)p_exinfo;
#endif
}

void breakpad_write_minidump() {
	if (g_handler) {
		g_handler->WriteMinidump();
	}
}

const char *breakpad_last_dump_path() {
	return g_last_dump;
}

#endif
