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

#include "core/config/engine.h"
#include "core/object/class_db.h"
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
