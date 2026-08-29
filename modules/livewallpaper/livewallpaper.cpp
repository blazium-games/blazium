/**************************************************************************/
/*  livewallpaper.cpp                                                     */
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

#include "livewallpaper.h"

#include "core/config/engine.h"
#include "core/config/project_settings.h"
#include "core/io/image.h"
#include "core/math/math_funcs.h"
#include "core/object/class_db.h"
#include "modules/livewallpaper/livewallpaper_workerw.h"
#include "scene/main/scene_tree.h"
#include "scene/main/window.h"
#include "servers/display_server.h"
#include "servers/rendering_server.h"

#ifdef WINDOWS_ENABLED
#include <windows.h>
#endif

LiveWallpaper *LiveWallpaper::singleton = nullptr;

LiveWallpaper *LiveWallpaper::get_singleton() {
	return singleton;
}

void LiveWallpaper::register_project_settings() {
	GLOBAL_DEF_BASIC("blazium/livewallpaper/enabled", false);
	GLOBAL_DEF_BASIC("blazium/livewallpaper/cover_all_screens", true);
	GLOBAL_DEF_BASIC("blazium/livewallpaper/pause_on_lock", true);
}

LiveWallpaper::LiveWallpaper() {
	singleton = this;
}

LiveWallpaper::~LiveWallpaper() {
	_disconnect_frame_hook();
	_free_overlay_canvas();
	_release_sync();
	if (singleton == this) {
		singleton = nullptr;
	}
}

void LiveWallpaper::_bind_methods() {
	ClassDB::bind_method(D_METHOD("get_mode"), &LiveWallpaper::get_mode);
	ClassDB::bind_method(D_METHOD("is_attached"), &LiveWallpaper::is_attached);
	ClassDB::bind_method(D_METHOD("get_workerw"), &LiveWallpaper::get_workerw);
	ClassDB::bind_method(D_METHOD("request_exit"), &LiveWallpaper::request_exit);

	BIND_ENUM_CONSTANT(MODE_DISABLED);
	BIND_ENUM_CONSTANT(MODE_RUN);
	BIND_ENUM_CONSTANT(MODE_PREVIEW);
	BIND_ENUM_CONSTANT(MODE_QUIT);
}

LiveWallpaper::Mode LiveWallpaper::get_mode() const {
	if (!bool(GLOBAL_GET("blazium/livewallpaper/enabled"))) {
		return MODE_DISABLED;
	}
	return (Mode)LiveWallpaperCmdline::get_mode();
}

bool LiveWallpaper::is_attached() const {
	return get_mode() == MODE_RUN && LiveWallpaperCmdline::get_embed_hwnd() != 0;
}

int64_t LiveWallpaper::get_workerw() const {
	return LiveWallpaperCmdline::get_embed_hwnd();
}

void LiveWallpaper::request_exit() {
	SceneTree *tree = SceneTree::get_singleton();
	if (tree) {
		tree->quit();
	}
}

void LiveWallpaper::_connect_frame_hook() {
	if (!frame_hooked) {
		SceneTree *tree = SceneTree::get_singleton();
		if (tree) {
			tree->connect("process_frame", callable_mp(this, &LiveWallpaper::process_frame));
			frame_hooked = true;
		}
	}
	if (!present_hooked) {
		RenderingServer *rs = RenderingServer::get_singleton();
		if (rs) {
			rs->connect("frame_post_draw", callable_mp(this, &LiveWallpaper::present_frame));
			present_hooked = true;
		}
	}
}

void LiveWallpaper::_disconnect_frame_hook() {
	if (frame_hooked) {
		SceneTree *tree = SceneTree::get_singleton();
		if (tree) {
			tree->disconnect("process_frame", callable_mp(this, &LiveWallpaper::process_frame));
		}
		frame_hooked = false;
	}
	if (present_hooked) {
		RenderingServer *rs = RenderingServer::get_singleton();
		if (rs) {
			rs->disconnect("frame_post_draw", callable_mp(this, &LiveWallpaper::present_frame));
		}
		present_hooked = false;
	}
}

void LiveWallpaper::_release_sync() {
#ifdef WINDOWS_ENABLED
	if (quit_event) {
		CloseHandle((HANDLE)quit_event);
		quit_event = nullptr;
	}
	if (mutex_handle) {
		ReleaseMutex((HANDLE)mutex_handle);
		CloseHandle((HANDLE)mutex_handle);
		mutex_handle = nullptr;
	}
#endif
}

void LiveWallpaper::_signal_quit_event() {
#ifdef WINDOWS_ENABLED
	const Char16String name = LiveWallpaperCmdline::quit_event_name().utf16();
	HANDLE ev = OpenEventW(EVENT_MODIFY_STATE, FALSE, (LPCWSTR)name.get_data());
	if (ev) {
		SetEvent(ev);
		CloseHandle(ev);
	}
#endif
}

