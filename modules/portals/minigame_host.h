/**************************************************************************/
/*  minigame_host.h                                                       */
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

#pragma once

#include "portal_host.h"

#define MINIGAME_HOST_HANDLERS(m_class)                                                                 \
	PORTAL_HOST_HANDLERS(m_class)                                                                       \
	friend class MiniGameApi<m_class>;                                                                  \
	void _portal_shown() {                                                                              \
		emit_signal(SNAME("shown"));                                                                    \
		emit_signal(SNAME("resumed"));                                                                  \
	}                                                                                                   \
	void _portal_hidden() {                                                                             \
		emit_signal(SNAME("hidden"));                                                                   \
		emit_signal(SNAME("paused"));                                                                   \
	}                                                                                                   \
	void _portal_host_error(const String &p_message) { emit_signal(SNAME("host_error"), p_message); }

#define MINIGAME_HOST_PUBLIC(m_class)                                                          \
	PORTAL_HOST_PUBLIC(m_class)                                                                \
	void login() { MiniGameApi<m_class>::login(); }                                            \
	void check_session() { MiniGameApi<m_class>::check_session(); }                            \
	void get_profile() { MiniGameApi<m_class>::get_profile(); }                                \
	void set_storage(const String &p_key, const String &p_value) {                             \
		MiniGameApi<m_class>::set_storage(p_key, p_value);                                     \
	}                                                                                          \
	String get_storage(const String &p_key) { return MiniGameApi<m_class>::get_storage(p_key); } \
	void remove_storage(const String &p_key) { MiniGameApi<m_class>::remove_storage(p_key); }  \
	void clear_storage() { MiniGameApi<m_class>::clear_storage(); }                            \
	void show_rewarded_ad(const String &p_ad_unit_id) {                                        \
		MiniGameApi<m_class>::show_rewarded_ad(p_ad_unit_id);                                  \
	}                                                                                          \
	void share(const Dictionary &p_payload) { MiniGameApi<m_class>::share(p_payload); }        \
	void set_clipboard(const String &p_text) { MiniGameApi<m_class>::set_clipboard(p_text); }  \
	String get_clipboard() { return MiniGameApi<m_class>::get_clipboard(); }                   \
	void show_toast(const String &p_message) { MiniGameApi<m_class>::show_toast(p_message); }  \
	void show_modal(const String &p_title, const String &p_content) {                          \
		MiniGameApi<m_class>::show_modal(p_title, p_content);                                  \
	}                                                                                          \
	Dictionary get_system_info() { return MiniGameApi<m_class>::get_system_info(); }           \
	Dictionary get_launch_info() { return MiniGameApi<m_class>::get_launch_info(); }           \
	void load_subpackage(const String &p_name) { MiniGameApi<m_class>::load_subpackage(p_name); } \
	PackedByteArray read_host_file(const String &p_path) {                                     \
		return MiniGameApi<m_class>::read_host_file(p_path);                                   \
	}                                                                                          \
	void write_host_file(const String &p_path, const PackedByteArray &p_data) {                \
		MiniGameApi<m_class>::write_host_file(p_path, p_data);                                 \
	}                                                                                          \
	void play_inner_audio(const String &p_path) { MiniGameApi<m_class>::play_inner_audio(p_path); }

template <class T>
class MiniGameApi : public PortalApi<T> {
	Variant shown_callback;
	Variant hidden_callback;
	Variant error_callback;

public:
	MiniGameApi() { this->js_interface = "blaziumHostBridge"; }

	static void bind_shared();
	void bind_host_extra();

	void login();
	void check_session();
	void get_profile();
	void set_storage(const String &p_key, const String &p_value);
	String get_storage(const String &p_key);
	void remove_storage(const String &p_key);
	void clear_storage();
	void show_rewarded_ad(const String &p_ad_unit_id);
	void share(const Dictionary &p_payload);
	void set_clipboard(const String &p_text);
	String get_clipboard();
	void show_toast(const String &p_message);
	void show_modal(const String &p_title, const String &p_content);
	Dictionary get_system_info();
	Dictionary get_launch_info();
	void load_subpackage(const String &p_name);
	PackedByteArray read_host_file(const String &p_path);
	void write_host_file(const String &p_path, const PackedByteArray &p_data);
	void play_inner_audio(const String &p_path);
};

class WeChat : public Object, public MiniGameApi<WeChat> {
	GDCLASS(WeChat, Object);

protected:
	static void _bind_methods();
	MINIGAME_HOST_HANDLERS(WeChat)

public:
	MINIGAME_HOST_PUBLIC(WeChat)
	WeChat();
	void request_midas_payment(const Dictionary &p_options);
};

class Douyin : public Object, public MiniGameApi<Douyin> {
	GDCLASS(Douyin, Object);

protected:
	static void _bind_methods();
	MINIGAME_HOST_HANDLERS(Douyin)

public:
	MINIGAME_HOST_PUBLIC(Douyin)
	Douyin();
	void request_game_payment(const Dictionary &p_options);
};

class TikTok : public Object, public MiniGameApi<TikTok> {
	GDCLASS(TikTok, Object);

protected:
	static void _bind_methods();
	MINIGAME_HOST_HANDLERS(TikTok)

public:
	MINIGAME_HOST_PUBLIC(TikTok)
	TikTok();
};
