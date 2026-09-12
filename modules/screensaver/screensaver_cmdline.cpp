/**************************************************************************/
/*  screensaver_cmdline.cpp                                               */
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

#include "screensaver_cmdline.h"

#include "core/config/engine.h"
#include "core/config/project_settings.h"
#include "core/os/os.h"

#ifdef WINDOWS_ENABLED
// clang-format off
#include <windows.h>
#include <shellapi.h>
#include <tlhelp32.h>
// clang-format on
#endif

ScreensaverCmdline::Mode ScreensaverCmdline::mode = ScreensaverCmdline::MODE_NONE;
int64_t ScreensaverCmdline::parent_hwnd = 0;
String ScreensaverCmdline::cover_override;
int ScreensaverCmdline::screen_override = -1;
bool ScreensaverCmdline::virtual_rect_overridden = false;
Rect2i ScreensaverCmdline::virtual_rect_override;
bool ScreensaverCmdline::monitor_rect_overridden = false;
int ScreensaverCmdline::monitor_rect_override_index = -1;
Rect2i ScreensaverCmdline::monitor_rect_override;
bool ScreensaverCmdline::os_monitor_count_overridden = false;
int ScreensaverCmdline::os_monitor_count_override = 0;
bool ScreensaverCmdline::host_launch_overridden = false;
bool ScreensaverCmdline::host_launch_override = false;
bool ScreensaverCmdline::executable_is_scr_overridden = false;
bool ScreensaverCmdline::executable_is_scr_override = false;
bool ScreensaverCmdline::hwnd_valid_overridden = false;
int64_t ScreensaverCmdline::hwnd_valid_override = 0;

ScreensaverCmdline::Mode ScreensaverCmdline::get_mode() {
	return mode;
}

int64_t ScreensaverCmdline::get_parent_hwnd() {
	return parent_hwnd;
}

void ScreensaverCmdline::reset() {
	mode = MODE_NONE;
	parent_hwnd = 0;
	cover_override = String();
	screen_override = -1;
}

void ScreensaverCmdline::apply_default_mode_if_scr() {
	if (mode != MODE_NONE) {
		return;
	}
	bool is_scr = executable_is_scr_overridden ? executable_is_scr_override : false;
	if (!executable_is_scr_overridden && OS::get_singleton()) {
		is_scr = OS::get_singleton()->get_executable_path().get_extension().to_lower() == "scr";
	}
	if (is_scr) {
		mode = MODE_CONFIGURE;
	}
}

void ScreensaverCmdline::set_executable_is_scr_for_tests(bool p_scr) {
	executable_is_scr_overridden = true;
	executable_is_scr_override = p_scr;
}

void ScreensaverCmdline::clear_executable_is_scr_for_tests() {
	executable_is_scr_overridden = false;
	executable_is_scr_override = false;
}

void ScreensaverCmdline::set_target_screen(int p_screen) {
	if (p_screen >= 0) {
		screen_override = p_screen;
	}
}

int ScreensaverCmdline::get_target_screen_override() {
	return screen_override;
}

#ifdef WINDOWS_ENABLED
static DWORD _screensaver_parent_pid() {
	const DWORD pid = GetCurrentProcessId();
	HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
	if (snap == INVALID_HANDLE_VALUE) {
		return 0;
	}
	PROCESSENTRY32W pe;
	pe.dwSize = sizeof(pe);
	DWORD parent = 0;
	if (Process32FirstW(snap, &pe)) {
		do {
			if (pe.th32ProcessID == pid) {
				parent = pe.th32ParentProcessID;
				break;
			}
		} while (Process32NextW(snap, &pe));
	}
	CloseHandle(snap);
	return parent;
}

static String _screensaver_process_basename(DWORD p_pid) {
	if (p_pid == 0) {
		return String();
	}
	HANDLE h = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, p_pid);
	if (h == nullptr) {
		return String();
	}
	WCHAR buf[MAX_PATH] = {};
	DWORD size = MAX_PATH;
	String name;
	if (QueryFullProcessImageNameW(h, 0, buf, &size)) {
		name = String::utf16((const char16_t *)buf).get_file().to_lower();
	}
	CloseHandle(h);
	return name;
}
#endif

