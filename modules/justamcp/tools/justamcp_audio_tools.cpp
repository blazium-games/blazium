/**************************************************************************/
/*  justamcp_audio_tools.cpp                                              */
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

#include "justamcp_audio_tools.h"

#include "core/io/resource_loader.h"
#include "scene/2d/audio_stream_player_2d.h"
#include "scene/3d/audio_stream_player_3d.h"
#include "scene/audio/audio_stream_player.h"
#include "servers/audio/audio_effect.h"
#include "servers/audio_server.h"

#include "../justamcp_editor_scene_access.h"
#include "../justamcp_mcp_tool_macros.h"

void JustAMCPAudioTools::_bind_methods() {}

JustAMCPAudioTools::JustAMCPAudioTools() {}
JustAMCPAudioTools::~JustAMCPAudioTools() {}

Node *JustAMCPAudioTools::_find_node_by_path(const String &p_path) {
	Node *root = JustAMCPEditorSceneAccess::get_edited_root();
	if (!root) {
		return nullptr;
	}

	if (p_path == "." || p_path == root->get_name()) {
		return root;
	}
	if (root->has_node(p_path)) {
		return root->get_node(p_path);
	}

	if (p_path.begins_with(String(root->get_name()) + "/")) {
		String rel = p_path.substr(String(root->get_name()).length() + 1);
		if (root->has_node(rel)) {
			return root->get_node(rel);
		}
	}
	return nullptr;
}

Dictionary JustAMCPAudioTools::execute_tool(const String &p_tool_name, const Dictionary &p_args) {
	if (p_tool_name == "get_audio_bus_layout") {
		return _get_audio_bus_layout(p_args);
	}
	if (p_tool_name == "add_audio_bus") {
		return _add_audio_bus(p_args);
	}
	if (p_tool_name == "set_audio_bus") {
		return _set_audio_bus(p_args);
	}
	if (p_tool_name == "add_audio_bus_effect") {
		return _add_audio_bus_effect(p_args);
	}
	if (p_tool_name == "add_audio_player") {
		return _add_audio_player(p_args);
	}
	if (p_tool_name == "audio_get_players_info") {
		return _get_audio_info(p_args);
	}

	Dictionary err;
	err["code"] = -32601;
	err["message"] = "Method not found: " + p_tool_name;
	Dictionary res;
	res["error"] = err;
	return Dictionary();
}

Dictionary JustAMCPAudioTools::_get_effect_params(Object *p_effect) {
	Dictionary params;
	if (!p_effect) {
		return params;
	}

	List<PropertyInfo> plist;
	p_effect->get_property_list(&plist);

	for (const PropertyInfo &pi : plist) {
		if (pi.usage & PROPERTY_USAGE_EDITOR) {
			params[pi.name] = p_effect->get(pi.name);
		}
	}
	return params;
}

Dictionary JustAMCPAudioTools::_get_audio_bus_layout(const Dictionary &p_params) {
	AudioServer *as = AudioServer::get_singleton();
	if (!as) {
		return MCP_INTERNAL("AudioServer not available");
	}

	Array buses;
	for (int i = 0; i < as->get_bus_count(); i++) {
		Dictionary bus_data;
		bus_data["index"] = i;
		bus_data["name"] = as->get_bus_name(i);
		bus_data["volume_db"] = as->get_bus_volume_db(i);
		bus_data["solo"] = as->is_bus_solo(i);
		bus_data["mute"] = as->is_bus_mute(i);
		bus_data["bypass_effects"] = as->is_bus_bypassing_effects(i);
		bus_data["send"] = as->get_bus_send(i);

		Array effects;
		for (int j = 0; j < as->get_bus_effect_count(i); j++) {
			Ref<AudioEffect> effect = as->get_bus_effect(i, j);
			if (effect.is_valid()) {
				Dictionary effect_data;
				effect_data["index"] = j;
				effect_data["type"] = effect->get_class();
				effect_data["enabled"] = as->is_bus_effect_enabled(i, j);
				effect_data["params"] = _get_effect_params(effect.ptr());
				effects.push_back(effect_data);
			}
		}
		bus_data["effects"] = effects;
		buses.push_back(bus_data);
	}

	Dictionary res;
	res["bus_count"] = as->get_bus_count();
	res["buses"] = buses;
	return MCP_SUCCESS(res);
}

