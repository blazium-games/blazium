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
#include "core/io/image.h"
#include "core/object/class_db.h"
#include "core/os/os.h"

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
	if (!rgba || w <= 0 || h <= 0) {
		return;
	}
	(void)bpp;
	Ref<Image> image;
	image.instantiate();
	Vector<uint8_t> bytes;
	bytes.resize(w * h * 4);
	memcpy(bytes.ptrw(), rgba, bytes.size());
	image->set_data(w, h, false, Image::FORMAT_RGBA8, bytes);
	(void)image;
}

void Anticheat::_register_screenshot_receiver() {
	loader.cl_set_screenshot_receiver(&Anticheat::_screenshot_receiver);
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
		return ANTICHEAT_ERR_UNAVAILABLE;
	}
	const int err = loader.cl_init();
	if (err != 0) {
		return ANTICHEAT_ERR_INIT;
	}
	_register_screenshot_receiver();
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
	return loader.cl_on_command(utf8.get_data());
}

int Anticheat::ops_connect() {
	if (!get_server_available()) {
		return ANTICHEAT_ERR_UNAVAILABLE;
	}
	if (loader.sv_init() != 0) {
		return ANTICHEAT_ERR_INIT;
	}

	const String mode = ProjectSettings::get_singleton()->get("anticheat/ops/mode");
	const String address = ProjectSettings::get_singleton()->get("anticheat/ops/address");
	const int port = ProjectSettings::get_singleton()->get("anticheat/ops/port");
	String source_id = ProjectSettings::get_singleton()->get("anticheat/ops/source_id");
	if (source_id.is_empty()) {
		source_id = "dedicated";
	}

	int err = 1;
	if (mode == "saas") {
		String endpoint = ProjectSettings::get_singleton()->get("anticheat/ops/saas_endpoint");
		const String api_key = OS::get_singleton()->get_environment("BLAZIUM_AC_API_KEY");
		(void)api_key;
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