bool ScreensaverCmdline::is_host_screensaver_launch() {
	if (host_launch_overridden) {
		return host_launch_override;
	}
	if (parent_hwnd != 0) {
		return false;
	}
#ifdef WINDOWS_ENABLED
	const String parent = _screensaver_process_basename(_screensaver_parent_pid());
	if (parent == "explorer.exe" || parent == "winlogon.exe" || parent == "svchost.exe" || parent == "userinit.exe" || parent == "logonui.exe") {
		return true;
	}
	return parent.is_empty();
#else
	return false;
#endif
}

void ScreensaverCmdline::set_host_screensaver_launch_for_tests(bool p_host) {
	host_launch_overridden = true;
	host_launch_override = p_host;
}

void ScreensaverCmdline::clear_host_screensaver_launch_for_tests() {
	host_launch_overridden = false;
	host_launch_override = false;
}

Vector<Rect2i> ScreensaverCmdline::order_monitor_rects(const Vector<Rect2i> &p_rects, int p_primary) {
	Vector<Rect2i> ordered;
	if (p_rects.is_empty()) {
		return ordered;
	}
	int primary = p_primary;
	if (primary < 0 || primary >= p_rects.size()) {
		primary = 0;
		for (int i = 0; i < p_rects.size(); i++) {
			const Rect2i &r = p_rects[i];
			if (r.position.x <= 0 && r.position.x + r.size.width > 0 && r.position.y <= 0 && r.position.y + r.size.height > 0) {
				primary = i;
				break;
			}
		}
	}
	ordered.push_back(p_rects[primary]);
	Vector<int> rest;
	for (int i = 0; i < p_rects.size(); i++) {
		if (i != primary) {
			rest.push_back(i);
		}
	}
	for (int i = 0; i < rest.size(); i++) {
		for (int j = i + 1; j < rest.size(); j++) {
			const Rect2i &a = p_rects[rest[i]];
			const Rect2i &b = p_rects[rest[j]];
			if (b.position.x < a.position.x || (b.position.x == a.position.x && b.position.y < a.position.y)) {
				const int tmp = rest[i];
				rest.write[i] = rest[j];
				rest.write[j] = tmp;
			}
		}
	}
	for (int i = 0; i < rest.size(); i++) {
		ordered.push_back(p_rects[rest[i]]);
	}
	return ordered;
}

bool ScreensaverCmdline::parse_hwnd_token(const String &p_token, int64_t &r_hwnd) {
	String token = p_token.strip_edges();
	if (token.is_empty()) {
		return false;
	}
	if (token.begins_with("0x") || token.begins_with("0X")) {
		r_hwnd = token.hex_to_int();
		return r_hwnd != 0;
	}
	if (!token.is_valid_int()) {
		return false;
	}
	r_hwnd = token.to_int();
	return r_hwnd != 0;
}

bool ScreensaverCmdline::is_hwnd_valid(int64_t p_hwnd) {
	if (p_hwnd == 0) {
		return false;
	}
	if (hwnd_valid_overridden) {
		return p_hwnd == hwnd_valid_override;
	}
#ifdef WINDOWS_ENABLED
	return IsWindow((HWND)(INT_PTR)p_hwnd) != FALSE;
#else
	return false;
#endif
}

void ScreensaverCmdline::set_hwnd_valid_for_tests(int64_t p_hwnd) {
	hwnd_valid_overridden = true;
	hwnd_valid_override = p_hwnd;
}

void ScreensaverCmdline::clear_hwnd_valid_for_tests() {
	hwnd_valid_overridden = false;
	hwnd_valid_override = 0;
}

Size2i ScreensaverCmdline::parent_client_size() {
#ifdef WINDOWS_ENABLED
	if (parent_hwnd == 0) {
		return Size2i();
	}
	RECT rc = {};
	if (!GetClientRect((HWND)(INT_PTR)parent_hwnd, &rc)) {
		return Size2i();
	}
	return Size2i(rc.right - rc.left, rc.bottom - rc.top);
#else
	return Size2i();
#endif
}

static bool _is_switch(const String &p_arg, const char *p_letter) {
	if (p_arg.length() < 2 || p_arg[0] != '/') {
		return false;
	}
	return p_arg.substr(1, 1).to_upper() == String(p_letter).to_upper() && p_arg.length() == 2;
}

static bool _split_colon_hwnd(const String &p_arg, const char *p_letter, int64_t &r_hwnd) {
	if (p_arg.length() < 4 || p_arg[0] != '/') {
		return false;
	}
	if (p_arg.substr(1, 1).to_upper() != String(p_letter).to_upper()) {
		return false;
	}
	if (p_arg[2] != ':') {
		return false;
	}
	return ScreensaverCmdline::parse_hwnd_token(p_arg.substr(3), r_hwnd);
}

