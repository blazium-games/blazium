/**************************************************************************/
/*  ps3_boot_log.cpp                                                      */
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

#include "ps3_boot_log.h"

#include "core/string/print_string.h"

#include <cstdio>
#include <cstring>

#ifndef PS3_TITLE_ID
#define PS3_TITLE_ID "BLAZ00001"
#endif

extern "C" {
void ps3_boot_log_set_event(const char *p_event);
}

static FILE *g_boot_log_file = nullptr;
static bool g_boot_log_tried_open = false;

static char g_hdd0_boot_log_path[128];
static bool g_hdd0_boot_log_path_ready = false;

static void _ensure_hdd0_boot_log_path() {
	if (g_hdd0_boot_log_path_ready) {
		return;
	}
	snprintf(
			g_hdd0_boot_log_path,
			sizeof(g_hdd0_boot_log_path),
			"/dev_hdd0/game/%s/USRDIR/boot.log",
			PS3_TITLE_ID);
	g_hdd0_boot_log_path_ready = true;
}

static const char *BOOT_LOG_PATHS[] = {
	nullptr,
	"/app_home/USRDIR/boot.log",
	"/app_home/boot.log",
	"/dev_hdd0/home/00000001/blazium/boot.log",
	nullptr,
};

static void _append_via_fopen_paths(const char *const *p_paths, const char *p_event) {
	if (!p_event || !p_paths) {
		return;
	}
	_ensure_hdd0_boot_log_path();
	BOOT_LOG_PATHS[0] = g_hdd0_boot_log_path;

	for (int i = 0; p_paths[i] != nullptr; i++) {
		FILE *f = fopen(p_paths[i], "a");
		if (f) {
			fprintf(f, "%s\n", p_event);
			fflush(f);
			fclose(f);
			return;
		}
	}
}

static void _ensure_boot_log_open() {
	if (g_boot_log_tried_open) {
		return;
	}
	g_boot_log_tried_open = true;
	_ensure_hdd0_boot_log_path();
	BOOT_LOG_PATHS[0] = g_hdd0_boot_log_path;

	for (int i = 0; BOOT_LOG_PATHS[i] != nullptr; i++) {
		g_boot_log_file = fopen(BOOT_LOG_PATHS[i], "a");
		if (g_boot_log_file) {
			break;
		}
	}
}

void ps3_boot_log(const char *p_event) {
	if (!p_event) {
		return;
	}

	print_line(vformat("PS3: %s", p_event));
	ps3_boot_log_set_event(p_event);
	_ensure_boot_log_open();
	if (!g_boot_log_file) {
		_append_via_fopen_paths(BOOT_LOG_PATHS, p_event);
		return;
	}

	fprintf(g_boot_log_file, "%s\n", p_event);
	fflush(g_boot_log_file);
}

void ps3_boot_logf(const char *p_event, const String &p_detail) {
	if (!p_event) {
		return;
	}

	const String line = vformat("%s %s", p_event, p_detail);
	print_line(vformat("PS3: %s", line));
	ps3_boot_log_set_event(line.utf8().get_data());
	_ensure_boot_log_open();
	if (!g_boot_log_file) {
		_append_via_fopen_paths(BOOT_LOG_PATHS, line.utf8().get_data());
		return;
	}

	const CharString utf8 = line.utf8();
	fprintf(g_boot_log_file, "%s\n", utf8.get_data());
	fflush(g_boot_log_file);
}
