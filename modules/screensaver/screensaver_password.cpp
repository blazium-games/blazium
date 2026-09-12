/**************************************************************************/
/*  screensaver_password.cpp                                              */
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

#include "screensaver_password.h"

#include "core/config/project_settings.h"
#include "core/crypto/crypto_core.h"
#include "core/io/config_file.h"
#include "core/os/os.h"

#ifdef WINDOWS_ENABLED
#include <windows.h>
#endif

bool ScreensaverPassword::test_override = false;
String ScreensaverPassword::test_salt;
String ScreensaverPassword::test_hash;
bool ScreensaverPassword::test_enabled_override = false;
bool ScreensaverPassword::test_enabled = false;

String ScreensaverPassword::hash_password(const String &p_plain, const String &p_salt) {
	const String payload = p_salt + ":" + p_plain;
	const CharString utf8 = payload.utf8();
	unsigned char digest[32];
	CryptoCore::sha256((const uint8_t *)utf8.get_data(), utf8.length(), digest);
	return String::hex_encode_buffer(digest, 32);
}

String ScreensaverPassword::generate_salt() {
	uint8_t bytes[16];
	CryptoCore::RandomGenerator rng;
	if (rng.init() != OK || rng.get_random_bytes(bytes, 16) != OK) {
		const uint64_t ticks = OS::get_singleton() ? OS::get_singleton()->get_ticks_usec() : 1;
		for (int i = 0; i < 16; i++) {
			bytes[i] = uint8_t((ticks >> ((i * 5) % 56)) & 0xFF);
		}
	}
	return String::hex_encode_buffer(bytes, 16);
}

String ScreensaverPassword::storage_key() {
	String name = "Blazium";
	if (ProjectSettings::get_singleton()) {
		const String app = ProjectSettings::get_singleton()->get_setting("application/config/name", "Blazium");
		if (!String(app).is_empty()) {
			name = String(app);
		}
	}
	return name.validate_filename();
}

#ifdef WINDOWS_ENABLED
static String _reg_path() {
	return "Software\\Blazium\\Screensaver\\" + ScreensaverPassword::storage_key();
}

static bool _reg_read(const String &p_value, String &r_out) {
	HKEY key = nullptr;
	const Char16String path = _reg_path().utf16();
	if (RegOpenKeyExW(HKEY_CURRENT_USER, (LPCWSTR)path.get_data(), 0, KEY_READ, &key) != ERROR_SUCCESS) {
		return false;
	}
	wchar_t buf[512];
	DWORD size = sizeof(buf);
	DWORD type = 0;
	const Char16String value = p_value.utf16();
	const LSTATUS st = RegQueryValueExW(key, (LPCWSTR)value.get_data(), nullptr, &type, (LPBYTE)buf, &size);
	RegCloseKey(key);
	if (st != ERROR_SUCCESS || type != REG_SZ) {
		return false;
	}
	r_out = String::utf16((const char16_t *)buf);
	return !r_out.is_empty();
}

static Error _reg_write_sz(const String &p_value, const String &p_data) {
	HKEY key = nullptr;
	DWORD disp = 0;
	const Char16String path = _reg_path().utf16();
	if (RegCreateKeyExW(HKEY_CURRENT_USER, (LPCWSTR)path.get_data(), 0, nullptr, 0, KEY_WRITE, nullptr, &key, &disp) != ERROR_SUCCESS) {
		return ERR_CANT_CREATE;
	}
	const Char16String value = p_value.utf16();
	const Char16String data = p_data.utf16();
	const LSTATUS st = RegSetValueExW(key, (LPCWSTR)value.get_data(), 0, REG_SZ, (const BYTE *)data.get_data(), (DWORD)((data.length() + 1) * sizeof(wchar_t)));
	RegCloseKey(key);
	return st == ERROR_SUCCESS ? OK : ERR_CANT_CREATE;
}

static Error _reg_write(const String &p_salt, const String &p_hash) {
	if (_reg_write_sz("Salt", p_salt) != OK) {
		return ERR_CANT_CREATE;
	}
	return _reg_write_sz("Hash", p_hash);
}

static Error _reg_delete_value(const String &p_value) {
	HKEY key = nullptr;
	const Char16String path = _reg_path().utf16();
	if (RegOpenKeyExW(HKEY_CURRENT_USER, (LPCWSTR)path.get_data(), 0, KEY_WRITE, &key) != ERROR_SUCCESS) {
		return OK;
	}
	const Char16String value = p_value.utf16();
	RegDeleteValueW(key, (LPCWSTR)value.get_data());
	RegCloseKey(key);
	return OK;
}

static Error _reg_clear_password() {
	_reg_delete_value("Salt");
	_reg_delete_value("Hash");
	return OK;
}
#endif

static String _cfg_path() {
	return "user://screensaver_password.cfg";
}