static bool _split_colon_screen(const String &p_arg, int &r_screen) {
	if (p_arg.length() < 4 || p_arg[0] != '/') {
		return false;
	}
	if (p_arg.substr(1, 1).to_upper() != "S") {
		return false;
	}
	if (p_arg[2] != ':') {
		return false;
	}
	const String token = p_arg.substr(3).strip_edges();
	if (!token.is_valid_int()) {
		return false;
	}
	const int screen = token.to_int();
	if (screen < 0) {
		return false;
	}
	r_screen = screen;
	return true;
}

static bool _parse_cover_mode(const String &p_token, String &r_mode) {
	const String token = p_token.strip_edges().to_lower();
	if (token == "single" || token == "virtual" || token == "clone") {
		r_mode = token;
		return true;
	}
	return false;
}

static bool _is_slash_letter(const String &p_arg, const char *p_letter) {
	if (p_arg.length() < 2 || p_arg[0] != '/') {
		return false;
	}
	return p_arg.substr(1, 1).to_upper() == String(p_letter).to_upper();
}

static bool _is_screen_index_token(const String &p_token) {
	if (!p_token.is_valid_int()) {
		return false;
	}
	const int64_t value = p_token.to_int();
	if (value < 0) {
		return false;
	}
	if (value <= 16) {
		return true;
	}
	return value < ScreensaverCmdline::os_monitor_count();
}

static bool _try_consume_configure(const String &p_arg, const String &p_next, int64_t &r_hwnd, bool &r_consumed_next) {
	r_consumed_next = false;
	r_hwnd = 0;
	if (!_is_slash_letter(p_arg, "c")) {
		return false;
	}

	if (p_arg.length() == 2) {
		int64_t hwnd = 0;
		if (ScreensaverCmdline::parse_hwnd_token(p_next, hwnd)) {
			r_hwnd = hwnd;
			r_consumed_next = true;
		}
		return true;
	}

	String rest = p_arg.substr(2);
	if (rest.begins_with(":")) {
		rest = rest.substr(1);
	}
	int64_t hwnd = 0;
	if (ScreensaverCmdline::parse_hwnd_token(rest, hwnd)) {
		r_hwnd = hwnd;
	}
	return true;
}

bool ScreensaverCmdline::try_consume(const String &p_arg, const String &p_next, bool &r_consumed_next) {
	r_consumed_next = false;

	if (p_arg == "-s" || p_arg == "--script" || p_arg == "--screen" || p_arg == "-p" || p_arg == "--project-manager") {
		return false;
	}

	if (p_arg == "--screensaver-cover") {
		String parsed;
		if (_parse_cover_mode(p_next, parsed)) {
			cover_override = parsed;
			r_consumed_next = true;
		}
		return true;
	}

	int screen = 0;
	if (_split_colon_screen(p_arg, screen)) {
		mode = MODE_RUN;
		screen_override = screen;
		return true;
	}

	if (_is_switch(p_arg, "s")) {
		if (_is_screen_index_token(p_next)) {
			mode = MODE_RUN;
			screen_override = p_next.to_int();
			r_consumed_next = true;
			return true;
		}
		int64_t hwnd = 0;
		if (parse_hwnd_token(p_next, hwnd) && is_hwnd_valid(hwnd)) {
			mode = MODE_PREVIEW;
			parent_hwnd = hwnd;
			r_consumed_next = true;
			return true;
		}
		mode = MODE_RUN;
		return true;
	}

	int64_t hwnd = 0;
	if (_split_colon_hwnd(p_arg, "p", hwnd)) {
		mode = MODE_PREVIEW;
		parent_hwnd = hwnd;
		return true;
	}
	if (_is_switch(p_arg, "p")) {
		mode = MODE_PREVIEW;
		if (parse_hwnd_token(p_next, hwnd)) {
			parent_hwnd = hwnd;
			r_consumed_next = true;
		}
		return true;
	}

	if (_try_consume_configure(p_arg, p_next, hwnd, r_consumed_next)) {
		mode = MODE_CONFIGURE;
		parent_hwnd = hwnd;
		return true;
	}

	if (_split_colon_hwnd(p_arg, "a", hwnd)) {
		mode = MODE_CHANGE_PASSWORD;
		parent_hwnd = hwnd;
		return true;
	}
	if (_is_switch(p_arg, "a")) {
		mode = MODE_CHANGE_PASSWORD;
		if (parse_hwnd_token(p_next, hwnd)) {
			parent_hwnd = hwnd;
			r_consumed_next = true;
		}
		return true;
	}

	if (parse_hwnd_token(p_arg, hwnd) && is_hwnd_valid(hwnd)) {
		mode = MODE_PREVIEW;
		parent_hwnd = hwnd;
		return true;
	}

	return false;
}

