/**************************************************************************/
/*  screensaver.cpp                                                       */
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

#include "screensaver.h"

#include "core/config/engine.h"
#include "core/config/project_settings.h"
#include "core/input/input.h"
#include "core/io/file_access.h"
#include "core/io/resource_loader.h"
#include "core/object/class_db.h"
#include "core/os/os.h"
#include "modules/screensaver/screensaver_password.h"
#include "scene/gui/box_container.h"
#include "scene/gui/button.h"
#include "scene/gui/check_box.h"
#include "scene/gui/dialogs.h"
#include "scene/gui/label.h"
#include "scene/gui/line_edit.h"
#include "scene/gui/texture_rect.h"
#include "scene/main/canvas_item.h"
#include "scene/main/scene_tree.h"
#include "scene/main/window.h"
#include "servers/display_server.h"

#ifdef WINDOWS_ENABLED
#include <windows.h>
#endif

Screensaver *Screensaver::singleton = nullptr;

Screensaver *Screensaver::get_singleton() {
	return singleton;
}

void Screensaver::register_project_settings() {
	GLOBAL_DEF_BASIC("blazium/screensaver/enabled", false);
	GLOBAL_DEF_BASIC(PropertyInfo(Variant::STRING, "blazium/screensaver/configure_scene", PROPERTY_HINT_FILE, "*.tscn,*.scn"), String());
	GLOBAL_DEF_BASIC("blazium/screensaver/password_enabled", false);
	GLOBAL_DEF_BASIC(PropertyInfo(Variant::STRING, "blazium/screensaver/unlock_scene", PROPERTY_HINT_FILE, "*.tscn,*.scn"), String());
	GLOBAL_DEF_BASIC(PropertyInfo(Variant::STRING, "blazium/screensaver/change_password_scene", PROPERTY_HINT_FILE, "*.tscn,*.scn"), String());
	GLOBAL_DEF_BASIC("blazium/screensaver/lock_workstation_on_exit", false);
	GLOBAL_DEF_BASIC("blazium/screensaver/quit_on_input", true);
	GLOBAL_DEF_BASIC(PropertyInfo(Variant::INT, "blazium/screensaver/quit_on_mouse_move_threshold", PROPERTY_HINT_RANGE, "0,64,1"), 5);
	GLOBAL_DEF_BASIC("blazium/screensaver/cover_all_screens", true);
	GLOBAL_DEF_BASIC(PropertyInfo(Variant::STRING, "blazium/screensaver/cover_mode", PROPERTY_HINT_ENUM, "single,virtual,clone"), "virtual");
	GLOBAL_DEF_BASIC(PropertyInfo(Variant::INT, "blazium/screensaver/screen", PROPERTY_HINT_RANGE, "-1,16,1"), -1);
}

Screensaver::Screensaver() {
	singleton = this;
}

Screensaver::~Screensaver() {
	_disconnect_frame_hook();
	if (singleton == this) {
		singleton = nullptr;
	}
}

void Screensaver::_bind_methods() {
	ClassDB::bind_method(D_METHOD("get_mode"), &Screensaver::get_mode);
	ClassDB::bind_method(D_METHOD("is_preview"), &Screensaver::is_preview);
	ClassDB::bind_method(D_METHOD("is_password_enabled"), &Screensaver::is_password_enabled);
	ClassDB::bind_method(D_METHOD("set_password_enabled", "enabled"), &Screensaver::set_password_enabled);
	ClassDB::bind_method(D_METHOD("has_password"), &Screensaver::has_password);
	ClassDB::bind_method(D_METHOD("request_exit"), &Screensaver::request_exit);
	ClassDB::bind_method(D_METHOD("verify_password", "plain"), &Screensaver::verify_password);
	ClassDB::bind_method(D_METHOD("set_password", "old_plain", "new_plain"), &Screensaver::set_password);
	ClassDB::bind_method(D_METHOD("clear_password", "current_plain"), &Screensaver::clear_password);

	ADD_SIGNAL(MethodInfo("unlock_succeeded"));
	ADD_SIGNAL(MethodInfo("unlock_failed"));
	ADD_SIGNAL(MethodInfo("password_changed"));

	BIND_ENUM_CONSTANT(MODE_DISABLED);
	BIND_ENUM_CONSTANT(MODE_RUN);
	BIND_ENUM_CONSTANT(MODE_PREVIEW);
	BIND_ENUM_CONSTANT(MODE_CONFIGURE);
	BIND_ENUM_CONSTANT(MODE_CHANGE_PASSWORD);
}

