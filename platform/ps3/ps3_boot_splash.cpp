/**************************************************************************/
/*  ps3_boot_splash.cpp                                                   */
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

#include "ps3_boot_splash.h"

#include "platform/ps3/ps3_boot_log.h"

#include "core/os/os.h"

#ifdef NORSX_ENABLED
#include "drivers/norsx/norsx_framebuffer.h"

static bool g_boot_splash_ready = false;
static bool g_setup_in_progress = false;
static uint64_t g_last_splash_refresh_usec = 0;

static const uint64_t SPLASH_REFRESH_INTERVAL_USEC = 100000;
#endif

void ps3_boot_splash_set_setup_in_progress(bool p_active) {
#ifdef NORSX_ENABLED
	g_setup_in_progress = p_active;
#endif
}

void ps3_boot_splash_present() {
#ifdef NORSX_ENABLED
	NoRSXFramebuffer *fb = ps3_framebuffer_get_shared();
	if (fb->initialize() != OK) {
		ps3_boot_log("boot_splash_init_failed");
		return;
	}
	fb->clear(NoRSXFramebuffer::SPLASH_COLOR);
	fb->present();
	g_boot_splash_ready = true;
	g_last_splash_refresh_usec = OS::get_singleton() ? OS::get_singleton()->get_ticks_usec() : 0;
	ps3_boot_log("boot_splash_presented");
#endif
}

void ps3_boot_splash_refresh_if_due() {
#ifdef NORSX_ENABLED
	if (!g_boot_splash_ready || !g_setup_in_progress) {
		return;
	}
	if (!OS::get_singleton()) {
		return;
	}
	const uint64_t now = OS::get_singleton()->get_ticks_usec();
	if (g_last_splash_refresh_usec != 0 && now - g_last_splash_refresh_usec < SPLASH_REFRESH_INTERVAL_USEC) {
		return;
	}
	NoRSXFramebuffer *fb = ps3_framebuffer_get_shared();
	if (fb && fb->is_initialized()) {
		fb->present();
	}
	g_last_splash_refresh_usec = now;
#endif
}