Dictionary JustAMCPAudioTools::_add_audio_bus(const Dictionary &p_params) {
	if (!p_params.has("name")) {
		return MCP_INVALID_PARAMS("Missing param: name");
	}
	String bus_name = p_params["name"];

	AudioServer *as = AudioServer::get_singleton();
	if (!as) {
		return MCP_INTERNAL("AudioServer not available");
	}

	for (int i = 0; i < as->get_bus_count(); i++) {
		if (as->get_bus_name(i) == bus_name) {
			return MCP_INVALID_PARAMS("Audio bus '" + bus_name + "' already exists");
		}
	}

	int at_position = p_params.has("at_position") ? int(p_params["at_position"]) : -1;
	as->add_bus(at_position);

	int idx = (at_position < 0) ? as->get_bus_count() - 1 : at_position;
	as->set_bus_name(idx, bus_name);

	if (p_params.has("volume_db")) {
		as->set_bus_volume_db(idx, float(p_params["volume_db"]));
	}
	if (p_params.has("send")) {
		String send = p_params["send"];
		if (!send.is_empty()) {
			as->set_bus_send(idx, send);
		}
	}
	if (p_params.has("solo")) {
		as->set_bus_solo(idx, bool(p_params["solo"]));
	}
	if (p_params.has("mute")) {
		as->set_bus_mute(idx, bool(p_params["mute"]));
	}

	Dictionary res;
	res["name"] = bus_name;
	res["index"] = idx;
	res["bus_count"] = as->get_bus_count();
	return MCP_SUCCESS(res);
}

Dictionary JustAMCPAudioTools::_set_audio_bus(const Dictionary &p_params) {
	if (!p_params.has("name")) {
		return MCP_INVALID_PARAMS("Missing param: name");
	}
	String bus_name = p_params["name"];

	AudioServer *as = AudioServer::get_singleton();
	if (!as) {
		return MCP_INTERNAL("AudioServer not available");
	}

	int idx = -1;
	for (int i = 0; i < as->get_bus_count(); i++) {
		if (as->get_bus_name(i) == bus_name) {
			idx = i;
			break;
		}
	}
	if (idx < 0) {
		return MCP_NOT_FOUND("Audio bus '" + bus_name + "'");
	}

	int changes = 0;
	if (p_params.has("volume_db")) {
		as->set_bus_volume_db(idx, float(p_params["volume_db"]));
		changes++;
	}
	if (p_params.has("solo")) {
		as->set_bus_solo(idx, bool(p_params["solo"]));
		changes++;
	}
	if (p_params.has("mute")) {
		as->set_bus_mute(idx, bool(p_params["mute"]));
		changes++;
	}
	if (p_params.has("bypass_effects")) {
		as->set_bus_bypass_effects(idx, bool(p_params["bypass_effects"]));
		changes++;
	}
	if (p_params.has("send")) {
		String send = p_params["send"];
		if (!send.is_empty()) {
			as->set_bus_send(idx, send);
			changes++;
		}
	}
	if (p_params.has("rename")) {
		String new_name = p_params["rename"];
		as->set_bus_name(idx, new_name);
		bus_name = new_name;
		changes++;
	}

	Dictionary res;
	res["name"] = bus_name;
	res["index"] = idx;
	res["changes"] = changes;
	return MCP_SUCCESS(res);
}

