/**************************************************************************/
/*  anticheat_api_loader.cpp                                              */
/**************************************************************************/
/*                         This file is part of:                          */
/*                             BLAZIUM ENGINE                             */
/*                          https://blazium.app                           */
/**************************************************************************/

#include "anticheat_api_loader.h"
#include "anticheat_ed25519.h"

#include "core/config/engine.h"
#include "core/config/project_settings.h"
#include "core/io/dir_access.h"
#include "core/io/file_access.h"
#include "core/os/os.h"

namespace {

bool should_verify_signature() {
	if (!ProjectSettings::get_singleton()) {
		return false;
	}
	const bool verify = ProjectSettings::get_singleton()->get("anticheat/verify_runtime_signature");
	const bool require_agent = ProjectSettings::get_singleton()->get("anticheat/require_agent");
	if (Engine::get_singleton() && Engine::get_singleton()->is_editor_hint()) {
		return false;
	}
	return verify || require_agent;
}

void apply_hot_rename(const String &p_dir, const String &p_live, const String &p_new_name) {
	const String live_path = p_dir.path_join(p_live);
	const String new_path = p_dir.path_join(p_new_name);
	if (!FileAccess::exists(new_path)) {
		return;
	}
	Ref<DirAccess> da = DirAccess::create(DirAccess::ACCESS_FILESYSTEM);
	if (da.is_null()) {
		return;
	}
	if (FileAccess::exists(live_path)) {
		const String old_path = live_path + "old";
		da->remove(old_path);
		da->rename(live_path, old_path);
	}
	da->rename(new_path, live_path);
}

} // namespace

bool AnticheatAPILoader::verify_runtime_file(const String &p_path, bool p_require_sig) {
	if (!FileAccess::exists(p_path)) {
		return false;
	}
	if (!p_require_sig) {
		return true;
	}
	const String sig_path = p_path + ".sig";
	if (!FileAccess::exists(sig_path)) {
		return false;
	}
	const String sha = FileAccess::get_sha256(p_path).to_lower();
	const String body = FileAccess::get_file_as_string(sig_path);
	if (sha.is_empty() || body.is_empty()) {
		return false;
	}
	String sha_json;
	String sig_b64;
	if (!anticheat_parse_runtime_sig(body, sha_json, sig_b64)) {
		return false;
	}
	if (sha_json.to_lower() != sha) {
		return false;
	}
	uint8_t sig[64];
	if (anticheat_b64_decode(sig_b64, sig, 64) != 64) {
		return false;
	}
	String pk_hex;
	if (OS::get_singleton()) {
		pk_hex = OS::get_singleton()->get_environment("BLAZIUM_AC_RUNTIME_PK");
	}
	if (pk_hex.is_empty() && ProjectSettings::get_singleton()) {
		pk_hex = ProjectSettings::get_singleton()->get("anticheat/runtime_public_key");
	}
	if (pk_hex.is_empty()) {
		return false;
	}
	uint8_t pk[32];
	if (anticheat_hex_decode(pk_hex, pk, 32) != 0) {
		return false;
	}
	const CharString msg = sha.utf8();
	return anticheat_ed25519_verify(pk, (const uint8_t *)msg.get_data(), (size_t)msg.length(), sig);
}

bool AnticheatAPILoader::_load_symbol(void *p_handle, const char *p_name, void *&r_symbol) {
	r_symbol = nullptr;
	if (!p_handle) {
		return false;
	}
	Error err = OS::get_singleton()->get_dynamic_library_symbol_handle(p_handle, p_name, r_symbol);
	return err == OK && r_symbol != nullptr;
}

void AnticheatAPILoader::_hot_rename(const String &p_live_name, const String &p_new_name) {
	const String exe_dir = OS::get_singleton()->get_executable_path().get_base_dir();
	apply_hot_rename(exe_dir, p_live_name, p_new_name);
	apply_hot_rename(OS::get_singleton()->get_executable_path().get_base_dir(), p_live_name, p_new_name);
}

void *AnticheatAPILoader::_open_library(const Vector<String> &p_names, bool p_require_sig) {
	const String exe_dir = OS::get_singleton()->get_executable_path().get_base_dir();
	for (int i = 0; i < p_names.size(); i++) {
		String path = exe_dir.path_join(p_names[i]);
		if (!FileAccess::exists(path)) {
			path = p_names[i];
		}
		if (!FileAccess::exists(path)) {
			continue;
		}
		if (!verify_runtime_file(path, p_require_sig)) {
			continue;
		}
		void *handle = nullptr;
		Error err = OS::get_singleton()->open_dynamic_library(path, handle);
		if (err == OK && handle) {
			return handle;
		}
	}
	return nullptr;
}

