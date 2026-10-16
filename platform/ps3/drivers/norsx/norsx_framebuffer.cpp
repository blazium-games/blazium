/**************************************************************************/
/*  norsx_framebuffer.cpp                                                 */
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

#include "norsx_framebuffer.h"

#include "platform/ps3/ps3_boot_log.h"
#include "platform/ps3/ps3_wrapper.h"

#include "core/string/print_string.h"

#include <cstring>

#ifdef NORSX_ENABLED
#include <NoRSX.h>
#include <NoRSX/NoRSXutil.h>

static NoRSX *g_norsx = nullptr;
#endif

static NoRSXFramebuffer g_shared_framebuffer;

NoRSXFramebuffer *ps3_framebuffer_get_shared() {
	return &g_shared_framebuffer;
}

void ps3_framebuffer_shutdown_shared() {
	g_shared_framebuffer.shutdown();
}

static uint32_t _argb_to_display_pixel(uint32_t p_argb) {
#if defined(__BYTE_ORDER__) && __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
	return p_argb;
#else
	return ((p_argb & 0xFF000000) >> 24) | ((p_argb & 0x00FF0000) >> 8) | ((p_argb & 0x0000FF00) << 8) | ((p_argb & 0x000000FF) << 24);
#endif
}

static void _copy_framebuffer_to_norsx(const uint8_t *p_src, uint8_t *p_dst, size_t p_pixels) {
	const uint32_t *src = reinterpret_cast<const uint32_t *>(p_src);
	uint32_t *dst = reinterpret_cast<uint32_t *>(p_dst);
	for (size_t i = 0; i < p_pixels; i++) {
		dst[i] = _argb_to_display_pixel(src[i]);
	}
}

Error NoRSXFramebuffer::initialize() {
	if (initialized) {
		return OK;
	}

	buffer.resize(WIDTH * HEIGHT * 4);
	buffer.fill(0);

#ifdef NORSX_ENABLED
	if (!g_norsx) {
		g_norsx = memnew(NoRSX(RESOLUTION_1280x720, RESOLUTION_1280x720));
		if (g_norsx && g_norsx->buffer) {
			ps3_boot_log("norsx_initialized");
			ps3_boot_logf("norsx_buffer_size", vformat("%u", (unsigned)g_norsx->buffer_size));
		} else {
			ps3_boot_log("norsx_init_failed");
			if (!g_norsx) {
				ps3_boot_log("norsx_object_null");
			} else {
				ps3_boot_log("norsx_buffer_null");
			}
		}
	}
#else
	ps3_boot_log("norsx_disabled");
#endif

	initialized = true;
	return OK;
}

void NoRSXFramebuffer::shutdown() {
#ifdef NORSX_ENABLED
	if (g_norsx) {
		g_norsx->NoRSX_Exit();
		memdelete(g_norsx);
		g_norsx = nullptr;
		ps3_boot_log("norsx_shutdown");
	}
#endif
	buffer.clear();
	initialized = false;
}

void NoRSXFramebuffer::clear(uint32_t p_color) {
	const int pixels = WIDTH * HEIGHT;
	uint32_t *ptr = reinterpret_cast<uint32_t *>(buffer.ptrw());
	for (int i = 0; i < pixels; i++) {
		ptr[i] = p_color;
	}
}

void NoRSXFramebuffer::present() {
	if (!initialized) {
		ps3_boot_log("present_skipped_not_initialized");
		return;
	}

#ifdef NORSX_ENABLED
	if (!g_norsx) {
		ps3_boot_log("present_skipped_norsx_null");
		return;
	}
	if (!g_norsx->buffer) {
		ps3_boot_log("present_skipped_buffer_null");
		return;
	}

	const size_t src_bytes = (size_t)buffer.size();
	const size_t dst_bytes = (size_t)g_norsx->buffer_size;
	const size_t copy_pixels = MIN(src_bytes, dst_bytes) / 4;
	if (copy_pixels == 0) {
		ps3_boot_log("present_skipped_zero_size");
		return;
	}
	if (src_bytes != dst_bytes) {
		ps3_boot_logf("present_size_mismatch", vformat("src=%u dst=%u", (unsigned)src_bytes, (unsigned)dst_bytes));
	}

	_copy_framebuffer_to_norsx(buffer.ptr(), reinterpret_cast<uint8_t *>(g_norsx->buffer), copy_pixels);
	g_norsx->Flip();
	waitFlip();

	static bool logged_first_present = false;
	if (!logged_first_present) {
		ps3_boot_log("norsx_first_frame_presented");
		logged_first_present = true;
	}
#else
	ps3_boot_log("present_skipped_norsx_disabled");
#endif
}

uint8_t *NoRSXFramebuffer::get_buffer() {
	return buffer.ptrw();
}