bool LiveWallpaper::_session_locked() const {
#ifdef WINDOWS_ENABLED
	HDESK desk = OpenInputDesktop(0, FALSE, DESKTOP_READOBJECTS);
	if (desk == nullptr) {
		return false;
	}
	WCHAR name[64] = {};
	DWORD needed = 0;
	const BOOL ok = GetUserObjectInformationW(desk, UOI_NAME, name, sizeof(name), &needed);
	CloseDesktop(desk);
	if (!ok) {
		return false;
	}
	return wcsstr(name, L"Winlogon") != nullptr;
#else
	return false;
#endif
}

void LiveWallpaper::setup_runtime() {
	if (Engine::get_singleton() && Engine::get_singleton()->is_editor_hint()) {
		return;
	}
	if (get_mode() == MODE_DISABLED) {
		return;
	}

	if (get_mode() == MODE_QUIT) {
		_signal_quit_event();
		request_exit();
		return;
	}

#ifdef WINDOWS_ENABLED
	if (get_mode() == MODE_RUN) {
		const Char16String mutex_name = LiveWallpaperCmdline::mutex_name().utf16();
		mutex_handle = CreateMutexW(nullptr, FALSE, (LPCWSTR)mutex_name.get_data());
		if (mutex_handle && GetLastError() == ERROR_ALREADY_EXISTS) {
			CloseHandle((HANDLE)mutex_handle);
			mutex_handle = nullptr;
			request_exit();
			return;
		}
		const Char16String quit_name = LiveWallpaperCmdline::quit_event_name().utf16();
		quit_event = CreateEventW(nullptr, TRUE, FALSE, (LPCWSTR)quit_name.get_data());
		_ensure_attached();
	}
#endif

	_connect_frame_hook();
}

void LiveWallpaper::_ensure_attached() {
#ifdef WINDOWS_ENABLED
	if (get_mode() != MODE_RUN) {
		return;
	}
	DisplayServer *ds = DisplayServer::get_singleton();
	if (ds == nullptr) {
		return;
	}
	const int64_t hwnd = ds->window_get_native_handle(DisplayServer::WINDOW_HANDLE);
	int64_t workerw = LiveWallpaperCmdline::get_embed_hwnd();
	if (workerw == 0) {
		workerw = LiveWallpaperWorkerW::find_workerw();
		LiveWallpaperCmdline::set_embed_hwnd(workerw);
	}
	if (hwnd == 0 || workerw == 0) {
		return;
	}
	const bool cover_all = bool(GLOBAL_GET("blazium/livewallpaper/cover_all_screens"));
	if (!LiveWallpaperWorkerW::is_fully_attached(hwnd, workerw, cover_all) || attach_settle_frames < 8) {
		if (LiveWallpaperWorkerW::attach_window(hwnd, workerw, cover_all)) {
			attach_settle_frames++;
		} else {
			attach_settle_frames = 0;
		}
	}
	_sync_root_viewport(LiveWallpaperWorkerW::attach_rect(workerw, cover_all).size);
#endif
}

void LiveWallpaper::_sync_root_viewport(const Size2i &p_size) {
	if (p_size.x < 2 || p_size.y < 2) {
		return;
	}
	SceneTree *tree = SceneTree::get_singleton();
	RenderingServer *rs = RenderingServer::get_singleton();
	if (tree == nullptr || rs == nullptr) {
		return;
	}
	Window *root = tree->get_root();
	if (root == nullptr) {
		return;
	}
	const RID vp = root->get_viewport_rid();
	rs->viewport_set_update_mode(vp, RenderingServer::VIEWPORT_UPDATE_ALWAYS);
	rs->viewport_set_disable_2d(vp, false);
	if (last_synced_viewport == p_size) {
		return;
	}
	last_synced_viewport = p_size;
	rs->viewport_set_size(vp, p_size.x, p_size.y);

	rs->viewport_attach_to_screen(vp, Rect2(), DisplayServer::INVALID_WINDOW_ID);
}

void LiveWallpaper::process_frame() {
	SceneTree *tree = SceneTree::get_singleton();
#ifdef WINDOWS_ENABLED
	if (quit_event && WaitForSingleObject((HANDLE)quit_event, 0) == WAIT_OBJECT_0) {
		request_exit();
		return;
	}
	_ensure_attached();
#endif
	if (get_mode() != MODE_RUN) {
		return;
	}
	if (last_synced_viewport.x > 1 && last_synced_viewport.y > 1) {
		_ensure_overlay_canvas();
		_update_overlay_canvas(last_synced_viewport);
	}
	if (tree == nullptr || !bool(GLOBAL_GET("blazium/livewallpaper/pause_on_lock"))) {
		return;
	}
	const bool locked = _session_locked();
	if (locked != session_paused) {
		session_paused = locked;
		tree->set_pause(locked);
	}
}

