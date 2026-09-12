/**************************************************************************/
/*  justamcp_prompt_blazium_multiplayer_architect.cpp                     */
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

#ifdef TOOLS_ENABLED

#include "justamcp_prompt_blazium_multiplayer_architect.h"

void JustAMCPPromptBlaziumMultiplayerArchitect::_bind_methods() {}

JustAMCPPromptBlaziumMultiplayerArchitect::JustAMCPPromptBlaziumMultiplayerArchitect() {}
JustAMCPPromptBlaziumMultiplayerArchitect::~JustAMCPPromptBlaziumMultiplayerArchitect() {}

String JustAMCPPromptBlaziumMultiplayerArchitect::get_name() const {
	return "blazium_multiplayer_architect";
}

Dictionary JustAMCPPromptBlaziumMultiplayerArchitect::get_prompt() const {
	Dictionary result;
	result["name"] = "blazium_multiplayer_architect";
	result["title"] = "Blazium Multiplayer Architect";
	result["description"] = "Produces authoritative Blazium multiplayer networking code using Godot 4 RPCs, MultiplayerSpawner, and MultiplayerSynchronizer nodes.";

	Array arguments;
	Dictionary target;
	target["name"] = "networking_layer";
	target["description"] = "The scene name or system description needing networked bindings (e.g. 'Player movement sync', 'Enemy spawning', 'Lobby system').";
	target["required"] = true;
	arguments.push_back(target);

	result["arguments"] = arguments;
	return result;
}

Dictionary JustAMCPPromptBlaziumMultiplayerArchitect::get_messages(const Dictionary &p_args) {
	Dictionary result;
	result["description"] = "Blazium Multiplayer Architecture Design";

	String target = p_args.has("networking_layer") ? String(p_args["networking_layer"]) : "[Networking Target]";

	Array messages;
	Dictionary msg;
	msg["role"] = "user";

	Dictionary content;
	content["type"] = "text";

	String text = String("You are a senior Blazium multiplayer developer. Produce production-quality Godot 4 networking code for: ") + target + String("\n\n");
	text += String("Use ONLY the Godot 4 High-Level Multiplayer API. Never suggest raw TCP/UDP sockets, ENet manual calls, or Godot 3 patterns.\n\n");

	text += String("## Rule 1: RPC Declarations - Always explicit\n");
	text += String("BAD:  remote func fire(): pass\n");
	text += String("GOOD: @rpc(\"any_peer\", \"call_local\", \"reliable\")\n");
	text += String("      func fire() -> void: pass\n");
	text += String("Available modes: \"any_peer\" | \"authority\", \"call_local\" | \"call_remote\", \"reliable\" | \"unreliable\" | \"unreliable_ordered\"\n\n");

	text += String("## Rule 2: Authority Checks - Always semantic\n");
	text += String("BAD:  if get_multiplayer_authority() == 1:\n");
	text += String("GOOD: if is_multiplayer_authority():\n");
	text += String("Server-only logic must be guarded: if not multiplayer.is_server(): return\n\n");

	text += String("## Rule 3: Player Spawning - Use MultiplayerSpawner\n");
	text += String("BAD:  rpc(\"spawn_player\", peer_id) then manual add_child()\n");
	text += String("GOOD: Add a MultiplayerSpawner node to the scene.\n");
	text += String("      Set its spawn_path and register the player PackedScene.\n");
	text += String("      It handles spawn/despawn automatically when peers connect/disconnect.\n\n");

	text += String("## Rule 4: State Sync - Use MultiplayerSynchronizer\n");
	text += String("BAD:  Manually calling rpc() every frame to sync position/health.\n");
	text += String("GOOD: Add MultiplayerSynchronizer as a child of the synchronized node.\n");
	text += String("      Configure its replication properties in the inspector or via code.\n");
	text += String("      Set sync interval appropriate to the property (position: unreliable, health: reliable).\n\n");

	text += String("## Rule 5: Peer ID Management\n");
	text += String("- Assign unique IDs via multiplayer.get_unique_id() on each peer.\n");
	text += String("- Track connected peers via multiplayer.peer_connected and peer_disconnected signals.\n");
	text += String("- The server peer ID is always 1.\n\n");

	text += String("## Required Output\n");
	text += String("1. Updated GDScript with full @rpc annotations and authority guards.\n");
	text += String("2. Scene additions needed (MultiplayerSpawner / MultiplayerSynchronizer nodes and their configuration).\n");
	text += String("3. Architecture notes clearly separating what runs server-side vs. client-side.");

	content["text"] = text;
	msg["content"] = content;

	messages.push_back(msg);
	result["messages"] = messages;
	result["ok"] = true;
	return result;
}

Dictionary JustAMCPPromptBlaziumMultiplayerArchitect::complete(const Dictionary &p_argument) {
	Dictionary completion;
	completion["values"] = Array();
	completion["total"] = 0;
	completion["hasMore"] = false;
	return completion;
}

#endif