bool AnticheatAPILoader::try_load() {
	if (client_loaded) {
		return true;
	}

#ifdef WINDOWS_ENABLED
	_hot_rename("bzcl.dll", "bzclnew.dll");
	_hot_rename("bzcl64.dll", "bzcl64new.dll");
	Vector<String> names;
	names.push_back("bzcl.dll");
	names.push_back("bzcl64.dll");
#else
	_hot_rename("libbzcl.so", "libbzclnew.so");
	Vector<String> names;
	names.push_back("libbzcl.so");
#endif

	client_handle = _open_library(names, should_verify_signature());
	if (!client_handle) {
		return false;
	}

	void *symbol = nullptr;
	if (!_load_symbol(client_handle, "BZCL_Init", symbol)) {
		unload();
		return false;
	}
	fn_cl_init = (BZCL_InitFn)symbol;
	if (!_load_symbol(client_handle, "BZCL_Shutdown", symbol)) {
		unload();
		return false;
	}
	fn_cl_shutdown = (BZCL_ShutdownFn)symbol;
	if (!_load_symbol(client_handle, "BZCL_IsEnabled", symbol)) {
		unload();
		return false;
	}
	fn_cl_is_enabled = (BZCL_IsEnabledFn)symbol;
	if (!_load_symbol(client_handle, "BZCL_Tick", symbol)) {
		unload();
		return false;
	}
	fn_cl_tick = (BZCL_TickFn)symbol;
	if (!_load_symbol(client_handle, "BZCL_OnPacket", symbol)) {
		unload();
		return false;
	}
	fn_cl_on_packet = (BZCL_OnPacketFn)symbol;
	if (!_load_symbol(client_handle, "BZCL_OnCommand", symbol)) {
		unload();
		return false;
	}
	fn_cl_on_command = (BZCL_OnCommandFn)symbol;
	if (!_load_symbol(client_handle, "BZCL_SetScreenshotReceiver", symbol)) {
		unload();
		return false;
	}
	fn_cl_set_screenshot = (BZCL_SetScreenshotReceiverFn)symbol;
	if (!_load_symbol(client_handle, "BZCL_SetSendPacket", symbol)) {
		unload();
		return false;
	}
	fn_cl_set_send = (BZCL_SetSendPacketFn)symbol;
	if (!_load_symbol(client_handle, "BZCL_SetFileHash", symbol)) {
		unload();
		return false;
	}
	fn_cl_set_hash = (BZCL_SetFileHashFn)symbol;
	_load_symbol(client_handle, "BZCL_ProvideScreenshot", symbol);
	fn_cl_provide_screenshot = (BZCL_ProvideScreenshotFn)symbol;

	client_loaded = true;
	return true;
}

