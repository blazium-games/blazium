/**************************************************************************/
/*  html_portals.h                                                        */
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

class YouTube : public Object, public PortalApi<YouTube> {
	GDCLASS(YouTube, Object);

	Variant audio_callback;

protected:
	static void _bind_methods();
	PORTAL_HOST_HANDLERS(YouTube)
	void bind_host_extra();
	void _on_audio(bool p_enabled);

public:
	PORTAL_HOST_PUBLIC(YouTube)
	YouTube();
	void send_first_frame();
	void game_ready();
	void send_score(int p_score);
	void save_data(const String &p_data);
	void load_data();
	String get_language();
	bool is_audio_enabled();
};

class Telegram : public Object, public PortalApi<Telegram> {
	GDCLASS(Telegram, Object);

protected:
	static void _bind_methods();
	PORTAL_HOST_HANDLERS(Telegram)

public:
	PORTAL_HOST_PUBLIC(Telegram)
	Telegram();
	String get_init_data();
	Dictionary get_theme();
	Dictionary get_viewport();
	Dictionary get_safe_area();
	void request_fullscreen();
	void send_data(const String &p_data);
};

class Reddit : public Object, public PortalApi<Reddit> {
	GDCLASS(Reddit, Object);

protected:
	static void _bind_methods();
	PORTAL_HOST_HANDLERS(Reddit)

public:
	PORTAL_HOST_PUBLIC(Reddit)
	Reddit();
	String get_username();
	void submit_score(int p_score);
};

class Newgrounds : public Object, public PortalApi<Newgrounds> {
	GDCLASS(Newgrounds, Object);

protected:
	static void _bind_methods();
	PORTAL_HOST_HANDLERS(Newgrounds)

public:
	PORTAL_HOST_PUBLIC(Newgrounds)
	Newgrounds();
	void log_in();
	void unlock_medal(int p_id);
	void submit_score(const String &p_board, int p_score);
	void cloud_save(const String &p_key, const String &p_value);
	String cloud_load(const String &p_key);
};

class CrazyGames : public Object, public PortalApi<CrazyGames> {
	GDCLASS(CrazyGames, Object);

protected:
	static void _bind_methods();
	PORTAL_HOST_HANDLERS(CrazyGames)

public:
	PORTAL_HOST_PUBLIC(CrazyGames)
	CrazyGames();
	void gameplay_start();
	void gameplay_stop();
	void request_midgame_ad();
	void request_rewarded_ad();
	String get_environment();
};

class Poki : public Object, public PortalApi<Poki> {
	GDCLASS(Poki, Object);

	Variant break_callback;

protected:
	static void _bind_methods();
	PORTAL_HOST_HANDLERS(Poki)
	void bind_host_extra();
	void _on_break_done(const String &p_kind);

public:
	PORTAL_HOST_PUBLIC(Poki)
	Poki();
	void gameplay_start();
	void gameplay_stop();
	void commercial_break();
	void rewarded_break();
	String get_shareable_url();
};

class Meta : public Object, public PortalApi<Meta> {
	GDCLASS(Meta, Object);

protected:
	static void _bind_methods();
	PORTAL_HOST_HANDLERS(Meta)

public:
	PORTAL_HOST_PUBLIC(Meta)
	Meta();
	String get_player_id();
	String get_player_name();
	String get_player_photo();
	String get_locale();
	String get_entry_point_data();
	String get_context_id();
};

class GameDistribution : public Object, public PortalApi<GameDistribution> {
	GDCLASS(GameDistribution, Object);

protected:
	static void _bind_methods();
	PORTAL_HOST_HANDLERS(GameDistribution)

public:
	PORTAL_HOST_PUBLIC(GameDistribution)
	GameDistribution();
	void show_ad();
};
