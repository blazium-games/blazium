/**************************************************************************/
/*  display_server_ps3.cpp                                                */
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

#include "display_server_ps3.h"

#include "core/input/input.h"

#include "drivers/software/rasterizer_compositor_ps3.h"
#include "servers/rendering/dummy/rasterizer_dummy.h"

#ifdef RSX_ENABLED
#include "drivers/rsx/rsx_context.h"
#endif

DisplayServerPS3 *DisplayServerPS3::get_singleton() {
	return static_cast<DisplayServerPS3 *>(DisplayServer::get_singleton());
}

DisplayServer *DisplayServerPS3::create_func(const String &p_rendering_driver, DisplayServer::WindowMode p_mode, DisplayServer::VSyncMode p_vsync_mode, uint32_t p_flags, const Vector2i *p_position, const Vector2i &p_resolution, int p_screen, Context p_context, int64_t p_parent_window, Error &r_error) {
	r_error = OK;

#ifdef RSX_ENABLED
	if (p_rendering_driver == "rsx") {
		RSXContext::make_current();
	} else
#endif
	{
#if defined(NORSX_ENABLED) && defined(SOFTWARE_CANVAS_ENABLED)
		RasterizerCompositorPS3::make_current();
#else
		RasterizerDummy::make_current();
#endif
	}

	DisplayServerPS3 *ds = memnew(DisplayServerPS3());
	if (p_resolution.x > 0 && p_resolution.y > 0) {
		ds->window_set_size(p_resolution);
	}
	return ds;
}

void DisplayServerPS3::register_ps3_driver() {
	register_create_function("ps3", create_func, get_rendering_drivers_func);
}

bool DisplayServerPS3::has_feature(Feature p_feature) const {
	switch (p_feature) {
		case FEATURE_ICON:
		case FEATURE_MOUSE:
		case FEATURE_TOUCHSCREEN:
			return true;
		default:
			return false;
	}
}

Vector<DisplayServer::WindowID> DisplayServerPS3::get_window_list() const {
	Vector<DisplayServer::WindowID> windows;
	windows.push_back(MAIN_WINDOW_ID);
	return windows;
}

void DisplayServerPS3::window_set_input_event_callback(const Callable &p_callable, WindowID p_window) {
	input_event_callback = p_callable;
}

void DisplayServerPS3::window_set_size(const Size2i p_size, WindowID p_window) {
	window_size = p_size;
}

void DisplayServerPS3::process_events() {
	Input::get_singleton()->flush_buffered_events();
}

void DisplayServerPS3::swap_buffers() {
#ifdef RSX_ENABLED
	if (RSXContext::is_available()) {
		RSXContext::present_stub();
		return;
	}
#endif
	NoRSXFramebuffer *fb = ps3_framebuffer_get_shared();
	if (fb) {
		fb->present();
	}
}

DisplayServerPS3::DisplayServerPS3() {
	native_menu = memnew(NativeMenu);
	NoRSXFramebuffer *fb = ps3_framebuffer_get_shared();
	if (fb) {
		fb->initialize();
		fb->clear(NoRSXFramebuffer::SPLASH_COLOR);
		fb->present();
	}
	Input::get_singleton()->set_event_dispatch_function(_dispatch_input_events);
}

DisplayServerPS3::~DisplayServerPS3() {
	if (native_menu) {
		memdelete(native_menu);
		native_menu = nullptr;
	}
}
