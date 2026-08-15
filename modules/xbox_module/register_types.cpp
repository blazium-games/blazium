/**************************************************************************/
/*  register_types.cpp                                                    */
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

#include "core/object/class_db.h"
#include "core/object/callable_mp.h"
#include "register_types.h"

#include "core/config/engine.h"
#include "core/config/project_settings.h"
#include "scene/main/node.h"
#include "scene/main/scene_tree.h"

#include "gdk/gdk.h"
#include "gdk/gdk_accessibility.h"
#include "gdk/gdk_achievement.h"
#include "gdk/gdk_activation.h"
#include "gdk/gdk_capture.h"
#include "gdk/gdk_display.h"
#include "gdk/gdk_error_reporting.h"
#include "gdk/gdk_game_ui.h"
#include "gdk/gdk_launcher.h"
#include "gdk/gdk_leaderboards.h"
#include "gdk/gdk_multiplayer_activity.h"
#include "gdk/gdk_package.h"
#include "gdk/gdk_pending_signal.h"
#include "gdk/gdk_presence.h"
#include "gdk/gdk_privacy.h"
#include "gdk/gdk_profile.h"
#include "gdk/gdk_result.h"
#include "gdk/gdk_social.h"
#include "gdk/gdk_stats.h"
#include "gdk/gdk_store.h"
#include "gdk/gdk_string_verify.h"
#include "gdk/gdk_system.h"
#include "gdk/gdk_title_storage.h"
#ifdef TOOLS_ENABLED
#include "editor/plugins/editor_plugin.h"
#include "editor/xbox_editor_plugin.h"
#endif

#include "gdk/gdk_user.h"

static GDK *gdk_singleton = nullptr;
static bool gdk_frame_hook_connected = false;

namespace {

constexpr const char *GDK_RUNTIME_INITIALIZE_ON_STARTUP_SETTING = "gdk/runtime/initialize_on_startup";
constexpr bool GDK_RUNTIME_INITIALIZE_ON_STARTUP_DEFAULT = false;
constexpr const char *GDK_RUNTIME_EMBED_DISPATCH_SETTING = "gdk/runtime/embed_dispatch";
constexpr bool GDK_RUNTIME_EMBED_DISPATCH_DEFAULT = true;
constexpr const char *GDK_RUNTIME_AUTO_ADD_PRIMARY_USER_SETTING = "gdk/runtime/auto_add_primary_user";
constexpr bool GDK_RUNTIME_AUTO_ADD_PRIMARY_USER_DEFAULT = false;

void register_gdk_project_settings() {
	GLOBAL_DEF_BASIC(GDK_RUNTIME_INITIALIZE_ON_STARTUP_SETTING, GDK_RUNTIME_INITIALIZE_ON_STARTUP_DEFAULT);
	GLOBAL_DEF_BASIC(GDK_RUNTIME_EMBED_DISPATCH_SETTING, GDK_RUNTIME_EMBED_DISPATCH_DEFAULT);
	GLOBAL_DEF_BASIC(GDK_RUNTIME_AUTO_ADD_PRIMARY_USER_SETTING, GDK_RUNTIME_AUTO_ADD_PRIMARY_USER_DEFAULT);
}

bool is_embed_dispatch_enabled() {
	ProjectSettings *project_settings = ProjectSettings::get_singleton();
	if (project_settings == nullptr) {
		return GDK_RUNTIME_EMBED_DISPATCH_DEFAULT;
	}

	return static_cast<bool>(project_settings->get_setting(
			GDK_RUNTIME_EMBED_DISPATCH_SETTING,
			GDK_RUNTIME_EMBED_DISPATCH_DEFAULT));
}

void gdk_frame_callback() {
	if (gdk_singleton == nullptr || !gdk_singleton->is_initialized() || !is_embed_dispatch_enabled()) {
		return;
	}

	gdk_singleton->dispatch();
}

void connect_gdk_frame_hook() {
	if (gdk_frame_hook_connected) {
		return;
	}

	SceneTree *scene_tree = SceneTree::get_singleton();
	if (scene_tree == nullptr) {
		return;
	}

	scene_tree->connect("process_frame", callable_mp_static(&gdk_frame_callback));
	gdk_frame_hook_connected = true;
}

void disconnect_gdk_frame_hook() {
	if (!gdk_frame_hook_connected) {
		return;
	}

	SceneTree *scene_tree = SceneTree::get_singleton();
	if (scene_tree != nullptr) {
		scene_tree->disconnect("process_frame", callable_mp_static(&gdk_frame_callback));
	}

	gdk_frame_hook_connected = false;
}

} //namespace