Screensaver::Mode Screensaver::get_mode() const {
	if (!bool(GLOBAL_GET("blazium/screensaver/enabled"))) {
		return MODE_DISABLED;
	}
	return (Mode)ScreensaverCmdline::get_mode();
}

bool Screensaver::is_preview() const {
	return get_mode() == MODE_PREVIEW;
}

bool Screensaver::is_password_enabled() const {
	if (ScreensaverPassword::has_password_enabled_override()) {
		return ScreensaverPassword::get_password_enabled();
	}
	return bool(GLOBAL_GET("blazium/screensaver/password_enabled"));
}

void Screensaver::set_password_enabled(bool p_enabled) {
	ScreensaverPassword::set_password_enabled(p_enabled);
	if (ProjectSettings::get_singleton()) {
		ProjectSettings::get_singleton()->set_setting("blazium/screensaver/password_enabled", p_enabled);
	}
}

bool Screensaver::has_password() const {
	return ScreensaverPassword::has_password();
}

bool Screensaver::verify_password(const String &p_plain) const {
	return ScreensaverPassword::verify(p_plain);
}

Error Screensaver::set_password(const String &p_old_plain, const String &p_new_plain) {
	const Error err = ScreensaverPassword::set_password(p_old_plain, p_new_plain);
	if (err == OK) {
		emit_signal(SNAME("password_changed"));
	}
	return err;
}

Error Screensaver::clear_password(const String &p_current_plain) {
	return ScreensaverPassword::clear_password(p_current_plain);
}

void Screensaver::_connect_frame_hook() {
	if (frame_hooked) {
		return;
	}
	SceneTree *tree = SceneTree::get_singleton();
	if (tree == nullptr) {
		return;
	}
	tree->connect("process_frame", callable_mp(this, &Screensaver::process_frame));
	frame_hooked = true;
}

void Screensaver::_disconnect_frame_hook() {
	if (!frame_hooked) {
		return;
	}
	SceneTree *tree = SceneTree::get_singleton();
	if (tree) {
		tree->disconnect("process_frame", callable_mp(this, &Screensaver::process_frame));
	}
	frame_hooked = false;
}

void Screensaver::setup_runtime() {
	if (Engine::get_singleton() && Engine::get_singleton()->is_editor_hint()) {
		return;
	}
	ScreensaverCmdline::apply_default_mode_if_scr();
	if (get_mode() == MODE_DISABLED) {
		return;
	}

	_connect_frame_hook();

	if (get_mode() == MODE_RUN && Input::get_singleton()) {
		Input::get_singleton()->set_mouse_mode(Input::MOUSE_MODE_HIDDEN);
	}

	if (get_mode() == MODE_RUN) {
		ScreensaverCmdline::ingest_from_os_command_line();
		_ensure_run_chrome();
		_apply_run_screens();
		_hide_restricted_main_if_needed();
	} else if (get_mode() == MODE_PREVIEW) {
		_hide_restricted_main_if_needed();
		_size_preview_to_parent();
	} else if (get_mode() == MODE_CONFIGURE) {
		_show_configure_instance();
		_load_or_show_scene("blazium/screensaver/configure_scene", &Screensaver::_show_configure_dialog);
	} else if (get_mode() == MODE_CHANGE_PASSWORD) {
		_show_configure_instance();
		_load_or_show_scene("blazium/screensaver/change_password_scene", &Screensaver::_show_change_password_dialog);
	}
}