Dictionary JustAMCPAudioTools::_add_audio_bus_effect(const Dictionary &p_params) {
	if (!p_params.has("bus")) {
		return MCP_INVALID_PARAMS("Missing param: bus");
	}
	if (!p_params.has("effect_type")) {
		return MCP_INVALID_PARAMS("Missing param: effect_type");
	}

	String bus_name = p_params["bus"];
	String effect_type = p_params["effect_type"];

	AudioServer *as = AudioServer::get_singleton();
	if (!as) {
		return MCP_INTERNAL("AudioServer not available");
	}

	int bus_idx = -1;
	for (int i = 0; i < as->get_bus_count(); i++) {
		if (as->get_bus_name(i) == bus_name) {
			bus_idx = i;
			break;
		}
	}
	if (bus_idx < 0) {
		return MCP_NOT_FOUND("Audio bus '" + bus_name + "'");
	}

	if (!ClassDB::class_exists(effect_type) || !ClassDB::is_parent_class(effect_type, "AudioEffect")) {
		return MCP_INVALID_PARAMS("Unknown audio effect type: " + effect_type);
	}

	Object *_effect_obj = ClassDB::instantiate(effect_type);
	Ref<AudioEffect> effect = Ref<AudioEffect>(Object::cast_to<AudioEffect>(_effect_obj));
	if (effect.is_null() && _effect_obj) {
		memdelete(_effect_obj);
	}
	if (effect.is_null()) {
		return MCP_INTERNAL("Failed to instantiate effect");
	}

	if (p_params.has("params")) {
		Dictionary ep = p_params["params"];
		Array keys = ep.keys();
		for (int i = 0; i < keys.size(); i++) {
			effect->set(keys[i], ep[keys[i]]);
		}
	}

	int at_position = p_params.has("at_position") ? int(p_params["at_position"]) : -1;
	as->add_bus_effect(bus_idx, effect, at_position);

	int effect_idx = (at_position < 0) ? as->get_bus_effect_count(bus_idx) - 1 : at_position;

	Dictionary res;
	res["bus"] = bus_name;
	res["bus_index"] = bus_idx;
	res["effect_type"] = effect->get_class();
	res["effect_index"] = effect_idx;
	return MCP_SUCCESS(res);
}

Dictionary JustAMCPAudioTools::_add_audio_player(const Dictionary &p_params) {
	if (!p_params.has("node_path")) {
		return MCP_INVALID_PARAMS("Missing param: node_path");
	}
	if (!p_params.has("name")) {
		return MCP_INVALID_PARAMS("Missing param: name");
	}

	String node_path = p_params["node_path"];
	String player_name = p_params["name"];
	String player_type = p_params.has("type") ? String(p_params["type"]) : "AudioStreamPlayer";

	Node *root = JustAMCPEditorSceneAccess::get_edited_root();
	if (!root) {
		return MCP_ERROR(-32000, "No scene is currently open");
	}

	Node *parent = _find_node_by_path(node_path);
	if (!parent) {
		return MCP_NOT_FOUND("Node at '" + node_path + "'");
	}

	if (player_type != "AudioStreamPlayer" && player_type != "AudioStreamPlayer2D" && player_type != "AudioStreamPlayer3D") {
		return MCP_INVALID_PARAMS("Invalid player type: " + player_type);
	}

	Object *_player_obj = ClassDB::instantiate(player_type);
	Node *player = Object::cast_to<Node>(_player_obj);
	if (!player && _player_obj) {
		memdelete(_player_obj);
	}
	if (!player) {
		return MCP_INTERNAL("Failed to instantiate player");
	}

	player->set_name(player_name);

	String stream_path = p_params.has("stream") ? String(p_params["stream"]) : "";
	if (!stream_path.is_empty()) {
		if (ResourceLoader::exists(stream_path)) {
			Ref<Resource> stream = ResourceLoader::load(stream_path);
			if (stream.is_valid() && stream->is_class("AudioStream")) {
				player->set("stream", stream);
			} else {
				memdelete(player);
				return MCP_INVALID_PARAMS("Resource at '" + stream_path + "' is not an AudioStream");
			}
		} else {
			memdelete(player);
			return MCP_NOT_FOUND("Audio stream at '" + stream_path + "'");
		}
	}

	if (p_params.has("volume_db")) {
		player->set("volume_db", float(p_params["volume_db"]));
	}
	if (p_params.has("bus")) {
		String bus = p_params["bus"];
		if (!bus.is_empty()) {
			player->set("bus", bus);
		}
	}
	if (p_params.has("autoplay")) {
		player->set("autoplay", bool(p_params["autoplay"]));
	}

	if (player_type == "AudioStreamPlayer2D") {
		if (p_params.has("max_distance")) {
			player->set("max_distance", float(p_params["max_distance"]));
		}
		if (p_params.has("attenuation")) {
			player->set("attenuation", float(p_params["attenuation"]));
		}
	} else if (player_type == "AudioStreamPlayer3D") {
		if (p_params.has("max_distance")) {
			player->set("max_distance", float(p_params["max_distance"]));
		}
		if (p_params.has("attenuation_model")) {
			player->set("attenuation_model", int(p_params["attenuation_model"]));
		}
		if (p_params.has("unit_size")) {
			player->set("unit_size", float(p_params["unit_size"]));
		}
	}

	parent->add_child(player);
	player->set_owner(root);

	Dictionary res;
	res["name"] = player_name;
	res["type"] = player_type;
	res["parent"] = node_path;
	res["stream"] = stream_path;
	res["bus"] = player->get("bus");
	res["volume_db"] = player->get("volume_db");
	res["autoplay"] = player->get("autoplay");
	return MCP_SUCCESS(res);
}

