/**************************************************************************/
/*  analytics.h                                                           */
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

#include "core/object/class_db.h"
#include "core/object/object.h"
#include "core/variant/dictionary.h"

class Analytics : public Object {
	GDCLASS(Analytics, Object);

public:
	enum Consent {
		CONSENT_UNSET = 0,
		CONSENT_ACCEPTED = 1,
		CONSENT_DECLINED = 2,
	};

private:
	static Analytics *singleton;

	String session_id;
	uint64_t session_start_msec = 0;
	bool shutdown_notified = false;
	bool editor_launched_sent = false;
	bool session_start_sent = false;
	bool has_consent_override = false;
	Consent consent_override = CONSENT_UNSET;
	bool has_anonymous_override = false;
	bool anonymous_override = true;
	String user_id;
	Dictionary user_properties;

	String _cmdline_value(const String &p_prefix) const;
	String _env(const String &p_name) const;
	Consent _parse_consent(const String &p_value) const;
	bool _parse_bool_env(const String &p_value, bool p_fallback) const;
	String _setting_string(const String &p_key, const String &p_fallback) const;
	bool _setting_bool(const String &p_key, bool p_fallback) const;
	bool _is_editor_context() const;
	bool _can_queue() const;
	String _baked_editor(const String &p_macro_value, const String &p_fallback) const;
	String _iso_timestamp() const;
	String _size_string(const Size2i &p_size) const;
	void _attach_editor_context(Dictionary &r_props) const;
	void _attach_game_context(Dictionary &r_props) const;
	Dictionary _build_event(const String &p_event, const Dictionary &p_properties) const;
	void _enqueue(const String &p_event, const Dictionary &p_properties, bool p_editor_context);

protected:
	static void _bind_methods();

public:
	static Analytics *get_singleton();

	void report_user_data_dir_ready();
	void notify_editor_ready();
	void notify_shutdown();

	bool is_available() const;
	bool is_enabled() const;
	String get_consent() const;
	void set_consent(bool p_accepted);
	bool is_anonymous() const;
	void set_anonymous(bool p_anonymous);

	String get_app_id() const;
	String get_build_id() const;
	String get_app_version() const;
	String get_build_channel() const;
	String get_device_uid() const;
	String get_endpoint() const;
	String get_queue_directory() const;

	void track(const String &p_event, const Dictionary &p_properties = Dictionary());
	void identify(const String &p_user_id);
	void set_user_properties(const Dictionary &p_properties);
	void flush();
	int get_queue_size() const;
	Dictionary get_resolved_config() const;

	Analytics();
	~Analytics();
};

VARIANT_ENUM_CAST(Analytics::Consent);