#ifdef WINDOWS_ENABLED
static void _native_place_hwnd(HWND p_hwnd, const Rect2i &p_os_rect) {
	if (p_hwnd == nullptr || p_os_rect.size.width <= 0 || p_os_rect.size.height <= 0) {
		return;
	}

	LONG_PTR style = GetWindowLongPtr(p_hwnd, GWL_STYLE);
	style &= ~(WS_CAPTION | WS_THICKFRAME | WS_SYSMENU | WS_MINIMIZEBOX | WS_MAXIMIZEBOX);
	style |= WS_POPUP | WS_VISIBLE;
	SetWindowLongPtr(p_hwnd, GWL_STYLE, style);

	LONG_PTR ex = GetWindowLongPtr(p_hwnd, GWL_EXSTYLE);
	ex |= WS_EX_TOPMOST;
	SetWindowLongPtr(p_hwnd, GWL_EXSTYLE, ex);

	SetWindowPos(p_hwnd, HWND_TOPMOST, p_os_rect.position.x, p_os_rect.position.y, p_os_rect.size.width, p_os_rect.size.height, SWP_SHOWWINDOW | SWP_FRAMECHANGED);
}

static HWND _main_hwnd() {
	DisplayServer *ds = DisplayServer::get_singleton();
	if (ds == nullptr) {
		return nullptr;
	}
	return (HWND)ds->window_get_native_handle(DisplayServer::WINDOW_HANDLE);
}
#endif

void Screensaver::_place_hwnd(const Rect2i &p_os_rect) {
	if (p_os_rect.size.width <= 0 || p_os_rect.size.height <= 0) {
		return;
	}

#ifdef WINDOWS_ENABLED
	const HWND hwnd = _main_hwnd();
	_native_place_hwnd(hwnd, p_os_rect);
	_sync_root_window_size(p_os_rect.size);
	_native_place_hwnd(hwnd, p_os_rect);
	if (!place_log_written) {
		_write_place_log(p_os_rect, (int64_t)hwnd);
		place_log_written = true;
	}
#else
	_sync_root_window_size(p_os_rect.size);
#endif
}

void Screensaver::_place_window_hwnd(Window *p_window, const Rect2i &p_os_rect) {
	if (p_window == nullptr || p_os_rect.size.width <= 0 || p_os_rect.size.height <= 0) {
		return;
	}

#ifdef WINDOWS_ENABLED
	DisplayServer *ds = DisplayServer::get_singleton();
	if (ds == nullptr) {
		return;
	}
	const DisplayServer::WindowID id = p_window->get_window_id();
	if (id == DisplayServer::INVALID_WINDOW_ID) {
		return;
	}
	const HWND hwnd = (HWND)ds->window_get_native_handle(DisplayServer::WINDOW_HANDLE, id);
	_native_place_hwnd(hwnd, p_os_rect);
#endif
}

void Screensaver::_sync_root_window_size(const Size2i &p_size) {
	if (p_size.width <= 0 || p_size.height <= 0) {
		return;
	}
	SceneTree *tree = SceneTree::get_singleton();
	if (tree == nullptr || tree->get_root() == nullptr) {
		return;
	}
	Window *root = tree->get_root();
	const Size2i max = root->get_max_size();
	if (max != Size2i() && (max.x < p_size.x || max.y < p_size.y)) {
		root->set_max_size(Size2i());
	}
	if (root->get_min_size() != p_size) {
		root->set_min_size(p_size);
	}
	if (root->get_max_size() != p_size) {
		root->set_max_size(p_size);
	}
	if (root->get_size() != p_size) {
		root->set_size(p_size);
	}
}

