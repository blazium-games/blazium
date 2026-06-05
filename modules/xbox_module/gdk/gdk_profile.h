/**************************************************************************/
/*  gdk_profile.h                                                         */
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

#include "core/object/class_db.h"
#include "core/object/ref_counted.h"
#include "core/string/ustring.h"
#include "core/variant/array.h"
#include "core/variant/callable.h"
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

class GDKUserProfile : public RefCounted {
	GDCLASS(GDKUserProfile, RefCounted);

	String m_xuid;
	String m_app_display_name;
	String m_app_display_picture_resize_uri;
	String m_game_display_name;
	String m_game_display_picture_resize_uri;
	String m_gamerscore;
	String m_gamertag;
	String m_modern_gamertag;
	String m_modern_gamertag_suffix;
	String m_unique_modern_gamertag;

protected:
	static void _bind_methods();

public:
	String get_xuid() const;
	String get_app_display_name() const;
	String get_app_display_picture_resize_uri() const;
	String get_game_display_name() const;
	String get_game_display_picture_resize_uri() const;
	String get_gamerscore() const;
	String get_gamertag() const;
	String get_modern_gamertag() const;
	String get_modern_gamertag_suffix() const;
	String get_unique_modern_gamertag() const;

	void populate_from_native(const XblUserProfile &p_profile);
};

class GDKProfile : public RefCounted {
	GDCLASS(GDKProfile, RefCounted);

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

	Signal get_profile_async(const Ref<GDKUser> &p_user, const String &p_xuid);
	Signal get_profiles_async(const Ref<GDKUser> &p_user, const PackedStringArray &p_xuids);
	Signal get_profiles_for_social_group_async(const Ref<GDKUser> &p_user, const String &p_social_group);

	void on_user_removed(const Ref<GDKUser> &p_user);
};