void LiveWallpaper::present_frame() {
#ifdef WINDOWS_ENABLED
	if (get_mode() != MODE_RUN) {
		return;
	}
	SceneTree *tree = SceneTree::get_singleton();
	RenderingServer *rs = RenderingServer::get_singleton();
	if (tree == nullptr || rs == nullptr) {
		return;
	}
	Window *root = tree->get_root();
	if (root == nullptr) {
		return;
	}
	const int64_t workerw = LiveWallpaperCmdline::get_embed_hwnd();
	if (workerw == 0) {
		return;
	}
	const RID tex = rs->viewport_get_texture(root->get_viewport_rid());
	if (!tex.is_valid()) {
		return;
	}
	Ref<Image> img = rs->texture_2d_get(tex);
	if (img.is_null() || img->is_empty()) {
		return;
	}
	if (img->get_format() != Image::FORMAT_RGBA8) {
		img = img->duplicate();
		img->convert(Image::FORMAT_RGBA8);
	}
	PackedByteArray data = img->get_data();
	uint8_t *px = data.ptrw();
	const int n = data.size();
	for (int i = 0; i + 3 < n; i += 4) {
		SWAP(px[i], px[i + 2]);
	}
	const bool cover_all = bool(GLOBAL_GET("blazium/livewallpaper/cover_all_screens"));
	LiveWallpaperWorkerW::present_bgra(workerw, cover_all, img->get_width(), img->get_height(), px);
#else
#endif
}

void LiveWallpaper::_ensure_overlay_canvas() {
	RenderingServer *rs = RenderingServer::get_singleton();
	SceneTree *tree = SceneTree::get_singleton();
	if (rs == nullptr || tree == nullptr || overlay_item.is_valid()) {
		return;
	}
	Window *root = tree->get_root();
	if (root == nullptr) {
		return;
	}
	overlay_canvas = rs->canvas_create();
	overlay_item = rs->canvas_item_create();
	rs->canvas_item_set_parent(overlay_item, overlay_canvas);
	rs->viewport_attach_canvas(root->get_viewport_rid(), overlay_canvas);
	rs->viewport_set_canvas_stacking(root->get_viewport_rid(), overlay_canvas, 32, 0);
}

void LiveWallpaper::_update_overlay_canvas(const Size2i &p_size) {
	RenderingServer *rs = RenderingServer::get_singleton();
	if (rs == nullptr || !overlay_item.is_valid() || p_size.x < 2 || p_size.y < 2) {
		return;
	}
	overlay_time += 1.0f / 60.0f;
	rs->canvas_item_clear(overlay_item);
	const Color bg = Color::from_hsv(Math::fmod(0.58f + overlay_time * 0.03f, 1.0f), 0.55f, 0.35f);
	rs->canvas_item_add_rect(overlay_item, Rect2(Point2(), p_size), bg);
	const float travel = MAX(float(p_size.x) - 220.0f, 1.0f);
	const float x = Math::fmod(overlay_time * 180.0f, travel);
	const float y = float(p_size.y) * 0.45f + Math::sin(overlay_time * 1.6f) * float(p_size.y) * 0.08f;
	rs->canvas_item_add_rect(overlay_item, Rect2(x, y, 220, 70), Color(1, 0.85, 0.2));
	rs->canvas_item_add_circle(overlay_item, Point2(Math::fmod(overlay_time * 140.0f, float(p_size.x)), float(p_size.y) * 0.2f), 36.0f, Color(0.2, 0.9, 1));
	for (int i = 0; i < 18; i++) {
		const float px = Math::fmod(40.0f + float(i) * 90.0f + overlay_time * 70.0f, float(p_size.x));
		const float py = Math::fmod(float(i) * 73.0f + overlay_time * (80.0f + float(i)), float(p_size.y));
		rs->canvas_item_add_circle(overlay_item, Point2(px, py), 10.0f + float(i % 4) * 4.0f, Color::from_hsv(Math::fmod(float(i) * 0.07f + overlay_time * 0.1f, 1.0f), 0.7f, 1.0f));
	}
}

void LiveWallpaper::_free_overlay_canvas() {
	RenderingServer *rs = RenderingServer::get_singleton();
	if (rs == nullptr) {
		overlay_item = RID();
		overlay_canvas = RID();
		return;
	}
	if (overlay_item.is_valid()) {
		rs->free(overlay_item);
		overlay_item = RID();
	}
	if (overlay_canvas.is_valid()) {
		rs->free(overlay_canvas);
		overlay_canvas = RID();
	}
}
