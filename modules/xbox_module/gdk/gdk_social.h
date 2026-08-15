/**************************************************************************/
/*  gdk_social.h                                                          */
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

#include "core/variant/callable.h"
#include "gdk_gdk_stubs.h"

#include <vector>

#include "core/object/ref_counted.h"
#include "core/string/ustring.h"
#include "core/variant/array.h"
#include "core/variant/dictionary.h"

#ifdef XBOX_MODULE_GDK_ENABLED
#include <XUser.h>
#include <xsapi-c/services_c.h>
#endif

#include "gdk_pending_signal.h"
#include "gdk_presence.h"

class GDK;
class GDKResult;
class GDKRuntime;
class GDKUser;
class GDKXboxServices;

class GDKSocialFilter : public RefCounted {
	GDCLASS(GDKSocialFilter, RefCounted);

public:
	enum PresenceFilter {
		PRESENCE_FILTER_UNKNOWN = 0,
		PRESENCE_FILTER_TITLE_ONLINE,
		PRESENCE_FILTER_TITLE_OFFLINE,
		PRESENCE_FILTER_TITLE_ONLINE_OUTSIDE_TITLE,
		PRESENCE_FILTER_ALL_ONLINE,
		PRESENCE_FILTER_ALL_OFFLINE,
		PRESENCE_FILTER_ALL_TITLE,
		PRESENCE_FILTER_ALL,
	};

	enum RelationshipFilter {
		RELATIONSHIP_FILTER_UNKNOWN = 0,
		RELATIONSHIP_FILTER_FRIENDS,
		RELATIONSHIP_FILTER_FAVORITE,
	};

private:
	PresenceFilter m_presence_filter = PRESENCE_FILTER_ALL;
	RelationshipFilter m_relationship_filter = RELATIONSHIP_FILTER_FRIENDS;

protected:
	static void _bind_methods();

public:
	PresenceFilter get_presence_filter() const;
	void set_presence_filter(PresenceFilter p_presence_filter);
	RelationshipFilter get_relationship_filter() const;
	void set_relationship_filter(RelationshipFilter p_relationship_filter);
};

class GDKSocialGroup : public RefCounted {
	GDCLASS(GDKSocialGroup, RefCounted);

public:
	enum GroupType {
		GROUP_TYPE_FILTER = 0,
		GROUP_TYPE_USER_LIST,
	};

private:
	Ref<GDKUser> m_local_user;
	XblSocialManagerUserGroupHandle m_group_handle = nullptr;
	bool m_loaded = false;
	GroupType m_group_type = GROUP_TYPE_FILTER;
	GDKSocialFilter::PresenceFilter m_presence_filter = GDKSocialFilter::PRESENCE_FILTER_ALL;
	GDKSocialFilter::RelationshipFilter m_relationship_filter = GDKSocialFilter::RELATIONSHIP_FILTER_FRIENDS;
	PackedStringArray m_tracked_xuids;

protected:
	static void _bind_methods();

public:
	Ref<GDKUser> get_local_user() const;
	bool is_loaded() const;
	GroupType get_group_type() const;
	String get_group_type_name() const;
	GDKSocialFilter::PresenceFilter get_presence_filter() const;
	GDKSocialFilter::RelationshipFilter get_relationship_filter() const;
	PackedStringArray get_tracked_xuids() const;

	void attach(const Ref<GDKUser> &p_local_user, XblSocialManagerUserGroupHandle p_group_handle);
	void set_group_type(GroupType p_group_type);
	void set_filters(GDKSocialFilter::PresenceFilter p_presence_filter, GDKSocialFilter::RelationshipFilter p_relationship_filter);
	void set_tracked_xuids(const PackedStringArray &p_tracked_xuids);
	void set_loaded(bool p_loaded);
	bool matches_handle(XblSocialManagerUserGroupHandle p_group_handle) const;
	XblSocialManagerUserGroupHandle get_handle() const;
	void invalidate();
};

class GDKSocialUser : public RefCounted {
	GDCLASS(GDKSocialUser, RefCounted);

	String m_xuid;
	bool m_is_favorite = false;
	bool m_is_friend = false;
	bool m_is_following_user = false;
	bool m_is_followed_by_caller = false;
	String m_display_name;
	String m_real_name;
	String m_display_picture_url;
	bool m_use_avatar = false;
	String m_gamerscore;
	String m_gamertag;
	String m_modern_gamertag;
	String m_modern_gamertag_suffix;
	String m_unique_modern_gamertag;
	Ref<GDKPresenceRecord> m_presence;
	Dictionary m_title_history;
	Dictionary m_preferred_color;

protected:
	static void _bind_methods();

public:
	String get_xuid() const;
	bool is_favorite() const;
	bool is_friend() const;
	bool is_following_user() const;
	bool is_followed_by_caller() const;
	String get_display_name() const;
	String get_real_name() const;
	String get_display_picture_url() const;
	bool uses_avatar() const;
	String get_gamerscore() const;
	String get_gamertag() const;
	String get_modern_gamertag() const;
	String get_modern_gamertag_suffix() const;
	String get_unique_modern_gamertag() const;
	Ref<GDKPresenceRecord> get_presence() const;
	Dictionary get_title_history() const;
	Dictionary get_preferred_color() const;