void initialize_xbox_module_module(ModuleInitializationLevel p_level) {
	if (p_level != MODULE_INITIALIZATION_LEVEL_SCENE && p_level != MODULE_INITIALIZATION_LEVEL_EDITOR) {
		return;
	}

	if (p_level != MODULE_INITIALIZATION_LEVEL_SCENE) {
#ifdef TOOLS_ENABLED
		GDREGISTER_CLASS(XboxEditorPlugin);
		EditorPlugins::add_by_type<XboxEditorPlugin>();
#endif
		return;
	}

	GDREGISTER_CLASS(GDK);
	GDREGISTER_CLASS(GDKResult);
	GDREGISTER_INTERNAL_CLASS(GDKPendingSignal);
	GDREGISTER_CLASS(GDKUser);
	GDREGISTER_CLASS(GDKUsers);
	GDREGISTER_CLASS(GDKGameUI);
	GDREGISTER_CLASS(GDKClosedCaptionProperties);
	GDREGISTER_CLASS(GDKAccessibility);
	GDREGISTER_CLASS(GDKAchievement);
	GDREGISTER_CLASS(GDKAchievements);
	GDREGISTER_CLASS(GDKPackageMount);
	GDREGISTER_CLASS(GDKPackageResourcePack);
	GDREGISTER_CLASS(GDKPackage);
	GDREGISTER_CLASS(GDKStats);
	GDREGISTER_CLASS(GDKLeaderboardColumn);
	GDREGISTER_CLASS(GDKLeaderboardRow);
	GDREGISTER_CLASS(GDKLeaderboard);
	GDREGISTER_CLASS(GDKLeaderboards);
	GDREGISTER_CLASS(GDKPrivacy);
	GDREGISTER_CLASS(GDKPresenceRecord);
	GDREGISTER_CLASS(GDKPresence);
	GDREGISTER_CLASS(GDKSocialFilter);
	GDREGISTER_CLASS(GDKSocialGroup);
	GDREGISTER_CLASS(GDKSocialUser);
	GDREGISTER_CLASS(GDKSocial);
	GDREGISTER_CLASS(GDKStoreLicenseStatus);
	GDREGISTER_CLASS(GDKStore);
	GDREGISTER_CLASS(GDKUserProfile);
	GDREGISTER_CLASS(GDKProfile);
	GDREGISTER_CLASS(GDKStringVerify);
	GDREGISTER_CLASS(GDKTitleStorageBlobMetadata);
	GDREGISTER_CLASS(GDKTitleStorageBlobMetadataResult);
	GDREGISTER_CLASS(GDKTitleStorage);
	GDREGISTER_CLASS(GDKErrorReporting);
	GDREGISTER_CLASS(GDKLauncher);
	GDREGISTER_CLASS(GDKMultiplayerActivityInfo);
	GDREGISTER_CLASS(GDKMultiplayerActivity);
	GDREGISTER_CLASS(GDKCaptureMetaData);
	GDREGISTER_CLASS(GDKCapture);
	GDREGISTER_CLASS(GDKSystem);
	GDREGISTER_CLASS(GDKDisplayTimeoutDeferral);
	GDREGISTER_CLASS(GDKDisplay);
	GDREGISTER_CLASS(GDKActivation);

	gdk_singleton = memnew(GDK);
	Engine::get_singleton()->add_singleton(Engine::Singleton("GDK", GDK::get_singleton()));
	register_gdk_project_settings();
	connect_gdk_frame_hook();
}

void uninitialize_xbox_module_module(ModuleInitializationLevel p_level) {
#ifdef TOOLS_ENABLED
	if (p_level == MODULE_INITIALIZATION_LEVEL_EDITOR) {
		return;
	}
#endif

	if (p_level != MODULE_INITIALIZATION_LEVEL_SCENE) {
		return;
	}

	disconnect_gdk_frame_hook();

	Engine::get_singleton()->remove_singleton("GDK");

	if (gdk_singleton) {
		memdelete(gdk_singleton);
		gdk_singleton = nullptr;
	}
}