void Screensaver::_write_place_log(const Rect2i &p_target, int64_t p_hwnd) {
	if (OS::get_singleton() == nullptr) {
		return;
	}
	String temp = OS::get_singleton()->get_environment("TEMP");
	if (temp.is_empty()) {
		temp = OS::get_singleton()->get_environment("TMP");
	}
	if (temp.is_empty()) {
		return;
	}
	const String path = temp.path_join("blazium_screensaver_place.txt");
	Ref<FileAccess> f = FileAccess::open(path, FileAccess::WRITE);
	if (f.is_null()) {
		return;
	}
	f->store_line("cover=" + ScreensaverCmdline::resolved_cover_mode());
	f->store_line(String("host_launch=") + (ScreensaverCmdline::is_host_screensaver_launch() ? "true" : "false"));
	f->store_line("screen_override=" + itos(ScreensaverCmdline::get_target_screen_override()));
	f->store_line("screen=" + itos(ScreensaverCmdline::resolved_target_screen(0)));
#ifdef WINDOWS_ENABLED
	f->store_line(String("cmdline=") + String::utf16((const char16_t *)GetCommandLineW()));
#endif
	if (OS::get_singleton()) {
		f->store_line("env_screen=" + OS::get_singleton()->get_environment("BLAZIUM_SCREENSAVER_SCREEN"));
		f->store_line("env_cover=" + OS::get_singleton()->get_environment("BLAZIUM_SCREENSAVER_COVER"));
	}
	const int count = ScreensaverCmdline::os_monitor_count();
	f->store_line("os_monitor_count=" + itos(count));
	for (int i = 0; i < count; i++) {
		const Rect2i r = ScreensaverCmdline::monitor_os_rect(i);
		f->store_line(vformat("monitor[%d]=%d,%d %dx%d", i, r.position.x, r.position.y, r.size.width, r.size.height));
	}
	f->store_line(vformat("target=%d,%d %dx%d", p_target.position.x, p_target.position.y, p_target.size.width, p_target.size.height));
	f->store_line(vformat("hwnd=0x%x", (uint64_t)p_hwnd));
#ifdef WINDOWS_ENABLED
	RECT got = {};
	if (p_hwnd) {
		GetWindowRect((HWND)p_hwnd, &got);
	}
	f->store_line(vformat("GetWindowRect=%d,%d %dx%d", (int)got.left, (int)got.top, (int)(got.right - got.left), (int)(got.bottom - got.top)));
	f->store_line(vformat("virtual=%d,%d %dx%d", ScreensaverCmdline::virtual_screen_rect().position.x, ScreensaverCmdline::virtual_screen_rect().position.y, ScreensaverCmdline::virtual_screen_rect().size.width, ScreensaverCmdline::virtual_screen_rect().size.height));
#endif
}

bool Screensaver::_virtual_span_clipped(const Rect2i &p_want) const {
#ifdef WINDOWS_ENABLED
	const HWND hwnd = _main_hwnd();
	if (hwnd == nullptr || p_want.size.width <= 0 || p_want.size.height <= 0) {
		return false;
	}
	if (ScreensaverCmdline::os_monitor_count() < 2) {
		return false;
	}
	RECT got = {};
	GetWindowRect(hwnd, &got);
	const int got_w = got.right - got.left;
	const int got_h = got.bottom - got.top;
	if (got_w <= 0 || got_h <= 0) {
		return false;
	}
	return (got_w * 2 < p_want.size.width) || (got_h * 2 < p_want.size.height);
#else
	return false;
#endif
}

void Screensaver::_pin_window_to_screen(int p_screen) {
	const int count = ScreensaverCmdline::os_monitor_count();
	const int screen = ScreensaverCmdline::normalize_screen_index(p_screen, count);
	if (screen < 0) {
		return;
	}
	if (count > 0 && screen >= count) {
		return;
	}

	_place_hwnd(ScreensaverCmdline::cover_os_rect(screen));
}

void Screensaver::_apply_run_screens() {
	const String cover = ScreensaverCmdline::resolved_cover_mode();
	const int count = ScreensaverCmdline::os_monitor_count();
	const int screen = ScreensaverCmdline::normalize_screen_index(ScreensaverCmdline::resolved_target_screen(0), count);

	if (cover == "virtual") {
		const Rect2i virt = ScreensaverCmdline::virtual_os_rect();
		_place_hwnd(virt);
		if (_virtual_span_clipped(virt)) {
			_pin_window_to_screen(0);
			if (!clones_spawned) {
				_spawn_clone_windows(0);
				clones_spawned = true;
			}
			_place_clone_windows();
		}
	} else if (cover == "single" || cover == "clone") {
		_pin_window_to_screen(screen);
		if (cover == "clone") {
			if (!clones_spawned) {
				_spawn_clone_windows(screen >= 0 ? screen : 0);
				clones_spawned = true;
			}
			_place_clone_windows();
		}
	}
}

void Screensaver::_place_clone_windows() {
	for (int i = 0; i < clone_covers.size(); i++) {
		if (clone_covers[i].window == nullptr) {
			continue;
		}
		_place_window_hwnd(clone_covers[i].window, ScreensaverCmdline::cover_os_rect(clone_covers[i].screen));
	}
}