	void populate_from_native(const XblSocialManagerUser &p_social_user);
};

class GDKSocial : public RefCounted {
	GDCLASS(GDKSocial, RefCounted);

	struct LocalUserState {
		Ref<GDKUser> user;
		XUserLocalId local_id = {};
		bool graph_started = false;
		bool graph_ready = false;
		Ref<GDKSocialGroup> friends_group;
	};

	struct PendingFriendsOp {
		XUserLocalId local_id = {};
		XblSocialManagerUserGroupHandle group_handle = nullptr;
		Ref<GDKPendingSignal> request;
	};

	GDK *m_owner = nullptr;
	bool m_runtime_ready = false;
	std::vector<LocalUserState> m_local_user_states;
	std::vector<Ref<GDKSocialGroup>> m_groups;
	std::vector<PendingFriendsOp> m_pending_friend_ops;
	std::vector<Ref<GDKSocialUser>> m_cached_users;

	GDKRuntime *_get_runtime() const;
	GDKXboxServices *_get_xbox_services() const;
	GDKPresence *_get_presence_service() const;
	Signal _make_completed_signal(const Ref<GDKResult> &p_result) const;
	Signal _make_error_signal(HRESULT p_hresult, const String &p_code, const String &p_message) const;
	Ref<GDKResult> _ensure_ready_user(const Ref<GDKUser> &p_user) const;
	Ref<GDKResult> _ensure_local_user_state(const Ref<GDKUser> &p_user, LocalUserState **r_state, bool p_auto_start);
	LocalUserState *_find_local_user_state(XUserLocalId p_local_id);
	Ref<GDKSocialGroup> _find_group_by_handle(XblSocialManagerUserGroupHandle p_group_handle) const;
	Ref<GDKSocialUser> _find_cached_user(const String &p_xuid) const;
	Ref<GDKSocialUser> _cache_social_user(const XblSocialManagerUser &p_social_user, bool p_emit_social_signal, bool p_emit_presence_signal);
	Ref<GDKSocialGroup> _create_social_group_internal(const Ref<GDKUser> &p_user, const Ref<GDKSocialFilter> &p_filter, Ref<GDKResult> *r_error);
	Ref<GDKSocialGroup> _create_social_group_from_xuids_internal(const Ref<GDKUser> &p_user, const PackedStringArray &p_xuids, Ref<GDKResult> *r_error);
	Array _get_group_users_internal(const Ref<GDKSocialGroup> &p_group, bool p_emit_social_signal, bool p_emit_presence_signal, Ref<GDKResult> *r_error = nullptr);
	Ref<GDKResult> _refresh_group_metadata(const Ref<GDKSocialGroup> &p_group);
	void _complete_pending_friend_ops(XblSocialManagerUserGroupHandle p_group_handle);
	void _fail_pending_friend_ops(XUserLocalId p_local_id, const Ref<GDKResult> &p_result);
	void _fail_pending_friend_ops_for_group(XblSocialManagerUserGroupHandle p_group_handle, const Ref<GDKResult> &p_result);
	void _cancel_pending_friend_signal(GDKPendingSignal *p_request);
	void _destroy_group_internal(const Ref<GDKSocialGroup> &p_group, bool p_remove_from_collection);
	void _erase_local_user_state(XUserLocalId p_local_id);
	void _emit_filter_group_updates(XUserLocalId p_local_id);
	void _emit_social_graph_changed(XUserLocalId p_local_id);

protected:
	static void _bind_methods();

public:
	void set_owner(GDK *p_owner);

	Ref<GDKResult> on_runtime_initialized();
	void shutdown();
	int dispatch();

	Ref<GDKResult> start_social_graph(const Ref<GDKUser> &p_user);
	void stop_social_graph(const Ref<GDKUser> &p_user);
	Signal get_friends_async(const Ref<GDKUser> &p_user);
	Ref<GDKResult> create_social_group(const Ref<GDKUser> &p_user, const Ref<GDKSocialFilter> &p_filter = Ref<GDKSocialFilter>());
	Ref<GDKResult> create_social_group_from_xuids(const Ref<GDKUser> &p_user, const PackedStringArray &p_xuids);
	void destroy_social_group(const Ref<GDKSocialGroup> &p_group);
	Ref<GDKResult> get_group_users(const Ref<GDKSocialGroup> &p_group);
	Signal submit_reputation_feedback_async(const Ref<GDKUser> &p_user, const String &p_target_xuid, const String &p_feedback_type, const String &p_reason = String(), const String &p_evidence_id = String());
	Signal submit_batch_reputation_feedback_async(const Ref<GDKUser> &p_user, const Array &p_feedback_items);

	void on_user_removed(const Ref<GDKUser> &p_user);
};

VARIANT_ENUM_CAST(GDKSocialFilter::PresenceFilter);
VARIANT_ENUM_CAST(GDKSocialFilter::RelationshipFilter);
VARIANT_ENUM_CAST(GDKSocialGroup::GroupType);
