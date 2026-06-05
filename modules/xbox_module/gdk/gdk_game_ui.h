/**************************************************************************/
/*  gdk_game_ui.h                                                         */
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

#include "core/variant/callable.h"
#include "gdk_gdk_stubs.h"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifdef XBOX_MODULE_GDK_ENABLED
#include "gdk_windows.h"
#endif

#include <vector>

#include "core/object/class_db.h"
#include "core/object/ref_counted.h"
#include "core/string/ustring.h"

class GDK;
class GDKResult;
class GDKRuntime;
class GDKUser;
class GDKUsers;

class GDKGameUI : public RefCounted {
	GDCLASS(GDKGameUI, RefCounted);

	GDK *m_owner = nullptr;
	bool m_runtime_ready = false;

protected:
	static void _bind_methods();

public:
	void set_owner(GDK *p_owner);

	Ref<GDKResult> on_runtime_initialized();
	void shutdown();

	Signal show_message_dialog_async(
			const String &p_title,
			const String &p_message,
			const String &p_first_button = "OK",
			const String &p_second_button = String(),
			const String &p_third_button = String(),
			const String &p_default_button = "first",
			const String &p_cancel_button = "first");
	Ref<GDKResult> set_notification_position_hint(const String &p_position);
	Signal show_player_profile_card_async(const Ref<GDKUser> &p_requesting_user, const String &p_target_xuid);
	Signal show_player_picker_async(
			const Ref<GDKUser> &p_requesting_user,
			const String &p_prompt,
			const PackedStringArray &p_selectable_xuids,
			const PackedStringArray &p_preselected_xuids = PackedStringArray(),
			int64_t p_min_selection_count = 1,
			int64_t p_max_selection_count = 1);
	Signal resolve_privilege_with_ui_async(const Ref<GDKUser> &p_user, int64_t p_privilege);

	GDKRuntime *get_runtime_internal() const;
	GDKUsers *get_users_internal() const;
	Signal make_completed_signal_internal(const Ref<GDKResult> &p_result) const;
	Signal make_error_signal_internal(
			HRESULT p_hresult,
			const String &p_code,
			const String &p_message) const;
};