void Screensaver::_spawn_clone_windows(int p_skip_screen) {
	DisplayServer *ds = DisplayServer::get_singleton();
	SceneTree *tree = SceneTree::get_singleton();
	if (ds == nullptr || tree == nullptr || tree->get_root() == nullptr) {
		return;
	}
	if (Engine::get_singleton() && Engine::get_singleton()->is_editor_hint()) {
		return;
	}
	const String name = ds->get_name();
	if (name == "headless" || name == "dummy") {
		return;
	}

	Window *root = tree->get_root();
	const int count = ScreensaverCmdline::os_monitor_count();
	const Ref<Texture2D> tex = root->get_texture();

	for (int i = 0; i < count; i++) {
		if (i == p_skip_screen) {
			continue;
		}
		bool exists = false;
		for (int c = 0; c < clone_covers.size(); c++) {
			if (clone_covers[c].screen == i) {
				exists = true;
				break;
			}
		}
		if (exists) {
			continue;
		}

		Window *clone = memnew(Window);
		clone->set_flag(Window::FLAG_BORDERLESS, true);
		clone->set_flag(Window::FLAG_ALWAYS_ON_TOP, true);
		TextureRect *rect = memnew(TextureRect);
		rect->set_anchors_and_offsets_preset(Control::PRESET_FULL_RECT);
		rect->set_expand_mode(TextureRect::EXPAND_IGNORE_SIZE);
		rect->set_stretch_mode(TextureRect::STRETCH_SCALE);
		rect->set_texture(tex);
		clone->add_child(rect);
		root->add_child(clone);
		clone->set_mode(Window::MODE_WINDOWED);
		clone->show();
		const Rect2i os_rect = ScreensaverCmdline::cover_os_rect(i);
		if (os_rect.size.width > 0 && os_rect.size.height > 0) {
			clone->set_min_size(os_rect.size);
			clone->set_max_size(os_rect.size);
			clone->set_size(os_rect.size);
		}
		_place_window_hwnd(clone, os_rect);
		CloneCover cover;
		cover.window = clone;
		cover.screen = i;
		clone_covers.push_back(cover);
	}
}

bool Screensaver::_is_restricted_scene_path(const String &p_path) const {
	if (p_path.is_empty()) {
		return false;
	}
	const String unlock = String(GLOBAL_GET("blazium/screensaver/unlock_scene")).strip_edges();
	const String configure = String(GLOBAL_GET("blazium/screensaver/configure_scene")).strip_edges();
	const String change = String(GLOBAL_GET("blazium/screensaver/change_password_scene")).strip_edges();
	return p_path == unlock || p_path == configure || p_path == change;
}

void Screensaver::_hide_current_scene() {
	SceneTree *tree = SceneTree::get_singleton();
	Node *cur = tree ? tree->get_current_scene() : nullptr;
	if (cur == nullptr) {
		return;
	}
	CanvasItem *ci = Object::cast_to<CanvasItem>(cur);
	if (ci) {
		ci->set_visible(false);
		return;
	}
	for (int i = 0; i < cur->get_child_count(); i++) {
		CanvasItem *child = Object::cast_to<CanvasItem>(cur->get_child(i));
		if (child) {
			child->set_visible(false);
		}
	}
}

void Screensaver::_hide_restricted_main_if_needed() {
	const String main = String(GLOBAL_GET("application/run/main_scene")).strip_edges();
	if (!_is_restricted_scene_path(main)) {
		return;
	}
	_hide_current_scene();
}

