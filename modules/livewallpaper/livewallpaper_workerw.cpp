/**************************************************************************/
/*  livewallpaper_workerw.cpp                                             */
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

#include "livewallpaper_workerw.h"

#ifdef WINDOWS_ENABLED
#include <windows.h>
#endif

bool LiveWallpaperWorkerW::test_override = false;
int64_t LiveWallpaperWorkerW::test_hwnd = 0;
bool LiveWallpaperWorkerW::test_virtual_override = false;
Rect2i LiveWallpaperWorkerW::test_virtual;
bool LiveWallpaperWorkerW::test_primary_override = false;
Rect2i LiveWallpaperWorkerW::test_primary;

#ifdef WINDOWS_ENABLED
static void _compute_attach_rect(HWND p_workerw, bool p_cover_all, int &r_x, int &r_y, int &r_w, int &r_h) {
	RECT worker_rc;
	GetClientRect(p_workerw, &worker_rc);
	r_x = 0;
	r_y = 0;
	r_w = worker_rc.right - worker_rc.left;
	r_h = worker_rc.bottom - worker_rc.top;
	if (!p_cover_all) {
		const Rect2i primary = LiveWallpaperWorkerW::primary_screen_rect();
		RECT worker_screen;
		GetWindowRect(p_workerw, &worker_screen);
		r_x = primary.position.x - worker_screen.left;
		r_y = primary.position.y - worker_screen.top;
		r_w = primary.size.x;
		r_h = primary.size.y;
	}
	r_w = MAX(r_w, 1);
	r_h = MAX(r_h, 1);
}

static BOOL CALLBACK _enum_workerw(HWND p_hwnd, LPARAM p_lparam) {
	HWND defview = FindWindowExW(p_hwnd, nullptr, L"SHELLDLL_DefView", nullptr);
	if (defview != nullptr) {
		HWND *out = reinterpret_cast<HWND *>(p_lparam);
		HWND sibling = FindWindowExW(nullptr, p_hwnd, L"WorkerW", nullptr);
		if (sibling != nullptr) {
			*out = sibling;
		}
	}
	return TRUE;
}

static HWND _find_progman_wallpaper_workerw(HWND p_progman) {
	HWND cur = nullptr;
	HWND found = nullptr;
	while ((cur = FindWindowExW(p_progman, cur, L"WorkerW", nullptr)) != nullptr) {
		if (FindWindowExW(cur, nullptr, L"SHELLDLL_DefView", nullptr) == nullptr) {
			found = cur;
		}
	}
	return found;
}

static HWND _spawn_and_find_workerw() {
	HWND progman = FindWindowW(L"Progman", nullptr);
	if (progman == nullptr) {
		return nullptr;
	}
	DWORD_PTR result = 0;
	SendMessageTimeoutW(progman, 0x052C, 0, 0, SMTO_NORMAL, 1000, &result);
	SendMessageTimeoutW(progman, 0x052C, 0xD, 0x1, SMTO_NORMAL, 1000, &result);

	HWND workerw = nullptr;
	EnumWindows(_enum_workerw, reinterpret_cast<LPARAM>(&workerw));
	if (workerw != nullptr) {
		return workerw;
	}
	return _find_progman_wallpaper_workerw(progman);
}
#endif

int64_t LiveWallpaperWorkerW::find_workerw() {
	if (test_override) {
		return test_hwnd;
	}
#ifdef WINDOWS_ENABLED
	HWND workerw = _spawn_and_find_workerw();
	return (int64_t)(intptr_t)workerw;
#else
	return 0;
#endif
}

bool LiveWallpaperWorkerW::attach_window(int64_t p_hwnd, int64_t p_workerw, bool p_cover_all) {
#ifdef WINDOWS_ENABLED
	HWND hwnd = reinterpret_cast<HWND>((intptr_t)p_hwnd);
	HWND workerw = reinterpret_cast<HWND>((intptr_t)p_workerw);
	if (hwnd == nullptr || workerw == nullptr || !IsWindow(hwnd) || !IsWindow(workerw)) {
		return false;
	}

	SetParent(hwnd, workerw);

	LONG_PTR style = GetWindowLongPtrW(hwnd, GWL_STYLE);
	style &= ~(WS_POPUP | WS_CAPTION | WS_THICKFRAME | WS_SYSMENU | WS_MINIMIZEBOX | WS_MAXIMIZEBOX);
	style |= WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | WS_CLIPCHILDREN;
	SetWindowLongPtrW(hwnd, GWL_STYLE, style);

	LONG_PTR ex = GetWindowLongPtrW(hwnd, GWL_EXSTYLE);
	ex &= ~(WS_EX_APPWINDOW | WS_EX_TOPMOST | WS_EX_WINDOWEDGE);
	ex |= WS_EX_NOACTIVATE | WS_EX_TOOLWINDOW | WS_EX_NOINHERITLAYOUT;
	SetWindowLongPtrW(hwnd, GWL_EXSTYLE, ex);

	int x = 0;
	int y = 0;
	int w = 1;
	int h = 1;
	_compute_attach_rect(workerw, p_cover_all, x, y, w, h);

	SetWindowPos(hwnd, HWND_BOTTOM, 0, 0, 1, 1, SWP_NOACTIVATE | SWP_FRAMECHANGED | SWP_SHOWWINDOW);
	(void)x;
	(void)y;
	(void)w;
	(void)h;
	return GetParent(hwnd) == workerw;
#else
	(void)p_hwnd;
	(void)p_workerw;
	(void)p_cover_all;
	return false;
#endif
}

