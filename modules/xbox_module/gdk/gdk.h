/**************************************************************************/
/*  gdk.h                                                                 */
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

#include "core/object/object.h"
#include "core/object/ref_counted.h"
#include "core/variant/variant.h"

class GDKAccessibility;
class GDKAchievements;
class GDKActivation;
class GDKCapture;
class GDKDisplay;
class GDKErrorReporting;
class GDKGameUI;
class GDKLauncher;
class GDKLeaderboards;
class GDKMultiplayerActivity;
class GDKPackage;
class GDKPresence;
class GDKPrivacy;
class GDKProfile;
class GDKResult;
class GDKRuntime;
class GDKSocial;
class GDKStats;
class GDKStore;
class GDKStringVerify;
class GDKSystem;
class GDKTitleStorage;
class GDKUser;
class GDKUsers;
class GDKXboxServices;

class GDK : public Object {
	GDCLASS(GDK, Object);

	static GDK *singleton;

	GDKRuntime *m_runtime = nullptr;
	GDKXboxServices *m_xbox_services = nullptr;
	Ref<GDKUsers> m_users;
	Ref<GDKGameUI> m_game_ui;
	Ref<GDKAccessibility> m_accessibility;
	Ref<GDKAchievements> m_achievements;
	Ref<GDKPackage> m_package;
	Ref<GDKStats> m_stats;
	Ref<GDKLeaderboards> m_leaderboards;
	Ref<GDKPrivacy> m_privacy;
	Ref<GDKPresence> m_presence;
	Ref<GDKSocial> m_social;
	Ref<GDKStore> m_store;
	Ref<GDKProfile> m_profile;
	Ref<GDKStringVerify> m_string_verify;
	Ref<GDKTitleStorage> m_title_storage;
	Ref<GDKErrorReporting> m_error_reporting;
	Ref<GDKLauncher> m_launcher;
	Ref<GDKMultiplayerActivity> m_multiplayer_activity;
	Ref<GDKCapture> m_capture;
	Ref<GDKSystem> m_system;
	Ref<GDKDisplay> m_display;
	Ref<GDKActivation> m_activation;

protected:
	static void _bind_methods();

public:
	static GDK *get_singleton();

	GDK();
	~GDK();

	Ref<GDKResult> initialize(const Variant &p_config = Variant());
	void shutdown();
	bool is_available() const;
	bool is_initialized() const;
	int64_t dispatch();
	Ref<GDKUsers> get_users() const;
	Ref<GDKGameUI> get_game_ui() const;
	Ref<GDKAccessibility> get_accessibility() const;
	Ref<GDKAchievements> get_achievements() const;
	Ref<GDKPackage> get_package() const;
	Ref<GDKStats> get_stats() const;
	Ref<GDKLeaderboards> get_leaderboards() const;
	Ref<GDKPrivacy> get_privacy() const;
	Ref<GDKPresence> get_presence() const;
	Ref<GDKSocial> get_social() const;
	Ref<GDKStore> get_store() const;
	Ref<GDKProfile> get_profile() const;
	Ref<GDKStringVerify> get_string_verify() const;
	Ref<GDKTitleStorage> get_title_storage() const;
	Ref<GDKErrorReporting> get_error_reporting() const;
	Ref<GDKLauncher> get_launcher() const;
	Ref<GDKMultiplayerActivity> get_multiplayer_activity() const;
	Ref<GDKCapture> get_capture() const;
	Ref<GDKSystem> get_system() const;
	Ref<GDKDisplay> get_display() const;
	Ref<GDKActivation> get_activation() const;

	GDKRuntime *get_runtime() const;
	GDKXboxServices *get_xbox_services() const;
	void emit_runtime_error(const Ref<GDKResult> &p_result);
	void notify_user_removed(const Ref<GDKUser> &p_user);
};