void Screensaver::_size_preview_to_parent() {
	const int64_t parent = ScreensaverCmdline::get_parent_hwnd();
	if (parent == 0) {
		return;
	}

#ifdef WINDOWS_ENABLED
	const HWND hwnd = _main_hwnd();
	const HWND parent_hwnd = (HWND)(INT_PTR)parent;
	if (hwnd == nullptr || !ScreensaverCmdline::is_hwnd_valid(parent)) {
		_sync_root_window_size(ScreensaverCmdline::parent_client_size());
		return;
	}

	LONG_PTR style = GetWindowLongPtr(hwnd, GWL_STYLE);
	style &= ~(WS_POPUP | WS_CAPTION | WS_THICKFRAME | WS_SYSMENU | WS_MINIMIZEBOX | WS_MAXIMIZEBOX | WS_OVERLAPPED);
	style |= WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | WS_CLIPCHILDREN;
	SetWindowLongPtr(hwnd, GWL_STYLE, style);

	LONG_PTR ex = GetWindowLongPtr(hwnd, GWL_EXSTYLE);
	ex &= ~(WS_EX_APPWINDOW | WS_EX_TOPMOST | WS_EX_WINDOWEDGE | WS_EX_DLGMODALFRAME);
	ex |= WS_EX_TOOLWINDOW;
	SetWindowLongPtr(hwnd, GWL_EXSTYLE, ex);

	if (GetParent(hwnd) != parent_hwnd) {
		SetParent(hwnd, parent_hwnd);
	}

	RECT rc = {};
	GetClientRect(parent_hwnd, &rc);
	const int width = rc.right - rc.left;
	const int height = rc.bottom - rc.top;
	SetWindowPos(hwnd, HWND_TOP, 0, 0, width, height, SWP_SHOWWINDOW | SWP_FRAMECHANGED | SWP_NOACTIVATE);
	_sync_root_window_size(Size2i(width, height));
#else
	_sync_root_window_size(ScreensaverCmdline::parent_client_size());
#endif
}

void Screensaver::_ensure_run_chrome() {
	SceneTree *tree = SceneTree::get_singleton();
	if (tree == nullptr || tree->get_root() == nullptr) {
		return;
	}
	Window *root = tree->get_root();
	root->set_flag(Window::FLAG_BORDERLESS, true);
	root->set_flag(Window::FLAG_ALWAYS_ON_TOP, true);
}

void Screensaver::_ensure_dialog_chrome() {
	if (Input::get_singleton()) {
		Input::get_singleton()->set_mouse_mode(Input::MOUSE_MODE_VISIBLE);
	}
	SceneTree *tree = SceneTree::get_singleton();
	if (tree == nullptr || tree->get_root() == nullptr) {
		return;
	}
	Window *root = tree->get_root();
	root->set_flag(Window::FLAG_BORDERLESS, false);
	root->set_flag(Window::FLAG_ALWAYS_ON_TOP, false);
	root->set_min_size(Size2i());
	root->set_max_size(Size2i());
	if (root->get_size().x > 520 || root->get_size().y > 360) {
		root->set_size(Size2i(480, 320));
	}
}

void Screensaver::_raise_dialog_hwnd() {
#ifdef WINDOWS_ENABLED
	const HWND hwnd = _main_hwnd();
	if (hwnd == nullptr) {
		return;
	}
	const int64_t parent = ScreensaverCmdline::get_parent_hwnd();
	if (parent != 0 && ScreensaverCmdline::is_hwnd_valid(parent)) {
		const HWND owner = (HWND)(INT_PTR)parent;
		if (GetWindowLongPtr(hwnd, GWLP_HWNDPARENT) != (LONG_PTR)owner) {
			SetWindowLongPtr(hwnd, GWLP_HWNDPARENT, (LONG_PTR)owner);
		}
	}
	ShowWindow(hwnd, SW_SHOW);
	ShowWindow(hwnd, SW_RESTORE);
	SetWindowPos(hwnd, HWND_TOP, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_SHOWWINDOW);
	SetForegroundWindow(hwnd);
#endif
}

void Screensaver::_show_configure_instance() {
	_prepare_dialog_window();
}

void Screensaver::_prepare_dialog_window() {
	_hide_current_scene();
	_ensure_dialog_chrome();
	_raise_dialog_hwnd();
}

void Screensaver::_load_or_show_scene(const String &p_setting, void (Screensaver::*p_builtin)()) {
	const String scene = String(GLOBAL_GET(p_setting)).strip_edges();
	const String unlock = String(GLOBAL_GET("blazium/screensaver/unlock_scene")).strip_edges();
	const bool is_unlock = p_setting == "blazium/screensaver/unlock_scene";

	if (is_unlock) {
		if (get_mode() != MODE_RUN || !is_password_enabled() || !has_password()) {
			return;
		}
	} else {
		_prepare_dialog_window();
		if (!scene.is_empty() && !unlock.is_empty() && scene == unlock) {
			(this->*p_builtin)();
			return;
		}
	}

	if (!scene.is_empty() && ResourceLoader::exists(scene)) {
		SceneTree *tree = SceneTree::get_singleton();
		if (tree && tree->change_scene_to_file(scene) == OK) {
			_raise_dialog_hwnd();
			return;
		}
	}

	if (!is_unlock) {
		_prepare_dialog_window();
	}
	(this->*p_builtin)();
	if (!is_unlock) {
		_raise_dialog_hwnd();
	}
}

