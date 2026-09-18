/**************************************************************************/
/*  anticheat.cpp                                                         */
/**************************************************************************/
/*                         This file is part of:                          */
/*                             BLAZIUM ENGINE                             */
/*                          https://blazium.app                           */
/**************************************************************************/

#include "anticheat.h"

#include "core/config/engine.h"
#include "core/config/project_settings.h"
#include "core/io/file_access.h"
#include "core/io/image.h"
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
	ClassDB::bind_method(D_METHOD("shutdown"), &Anticheat::shutdown);
	ClassDB::bind_method(D_METHOD("is_initialized"), &Anticheat::is_initialized);
	ClassDB::bind_method(D_METHOD("tick", "tick_limit_ms"), &Anticheat::tick, DEFVAL(0));
	ClassDB::bind_method(D_METHOD("is_enabled"), &Anticheat::is_enabled);
	ClassDB::bind_method(D_METHOD("submit_packet", "data"), &Anticheat::submit_packet);
	ClassDB::bind_method(D_METHOD("submit_command", "command"), &Anticheat::submit_command);
	ClassDB::bind_method(D_METHOD("ops_connect"), &Anticheat::ops_connect);
	ClassDB::bind_method(D_METHOD("is_ops_connected"), &Anticheat::is_ops_connected);
	ClassDB::bind_method(D_METHOD("sv_on_packet", "client_index", "data"), &Anticheat::sv_on_packet);
	ClassDB::bind_method(D_METHOD("sv_client_join", "client_index"), &Anticheat::sv_client_join);
	ClassDB::bind_method(D_METHOD("gb_send", "data"), &Anticheat::gb_send);

	ADD_SIGNAL(MethodInfo("outgoing_packet", PropertyInfo(Variant::PACKED_BYTE_ARRAY, "blob")));
	ADD_SIGNAL(MethodInfo("outgoing_server_packet", PropertyInfo(Variant::INT, "client_index"), PropertyInfo(Variant::PACKED_BYTE_ARRAY, "blob")));
	ADD_SIGNAL(MethodInfo("screenshot_ready", PropertyInfo(Variant::PACKED_BYTE_ARRAY, "png"), PropertyInfo(Variant::INT, "width"), PropertyInfo(Variant::INT, "height")));
	ADD_SIGNAL(MethodInfo("server_drop_client", PropertyInfo(Variant::INT, "client_index"), PropertyInfo(Variant::STRING, "reason")));
	ADD_SIGNAL(MethodInfo("ops_action", PropertyInfo(Variant::STRING, "json_line")));
}

Anticheat *Anticheat::get_singleton() {
	return singleton;
}

Anticheat::Anticheat() {
	singleton = this;
}

Anticheat::~Anticheat() {
	if (initialized) {
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
	(void)bpp;
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
	if (server_available) {
		loader.sv_shutdown();
	}
}

bool Anticheat::is_initialized() const {
	return initialized;
}

void Anticheat::tick(int tick_limit_ms) {
	if (!initialized) {
		return;
	}
	loader.cl_tick(tick_limit_ms);
	if (server_available) {
		loader.sv_tick(tick_limit_ms);
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
	if (ops_connected) {
		const String line = vformat("{\"event\":\"Screenshot\",\"pairs\":{\"width\":%d,\"height\":%d}}\n", img->get_width(), img->get_height());
		const CharString utf8 = line.utf8();
		loader.gb_send(utf8.get_data(), utf8.length());
	}
}

int Anticheat::ops_connect() {
	if (!get_server_available()) {
		return ANTICHEAT_ERR_UNAVAILABLE;
	}
	const bool dev = OS::get_singleton() && OS::get_singleton()->get_environment("BLAZIUM_AC_DEV") == "1";
	if (!dev && ProjectSettings::get_singleton()) {
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
	if (loader.sv_init() != 0) {
		return ANTICHEAT_ERR_INIT;
	}
	loader.sv_set_send_packet(&Anticheat::_sv_send_packet);
	loader.sv_set_notify_drop(&Anticheat::_sv_notify_drop);
	loader.gb_set_action_receiver(&Anticheat::_ops_action);
	const int timeout_s = ProjectSettings::get_singleton()->get("anticheat/ops/timeout_s");
	loader.gb_set_timeout_ms(timeout_s > 0 ? timeout_s * 1000 : 30000);

	const String mode = ProjectSettings::get_singleton()->get("anticheat/ops/mode");
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
	singleton->emit_signal("server_drop_client", client_index, String::utf8(reason_utf8 ? reason_utf8 : ""));
}

void Anticheat::_ops_action(const char *json_line, int len) {
	if (!singleton || !json_line || len <= 0) {
		return;
	}
	singleton->emit_signal("ops_action", String::utf8(json_line, len));
}

int Anticheat::sv_on_packet(int p_client_index, const PackedByteArray &p_data) {
	if (!server_available || p_data.is_empty()) {
		return ANTICHEAT_ERR_INIT;
	}
	return loader.sv_on_packet(p_client_index, p_data.ptr(), p_data.size());
}

int Anticheat::sv_client_join(int p_client_index) {
	if (!server_available) {
		return ANTICHEAT_ERR_UNAVAILABLE;
	}
	return loader.sv_client_join(p_client_index);
}

int Anticheat::gb_send(const PackedByteArray &p_data) {
	if (!ops_connected || p_data.is_empty()) {
		return ANTICHEAT_ERR_CONNECT;
	}
	return loader.gb_send(p_data.ptr(), p_data.size());
}