bool AnticheatAPILoader::try_load_server() {
	if (server_loaded) {
		return true;
	}

#ifdef WINDOWS_ENABLED
	_hot_rename("bzsv.dll", "bzsvnew.dll");
	_hot_rename("bzgb.dll", "bzgbnew.dll");
	Vector<String> sv_names;
	sv_names.push_back("bzsv.dll");
	sv_names.push_back("bzsv64.dll");
	Vector<String> gb_names;
	gb_names.push_back("bzgb.dll");
	gb_names.push_back("bzgb64.dll");
#else
	_hot_rename("libbzsv.so", "libbzsvnew.so");
	_hot_rename("libbzgb.so", "libbzgbnew.so");
	Vector<String> sv_names;
	sv_names.push_back("libbzsv.so");
	Vector<String> gb_names;
	gb_names.push_back("libbzgb.so");
#endif

	const bool require_sig = should_verify_signature();
	server_handle = _open_library(sv_names, require_sig);
	gb_handle = _open_library(gb_names, require_sig);
	if (!server_handle || !gb_handle) {
		_clear_server();
		return false;
	}

	void *symbol = nullptr;
	if (!_load_symbol(server_handle, "BZSV_Init", symbol)) {
		_clear_server();
		return false;
	}
	fn_sv_init = (BZSV_InitFn)symbol;
	if (!_load_symbol(server_handle, "BZSV_Shutdown", symbol)) {
		_clear_server();
		return false;
	}
	fn_sv_shutdown = (BZSV_ShutdownFn)symbol;
	if (!_load_symbol(server_handle, "BZSV_Tick", symbol)) {
		_clear_server();
		return false;
	}
	fn_sv_tick = (BZSV_TickFn)symbol;
	if (!_load_symbol(server_handle, "BZSV_OnPacket", symbol)) {
		_clear_server();
		return false;
	}
	fn_sv_on_packet = (BZSV_OnPacketFn)symbol;
	if (!_load_symbol(server_handle, "BZSV_DropClient", symbol)) {
		_clear_server();
		return false;
	}
	fn_sv_drop_client = (BZSV_DropClientFn)symbol;
	if (!_load_symbol(server_handle, "BZSV_SetSendPacket", symbol)) {
		_clear_server();
		return false;
	}
	fn_sv_set_send = (BZSV_SetSendPacketFn)symbol;
	if (!_load_symbol(server_handle, "BZSV_ClientJoin", symbol)) {
		_clear_server();
		return false;
	}
	fn_sv_client_join = (BZSV_ClientJoinFn)symbol;
	if (!_load_symbol(server_handle, "BZSV_SetNotifyDrop", symbol)) {
		_clear_server();
		return false;
	}
	fn_sv_notify_drop = (BZSV_SetNotifyDropFn)symbol;

	if (!_load_symbol(gb_handle, "BZGB_Connect", symbol)) {
		_clear_server();
		return false;
	}
	fn_gb_connect = (BZGB_ConnectFn)symbol;
	if (!_load_symbol(gb_handle, "BZGB_ConnectUrl", symbol)) {
		_clear_server();
		return false;
	}
	fn_gb_connect_url = (BZGB_ConnectUrlFn)symbol;
	if (!_load_symbol(gb_handle, "BZGB_Close", symbol)) {
		_clear_server();
		return false;
	}
	fn_gb_close = (BZGB_CloseFn)symbol;
	if (!_load_symbol(gb_handle, "BZGB_Tick", symbol)) {
		_clear_server();
		return false;
	}
	fn_gb_tick = (BZGB_TickFn)symbol;
	if (!_load_symbol(gb_handle, "BZGB_Connected", symbol)) {
		_clear_server();
		return false;
	}
	fn_gb_connected = (BZGB_ConnectedFn)symbol;
	if (!_load_symbol(gb_handle, "BZGB_Send", symbol)) {
		_clear_server();
		return false;
	}
	fn_gb_send = (BZGB_SendFn)symbol;
	if (!_load_symbol(gb_handle, "BZGB_SetActionReceiver", symbol)) {
		_clear_server();
		return false;
	}
	fn_gb_set_action = (BZGB_SetActionFn)symbol;
	_load_symbol(gb_handle, "BZGB_SetTimeoutMs", symbol);
	fn_gb_set_timeout = (BZGB_SetTimeoutFn)symbol;
	_load_symbol(gb_handle, "BZGB_SetApiKey", symbol);
	fn_gb_set_api_key = (BZGB_SetApiKeyFn)symbol;

	server_loaded = true;
	return true;
}

void AnticheatAPILoader::_clear_client() {
	if (client_handle) {
		OS::get_singleton()->close_dynamic_library(client_handle);
		client_handle = nullptr;
	}
	client_loaded = false;
	fn_cl_init = nullptr;
	fn_cl_shutdown = nullptr;
	fn_cl_is_enabled = nullptr;
	fn_cl_tick = nullptr;
	fn_cl_on_packet = nullptr;
	fn_cl_on_command = nullptr;
	fn_cl_set_screenshot = nullptr;
	fn_cl_provide_screenshot = nullptr;
	fn_cl_set_send = nullptr;
	fn_cl_set_hash = nullptr;
}

void AnticheatAPILoader::_clear_server() {
	if (server_handle) {
		OS::get_singleton()->close_dynamic_library(server_handle);
		server_handle = nullptr;
	}
	if (gb_handle) {
		OS::get_singleton()->close_dynamic_library(gb_handle);
		gb_handle = nullptr;
	}
	server_loaded = false;
	fn_sv_init = nullptr;
	fn_sv_shutdown = nullptr;
	fn_sv_tick = nullptr;
	fn_sv_on_packet = nullptr;
	fn_sv_drop_client = nullptr;
	fn_sv_set_send = nullptr;
	fn_sv_notify_drop = nullptr;
	fn_sv_client_join = nullptr;
	fn_gb_connect = nullptr;
	fn_gb_connect_url = nullptr;
	fn_gb_close = nullptr;
	fn_gb_tick = nullptr;
	fn_gb_connected = nullptr;
	fn_gb_send = nullptr;
	fn_gb_set_action = nullptr;
	fn_gb_set_timeout = nullptr;
	fn_gb_set_api_key = nullptr;
}

void AnticheatAPILoader::unload() {
	_clear_client();
	_clear_server();
}