bool ScreensaverPassword::_load(String &r_salt, String &r_hash) {
	if (test_override) {
		r_salt = test_salt;
		r_hash = test_hash;
		return !r_salt.is_empty() && !r_hash.is_empty();
	}
#ifdef WINDOWS_ENABLED
	if (_reg_read("Salt", r_salt) && _reg_read("Hash", r_hash)) {
		return true;
	}
#endif
	Ref<ConfigFile> cf;
	cf.instantiate();
	if (cf->load(_cfg_path()) != OK) {
		return false;
	}
	r_salt = cf->get_value("password", "salt", "");
	r_hash = cf->get_value("password", "hash", "");
	return !r_salt.is_empty() && !r_hash.is_empty();
}

Error ScreensaverPassword::_save(const String &p_salt, const String &p_hash) {
	if (test_override) {
		test_salt = p_salt;
		test_hash = p_hash;
		return OK;
	}
#ifdef WINDOWS_ENABLED
	const Error reg_err = _reg_write(p_salt, p_hash);
	if (reg_err == OK) {
		return OK;
	}
#endif
	Ref<ConfigFile> cf;
	cf.instantiate();
	cf->load(_cfg_path());
	cf->set_value("password", "salt", p_salt);
	cf->set_value("password", "hash", p_hash);
	return cf->save(_cfg_path());
}

Error ScreensaverPassword::_clear_store() {
	if (test_override) {
		test_salt = String();
		test_hash = String();
		return OK;
	}
#ifdef WINDOWS_ENABLED
	_reg_clear_password();
#endif
	Ref<ConfigFile> cf;
	cf.instantiate();
	cf->load(_cfg_path());
	if (cf->has_section_key("password", "salt")) {
		cf->erase_section_key("password", "salt");
	}
	if (cf->has_section_key("password", "hash")) {
		cf->erase_section_key("password", "hash");
	}
	return cf->save(_cfg_path());
}

bool ScreensaverPassword::_load_enabled(bool &r_enabled) {
	if (test_override) {
		if (!test_enabled_override) {
			return false;
		}
		r_enabled = test_enabled;
		return true;
	}
#ifdef WINDOWS_ENABLED
	String raw;
	if (_reg_read("Enabled", raw)) {
		r_enabled = raw == "1" || raw.to_lower() == "true";
		return true;
	}
#endif
	Ref<ConfigFile> cf;
	cf.instantiate();
	if (cf->load(_cfg_path()) != OK) {
		return false;
	}
	if (!cf->has_section_key("password", "enabled")) {
		return false;
	}
	r_enabled = bool(cf->get_value("password", "enabled", false));
	return true;
}

Error ScreensaverPassword::_save_enabled(bool p_enabled) {
	if (test_override) {
		test_enabled_override = true;
		test_enabled = p_enabled;
		return OK;
	}
#ifdef WINDOWS_ENABLED
	if (_reg_write_sz("Enabled", p_enabled ? "1" : "0") == OK) {
		return OK;
	}
#endif
	Ref<ConfigFile> cf;
	cf.instantiate();
	cf->load(_cfg_path());
	cf->set_value("password", "enabled", p_enabled);
	return cf->save(_cfg_path());
}

bool ScreensaverPassword::has_password() {
	String salt;
	String hash;
	return _load(salt, hash);
}

bool ScreensaverPassword::verify(const String &p_plain) {
	String salt;
	String hash;
	if (!_load(salt, hash)) {
		return false;
	}
	return hash_password(p_plain, salt) == hash;
}

Error ScreensaverPassword::set_password(const String &p_old_plain, const String &p_new_plain) {
	if (has_password() && !verify(p_old_plain)) {
		return ERR_INVALID_PARAMETER;
	}
	if (p_new_plain.is_empty()) {
		return clear_password(p_old_plain);
	}
	const String salt = generate_salt();
	return _save(salt, hash_password(p_new_plain, salt));
}

Error ScreensaverPassword::clear_password(const String &p_current_plain) {
	if (has_password() && !verify(p_current_plain)) {
		return ERR_INVALID_PARAMETER;
	}
	return _clear_store();
}

bool ScreensaverPassword::has_password_enabled_override() {
	bool enabled = false;
	return _load_enabled(enabled);
}

bool ScreensaverPassword::get_password_enabled() {
	bool enabled = false;
	if (_load_enabled(enabled)) {
		return enabled;
	}
	return false;
}

Error ScreensaverPassword::set_password_enabled(bool p_enabled) {
	return _save_enabled(p_enabled);
}

void ScreensaverPassword::reset_for_tests() {
	test_override = true;
	test_salt = String();
	test_hash = String();
	test_enabled_override = false;
	test_enabled = false;
}

void ScreensaverPassword::set_store_for_tests(const String &p_salt, const String &p_hash) {
	test_override = true;
	test_salt = p_salt;
	test_hash = p_hash;
}

String ScreensaverPassword::get_stored_hash_for_tests() {
	return test_hash;
}

String ScreensaverPassword::get_stored_salt_for_tests() {
	return test_salt;
}

void ScreensaverPassword::set_enabled_for_tests(bool p_enabled) {
	test_override = true;
	test_enabled_override = true;
	test_enabled = p_enabled;
}

bool ScreensaverPassword::has_enabled_override_for_tests() {
	return test_enabled_override;
}

bool ScreensaverPassword::get_enabled_for_tests() {
	return test_enabled;
}
