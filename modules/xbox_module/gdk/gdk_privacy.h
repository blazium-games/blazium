/**************************************************************************/
/*  gdk_privacy.h                                                         */
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
#include "core/variant/array.h"
#include "core/variant/callable.h"
#include "core/variant/dictionary.h"
#include "core/variant/variant.h"

#ifdef XBOX_MODULE_GDK_ENABLED
#include <XUser.h>
#include <xsapi-c/services_c.h>
#endif

class GDK;
class GDKResult;
class GDKRuntime;
class GDKUser;
class GDKXboxServices;

class GDKPrivacy : public RefCounted {
	GDCLASS(GDKPrivacy, RefCounted);

	GDK *m_owner = nullptr;
	bool m_runtime_ready = false;

	GDKRuntime *_get_runtime() const;
	GDKXboxServices *_get_xbox_services() const;
	Signal _make_error_signal(HRESULT p_hresult, const String &p_code, const String &p_message, const Variant &p_data = Variant()) const;
	Ref<GDKResult> _ensure_ready_user(const Ref<GDKUser> &p_user) const;

protected:
	static void _bind_methods();

public:
	void set_owner(GDK *p_owner);

	Ref<GDKResult> on_runtime_initialized();
	void shutdown();
	int dispatch();

	Signal check_permission_async(const Ref<GDKUser> &p_user, const String &p_permission, const String &p_target_xuid);
	Signal check_permission_for_anonymous_user_async(const Ref<GDKUser> &p_user, const String &p_permission, const String &p_anonymous_user_type);
	Signal batch_check_permission_async(const Ref<GDKUser> &p_user, const String &p_permission, const PackedStringArray &p_target_xuids);
	Signal get_avoid_list_async(const Ref<GDKUser> &p_user);
	Signal get_mute_list_async(const Ref<GDKUser> &p_user);

	void on_user_removed(const Ref<GDKUser> &p_user);
};