String ScreensaverCmdline::resolved_cover_mode() {
	if (!cover_override.is_empty()) {
		return cover_override;
	}

	if (mode == MODE_RUN && screen_override < 0 && is_host_screensaver_launch()) {
		return "virtual";
	}

	String cover = "single";
	ProjectSettings *ps = ProjectSettings::get_singleton();
	if (ps) {
		const String setting = String(ps->get_setting("blazium/screensaver/cover_mode", String())).strip_edges().to_lower();
		if (setting == "single" || setting == "virtual" || setting == "clone") {
			cover = setting;
		} else {
			cover = bool(ps->get_setting("blazium/screensaver/cover_all_screens", true)) ? String("virtual") : String("single");
		}
	}

	if (cover == "virtual") {
		if (screen_override >= 0) {
			return "single";
		}
		if (ps && int(ps->get_setting("blazium/screensaver/screen", -1)) >= 0) {
			return "single";
		}
	}
	return cover;
}

int ScreensaverCmdline::resolved_target_screen(int p_current_screen) {
	if (screen_override >= 0) {
		return screen_override;
	}
	if (p_current_screen >= 0) {
		return p_current_screen;
	}
	ProjectSettings *ps = ProjectSettings::get_singleton();
	if (ps) {
		const int setting = int(ps->get_setting("blazium/screensaver/screen", -1));
		if (setting >= 0) {
			return setting;
		}
	}
	return p_current_screen;
}

int ScreensaverCmdline::normalize_screen_index(int p_screen, int p_count) {
	if (p_count <= 0) {
		return p_screen;
	}
	if (p_screen >= 0 && p_screen < p_count) {
		return p_screen;
	}

	if (p_screen >= 1 && (p_screen - 1) < p_count) {
		return p_screen - 1;
	}
	return -1;
}

void ScreensaverCmdline::set_os_monitor_count_for_tests(int p_count) {
	os_monitor_count_overridden = true;
	os_monitor_count_override = p_count;
}

void ScreensaverCmdline::clear_os_monitor_count_for_tests() {
	os_monitor_count_overridden = false;
	os_monitor_count_override = 0;
}

bool ScreensaverCmdline::should_clear_create_position() {
	ProjectSettings *ps = ProjectSettings::get_singleton();
	if (ps == nullptr || !bool(ps->get_setting("blazium/screensaver/enabled", false))) {
		return false;
	}
	if (mode != MODE_RUN) {
		return false;
	}
	return true;
}

void ScreensaverCmdline::set_virtual_rect_for_tests(const Rect2i &p_rect) {
	virtual_rect_overridden = true;
	virtual_rect_override = p_rect;
}

void ScreensaverCmdline::clear_virtual_rect_for_tests() {
	virtual_rect_overridden = false;
	virtual_rect_override = Rect2i();
}

void ScreensaverCmdline::set_monitor_rect_for_tests(int p_screen, const Rect2i &p_rect) {
	monitor_rect_overridden = true;
	monitor_rect_override_index = p_screen;
	monitor_rect_override = p_rect;
}

void ScreensaverCmdline::clear_monitor_rect_for_tests() {
	monitor_rect_overridden = false;
	monitor_rect_override_index = -1;
	monitor_rect_override = Rect2i();
}

Rect2i ScreensaverCmdline::virtual_screen_rect() {
	if (virtual_rect_overridden) {
		return virtual_rect_override;
	}
#ifdef WINDOWS_ENABLED
	return Rect2i(
			GetSystemMetrics(SM_XVIRTUALSCREEN),
			GetSystemMetrics(SM_YVIRTUALSCREEN),
			GetSystemMetrics(SM_CXVIRTUALSCREEN),
			GetSystemMetrics(SM_CYVIRTUALSCREEN));
#else
	return Rect2i(0, 0, 1920, 1080);
#endif
}

#ifdef WINDOWS_ENABLED
struct _ScreensaverMonitorCollect {
	Vector<Rect2i> rects;
	int primary = 0;
};

