/**************************************************************************/
/*  anticheat.h                                                           */
/**************************************************************************/
/*                         This file is part of:                          */
/*                             BLAZIUM ENGINE                             */
/*                          https://blazium.app                           */
/**************************************************************************/

#pragma once

#include "anticheat_api_loader.h"
#include "anticheat_types.h"
#include "core/object/object.h"
#include "core/variant/typed_array.h"

class Anticheat : public Object {
	GDCLASS(Anticheat, Object);

private:
	static Anticheat *singleton;
	AnticheatAPILoader loader;
	bool dll_available = false;
	bool server_available = false;
	bool initialized = false;
	bool ops_connected = false;

	void _register_screenshot_receiver();
	static void _screenshot_receiver(const unsigned char *rgba, int w, int h, int bpp);

protected:
	static void _bind_methods();

public:
	static Anticheat *get_singleton();

	bool is_available();
	bool get_server_available();
	int initialize();
	void shutdown();
	bool is_initialized() const;
	void tick(int tick_limit_ms = 0);
	bool is_enabled() const;
	int submit_packet(const PackedByteArray &p_data);
	int submit_command(const String &p_command);
	int ops_connect();
	bool is_ops_connected() const;

	Anticheat();
	~Anticheat();
};
