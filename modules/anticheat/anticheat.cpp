/**************************************************************************/
/*  anticheat.cpp                                                         */
/**************************************************************************/
/*                         This file is part of:                          */
/*                             BLAZIUM ENGINE                             */
/*                          https://blazium.app                           */
/**************************************************************************/

#include "anticheat.h"

#include "register_types.h"

#include "core/config/engine.h"
#include "core/config/project_settings.h"
#include "core/crypto/crypto_core.h"
#include "core/io/file_access.h"
#include "core/io/image.h"
#include "core/io/json.h"
#include "core/object/class_db.h"
#include "core/os/os.h"
#include "scene/main/scene_tree.h"
#include "scene/main/viewport.h"
#include "scene/main/window.h"

#include <cstdlib>
#include <cstring>

Anticheat *Anticheat::singleton = nullptr;

void Anticheat::_bind_methods() {
	ClassDB::bind_method(D_METHOD("is_available"), &Anticheat::is_available);
	ClassDB::bind_method(D_METHOD("get_server_available"), &Anticheat::get_server_available);
	ClassDB::bind_method(D_METHOD("initialize"), &Anticheat::initialize);
	ClassDB::bind_method(D_METHOD("sv_initialize"), &Anticheat::sv_initialize);
	ClassDB::bind_method(D_METHOD("shutdown"), &Anticheat::shutdown);
	ClassDB::bind_method(D_METHOD("is_initialized"), &Anticheat::is_initialized);
	ClassDB::bind_method(D_METHOD("is_server_initialized"), &Anticheat::is_server_initialized);
	ClassDB::bind_method(D_METHOD("tick", "tick_limit_ms"), &Anticheat::tick, DEFVAL(0));
	ClassDB::bind_method(D_METHOD("is_enabled"), &Anticheat::is_enabled);
	ClassDB::bind_method(D_METHOD("submit_packet", "data"), &Anticheat::submit_packet);
	ClassDB::bind_method(D_METHOD("submit_command", "command"), &Anticheat::submit_command);
	ClassDB::bind_method(D_METHOD("ops_connect"), &Anticheat::ops_connect);
	ClassDB::bind_method(D_METHOD("is_ops_connected"), &Anticheat::is_ops_connected);
	ClassDB::bind_method(D_METHOD("sv_on_packet", "client_index", "data"), &Anticheat::sv_on_packet);
	ClassDB::bind_method(D_METHOD("sv_client_join", "client_index"), &Anticheat::sv_client_join);
	ClassDB::bind_method(D_METHOD("sv_drop_client", "client_index", "reason"), &Anticheat::sv_drop_client, DEFVAL(String()));
	ClassDB::bind_method(D_METHOD("bind_player", "client_index", "player_id"), &Anticheat::bind_player);
	ClassDB::bind_method(D_METHOD("unbind_player", "client_index"), &Anticheat::unbind_player);
	ClassDB::bind_method(D_METHOD("player_client_index", "player_id"), &Anticheat::player_client_index);
	ClassDB::bind_method(D_METHOD("apply_ops_line", "json_line"), &Anticheat::apply_ops_line);
	ClassDB::bind_method(D_METHOD("gb_send", "data"), &Anticheat::gb_send);

	ADD_SIGNAL(MethodInfo("outgoing_packet", PropertyInfo(Variant::PACKED_BYTE_ARRAY, "blob")));
	ADD_SIGNAL(MethodInfo("outgoing_server_packet", PropertyInfo(Variant::INT, "client_index"), PropertyInfo(Variant::PACKED_BYTE_ARRAY, "blob")));
	ADD_SIGNAL(MethodInfo("screenshot_ready", PropertyInfo(Variant::PACKED_BYTE_ARRAY, "png"), PropertyInfo(Variant::INT, "width"), PropertyInfo(Variant::INT, "height")));
	ADD_SIGNAL(MethodInfo("server_drop_client", PropertyInfo(Variant::INT, "client_index"), PropertyInfo(Variant::STRING, "reason")));
	ADD_SIGNAL(MethodInfo("ops_action", PropertyInfo(Variant::STRING, "json_line")));
	ADD_SIGNAL(MethodInfo("ops_teleport", PropertyInfo(Variant::STRING, "player_id"), PropertyInfo(Variant::STRING, "region"), PropertyInfo(Variant::FLOAT, "x"), PropertyInfo(Variant::FLOAT, "y")));
	ADD_SIGNAL(MethodInfo("ops_message", PropertyInfo(Variant::STRING, "player_id"), PropertyInfo(Variant::STRING, "text")));
	ADD_SIGNAL(MethodInfo("ops_global_message", PropertyInfo(Variant::STRING, "text")));
	ADD_SIGNAL(MethodInfo("ops_kill", PropertyInfo(Variant::STRING, "player_id")));
	ADD_SIGNAL(MethodInfo("ops_screenshot_request", PropertyInfo(Variant::STRING, "player_id"), PropertyInfo(Variant::STRING, "side")));
}

