/**************************************************************************/
/*  gdk_display.h                                                         */
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

#include "gdk_gdk_stubs.h"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifdef XBOX_MODULE_GDK_ENABLED
#include "gdk_windows.h"
#endif

#include "core/object/ref_counted.h"

#ifdef XBOX_MODULE_GDK_ENABLED
#include <XDisplay.h>
#endif

class GDK;
class GDKResult;
class GDKRuntime;

class GDKDisplayTimeoutDeferral : public RefCounted {
	GDCLASS(GDKDisplayTimeoutDeferral, RefCounted);

	XDisplayTimeoutDeferralHandle m_handle = nullptr;

protected:
	static void _bind_methods();

public:
	GDKDisplayTimeoutDeferral() = default;
	~GDKDisplayTimeoutDeferral();

	bool is_valid() const;
	void release();

	void set_handle_internal(XDisplayTimeoutDeferralHandle p_handle);
};

class GDKDisplay : public RefCounted {
	GDCLASS(GDKDisplay, RefCounted);

	GDK *m_owner = nullptr;
	bool m_runtime_ready = false;

	GDKRuntime *_get_runtime() const;

protected:
	static void _bind_methods();

public:
	enum HdrMode {
		HDR_MODE_UNKNOWN = static_cast<uint32_t>(XDisplayHdrModeResult::Unknown),
		HDR_MODE_ENABLED = static_cast<uint32_t>(XDisplayHdrModeResult::Enabled),
		HDR_MODE_DISABLED = static_cast<uint32_t>(XDisplayHdrModeResult::Disabled),
	};

	enum HdrModePreference {
		HDR_MODE_PREFERENCE_PREFER_HDR = static_cast<uint32_t>(XDisplayHdrModePreference::PreferHdr),
		HDR_MODE_PREFERENCE_PREFER_REFRESH_RATE = static_cast<uint32_t>(XDisplayHdrModePreference::PreferRefreshRate),
	};

	void set_owner(GDK *p_owner);

	Ref<GDKResult> on_runtime_initialized();
	void shutdown();

	Ref<GDKResult> try_enable_hdr_mode(int64_t p_preference = HDR_MODE_PREFERENCE_PREFER_HDR);

	Ref<GDKResult> acquire_timeout_deferral();
};

VARIANT_ENUM_CAST(GDKDisplay::HdrMode);
VARIANT_ENUM_CAST(GDKDisplay::HdrModePreference);
