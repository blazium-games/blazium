/**************************************************************************/
/*  test_steam.h                                                          */
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

#include "../steam.h"
#include "../steam_api_loader.h"
#include "../steam_inventory_item.h"

#include "core/io/image.h"
#include "tests/test_macros.h"

namespace TestSteam {

TEST_CASE("[Steam] bytes_to_hex encoding") {
	const uint8_t data[] = { 0xDE, 0xAD, 0xBE, 0xEF };
	String hex = SteamAPILoader::bytes_to_hex(data, 4);
	CHECK(hex == "deadbeef");
}

TEST_CASE("[Steam] image_from_rgba conversion") {
	const uint8_t rgba[] = {
		255,
		0,
		0,
		255,
		0,
		255,
		0,
		255,
		0,
		0,
		255,
		255,
		255,
		255,
		255,
		255,
	};
	Ref<Image> image = SteamAPILoader::image_from_rgba(rgba, 2, 2);
	REQUIRE(image.is_valid());
	CHECK(image->get_width() == 2);
	CHECK(image->get_height() == 2);
	CHECK(image->get_format() == Image::FORMAT_RGBA8);
	Color top_left = image->get_pixel(0, 0);
	CHECK(top_left.get_r8() == 255);
	CHECK(top_left.get_a8() == 255);
}

TEST_CASE("[Steam] loader unavailable without dll") {
	SteamAPILoader loader;
	const bool loaded = loader.try_load();
	CHECK(loaded == loader.is_loaded());
}

TEST_CASE("[Steam] achievement methods no-op without init") {
	Steam *steam = Steam::get_singleton();
	REQUIRE(steam != nullptr);
	CHECK_FALSE(steam->set_achievement("TEST_ACHIEVEMENT"));
	CHECK_FALSE(steam->clear_achievement("TEST_ACHIEVEMENT"));
	CHECK_FALSE(steam->get_achievement("TEST_ACHIEVEMENT"));
	CHECK_FALSE(steam->request_current_stats());
	CHECK_FALSE(steam->refresh_current_stats());
	CHECK_FALSE(steam->store_stats());
	Ref<SteamAchievementInfo> info = steam->get_achievement_info("TEST_ACHIEVEMENT");
	CHECK_FALSE(info.is_valid());
	CHECK(steam->get_all_achievements().is_empty());
	CHECK(steam->get_stat_int("TEST_STAT") == -1);
	CHECK(steam->get_stat_float("TEST_STAT") == 0.0f);
	CHECK_FALSE(steam->set_stat_int("TEST_STAT", 1));
	CHECK_FALSE(steam->set_stat_float("TEST_STAT", 1.0f));
	CHECK_FALSE(steam->increment_stat_int("TEST_STAT", 1));
	CHECK_FALSE(steam->increment_stat_int("TEST_STAT", 2));
	CHECK_FALSE(steam->clear_stat("TEST_STAT"));
	CHECK(steam->get_persona_name().is_empty());
	CHECK_FALSE(steam->get_avatar_image().is_valid());
	CHECK_FALSE(steam->load_item_definitions());
	CHECK(steam->get_item_definition_ids().is_empty());
	CHECK_FALSE(steam->get_item_definition(1).is_valid());
	CHECK(steam->get_all_item_definitions().is_empty());
	CHECK(steam->get_all_items().is_empty());
	CHECK(steam->get_items_by_id(PackedInt64Array()).is_empty());
	CHECK(steam->find_items_by_def_id(1).is_empty());
	CHECK_FALSE(steam->get_item_definition_icon(1).is_valid());
	CHECK_FALSE(steam->consume_item(1, 1));
	CHECK_FALSE(steam->trigger_item_drop(1));
	CHECK(steam->get_eligible_promo_item_definition_ids().is_empty());
	CHECK(steam->get_inventory_result_status(-1) == 0);
	CHECK(steam->get_inventory_result_items(-1).is_empty());
	CHECK_FALSE(steam->grant_promo_items());
	CHECK_FALSE(steam->add_promo_item(1));
	CHECK_FALSE(steam->start_property_update());
	CHECK_FALSE(steam->submit_property_update());
	CHECK(steam->deserialize_inventory(PackedByteArray()) == -1);
	CHECK_FALSE(steam->has_inventory_support());
}

TEST_CASE("[Steam] inventory item without init") {
	Ref<SteamInventoryItem> item;
	item.instantiate();
	item->set_def_id(100);
	CHECK(item->get_def_id() == 100);
	CHECK_FALSE(item->get_definition().is_valid());
}

TEST_CASE("[Steam] singleton registration") {
	CHECK(Steam::get_singleton() != nullptr);
}

} // namespace TestSteam
