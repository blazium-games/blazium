/**************************************************************************/
/*  test_portals.h                                                        */
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

#include "../html_portals.h"
#include "../minigame_host.h"
#include "../portal_runtime.h"

#include "core/config/engine.h"
#include "core/io/file_access.h"
#include "core/object/callable_method_pointer.h"
#include "core/object/class_db.h"
#include "core/os/os.h"
#include "modules/modules_enabled.gen.h"
#include "tests/test_macros.h"

#ifdef MODULE_DISCORD_MODULE_ENABLED
#include "modules/discord_module/discord.h"
#endif

namespace TestPortals {

template <class T>
T *singleton_of(const char *p_name) {
	Object *object = Engine::get_singleton()->get_singleton_object(p_name);
	return Object::cast_to<T>(object);
}

TEST_CASE("[Portals] off-host fallbacks") {
	const String expected = "Not on this host";

#ifdef MODULE_DISCORD_MODULE_ENABLED
	Discord *discord = Discord::get_singleton();
	REQUIRE(discord != nullptr);
	CHECK_FALSE(discord->is_on_host());
	discord->ready();
	CHECK(discord->get_last_error() == expected);
	CHECK(ClassDB::has_method("Discord", "embedded_ready"));
	CHECK_FALSE(ClassDB::has_method("Discord", "ready"));
#endif

	YouTube *youtube = singleton_of<YouTube>("YouTube");
	REQUIRE(youtube != nullptr);
	youtube->send_score(1);
	CHECK(youtube->get_last_error() == expected);

	Telegram *telegram = singleton_of<Telegram>("Telegram");
	REQUIRE(telegram != nullptr);
	telegram->send_data("hello");
	CHECK(telegram->get_last_error() == expected);

	Reddit *reddit = singleton_of<Reddit>("Reddit");
	REQUIRE(reddit != nullptr);
	reddit->submit_score(10);
	CHECK(reddit->get_last_error() == expected);

	Newgrounds *newgrounds = singleton_of<Newgrounds>("Newgrounds");
	REQUIRE(newgrounds != nullptr);
	newgrounds->unlock_medal(1);
	CHECK(newgrounds->get_last_error() == expected);

	CrazyGames *crazygames = singleton_of<CrazyGames>("CrazyGames");
	REQUIRE(crazygames != nullptr);
	crazygames->gameplay_start();
	CHECK(crazygames->get_last_error() == expected);

	Poki *poki = singleton_of<Poki>("Poki");
	REQUIRE(poki != nullptr);
	poki->commercial_break();
	CHECK(poki->get_last_error() == expected);

	Meta *meta = singleton_of<Meta>("Meta");
	REQUIRE(meta != nullptr);
	CHECK(meta->get_player_id().is_empty());
	CHECK(meta->get_last_error() == expected);

	GameDistribution *distribution = singleton_of<GameDistribution>("GameDistribution");
	REQUIRE(distribution != nullptr);
	distribution->show_ad();
	CHECK(distribution->get_last_error() == expected);

	WeChat *wechat = singleton_of<WeChat>("WeChat");
	REQUIRE(wechat != nullptr);
	wechat->login();
	CHECK(wechat->get_last_error() == expected);

	Douyin *douyin = singleton_of<Douyin>("Douyin");
	REQUIRE(douyin != nullptr);
	douyin->show_toast("hello");
	CHECK(douyin->get_last_error() == expected);

	TikTok *tiktok = singleton_of<TikTok>("TikTok");
	REQUIRE(tiktok != nullptr);
	tiktok->check_session();
	CHECK(tiktok->get_last_error() == expected);

	Betacade *betacade = singleton_of<Betacade>("Betacade");
	REQUIRE(betacade != nullptr);
	CHECK_FALSE(betacade->is_available());
	CHECK_FALSE(betacade->is_replaying());
	betacade->event("level.complete");
	betacade->set_state(Dictionary());
	betacade->submit_score(3.0);
	betacade->achieve("three-stars");
	CHECK(betacade->get_last_error().is_empty());
	Dictionary saved;
	saved["level"] = 2;
	const bool saved_ok = betacade->save("portal_test_save", saved);
	if (!saved_ok) {
		Error err = OK;
		Ref<FileAccess> probe = FileAccess::open("user://betacade_probe.txt", FileAccess::WRITE, &err);
		INFO("user_data_dir=" << OS::get_singleton()->get_user_data_dir().utf8().get_data() << " probe=" << static_cast<int>(err) << " probe_valid=" << probe.is_valid());
	}
	CHECK(saved_ok);
	Variant loaded = betacade->load("portal_test_save");
	CHECK(loaded.get_type() == Variant::DICTIONARY);
	CHECK(int(Dictionary(loaded)["level"]) == 2);
	CHECK(betacade->save("portal_test_score", 4));
	Variant score = betacade->load("portal_test_score");
	CHECK(score.get_type() == Variant::FLOAT);
	CHECK(double(score) == 4.0);
	betacade->remove("portal_test_save");
	betacade->remove("portal_test_score");
	CHECK(betacade->load("portal_test_save", 0) == Variant(0));
}

TEST_CASE("[Portals] Betacade rejects an empty save key") {
	Betacade *betacade = singleton_of<Betacade>("Betacade");
	REQUIRE(betacade != nullptr);

	ERR_PRINT_OFF;
	const bool empty_saved = betacade->save("", 1);
	ERR_PRINT_ON;
	CHECK_FALSE(empty_saved);
	CHECK(betacade->load("", "missing") == Variant("missing"));

	CHECK(betacade->save("portal_test_key", 1));
	CHECK(betacade->load("portal_test_key", "missing") == Variant(1.0));
	betacade->remove("portal_test_key");
}

static int unpack_zero_calls = 0;
static String unpack_text;

static void unpack_zero() {
	unpack_zero_calls++;
}

static void unpack_one(const String &p_text) {
	unpack_text = p_text;
}

TEST_CASE("[Portals] page callbacks get their arguments, not JavaScriptBridge's Array") {
	unpack_zero_calls = 0;
	unpack_text = String();
	const Callable zero = callable_mp_static(&unpack_zero);
	const Callable one = callable_mp_static(&unpack_one);
	Array payload;
	payload.push_back("{\"level\":7}");

	// What JavaScriptBridge::create_callback does: one argument, the Array.
	const Variant packed = payload;
	const Variant *packed_args[1] = { &packed };
	Variant ret;
	Callable::CallError err;
	ERR_PRINT_OFF;
	one.callp(packed_args, 1, ret, err);
	ERR_PRINT_ON;
	CHECK(unpack_text != "{\"level\":7}");
#ifdef DEBUG_METHODS_ENABLED
	zero.callp(packed_args, 1, ret, err);
	CHECK(err.error == Callable::CallError::CALL_ERROR_TOO_MANY_ARGUMENTS);
	CHECK(unpack_zero_calls == 0);
#endif

	PortalRuntime::call_unpacked(payload, one);
	CHECK(unpack_text == "{\"level\":7}");
	PortalRuntime::call_unpacked(payload, zero);
	CHECK(unpack_zero_calls == 1);
	PortalRuntime::call_unpacked(Array(), zero);
	CHECK(unpack_zero_calls == 2);
}

TEST_CASE("[Portals] shared method table and host-only payments") {
#ifdef MODULE_DISCORD_MODULE_ENABLED
	CHECK(ClassDB::has_signal("Discord", "paused"));
#endif
	CHECK(ClassDB::has_signal("Poki", "resumed"));
	CHECK(ClassDB::has_signal("Poki", "break_done"));
	CHECK(ClassDB::has_signal("YouTube", "audio_enabled"));
	CHECK(ClassDB::has_signal("WeChat", "shown"));
	CHECK(ClassDB::has_signal("Douyin", "hidden"));
	CHECK(ClassDB::has_signal("TikTok", "host_error"));
	CHECK(ClassDB::has_signal("Betacade", "paused"));
	CHECK(ClassDB::has_signal("Betacade", "replay_entered"));
	CHECK(ClassDB::has_signal("Betacade", "replay_state"));
	CHECK(ClassDB::has_signal("Betacade", "replay_exited"));
	CHECK(ClassDB::has_method("Betacade", "is_available"));
	CHECK(ClassDB::has_method("Betacade", "save"));

	CHECK(ClassDB::has_method("WeChat", "login"));
	CHECK(ClassDB::has_method("Douyin", "login"));
	CHECK(ClassDB::has_method("TikTok", "read_host_file"));
	CHECK(ClassDB::has_method("WeChat", "request_midas_payment"));
	CHECK_FALSE(ClassDB::has_method("Douyin", "request_midas_payment"));
	CHECK_FALSE(ClassDB::has_method("TikTok", "request_midas_payment"));
	CHECK(ClassDB::has_method("Douyin", "request_game_payment"));
	CHECK_FALSE(ClassDB::has_method("WeChat", "request_game_payment"));
	CHECK_FALSE(ClassDB::has_method("TikTok", "request_game_payment"));
}

} // namespace TestPortals