Anticheat *Anticheat::get_singleton() {
	return singleton;
}

Anticheat::Anticheat() {
	singleton = this;
}

Anticheat::~Anticheat() {
	if (initialized || server_initialized || ops_connected) {
		shutdown();
	}
	if (singleton == this) {
		singleton = nullptr;
	}
}

void Anticheat::_screenshot_receiver(const unsigned char *rgba, int w, int h, int bpp) {
	if (!singleton || !rgba || w <= 0 || h <= 0) {
		return;
	}
	if (bpp != 0 && bpp != 32) {
		return;
	}
	Ref<Image> image;
	image.instantiate();
	Vector<uint8_t> bytes;
	bytes.resize(w * h * 4);
	memcpy(bytes.ptrw(), rgba, bytes.size());
	image->set_data(w, h, false, Image::FORMAT_RGBA8, bytes);
	const Vector<uint8_t> png = image->save_png_to_buffer();
	PackedByteArray out;
	out.resize(png.size());
	if (png.size()) {
		memcpy(out.ptrw(), png.ptr(), png.size());
	}
	singleton->emit_signal("screenshot_ready", out, w, h);
	singleton->_send_screenshot_data(out, w, h);
}

void Anticheat::_register_screenshot_receiver() {
	loader.cl_set_screenshot_receiver(&Anticheat::_screenshot_receiver);
}

int Anticheat::_send_packet(const void *data, int len) {
	if (!singleton || !data || len <= 0) {
		return 1;
	}
	PackedByteArray blob;
	blob.resize(len);
	memcpy(blob.ptrw(), data, static_cast<size_t>(len));
	singleton->emit_signal("outgoing_packet", blob);
	return 0;
}

int Anticheat::_file_hash(const char *path, unsigned char out32[32]) {
	if (!path || !out32) {
		return 1;
	}
	String p = String::utf8(path);
	if (!p.begins_with("res://") && !p.begins_with("user://") && !p.is_absolute_path()) {
		p = String("res://") + p;
	}
	const String sha = FileAccess::get_sha256(p).to_lower();
	if (sha.length() != 64) {
		memset(out32, 0, 32);
		return 1;
	}
	for (int i = 0; i < 32; i++) {
		CharString pair = sha.substr(i * 2, 2).utf8();
		out32[i] = (unsigned char)strtol(pair.get_data(), nullptr, 16);
	}
	return 0;
}

void Anticheat::_register_runtime_callbacks() {
	loader.cl_set_send_packet(&Anticheat::_send_packet);
	loader.cl_set_file_hash(&Anticheat::_file_hash);
	_register_screenshot_receiver();
}

bool Anticheat::is_available() {
	if (dll_available) {
		return true;
	}
	dll_available = loader.try_load();
	return dll_available;
}

bool Anticheat::get_server_available() {
	if (server_available) {
		return true;
	}
	const bool dedicated = OS::get_singleton() && OS::get_singleton()->has_feature("dedicated_server");
	if (!dedicated) {
		return false;
	}
	server_available = loader.try_load_server();
	return server_available;
}

