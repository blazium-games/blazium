/**************************************************************************/
/*  gdk_multiplayer_activity.h                                            */
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

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifdef XBOX_MODULE_GDK_ENABLED
#include "gdk_windows.h"
#endif

#include "gdk_gdk_stubs.h"

#include <cstdint>
#include <vector>

#include "core/object/ref_counted.h"
#include "core/string/ustring.h"
#include "core/variant/callable.h"
#include "core/variant/dictionary.h"
#include "core/variant/variant.h"

#ifdef XBOX_MODULE_GDK_ENABLED
#include <xsapi-c/services_c.h>
#endif

class GDK;
class GDKPendingSignal;
class GDKResult;
class GDKRuntime;
class GDKUser;
class GDKXboxServices;

class GDKMultiplayerActivityInfo : public RefCounted {
	GDCLASS(GDKMultiplayerActivityInfo, RefCounted);

	String m_xuid;
	String m_connection_string;
	String m_join_restriction;
	int64_t m_max_players = 0;
	int64_t m_current_players = 0;
	String m_group_id;
	String m_platform;

protected:
	static void _bind_methods();

public:
	String get_xuid() const;
	String get_connection_string() const;
	String get_join_restriction() const;
	int64_t get_max_players() const;
	int64_t get_current_players() const;
	String get_group_id() const;
	String get_platform() const;

	void set_values(
			const String &p_xuid,
			const String &p_connection_string,
			const String &p_join_restriction,
			int64_t p_max_players,
			int64_t p_current_players,
			const String &p_group_id,
			const String &p_platform);
	void populate_from_native(const XblMultiplayerActivityInfo &p_native_activity);
};

class GDKMultiplayerActivity : public RefCounted {
	GDCLASS(GDKMultiplayerActivity, RefCounted);

	struct CachedActivityState {
		String xuid;
		Ref<GDKMultiplayerActivityInfo> info;
	};

	GDK *m_owner = nullptr;
	bool m_runtime_ready = false;
	uint64_t m_activation_listener_id = 0;
	std::vector<CachedActivityState> m_cached_activities;

protected:
	static void _bind_methods();

public:
	void set_owner(GDK *p_owner);

	Ref<GDKResult> on_runtime_initialized();
	void shutdown();
	void on_user_removed(const Ref<GDKUser> &p_user);

	Signal set_activity_async(
			const Ref<GDKUser> &p_user,
			const String &p_connection_string,
			const String &p_join_restriction = "followed",
			int64_t p_max_players = 0,
			int64_t p_current_players = 0,
			const String &p_group_id = String(),
			bool p_allow_cross_platform_join = false);
	Signal get_activities_async(const Ref<GDKUser> &p_user, const PackedStringArray &p_xuids);
	Ref<GDKMultiplayerActivityInfo> get_cached_activity(const String &p_xuid) const;
	Signal delete_activity_async(const Ref<GDKUser> &p_user);
	Signal send_invites_async(
			const Ref<GDKUser> &p_user,
			const PackedStringArray &p_xuids,
			bool p_allow_cross_platform_join = true,
			const String &p_connection_string = String());
	Signal show_invite_ui_async(const Ref<GDKUser> &p_user);
	Ref<GDKResult> update_recent_players(
			const Ref<GDKUser> &p_user,
			const PackedStringArray &p_xuids,
			const String &p_encounter_type = "default");
	Signal flush_recent_players_async(const Ref<GDKUser> &p_user);
	Ref<GDKResult> accept_pending_invite(const String &p_invite_uri);

	GDKRuntime *get_runtime_internal() const;
	GDKXboxServices *get_xbox_services_internal() const;
	Signal make_completed_signal_internal(const Ref<GDKResult> &p_result) const;
	Signal make_error_signal_internal(
			HRESULT p_hresult,
			const String &p_code,
			const String &p_message,
			const Variant &p_data = Variant()) const;
	Ref<GDKResult> duplicate_context_for_user_internal(
			const Ref<GDKUser> &p_user,
			XblContextHandle *r_context,
			uint64_t *r_xbox_user_id = nullptr) const;
	Ref<GDKMultiplayerActivityInfo> cache_activity_internal(const Ref<GDKMultiplayerActivityInfo> &p_info);
	void remove_cached_activity_internal(const String &p_xuid);
	void emit_activities_updated_internal(const std::vector<String> &p_xuids);
	void handle_activation_internal(const Dictionary &p_activation_info);

	static String join_restriction_to_string_internal(XblMultiplayerActivityJoinRestriction p_join_restriction);
	static bool try_parse_join_restriction_internal(
			const String &p_join_restriction,
			XblMultiplayerActivityJoinRestriction *r_join_restriction);
	static String platform_to_string_internal(XblMultiplayerActivityPlatform p_platform);
	static String encounter_type_to_string_internal(XblMultiplayerActivityEncounterType p_encounter_type);
	static bool try_parse_encounter_type_internal(
			const String &p_encounter_type,
			XblMultiplayerActivityEncounterType *r_encounter_type);
	static Dictionary parse_invite_uri_internal(const String &p_uri, const String &p_activation_type);
	static bool try_parse_xuid_internal(const String &p_xuid, uint64_t *r_xuid);
};