static BOOL CALLBACK _screensaver_monitor_collect(HMONITOR p_monitor, HDC, LPRECT p_rect, LPARAM p_data) {
	_ScreensaverMonitorCollect *data = reinterpret_cast<_ScreensaverMonitorCollect *>(p_data);
	MONITORINFO mi = {};
	mi.cbSize = sizeof(mi);
	RECT src = {};
	if (p_monitor && GetMonitorInfo(p_monitor, &mi)) {
		src = mi.rcMonitor;
		if (mi.dwFlags & MONITORINFOF_PRIMARY) {
			data->primary = data->rects.size();
		}
	} else if (p_rect) {
		src = *p_rect;
	} else {
		return TRUE;
	}
	data->rects.push_back(Rect2i(src.left, src.top, src.right - src.left, src.bottom - src.top));
	return TRUE;
}

static Vector<Rect2i> _os_monitors_primary_first() {
	_ScreensaverMonitorCollect data;
	EnumDisplayMonitors(nullptr, nullptr, _screensaver_monitor_collect, reinterpret_cast<LPARAM>(&data));
	return ScreensaverCmdline::order_monitor_rects(data.rects, data.primary);
}
#endif

int ScreensaverCmdline::os_monitor_count() {
	if (os_monitor_count_overridden) {
		return os_monitor_count_override;
	}
#ifdef WINDOWS_ENABLED
	return _os_monitors_primary_first().size();
#else
	DisplayServer *ds = DisplayServer::get_singleton();
	if (ds) {
		return ds->get_screen_count();
	}
	return 1;
#endif
}

Rect2i ScreensaverCmdline::monitor_os_rect(int p_screen) {
	if (p_screen < 0) {
		return Rect2i();
	}
	if (monitor_rect_overridden && monitor_rect_override_index == p_screen) {
		return monitor_rect_override;
	}
#ifdef WINDOWS_ENABLED
	const Vector<Rect2i> ordered = _os_monitors_primary_first();
	if (p_screen >= ordered.size()) {
		return Rect2i();
	}
	return ordered[p_screen];
#else
	return Rect2i();
#endif
}

Rect2i ScreensaverCmdline::virtual_os_rect() {
	return virtual_screen_rect();
}

Rect2i ScreensaverCmdline::cover_os_rect(int p_screen) {
	return monitor_os_rect(p_screen);
}

Rect2i ScreensaverCmdline::cover_rect(int p_screen) {
	return monitor_rect(p_screen);
}

Rect2i ScreensaverCmdline::monitor_rect(int p_screen) {
	if (p_screen < 0) {
		return Rect2i();
	}
	if (monitor_rect_overridden && monitor_rect_override_index == p_screen) {
		return monitor_rect_override;
	}
	const Rect2i os = monitor_os_rect(p_screen);
	if (os.size.width <= 0 || os.size.height <= 0) {
		return Rect2i();
	}
	const Rect2i virt = virtual_screen_rect();
	return Rect2i(os.position.x - virt.position.x, os.position.y - virt.position.y, os.size.width, os.size.height);
}

bool ScreensaverCmdline::should_spawn_clone_windows() {
	if (mode != MODE_RUN) {
		return false;
	}
	if (resolved_cover_mode() != "clone") {
		return false;
	}
	if (Engine::get_singleton() && Engine::get_singleton()->is_editor_hint()) {
		return false;
	}
	DisplayServer *ds = DisplayServer::get_singleton();
	if (ds == nullptr) {
		return false;
	}
	const String name = ds->get_name();
	if (name == "headless" || name == "dummy") {
		return false;
	}
	return ScreensaverCmdline::os_monitor_count() >= 2;
}

static void _ingest_screen_tokens(const Vector<String> &p_args) {
	for (int i = 0; i < p_args.size(); i++) {
		const String &a = p_args[i];
		if ((a == "--screen" || a == "-screen") && i + 1 < p_args.size() && p_args[i + 1].is_valid_int()) {
			ScreensaverCmdline::set_target_screen(p_args[i + 1].to_int());
			continue;
		}
		if (a.begins_with("--screen=") || a.begins_with("-screen=")) {
			const String n = a.get_slicec('=', 1);
			if (n.is_valid_int()) {
				ScreensaverCmdline::set_target_screen(n.to_int());
			}
			continue;
		}
		if (a.length() >= 4 && (a[0] == '/' || a[0] == '-') && a.substr(1, 1).to_upper() == "S" && a[2] == ':') {
			const String n = a.substr(3).strip_edges();
			if (n.is_valid_int() && n.to_int() >= 0) {
				ScreensaverCmdline::set_target_screen(n.to_int());
			}
			continue;
		}
		if ((a == "/s" || a == "/S") && i + 1 < p_args.size() && _is_screen_index_token(p_args[i + 1])) {
			ScreensaverCmdline::set_target_screen(p_args[i + 1].to_int());
		}
	}
}

