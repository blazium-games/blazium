/**************************************************************************/
/*  anticheat_api_loader.h                                                */
/**************************************************************************/
/*                         This file is part of:                          */
/*                             BLAZIUM ENGINE                             */
/*                          https://blazium.app                           */
/**************************************************************************/

#pragma once

#include "core/string/ustring.h"
#include "core/templates/vector.h"

class AnticheatAPILoader {
public:
	typedef int (*BZCL_InitFn)();
	typedef void (*BZCL_ShutdownFn)();
	typedef int (*BZCL_IsEnabledFn)();
	typedef void (*BZCL_TickFn)(int tick_limit_ms);
	typedef int (*BZCL_OnPacketFn)(const void *data, int len);
	typedef int (*BZCL_OnCommandFn)(const char *utf8);
	typedef void (*BZCL_ScreenshotFn)(const unsigned char *rgba, int w, int h, int bpp);
	typedef void (*BZCL_SetScreenshotReceiverFn)(BZCL_ScreenshotFn fn);
	typedef int (*BZCL_SendPacketFn)(const void *data, int len);
	typedef int (*BZCL_FileHashFn)(const char *path, unsigned char out32[32]);
	typedef void (*BZCL_SetSendPacketFn)(BZCL_SendPacketFn fn);
	typedef void (*BZCL_SetFileHashFn)(BZCL_FileHashFn fn);

	typedef int (*BZSV_InitFn)();
	typedef void (*BZSV_ShutdownFn)();
	typedef void (*BZSV_TickFn)(int tick_limit_ms);
	typedef int (*BZSV_OnPacketFn)(int client_index, const void *data, int len);
	typedef int (*BZSV_DropClientFn)(int client_index, const char *reason_utf8);
	typedef int (*BZSV_SendPacketFn)(int client_index, const void *data, int len);
	typedef void (*BZSV_SetSendPacketFn)(BZSV_SendPacketFn fn);
	typedef void (*BZSV_NotifyDropFn)(int client_index, const char *reason_utf8);
	typedef void (*BZSV_SetNotifyDropFn)(BZSV_NotifyDropFn fn);
	typedef int (*BZSV_ClientJoinFn)(int client_index);

	typedef int (*BZGB_ConnectFn)(const char *address, unsigned short port, const char *source_id);
	typedef int (*BZGB_ConnectUrlFn)(const char *url, unsigned short port, const char *source_id);
	typedef void (*BZGB_CloseFn)();
	typedef void (*BZGB_TickFn)();
	typedef int (*BZGB_ConnectedFn)();
	typedef int (*BZGB_SendFn)(const void *data, int len);
	typedef void (*BZGB_ActionFn)(const char *json_line, int len);
	typedef void (*BZGB_SetActionFn)(BZGB_ActionFn fn);
	typedef void (*BZGB_SetTimeoutFn)(int timeout_ms);

	bool try_load();
	bool try_load_server();
	void unload();
	bool is_loaded() const { return client_loaded; }
	bool is_server_loaded() const { return server_loaded; }

	int cl_init();
	void cl_shutdown();
	int cl_is_enabled() const;
	void cl_tick(int tick_limit_ms);
	int cl_on_packet(const void *data, int len);
	int cl_on_command(const char *utf8);
	void cl_set_screenshot_receiver(BZCL_ScreenshotFn fn);
	void cl_set_send_packet(BZCL_SendPacketFn fn);
	void cl_set_file_hash(BZCL_FileHashFn fn);

	int sv_init();
	void sv_shutdown();
	void sv_tick(int tick_limit_ms);
	int sv_on_packet(int client_index, const void *data, int len);
	int sv_drop_client(int client_index, const char *reason_utf8);
	void sv_set_send_packet(BZSV_SendPacketFn fn);
	void sv_set_notify_drop(BZSV_NotifyDropFn fn);
	int sv_client_join(int client_index);

	int gb_connect(const char *address, unsigned short port, const char *source_id);
	int gb_connect_url(const char *url, unsigned short port, const char *source_id);
	void gb_close();
	void gb_tick();
	int gb_connected() const;
	int gb_send(const void *data, int len);
	void gb_set_action_receiver(BZGB_ActionFn fn);
	void gb_set_timeout_ms(int timeout_ms);

	static bool verify_runtime_file(const String &p_path, bool p_require_sig);

private:
	void *client_handle = nullptr;
	void *server_handle = nullptr;
	void *gb_handle = nullptr;
	bool client_loaded = false;
	bool server_loaded = false;

	BZCL_InitFn fn_cl_init = nullptr;
	BZCL_ShutdownFn fn_cl_shutdown = nullptr;
	BZCL_IsEnabledFn fn_cl_is_enabled = nullptr;
	BZCL_TickFn fn_cl_tick = nullptr;
	BZCL_OnPacketFn fn_cl_on_packet = nullptr;
	BZCL_OnCommandFn fn_cl_on_command = nullptr;
	BZCL_SetScreenshotReceiverFn fn_cl_set_screenshot = nullptr;
	BZCL_SetSendPacketFn fn_cl_set_send = nullptr;
	BZCL_SetFileHashFn fn_cl_set_hash = nullptr;

	BZSV_InitFn fn_sv_init = nullptr;
	BZSV_ShutdownFn fn_sv_shutdown = nullptr;
	BZSV_TickFn fn_sv_tick = nullptr;
	BZSV_OnPacketFn fn_sv_on_packet = nullptr;
	BZSV_DropClientFn fn_sv_drop_client = nullptr;
	BZSV_SetSendPacketFn fn_sv_set_send = nullptr;
	BZSV_SetNotifyDropFn fn_sv_notify_drop = nullptr;
	BZSV_ClientJoinFn fn_sv_client_join = nullptr;

	BZGB_ConnectFn fn_gb_connect = nullptr;
	BZGB_ConnectUrlFn fn_gb_connect_url = nullptr;
	BZGB_CloseFn fn_gb_close = nullptr;
	BZGB_TickFn fn_gb_tick = nullptr;
	BZGB_ConnectedFn fn_gb_connected = nullptr;
	BZGB_SendFn fn_gb_send = nullptr;
	BZGB_SetActionFn fn_gb_set_action = nullptr;
	BZGB_SetTimeoutFn fn_gb_set_timeout = nullptr;

	bool _load_symbol(void *p_handle, const char *p_name, void *&r_symbol);
	void *_open_library(const Vector<String> &p_names, bool p_require_sig);
	void _hot_rename(const String &p_live_name, const String &p_new_name);
	void _clear_client();
	void _clear_server();
};