bool LiveWallpaperWorkerW::is_child_of(int64_t p_hwnd, int64_t p_workerw) {
#ifdef WINDOWS_ENABLED
	HWND hwnd = reinterpret_cast<HWND>((intptr_t)p_hwnd);
	HWND workerw = reinterpret_cast<HWND>((intptr_t)p_workerw);
	if (hwnd == nullptr || workerw == nullptr) {
		return false;
	}
	if (GetParent(hwnd) != workerw) {
		return false;
	}
	const LONG_PTR style = GetWindowLongPtrW(hwnd, GWL_STYLE);
	return (style & WS_CHILD) != 0 && (style & WS_POPUP) == 0;
#else
	(void)p_hwnd;
	(void)p_workerw;
	return false;
#endif
}

Rect2i LiveWallpaperWorkerW::attach_rect(int64_t p_workerw, bool p_cover_all) {
#ifdef WINDOWS_ENABLED
	HWND workerw = reinterpret_cast<HWND>((intptr_t)p_workerw);
	if (workerw == nullptr || !IsWindow(workerw)) {
		return p_cover_all ? virtual_screen_rect() : primary_screen_rect();
	}
	int x = 0;
	int y = 0;
	int w = 1;
	int h = 1;
	_compute_attach_rect(workerw, p_cover_all, x, y, w, h);
	return Rect2i(x, y, w, h);
#else
	return p_cover_all ? virtual_screen_rect() : primary_screen_rect();
#endif
}

bool LiveWallpaperWorkerW::is_fully_attached(int64_t p_hwnd, int64_t p_workerw, bool p_cover_all) {
#ifdef WINDOWS_ENABLED
	if (!is_child_of(p_hwnd, p_workerw)) {
		return false;
	}
	HWND hwnd = reinterpret_cast<HWND>((intptr_t)p_hwnd);
	(void)p_cover_all;
	RECT have;
	GetClientRect(hwnd, &have);
	const int have_w = have.right - have.left;
	const int have_h = have.bottom - have.top;
	return have_w <= 2 && have_h <= 2;
#else
	(void)p_hwnd;
	(void)p_workerw;
	(void)p_cover_all;
	return false;
#endif
}

bool LiveWallpaperWorkerW::present_bgra(int64_t p_workerw, bool p_cover_all, int p_src_width, int p_src_height, const uint8_t *p_bgra) {
#ifdef WINDOWS_ENABLED
	HWND workerw = reinterpret_cast<HWND>((intptr_t)p_workerw);
	if (workerw == nullptr || !IsWindow(workerw) || p_bgra == nullptr || p_src_width < 2 || p_src_height < 2) {
		return false;
	}
	int x = 0;
	int y = 0;
	int w = 1;
	int h = 1;
	_compute_attach_rect(workerw, p_cover_all, x, y, w, h);
	BITMAPINFO bmi = {};
	bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
	bmi.bmiHeader.biWidth = p_src_width;
	bmi.bmiHeader.biHeight = -p_src_height;
	bmi.bmiHeader.biPlanes = 1;
	bmi.bmiHeader.biBitCount = 32;
	bmi.bmiHeader.biCompression = BI_RGB;
	HDC hdc = GetDC(workerw);
	if (hdc == nullptr) {
		return false;
	}
	const int drawn = StretchDIBits(hdc, x, y, w, h, 0, 0, p_src_width, p_src_height, p_bgra, &bmi, DIB_RGB_COLORS, SRCCOPY);
	ReleaseDC(workerw, hdc);
	return drawn != 0;
#else
	(void)p_workerw;
	(void)p_cover_all;
	(void)p_src_width;
	(void)p_src_height;
	(void)p_bgra;
	return false;
#endif
}

Rect2i LiveWallpaperWorkerW::virtual_screen_rect() {
	if (test_virtual_override) {
		return test_virtual;
	}
#ifdef WINDOWS_ENABLED
	const int x = GetSystemMetrics(SM_XVIRTUALSCREEN);
	const int y = GetSystemMetrics(SM_YVIRTUALSCREEN);
	const int w = GetSystemMetrics(SM_CXVIRTUALSCREEN);
	const int h = GetSystemMetrics(SM_CYVIRTUALSCREEN);
	if (w <= 0 || h <= 0) {
		return Rect2i(0, 0, 1920, 1080);
	}
	return Rect2i(x, y, w, h);
#else
	return Rect2i(0, 0, 1920, 1080);
#endif
}

Rect2i LiveWallpaperWorkerW::primary_screen_rect() {
	if (test_primary_override) {
		return test_primary;
	}
#ifdef WINDOWS_ENABLED
	const int w = GetSystemMetrics(SM_CXSCREEN);
	const int h = GetSystemMetrics(SM_CYSCREEN);
	return Rect2i(0, 0, MAX(w, 1), MAX(h, 1));
#else
	return Rect2i(0, 0, 1920, 1080);
#endif
}

bool LiveWallpaperWorkerW::has_test_override() {
	return test_override;
}

void LiveWallpaperWorkerW::reset_for_tests() {
	test_override = false;
	test_hwnd = 0;
	test_virtual_override = false;
	test_virtual = Rect2i();
	test_primary_override = false;
	test_primary = Rect2i();
}

void LiveWallpaperWorkerW::set_workerw_for_tests(int64_t p_hwnd) {
	test_override = true;
	test_hwnd = p_hwnd;
}

void LiveWallpaperWorkerW::set_virtual_rect_for_tests(const Rect2i &p_rect) {
	test_virtual_override = true;
	test_virtual = p_rect;
}

void LiveWallpaperWorkerW::set_primary_rect_for_tests(const Rect2i &p_rect) {
	test_primary_override = true;
	test_primary = p_rect;
}