void ScreensaverCmdline::ingest_from_os_command_line() {
#ifdef WINDOWS_ENABLED
	int argc = 0;
	LPWSTR *argv = CommandLineToArgvW(GetCommandLineW(), &argc);
	if (argv) {
		Vector<String> raw;
		for (int i = 0; i < argc; i++) {
			raw.push_back(String::utf16((const char16_t *)argv[i]));
		}
		LocalFree(argv);
		_ingest_screen_tokens(raw);
	}
#endif
	if (OS::get_singleton() == nullptr) {
		return;
	}
	Vector<String> leftover;
	for (const String &a : OS::get_singleton()->get_cmdline_args()) {
		leftover.push_back(a);
	}
	for (const String &a : OS::get_singleton()->get_cmdline_user_args()) {
		leftover.push_back(a);
	}
	_ingest_screen_tokens(leftover);
	if (screen_override < 0) {
		const String env_screen = OS::get_singleton()->get_environment("BLAZIUM_SCREENSAVER_SCREEN");
		if (env_screen.is_valid_int() && env_screen.to_int() >= 0) {
			set_target_screen(env_screen.to_int());
		}
	}
	if (cover_override.is_empty()) {
		const String env_cover = OS::get_singleton()->get_environment("BLAZIUM_SCREENSAVER_COVER").strip_edges().to_lower();
		if (env_cover == "single" || env_cover == "virtual" || env_cover == "clone") {
			cover_override = env_cover;
		}
	}
}

void ScreensaverCmdline::apply_recorded(DisplayServer::WindowMode &r_window_mode, uint32_t &r_window_flags, int64_t &r_embed_parent_hwnd, Vector2i &r_window_position, Size2i &r_window_size, int &r_screen, bool &r_use_position) {
	r_use_position = false;
	ingest_from_os_command_line();
	ProjectSettings *ps = ProjectSettings::get_singleton();
	if (ps == nullptr || !bool(ps->get_setting("blazium/screensaver/enabled", false))) {
		return;
	}

	apply_default_mode_if_scr();
	Mode applied = mode;
	if (applied == MODE_NONE) {
		return;
	}

	switch (applied) {
		case MODE_RUN: {
			if (r_screen >= 0 && screen_override < 0) {
				screen_override = r_screen;
			}
			const String cover = resolved_cover_mode();
			r_window_flags |= DisplayServer::WINDOW_FLAG_BORDERLESS_BIT;
			r_embed_parent_hwnd = 0;

			r_window_mode = DisplayServer::WINDOW_MODE_WINDOWED;
			r_window_flags |= DisplayServer::WINDOW_FLAG_ALWAYS_ON_TOP_BIT;
			if (cover != "virtual") {
				r_screen = resolved_target_screen(r_screen);
			}
		} break;
		case MODE_PREVIEW: {
			r_window_mode = DisplayServer::WINDOW_MODE_WINDOWED;
			r_window_flags |= DisplayServer::WINDOW_FLAG_BORDERLESS_BIT;
			r_embed_parent_hwnd = 0;
			if (parent_hwnd != 0) {
				r_embed_parent_hwnd = parent_hwnd;
				// Do not let DisplayServer center a standalone popup; the
				// preview HWND is reparented as a WS_CHILD at 0,0 in the pane.
				r_window_position = Vector2i();
				r_use_position = true;
				const Size2i parent_size = parent_client_size();
				if (parent_size.width > 0 && parent_size.height > 0) {
					r_window_size = parent_size;
				}
			}
		} break;
		case MODE_CONFIGURE:
		case MODE_CHANGE_PASSWORD: {
			r_window_mode = DisplayServer::WINDOW_MODE_WINDOWED;
			r_window_flags &= ~DisplayServer::WINDOW_FLAG_BORDERLESS_BIT;
			r_embed_parent_hwnd = 0;
			if (r_window_size.width <= 0 || r_window_size.height <= 0 || r_window_size.width > 520 || r_window_size.height > 360) {
				r_window_size = Size2i(480, 320);
			}
		} break;
		default:
			break;
	}
}
