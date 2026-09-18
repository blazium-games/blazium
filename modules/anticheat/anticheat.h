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
#include "core/templates/hash_map.h"

class Anticheat : public Object {
	GDCLASS(Anticheat, Object);

private:
	static Anticheat *singleton;
	AnticheatAPILoader loader;
	bool dll_available = false;
	bool server_available = false;
	bool initialized = false;
	bool server_initialized = false;
	bool ops_connected = false;
	HashMap<String, int> player_to_index;
	HashMap<int, String> index_to_player;
	String screenshot_player_id;

	void _register_screenshot_receiver();
	void _register_runtime_callbacks();
	void _capture_viewport_png();
	void _send_screenshot_data(const PackedByteArray &p_png, int p_width, int p_height);
	void _apply_mapped_kick(const String &p_player_id, const String &p_reason);
	static void _screenshot_receiver(const unsigned char *rgba, int w, int h, int bpp);
	static int _send_packet(const void *data, int len);
	static int _file_hash(const char *path, unsigned char out32[32]);
	static int _sv_send_packet(int client_index, const void *data, int len);
	static void _sv_notify_drop(int client_index, const char *reason_utf8);
	static void _ops_action(const char *json_line, int len);

protected:
	static void _bind_methods();

public:
	static Anticheat *get_singleton();

	bool is_available();
	bool get_server_available();
	int initialize();
	int sv_initialize();
	void shutdown();
	bool is_initialized() const;
	bool is_server_initialized() const;
	void tick(int tick_limit_ms = 0);
	bool is_enabled() const;
	int submit_packet(const PackedByteArray &p_data);
	int submit_command(const String &p_command);
	int ops_connect();
	bool is_ops_connected() const;
	int sv_on_packet(int p_client_index, const PackedByteArray &p_data);
	int sv_client_join(int p_client_index);
	int sv_drop_client(int p_client_index, const String &p_reason);
	void bind_player(int p_client_index, const String &p_player_id);
	void unbind_player(int p_client_index);
	int player_client_index(const String &p_player_id) const;
	void apply_ops_line(const String &p_json_line);
	int gb_send(const PackedByteArray &p_data);

	Anticheat();
	~Anticheat();
};
