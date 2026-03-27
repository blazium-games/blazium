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

#include "register_types.h"

#include "core/config/engine.h"
#include "core/object/class_db.h"

#include "enet_client.h"
#include "enet_packet.h"
#include "enet_server.h"
#include "enet_server_peer.h"

static ENetPacketUtils *enet_packet_singleton = nullptr;
static ENetServer *enet_server_singleton = nullptr;
static ENetClient *enet_client_singleton = nullptr;

void initialize_enet_server_module(ModuleInitializationLevel p_level) {
	if (p_level != MODULE_INITIALIZATION_LEVEL_SCENE) {
		return;
	}

	// Create and register ENetPacketUtils singleton
	enet_packet_singleton = memnew(ENetPacketUtils);
	GDREGISTER_CLASS(ENetPacketUtils);
	Engine::get_singleton()->add_singleton(Engine::Singleton("ENetPacketUtils", ENetPacketUtils::get_singleton()));

	// Register ENetServerPeer class
	GDREGISTER_CLASS(ENetServerPeer);

	// Create and register ENetServer singleton
	enet_server_singleton = memnew(ENetServer);
	GDREGISTER_CLASS(ENetServer);
	Engine::get_singleton()->add_singleton(Engine::Singleton("ENetServer", ENetServer::get_singleton()));

	// Create and register ENetClient singleton
	enet_client_singleton = memnew(ENetClient);
	GDREGISTER_CLASS(ENetClient);
	Engine::get_singleton()->add_singleton(Engine::Singleton("ENetClient", ENetClient::get_singleton()));
}

void uninitialize_enet_server_module(ModuleInitializationLevel p_level) {
	if (p_level != MODULE_INITIALIZATION_LEVEL_SCENE) {
		return;
	}

	// Cleanup singletons in reverse order
	if (enet_client_singleton) {
		Engine::get_singleton()->remove_singleton("ENetClient");
		memdelete(enet_client_singleton);
		enet_client_singleton = nullptr;
	}

	if (enet_server_singleton) {
		Engine::get_singleton()->remove_singleton("ENetServer");
		memdelete(enet_server_singleton);
		enet_server_singleton = nullptr;
	}

	if (enet_packet_singleton) {
		Engine::get_singleton()->remove_singleton("ENetPacketUtils");
		memdelete(enet_packet_singleton);
		enet_packet_singleton = nullptr;
	}
}