int Anticheat::initialize() {
	if (initialized) {
		return ANTICHEAT_OK;
	}
	if (!is_available()) {
		bool verify = false;
		bool require_agent = false;
		if (ProjectSettings::get_singleton()) {
			verify = ProjectSettings::get_singleton()->get("anticheat/verify_runtime_signature");
			require_agent = ProjectSettings::get_singleton()->get("anticheat/require_agent");
		}
		if (verify || require_agent) {
			return ANTICHEAT_ERR_SIGNATURE;
		}
		return ANTICHEAT_ERR_UNAVAILABLE;
	}
	const int err = loader.cl_init();
	if (err != 0) {
		return ANTICHEAT_ERR_INIT;
	}
	_register_runtime_callbacks();
	initialized = true;
	anticheat_ensure_frame_hook();
	return ANTICHEAT_OK;
}

int Anticheat::sv_initialize() {
	if (server_initialized) {
		return ANTICHEAT_OK;
	}
	if (!get_server_available()) {
		return ANTICHEAT_ERR_UNAVAILABLE;
	}
	if (loader.sv_init() != 0) {
		return ANTICHEAT_ERR_INIT;
	}
	loader.sv_set_send_packet(&Anticheat::_sv_send_packet);
	loader.sv_set_notify_drop(&Anticheat::_sv_notify_drop);
	server_initialized = true;
	anticheat_ensure_frame_hook();
	return ANTICHEAT_OK;
}

void Anticheat::shutdown() {
	if (ops_connected) {
		loader.gb_close();
		ops_connected = false;
	}
	if (initialized) {
		loader.cl_shutdown();
		initialized = false;
	}
	if (server_initialized) {
		loader.sv_shutdown();
		server_initialized = false;
	}
	player_to_index.clear();
	index_to_player.clear();
	screenshot_player_id = String();
}

bool Anticheat::is_initialized() const {
	return initialized;
}

bool Anticheat::is_server_initialized() const {
	return server_initialized;
}

void Anticheat::tick(int tick_limit_ms) {
	if (initialized) {
		loader.cl_tick(tick_limit_ms);
	}
	if (server_initialized) {
		loader.sv_tick(tick_limit_ms);
	}
	if (ops_connected) {
		loader.gb_tick();
	}
}

bool Anticheat::is_enabled() const {
	return initialized && loader.cl_is_enabled() != 0;
}

int Anticheat::submit_packet(const PackedByteArray &p_data) {
	if (!initialized || p_data.is_empty()) {
		return ANTICHEAT_ERR_INIT;
	}
	return loader.cl_on_packet(p_data.ptr(), p_data.size());
}

int Anticheat::submit_command(const String &p_command) {
	if (!initialized) {
		return ANTICHEAT_ERR_INIT;
	}
	const CharString utf8 = p_command.utf8();
	const int err = loader.cl_on_command(utf8.get_data());
	if (p_command.begins_with("screenshot")) {
		_capture_viewport_png();
	}
	return err;
}

void Anticheat::_send_screenshot_data(const PackedByteArray &p_png, int p_width, int p_height) {
	if (!ops_connected) {
		return;
	}
	Dictionary pairs;
	pairs["width"] = p_width;
	pairs["height"] = p_height;
	if (!p_png.is_empty()) {
		pairs["png_base64"] = CryptoCore::b64_encode_str(p_png.ptr(), p_png.size());
	}
	Dictionary ev;
	ev["event"] = "ScreenshotData";
	if (!screenshot_player_id.is_empty()) {
		ev["player_id"] = screenshot_player_id;
	}
	ev["pairs"] = pairs;
	const String line = JSON::stringify(ev) + "\n";
	const CharString utf8 = line.utf8();
	loader.gb_send(utf8.get_data(), utf8.length());
}

void Anticheat::_capture_viewport_png() {
	if (!Engine::get_singleton() || !OS::get_singleton()) {
		return;
	}
	SceneTree *tree = Object::cast_to<SceneTree>(OS::get_singleton()->get_main_loop());
	if (!tree || !tree->get_root()) {
		return;
	}
	Ref<ViewportTexture> tex = tree->get_root()->get_texture();
	if (tex.is_null()) {
		return;
	}
	Ref<Image> img = tex->get_image();
	if (img.is_null() || img->is_empty()) {
		return;
	}
	if (img->get_format() != Image::FORMAT_RGBA8) {
		img->convert(Image::FORMAT_RGBA8);
	}
	loader.cl_provide_screenshot(img->ptr(), img->get_width(), img->get_height(), 32);
	const Vector<uint8_t> png = img->save_png_to_buffer();
	PackedByteArray out;
	out.resize(png.size());
	if (png.size()) {
		memcpy(out.ptrw(), png.ptr(), png.size());
	}
	emit_signal("screenshot_ready", out, img->get_width(), img->get_height());
	_send_screenshot_data(out, img->get_width(), img->get_height());
}

