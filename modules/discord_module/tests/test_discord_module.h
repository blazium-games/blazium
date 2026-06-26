/**************************************************************************/
/*  test_discord_module.h                                                 */
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

#include "../discord.h"
#include "../discord_api_loader.h"

#include "tests/test_macros.h"

namespace TestDiscordModule {

TEST_CASE("[DiscordModule] runtime library name") {
#ifdef WINDOWS_ENABLED
	CHECK(DiscordAPILoader::get_runtime_library_name() == "discord_partner_sdk.dll");
#else
	CHECK(DiscordAPILoader::get_runtime_library_name() == "libdiscord_partner_sdk.so");
#endif
}

TEST_CASE("[DiscordModule] singleton registration") {
	CHECK(Discord::get_singleton() != nullptr);
}

TEST_CASE("[DiscordModule] loader unavailable without dll") {
	DiscordAPILoader api_loader;
	const bool loaded = api_loader.try_load();
	CHECK(loaded == api_loader.is_loaded());
}

TEST_CASE("[DiscordModule] unavailable without runtime library") {
	Discord *discord = Discord::get_singleton();
	REQUIRE(discord != nullptr);
	if (DiscordAPILoader::is_runtime_library_present()) {
		return;
	}
	CHECK_FALSE(discord->is_available());
	CHECK(discord->initialize(123456789) == ERR_UNAVAILABLE);
}

TEST_CASE("[DiscordModule] auth state starts none") {
	Discord *discord = Discord::get_singleton();
	REQUIRE(discord != nullptr);
	CHECK(discord->get_auth_state() == Discord::AUTH_NONE);
	CHECK_FALSE(discord->is_authorization_pending());
	CHECK_FALSE(discord->is_authorized());
}

TEST_CASE("[DiscordModule] rich presence requires ready client") {
	Discord *discord = Discord::get_singleton();
	REQUIRE(discord != nullptr);
	CHECK_FALSE(discord->is_ready_for_presence());
	CHECK(discord->get_relationships_count() == -1);
	Dictionary activity;
	CHECK(discord->update_rich_presence(activity) == ERR_UNAUTHORIZED);
}

TEST_CASE("[DiscordModule] auth lifecycle signals are registered") {
	Discord *discord = Discord::get_singleton();
	REQUIRE(discord != nullptr);
	CHECK(discord->has_signal("authorization_started"));
	CHECK(discord->has_signal("authorized"));
	CHECK(discord->has_signal("authorization_failed"));
	CHECK(discord->has_signal("connection_state_changed"));
	CHECK(discord->has_signal("ready"));
	CHECK(discord->has_signal("rich_presence_updated"));
}

TEST_CASE("[DiscordModule] default presence scopes when loaded") {
	DiscordAPILoader api_loader;
	if (!api_loader.try_load()) {
		return;
	}
	const String scopes = api_loader.get_default_presence_scopes();
	CHECK_FALSE(scopes.is_empty());
}

TEST_CASE("[DiscordModule] default communication scopes when loaded") {
	DiscordAPILoader api_loader;
	if (!api_loader.try_load()) {
		return;
	}
	const String scopes = api_loader.get_default_communication_scopes();
	CHECK_FALSE(scopes.is_empty());
}

TEST_CASE("[DiscordModule] game activity empty when not ready") {
	Discord *discord = Discord::get_singleton();
	REQUIRE(discord != nullptr);
	CHECK(discord->get_game_activity().is_empty());
	CHECK(discord->get_requested_scopes().is_empty());
	CHECK(discord->get_granted_scopes().is_empty());
}

TEST_CASE("[DiscordModule] server auth gated when not ready") {
	Discord *discord = Discord::get_singleton();
	REQUIRE(discord != nullptr);
	Ref<DiscordAuthResult> result = discord->authenticate_with_server("http://example.com/auth", "token", 1);
	REQUIRE(result.is_valid());
	CHECK_FALSE(result->is_success());
	CHECK_FALSE(result->get_error_message().is_empty());
}

TEST_CASE("[DiscordModule] auth client rejects empty url when ready would allow") {
	Discord *discord = Discord::get_singleton();
	REQUIRE(discord != nullptr);
	Ref<DiscordAuthResult> result = discord->authenticate_with_server("", "token", 1);
	REQUIRE(result.is_valid());
	CHECK_FALSE(result->is_success());
	CHECK_FALSE(result->get_error_message().is_empty());
}

TEST_CASE("[DiscordModule] version dictionary") {
	Discord *discord = Discord::get_singleton();
	REQUIRE(discord != nullptr);
	Dictionary version = discord->get_version();
	CHECK(version.has("runtime_library"));
	CHECK(version.has("runtime_present"));
	CHECK(version.has("sdk_enabled"));
}

} //namespace TestDiscordModule