void Screensaver::request_exit() {
	if (get_mode() == MODE_PREVIEW) {
		return;
	}
	if (get_mode() != MODE_RUN) {
		_finish_exit();
		return;
	}
	if (is_password_enabled() && has_password()) {
		if (!unlock_open) {
			unlock_open = true;
			_load_or_show_scene("blazium/screensaver/unlock_scene", &Screensaver::_show_unlock_dialog);
		}
		return;
	}
	_finish_exit();
}

void Screensaver::_finish_exit() {
#ifdef WINDOWS_ENABLED
	if (bool(GLOBAL_GET("blazium/screensaver/lock_workstation_on_exit"))) {
		LockWorkStation();
	}
#endif
	SceneTree *tree = SceneTree::get_singleton();
	if (tree) {
		tree->quit();
	}
}

bool Screensaver::_should_quit_on_input() const {
	return get_mode() == MODE_RUN && bool(GLOBAL_GET("blazium/screensaver/quit_on_input")) && !unlock_open;
}

void Screensaver::process_frame() {
	if (get_mode() == MODE_RUN) {
		_ensure_run_chrome();
		_apply_run_screens();
	} else if (get_mode() == MODE_PREVIEW) {
		_size_preview_to_parent();
	} else if (get_mode() == MODE_CONFIGURE || get_mode() == MODE_CHANGE_PASSWORD) {
		_ensure_dialog_chrome();
		_raise_dialog_hwnd();
	}

	if (!_should_quit_on_input() || Input::get_singleton() == nullptr || DisplayServer::get_singleton() == nullptr) {
		return;
	}

	if (!input_armed) {
		input_armed = true;
		mouse_origin = DisplayServer::get_singleton()->mouse_get_position();
		mouse_origin_set = true;
		return;
	}

	if (Input::get_singleton()->is_anything_pressed()) {
		request_exit();
		return;
	}

	const int threshold = int(GLOBAL_GET("blazium/screensaver/quit_on_mouse_move_threshold"));
	const Vector2 pos = DisplayServer::get_singleton()->mouse_get_position();
	if (mouse_origin_set && pos.distance_to(mouse_origin) >= threshold) {
		request_exit();
	}
}

void Screensaver::_show_configure_dialog() {
	SceneTree *tree = SceneTree::get_singleton();
	if (tree == nullptr || tree->get_root() == nullptr) {
		return;
	}
	AcceptDialog *dlg = memnew(AcceptDialog);
	dlg->set_title("Screensaver Setup");
	dlg->set_ok_button_text("OK");
	VBoxContainer *vb = memnew(VBoxContainer);
	CheckBox *pw = memnew(CheckBox);
	pw->set_text("Password protect");
	pw->set_pressed(is_password_enabled());
	pw->connect("toggled", callable_mp(this, &Screensaver::_on_configure_password_toggled));
	vb->add_child(pw);
	Button *change = memnew(Button);
	change->set_text("Change Password");
	change->connect(SceneStringName(pressed), callable_mp(this, &Screensaver::_on_configure_change_password));
	vb->add_child(change);
	dlg->add_child(vb);
	tree->get_root()->add_child(dlg);
	dlg->popup_centered();
	dlg->connect("confirmed", callable_mp(this, &Screensaver::_finish_exit));
	dlg->connect("canceled", callable_mp(this, &Screensaver::_finish_exit));
}

void Screensaver::_on_configure_password_toggled(bool p_pressed) {
	set_password_enabled(p_pressed);
}

void Screensaver::_on_configure_change_password() {
	_show_change_password_dialog();
}