int AnticheatAPILoader::cl_init() {
	return fn_cl_init ? fn_cl_init() : 1;
}

void AnticheatAPILoader::cl_shutdown() {
	if (fn_cl_shutdown) {
		fn_cl_shutdown();
	}
}

int AnticheatAPILoader::cl_is_enabled() const {
	return fn_cl_is_enabled ? fn_cl_is_enabled() : 0;
}

void AnticheatAPILoader::cl_tick(int tick_limit_ms) {
	if (fn_cl_tick) {
		fn_cl_tick(tick_limit_ms);
	}
}

int AnticheatAPILoader::cl_on_packet(const void *data, int len) {
	return fn_cl_on_packet ? fn_cl_on_packet(data, len) : 1;
}

int AnticheatAPILoader::cl_on_command(const char *utf8) {
	return fn_cl_on_command ? fn_cl_on_command(utf8) : 1;
}

void AnticheatAPILoader::cl_set_screenshot_receiver(BZCL_ScreenshotFn fn) {
	if (fn_cl_set_screenshot) {
		fn_cl_set_screenshot(fn);
	}
}

void AnticheatAPILoader::cl_set_send_packet(BZCL_SendPacketFn fn) {
	if (fn_cl_set_send) {
		fn_cl_set_send(fn);
	}
}

void AnticheatAPILoader::cl_set_file_hash(BZCL_FileHashFn fn) {
	if (fn_cl_set_hash) {
		fn_cl_set_hash(fn);
	}
}

void AnticheatAPILoader::cl_provide_screenshot(const unsigned char *rgba, int w, int h, int bpp) {
	if (fn_cl_provide_screenshot) {
		fn_cl_provide_screenshot(rgba, w, h, bpp);
	}
}

int AnticheatAPILoader::sv_init() {
	return fn_sv_init ? fn_sv_init() : 1;
}

void AnticheatAPILoader::sv_shutdown() {
	if (fn_sv_shutdown) {
		fn_sv_shutdown();
	}
}

void AnticheatAPILoader::sv_tick(int tick_limit_ms) {
	if (fn_sv_tick) {
		fn_sv_tick(tick_limit_ms);
	}
}

int AnticheatAPILoader::sv_on_packet(int client_index, const void *data, int len) {
	return fn_sv_on_packet ? fn_sv_on_packet(client_index, data, len) : 1;
}

int AnticheatAPILoader::sv_drop_client(int client_index, const char *reason_utf8) {
	return fn_sv_drop_client ? fn_sv_drop_client(client_index, reason_utf8) : 1;
}

void AnticheatAPILoader::sv_set_send_packet(BZSV_SendPacketFn fn) {
	if (fn_sv_set_send) {
		fn_sv_set_send(fn);
	}
}

void AnticheatAPILoader::sv_set_notify_drop(BZSV_NotifyDropFn fn) {
	if (fn_sv_notify_drop) {
		fn_sv_notify_drop(fn);
	}
}

int AnticheatAPILoader::sv_client_join(int client_index) {
	return fn_sv_client_join ? fn_sv_client_join(client_index) : 1;
}

int AnticheatAPILoader::gb_connect(const char *address, unsigned short port, const char *source_id) {
	return fn_gb_connect ? fn_gb_connect(address, port, source_id) : 1;
}

int AnticheatAPILoader::gb_connect_url(const char *url, unsigned short port, const char *source_id) {
	if (fn_gb_connect_url) {
		return fn_gb_connect_url(url, port, source_id);
	}
	return fn_gb_connect ? fn_gb_connect(url, port, source_id) : 1;
}

void AnticheatAPILoader::gb_close() {
	if (fn_gb_close) {
		fn_gb_close();
	}
}

void AnticheatAPILoader::gb_tick() {
	if (fn_gb_tick) {
		fn_gb_tick();
	}
}

int AnticheatAPILoader::gb_connected() const {
	return fn_gb_connected ? fn_gb_connected() : 0;
}

int AnticheatAPILoader::gb_send(const void *data, int len) {
	return fn_gb_send ? fn_gb_send(data, len) : 1;
}

void AnticheatAPILoader::gb_set_action_receiver(BZGB_ActionFn fn) {
	if (fn_gb_set_action) {
		fn_gb_set_action(fn);
	}
}

void AnticheatAPILoader::gb_set_timeout_ms(int timeout_ms) {
	if (fn_gb_set_timeout) {
		fn_gb_set_timeout(timeout_ms);
	}
}

void AnticheatAPILoader::gb_set_api_key(const char *api_key) {
	if (fn_gb_set_api_key) {
		fn_gb_set_api_key(api_key);
	}
}