int Anticheat::ops_connect() {
	if (!get_server_available()) {
		return ANTICHEAT_ERR_UNAVAILABLE;
	}
	const String mode = ProjectSettings::get_singleton() ? String(ProjectSettings::get_singleton()->get("anticheat/ops/mode")) : String();
	const bool dev = OS::get_singleton() && OS::get_singleton()->get_environment("BLAZIUM_AC_DEV") == "1";
	if (!dev && mode != "saas" && ProjectSettings::get_singleton()) {
		String lic = ProjectSettings::get_singleton()->get("anticheat/ops/license_path");
		if (!lic.is_empty()) {
			if (lic.begins_with("res://") || lic.begins_with("user://")) {
				lic = ProjectSettings::get_singleton()->globalize_path(lic);
			}
			if (!FileAccess::exists(lic)) {
				return ANTICHEAT_ERR_CONNECT;
			}
		}
	}
	const int sv_err = sv_initialize();
	if (sv_err != ANTICHEAT_OK) {
		return sv_err;
	}
	loader.gb_set_action_receiver(&Anticheat::_ops_action);
	const int timeout_s = ProjectSettings::get_singleton()->get("anticheat/ops/timeout_s");
	loader.gb_set_timeout_ms(timeout_s > 0 ? timeout_s * 1000 : 30000);

	const String address = ProjectSettings::get_singleton()->get("anticheat/ops/address");
	const int port = ProjectSettings::get_singleton()->get("anticheat/ops/port");
	String source_id = ProjectSettings::get_singleton()->get("anticheat/ops/source_id");
	if (source_id.is_empty()) {
		source_id = "dedicated";
	}

	int err = 1;
	if (mode == "saas") {
		String key;
		if (OS::get_singleton()) {
			key = OS::get_singleton()->get_environment("BLAZIUM_AC_API_KEY");
		}
		if (key.is_empty()) {
			return ANTICHEAT_ERR_CONNECT;
		}
		String endpoint = ProjectSettings::get_singleton()->get("anticheat/ops/saas_endpoint");
		if (endpoint.is_empty()) {
			endpoint = address;
		}
		const CharString url_utf8 = endpoint.utf8();
		const CharString src_utf8 = source_id.utf8();
		err = loader.gb_connect_url(url_utf8.get_data(), (unsigned short)port, src_utf8.get_data());
	} else {
		const CharString addr_utf8 = address.utf8();
		const CharString src_utf8 = source_id.utf8();
		err = loader.gb_connect(addr_utf8.get_data(), (unsigned short)port, src_utf8.get_data());
	}
	if (err != 0) {
		return ANTICHEAT_ERR_CONNECT;
	}
	ops_connected = loader.gb_connected() != 0;
	return ops_connected ? ANTICHEAT_OK : ANTICHEAT_ERR_CONNECT;
}

bool Anticheat::is_ops_connected() const {
	return ops_connected && loader.gb_connected() != 0;
}

int Anticheat::_sv_send_packet(int client_index, const void *data, int len) {
	if (!singleton || !data || len <= 0) {
		return 1;
	}
	PackedByteArray blob;
	blob.resize(len);
	memcpy(blob.ptrw(), data, static_cast<size_t>(len));
	singleton->emit_signal("outgoing_server_packet", client_index, blob);
	return 0;
}

void Anticheat::_sv_notify_drop(int client_index, const char *reason_utf8) {
	if (!singleton) {
		return;
	}
	singleton->unbind_player(client_index);
	singleton->emit_signal("server_drop_client", client_index, String::utf8(reason_utf8 ? reason_utf8 : ""));
}