Dictionary JustAMCPAudioTools::_get_audio_info(const Dictionary &p_params) {
	if (!p_params.has("node_path")) {
		return MCP_INVALID_PARAMS("Missing param: node_path");
	}
	String node_path = p_params["node_path"];

	Node *node = _find_node_by_path(node_path);
	if (!node) {
		return MCP_NOT_FOUND("Node at '" + node_path + "'");
	}

	Array players;
	_collect_audio_players(node, players);

	Dictionary res;
	res["node_path"] = node_path;
	res["audio_player_count"] = players.size();
	res["players"] = players;
	return MCP_SUCCESS(res);
}

void JustAMCPAudioTools::_collect_audio_players(Node *p_node, Array &r_result) {
	if (p_node->is_class("AudioStreamPlayer") || p_node->is_class("AudioStreamPlayer2D") || p_node->is_class("AudioStreamPlayer3D")) {
		Dictionary info;
		info["name"] = p_node->get_name();
		Node *root = JustAMCPEditorSceneAccess::get_edited_root();
		if (root) {
			info["path"] = JustAMCPEditorSceneAccess::safe_path_to(root, p_node);
		} else {
			info["path"] = String(p_node->get_name());
		}
		info["type"] = p_node->get_class();
		info["volume_db"] = p_node->get("volume_db");
		info["bus"] = p_node->get("bus");
		info["autoplay"] = p_node->get("autoplay");
		info["playing"] = p_node->get("playing");
		info["stream"] = "";

		Variant stream_var = p_node->get("stream");
		if (stream_var.get_type() == Variant::OBJECT) {
			Ref<Resource> stream = stream_var;
			if (stream.is_valid()) {
				info["stream"] = stream->get_path();
			}
		}

		if (p_node->is_class("AudioStreamPlayer2D")) {
			info["max_distance"] = p_node->get("max_distance");
			info["attenuation"] = p_node->get("attenuation");
		} else if (p_node->is_class("AudioStreamPlayer3D")) {
			info["max_distance"] = p_node->get("max_distance");
			info["attenuation_model"] = p_node->get("attenuation_model");
			info["unit_size"] = p_node->get("unit_size");
		}

		r_result.push_back(info);
	}

	for (int i = 0; i < p_node->get_child_count(); i++) {
		_collect_audio_players(p_node->get_child(i), r_result);
	}
}
