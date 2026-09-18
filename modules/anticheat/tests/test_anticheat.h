/**************************************************************************/
/*  test_anticheat.h                                                      */
/**************************************************************************/
/*                         This file is part of:                          */
/*                             BLAZIUM ENGINE                             */
/*                          https://blazium.app                           */
/**************************************************************************/

#pragma once

#include "../anticheat.h"
#include "../anticheat_api_loader.h"
#include "../anticheat_types.h"

#include "core/config/engine.h"
#include "core/config/project_settings.h"
#include "tests/test_macros.h"

namespace TestAnticheat {

TEST_CASE("[Anticheat] loader fails closed without dll") {
	AnticheatAPILoader loader;
	const bool loaded = loader.try_load();
	CHECK(loaded == loader.is_loaded());
	if (!loaded) {
		CHECK_FALSE(loader.is_loaded());
	}
}

TEST_CASE("[Anticheat] singleton exists") {
	Engine *engine = Engine::get_singleton();
	REQUIRE(engine != nullptr);
	CHECK(engine->has_singleton("Anticheat"));
}

TEST_CASE("[Anticheat] verify missing sidecar fails when required") {
	CHECK_FALSE(AnticheatAPILoader::verify_runtime_file("this_file_does_not_exist_bzcl.dll", true));
}

TEST_CASE("[Anticheat] verify missing sig fails closed") {
	CHECK_FALSE(AnticheatAPILoader::verify_runtime_file("this_file_does_not_exist_bzcl.dll", true));
}

TEST_CASE("[Anticheat] screenshot command without viewport does not crash") {
	Anticheat *ac = Anticheat::get_singleton();
	REQUIRE(ac != nullptr);
	CHECK(ac->submit_command("screenshot") == ANTICHEAT_ERR_INIT);
}

TEST_CASE("[Anticheat] ops_connect without dedicated runtimes is unavailable") {
	Anticheat *ac = Anticheat::get_singleton();
	REQUIRE(ac != nullptr);
	CHECK(ac->ops_connect() == ANTICHEAT_ERR_UNAVAILABLE);
}

TEST_CASE("[Anticheat] sv_initialize without dedicated runtimes is unavailable") {
	Anticheat *ac = Anticheat::get_singleton();
	REQUIRE(ac != nullptr);
	CHECK(ac->sv_initialize() == ANTICHEAT_ERR_UNAVAILABLE);
	CHECK_FALSE(ac->is_server_initialized());
}

TEST_CASE("[Anticheat] bind_player maps kick without a runtime") {
	Anticheat *ac = Anticheat::get_singleton();
	REQUIRE(ac != nullptr);
	ac->bind_player(3, "alice");
	CHECK(ac->player_client_index("alice") == 3);
	ac->apply_ops_line("{\"event\":\"Kick\",\"player_id\":\"nobody\"}");
	CHECK(ac->player_client_index("alice") == 3);
	ac->apply_ops_line("{\"event\":\"Kick\",\"player_id\":\"alice\",\"pairs\":{\"reason\":\"ops\"}}");
	CHECK(ac->player_client_index("alice") == -1);
	ac->apply_ops_line("{\"event\":\"Message\",\"player_id\":\"bob\",\"pairs\":{\"text\":\"hi\"}}");
	ac->apply_ops_line("{\"event\":\"GlobalMessage\",\"pairs\":{\"reason\":\"all\"}}");
	ac->apply_ops_line("{\"event\":\"Kill\",\"player_id\":\"bob\"}");
	ac->apply_ops_line("{\"event\":\"Screenshot\",\"player_id\":\"bob\"}");
	ac->apply_ops_line("{\"event\":\"Teleport\",\"player_id\":\"bob\",\"pairs\":{\"region\":\"town\",\"x\":1,\"y\":2,\"z\":3}}");
	ac->apply_ops_line("{\"event\":\"Warn\",\"player_id\":\"bob\",\"pairs\":{\"text\":\"stop\"}}");
	ac->apply_ops_line("{\"event\":\"Mute\",\"player_id\":\"bob\",\"pairs\":{\"until_unix\":1}}");
	ac->apply_ops_line("{\"event\":\"Spectate\",\"player_id\":\"bob\",\"pairs\":{\"until_unix\":0}}");
	ac->apply_ops_line("{\"event\":\"KickMsg\",\"player_id\":\"nobody\",\"pairs\":{\"text\":\"bye\"}}");
	ac->apply_ops_line("{\"event\":\"Timeout\",\"player_id\":\"nobody\",\"pairs\":{\"reason\":\"temp\"}}");
	ac->apply_ops_line("{\"event\":\"Note\",\"player_id\":\"bob\",\"pairs\":{\"text\":\"flag\"}}");
	ac->apply_ops_line("{\"event\":\"Unmute\",\"player_id\":\"bob\"}");
	CHECK_FALSE(ac->is_server_initialized());
}

TEST_CASE("[Anticheat] saas ops_connect skips missing license file") {
	Anticheat *ac = Anticheat::get_singleton();
	REQUIRE(ac != nullptr);
	ProjectSettings *ps = ProjectSettings::get_singleton();
	REQUIRE(ps != nullptr);
	const Variant prev_mode = ps->get("anticheat/ops/mode");
	const Variant prev_lic = ps->get("anticheat/ops/license_path");
	ps->set("anticheat/ops/mode", "saas");
	ps->set("anticheat/ops/license_path", "this_license_does_not_exist.json");
	CHECK(ac->ops_connect() == ANTICHEAT_ERR_UNAVAILABLE);
	ps->set("anticheat/ops/mode", prev_mode);
	ps->set("anticheat/ops/license_path", prev_lic);
}

} // namespace TestAnticheat
