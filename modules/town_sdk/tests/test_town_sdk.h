/**************************************************************************/
/*  test_town_sdk.h                                                       */
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

#include "core/config/engine.h"
#include "core/os/os.h"
#include "core/string/ustring.h"
#include "core/variant/array.h"
#include "core/variant/dictionary.h"
#include "core/variant/typed_array.h"
#include "tests/test_macros.h"

#include "modules/town_sdk/include/turnbattle/protocol.hpp"
#include "modules/town_sdk/town_sdk_client.h"

#include <string>

namespace TestTownSDK {

TEST_CASE("[TownSDK] singleton available") {
	Engine *engine = Engine::get_singleton();
	REQUIRE_MESSAGE(engine != nullptr, "Engine singleton must exist when running tests.");

	CHECK(engine->has_singleton("TownSDK"));

	Object *client = engine->get_singleton_object("TownSDK");
	REQUIRE_MESSAGE(client != nullptr, "Engine singleton 'TownSDK' should exist.");

	CHECK(client->is_class("TownSdkClient"));
}

TEST_CASE("[TownSDK] inventory move is bound and protocol id is 43") {
	CHECK(turnbattle::protocol::MessageType::INVENTORY_MOVE == 43);
	Engine *engine = Engine::get_singleton();
	REQUIRE(engine != nullptr);
	Object *client_obj = engine->get_singleton_object("TownSDK");
	REQUIRE(client_obj != nullptr);
	CHECK(client_obj->has_method("send_inventory_move"));
	CHECK(client_obj->has_method("admin_bank"));
}

TEST_CASE("[TownSDK] HELLO_ACK voip is stored and cleared") {
	Engine *engine = Engine::get_singleton();
	REQUIRE(engine != nullptr);
	Object *client_obj = engine->get_singleton_object("TownSDK");
	REQUIRE(client_obj != nullptr);
	TownSdkClient *sdk = Object::cast_to<TownSdkClient>(client_obj);
	REQUIRE(sdk != nullptr);

	Dictionary empty_ack;
	sdk->apply_hello_ack(empty_ack);
	CHECK_FALSE(sdk->has_voip());

	Dictionary voip;
	voip["host"] = "64.23.133.47";
	voip["port"] = 7100;
	Dictionary ack;
	ack["voip"] = voip;
	sdk->apply_hello_ack(ack);
	CHECK(sdk->has_voip());
	CHECK(sdk->get_voip_host() == String("64.23.133.47"));
	CHECK(sdk->get_voip_port() == 7100);

	sdk->apply_hello_ack(empty_ack);
	CHECK_FALSE(sdk->has_voip());

	sdk->apply_hello_ack(ack);
	sdk->disconnect_from_server();
	CHECK_FALSE(sdk->has_voip());
}

TEST_CASE("[TownSDK] debug log records voip and inventory move") {
	Engine *engine = Engine::get_singleton();
	REQUIRE(engine != nullptr);
	Object *client_obj = engine->get_singleton_object("TownSDK");
	REQUIRE(client_obj != nullptr);
	TownSdkClient *sdk = Object::cast_to<TownSdkClient>(client_obj);
	REQUIRE(sdk != nullptr);

	sdk->set_debug_logging_enabled(true, 64);
	sdk->clear_debug_log();

	Dictionary voip;
	voip["host"] = "64.23.133.47";
	voip["port"] = 7100;
	Dictionary ack;
	ack["voip"] = voip;
	sdk->apply_hello_ack(ack);

	Dictionary from;
	from["kind"] = "bag";
	from["x"] = 0;
	from["y"] = 0;
	Dictionary to;
	to["kind"] = "bag";
	to["x"] = 2;
	to["y"] = 1;
	to["rot"] = 1;
	sdk->send_inventory_move(from, to);
	sdk->send_fire();
	sdk->send_use(3);

	PackedStringArray log = sdk->get_debug_log();
	bool saw_voip = false;
	bool saw_move = false;
	bool saw_fire = false;
	bool saw_use = false;
	for (int i = 0; i < log.size(); ++i) {
		const String line = log[i];
		if (line.contains("voip stored")) {
			saw_voip = true;
		}
		if (line.contains("inventory_move")) {
			saw_move = true;
		}
		if (line.contains("send_fire")) {
			saw_fire = true;
		}
		if (line.contains("send_use")) {
			saw_use = true;
		}
	}
	CHECK(saw_voip);
	CHECK(saw_move);
	CHECK(saw_fire);
	CHECK(saw_use);
	sdk->set_debug_logging_enabled(false);
}

TEST_CASE("[TownSDK] optional live connection") {
	OS *os = OS::get_singleton();
	REQUIRE_MESSAGE(os != nullptr, "OS singleton must exist when running tests.");

	const String host = os->get_environment("TOWN_SDK_TEST_HOST");
	if (host.is_empty()) {
		INFO("TownSDK live connection test skipped: set TOWN_SDK_TEST_HOST to enable.");
		return;
	}

	const String port_env = os->get_environment("TOWN_SDK_TEST_PORT");
	int port = 7000;
	if (!port_env.is_empty()) {
		port = port_env.to_int();
	}

	Engine *engine = Engine::get_singleton();
	REQUIRE_MESSAGE(engine != nullptr, "Engine singleton must exist when running tests.");

	Object *client_obj = engine->get_singleton_object("TownSDK");
	REQUIRE_MESSAGE(client_obj != nullptr, "TownSDK singleton should exist when live test is enabled.");
	REQUIRE(client_obj->is_class("TownSdkClient"));

	Array logging_args;
	logging_args.push_back(true);
	logging_args.push_back(128);
	client_obj->callv("set_debug_logging_enabled", logging_args);

	Array connect_args;
	connect_args.push_back(host);
	connect_args.push_back(port);

	Variant result = client_obj->callv("connect_to_server", connect_args);
	const bool connected = result.operator bool();

	CharString host_utf8 = host.utf8();
	std::string failure_message = "TownSdkClient failed to connect to ";
	failure_message += host_utf8.get_data();
	failure_message += ":";
	failure_message += std::to_string(port);

	if (connected) {
		Variant is_connected = client_obj->call("is_client_connected");
		CHECK(is_connected.operator bool());
		client_obj->call("disconnect_from_server");
		client_obj->call("clear_debug_log");
	} else {
		PackedStringArray debug_log = client_obj->call("get_debug_log");
		if (!debug_log.is_empty()) {
			failure_message += "\nCaptured debug log:";
		}
		for (int i = 0; i < debug_log.size(); ++i) {
			CharString entry_utf8 = debug_log[i].utf8();
			std::string entry_str = entry_utf8.get_data();
			failure_message += "\n";
			failure_message += entry_str;
		}
		String last_error = client_obj->call("get_last_error_message");
		if (!last_error.is_empty()) {
			CharString err_utf8 = last_error.utf8();
			std::string error_str = err_utf8.get_data();
			failure_message += "\nLast error: ";
			failure_message += error_str;
		}
	}

	CHECK_MESSAGE(connected, failure_message.c_str());
}

} // namespace TestTownSDK
