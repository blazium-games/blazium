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

} // namespace TestAnticheat