void Anticheat::_ops_action(const char *json_line, int len) {
	if (!singleton || !json_line || len <= 0) {
		return;
	}
	singleton->apply_ops_line(String::utf8(json_line, len));
}

void Anticheat::_apply_mapped_kick(const String &p_player_id, const String &p_reason) {
	const int idx = player_client_index(p_player_id);
	if (idx < 0) {
		return;
	}
	sv_drop_client(idx, p_reason);
}

int Anticheat::sv_on_packet(int p_client_index, const PackedByteArray &p_data) {
	if (!server_initialized || p_data.is_empty()) {
		return ANTICHEAT_ERR_INIT;
	}
	return loader.sv_on_packet(p_client_index, p_data.ptr(), p_data.size());
}

int Anticheat::sv_client_join(int p_client_index) {
	if (!server_initialized) {
		return ANTICHEAT_ERR_UNAVAILABLE;
	}
	return loader.sv_client_join(p_client_index);
}

int Anticheat::sv_drop_client(int p_client_index, const String &p_reason) {
	if (server_initialized) {
		const CharString utf8 = p_reason.utf8();
		loader.sv_drop_client(p_client_index, utf8.get_data());
		return ANTICHEAT_OK;
	}
	emit_signal("server_drop_client", p_client_index, p_reason);
	unbind_player(p_client_index);
	return ANTICHEAT_OK;
}

void Anticheat::bind_player(int p_client_index, const String &p_player_id) {
	if (p_player_id.is_empty()) {
		unbind_player(p_client_index);
		return;
	}
	if (index_to_player.has(p_client_index)) {
		player_to_index.erase(index_to_player[p_client_index]);
	}
	if (player_to_index.has(p_player_id)) {
		index_to_player.erase(player_to_index[p_player_id]);
	}
	player_to_index[p_player_id] = p_client_index;
	index_to_player[p_client_index] = p_player_id;
}

void Anticheat::unbind_player(int p_client_index) {
	if (!index_to_player.has(p_client_index)) {
		return;
	}
	player_to_index.erase(index_to_player[p_client_index]);
	index_to_player.erase(p_client_index);
}

int Anticheat::player_client_index(const String &p_player_id) const {
	if (!player_to_index.has(p_player_id)) {
		return -1;
	}
	return player_to_index[p_player_id];
}

void Anticheat::apply_ops_line(const String &p_json_line) {
	emit_signal("ops_action", p_json_line);
	const Variant parsed = JSON::parse_string(p_json_line.strip_edges());
	if (parsed.get_type() != Variant::DICTIONARY) {
		return;
	}
	const Dictionary ev = parsed;
	const String event = ev.get("event", "");
	const String player_id = ev.get("player_id", "");
	Dictionary pairs;
	if (ev.get("pairs", Variant()).get_type() == Variant::DICTIONARY) {
		pairs = ev.get("pairs", Dictionary());
	}
	const String reason = pairs.get("reason", "ops");
	if (event == "Kick" || event == "KickMsg") {
		_apply_mapped_kick(player_id, reason);
		return;
	}
	if (event == "Message") {
		emit_signal("ops_message", player_id, reason);
		return;
	}
	if (event == "GlobalMessage") {
		emit_signal("ops_global_message", reason);
		return;
	}
	if (event == "Kill") {
		emit_signal("ops_kill", player_id);
		return;
	}
	if (event == "Screenshot" || event == "ScreenshotFront" || event == "ScreenshotBack") {
		String side = "front";
		if (event == "ScreenshotBack") {
			side = "back";
		}
		if (server_initialized) {
			emit_signal("ops_screenshot_request", player_id, side);
			return;
		}
		screenshot_player_id = player_id;
		_capture_viewport_png();
		screenshot_player_id = String();
		return;
	}
	if (event == "Teleport") {
		emit_signal("ops_teleport", player_id, String(pairs.get("region", "")), pairs.get("x", 0.0), pairs.get("y", 0.0));
	}
}

int Anticheat::gb_send(const PackedByteArray &p_data) {
	if (!ops_connected || p_data.is_empty()) {
		return ANTICHEAT_ERR_CONNECT;
	}
	return loader.gb_send(p_data.ptr(), p_data.size());
}