void Screensaver::_show_unlock_dialog() {
	SceneTree *tree = SceneTree::get_singleton();
	if (tree == nullptr || tree->get_root() == nullptr) {
		return;
	}
	if (Input::get_singleton()) {
		Input::get_singleton()->set_mouse_mode(Input::MOUSE_MODE_VISIBLE);
	}
	AcceptDialog *dlg = memnew(AcceptDialog);
	dlg->set_name("UnlockDialog");
	dlg->set_title("Unlock Screensaver");
	dlg->set_ok_button_text("Unlock");
	VBoxContainer *vb = memnew(VBoxContainer);
	LineEdit *edit = memnew(LineEdit);
	edit->set_secret(true);
	edit->set_placeholder("Password");
	edit->set_name("Password");
	vb->add_child(edit);
	Label *status = memnew(Label);
	status->set_name("UnlockStatus");
	status->set_visible(false);
	vb->add_child(status);
	dlg->add_child(vb);
	tree->get_root()->add_child(dlg);
	dlg->set_meta("password_edit", edit);
	dlg->popup_centered();
	dlg->connect("confirmed", callable_mp(this, &Screensaver::_on_unlock_accepted));
}

void Screensaver::_on_unlock_accepted() {
	SceneTree *tree = SceneTree::get_singleton();
	Node *root = tree ? tree->get_root() : nullptr;
	LineEdit *edit = nullptr;
	if (root) {
		edit = Object::cast_to<LineEdit>(root->find_child("Password", true, false));
	}
	const String plain = edit ? edit->get_text() : String();
	if (verify_password(plain)) {
		emit_signal(SNAME("unlock_succeeded"));
		_finish_exit();
	} else {
		emit_signal(SNAME("unlock_failed"));
		if (root) {
			Label *status = Object::cast_to<Label>(root->find_child("UnlockStatus", true, false));
			if (status) {
				status->set_text("Incorrect password");
				status->set_visible(true);
			}
			AcceptDialog *dlg = Object::cast_to<AcceptDialog>(root->find_child("UnlockDialog", true, false));
			if (dlg) {
				dlg->popup_centered();
			}
		}
	}
}

void Screensaver::_show_change_password_dialog() {
	SceneTree *tree = SceneTree::get_singleton();
	if (tree == nullptr || tree->get_root() == nullptr) {
		return;
	}
	AcceptDialog *dlg = memnew(AcceptDialog);
	dlg->set_title("Change Screensaver Password");
	dlg->set_ok_button_text("Save");
	VBoxContainer *vb = memnew(VBoxContainer);
	LineEdit *current = memnew(LineEdit);
	current->set_secret(true);
	current->set_placeholder("Current password");
	current->set_name("CurrentPassword");
	vb->add_child(current);
	LineEdit *next = memnew(LineEdit);
	next->set_secret(true);
	next->set_placeholder("New password (empty to clear)");
	next->set_name("NewPassword");
	vb->add_child(next);
	LineEdit *confirm = memnew(LineEdit);
	confirm->set_secret(true);
	confirm->set_placeholder("Confirm new password");
	confirm->set_name("ConfirmPassword");
	vb->add_child(confirm);
	dlg->add_child(vb);
	tree->get_root()->add_child(dlg);
	dlg->popup_centered();
	dlg->connect("confirmed", callable_mp(this, &Screensaver::_on_change_password_accepted));
	if (get_mode() == MODE_CHANGE_PASSWORD) {
		dlg->connect("canceled", callable_mp(this, &Screensaver::_finish_exit));
	}
}

void Screensaver::_on_change_password_accepted() {
	SceneTree *tree = SceneTree::get_singleton();
	Node *root = tree ? tree->get_root() : nullptr;
	if (root == nullptr) {
		return;
	}
	LineEdit *current = Object::cast_to<LineEdit>(root->find_child("CurrentPassword", true, false));
	LineEdit *next = Object::cast_to<LineEdit>(root->find_child("NewPassword", true, false));
	LineEdit *confirm = Object::cast_to<LineEdit>(root->find_child("ConfirmPassword", true, false));
	const String old_plain = current ? current->get_text() : String();
	const String new_plain = next ? next->get_text() : String();
	const String confirm_plain = confirm ? confirm->get_text() : String();
	if (new_plain != confirm_plain) {
		return;
	}
	if (set_password(old_plain, new_plain) != OK) {
		return;
	}
	if (get_mode() == MODE_CHANGE_PASSWORD) {
		_finish_exit();
	}
}
