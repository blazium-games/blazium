/**************************************************************************/
/*  gdk_launcher.h                                                        */
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
#include "core/string/ustring.h"
#include "core/variant/dictionary.h"

#ifdef XBOX_MODULE_GDK_ENABLED
#include <XLauncher.h>
#endif

class GDK;
class GDKResult;
class GDKUser;

class GDKLauncher : public RefCounted {
	GDCLASS(GDKLauncher, RefCounted);

	GDK *m_owner = nullptr;
	bool m_runtime_ready = false;

	static bool try_parse_uri_scheme_internal(const String &p_uri, String *r_scheme);
	static bool is_supported_scheme_internal(const String &p_scheme);
	static bool is_disallowed_scheme_internal(const String &p_scheme);

protected:
	static void _bind_methods();

public:
	void set_owner(GDK *p_owner);
	Ref<GDKResult> on_runtime_initialized();
	void shutdown();

	Ref<GDKResult> launch_uri(const String &p_uri, const Ref<GDKUser> &p_user = Ref<GDKUser>());
};
