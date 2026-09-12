/**************************************************************************/
/*  react_client.cpp                                                      */
/**************************************************************************/
/*                         This file is part of:                          */
/*                            BLAZIUM ENGINE                              */
/*                        https://blazium.app                             */
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

#include "react_client.h"
#include "core/io/json.h"
#include "core/os/os.h"
#include "core/os/time.h"

void ReactClient::_bind_methods() {
	ClassDB::bind_method(D_METHOD("_on_message", "args"), &ReactClient::_on_message);
	ClassDB::bind_method(D_METHOD("emit_to_react", "event_name", "payload"), &ReactClient::emit_to_react, DEFVAL(Dictionary()));

	ClassDB::bind_method(D_METHOD("set_app_key", "app_key"), &ReactClient::set_app_key);
	ClassDB::bind_method(D_METHOD("get_app_key"), &ReactClient::get_app_key);
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "app_key"), "set_app_key", "get_app_key");

	ClassDB::bind_method(D_METHOD("set_allowed_origin", "allowed_origin"), &ReactClient::set_allowed_origin);
	ClassDB::bind_method(D_METHOD("get_allowed_origin"), &ReactClient::get_allowed_origin);
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "allowed_origin"), "set_allowed_origin", "get_allowed_origin");

	ADD_SIGNAL(MethodInfo("on_react_event", PropertyInfo(Variant::STRING, "event_name"), PropertyInfo(Variant::DICTIONARY, "payload")));
}

void ReactClient::_on_message(Array p_args) {
	if (p_args.is_empty()) {
		return;
	}

	Ref<JavaScriptObject> event_obj = p_args[0];
	if (!event_obj.is_valid()) {
		return;
	}

	String origin = event_obj->get("origin");
	if (allowed_origin != "*" && origin != allowed_origin) {
		return;
	}

	Variant raw = event_obj->get("data");
	if (raw.get_type() != Variant::STRING) {
		return;
	}

	Variant envelope_var = JSON::parse_string(raw);
	if (envelope_var.get_type() != Variant::DICTIONARY) {
		return;
	}

	Dictionary envelope = envelope_var;
	if (!envelope.has("event") || envelope["event"].get_type() != Variant::STRING) {
		return;
	}

	if (!app_key.is_empty()) {
		Variant meta_var = envelope.get("metadata", Dictionary());
		if (meta_var.get_type() != Variant::DICTIONARY) {
			print_line("[ReactBridge] Message rejected: secret key mismatch.");
			return;
		}
		Dictionary meta = meta_var;
		if (meta.get("secret", "") != app_key) {
			print_line("[ReactBridge] Message rejected: secret key mismatch.");
			return;
		}
	}

	String event_name = envelope["event"];
	Dictionary payload = envelope.get("payload", Dictionary());

	emit_signal("on_react_event", event_name, payload);
}

void ReactClient::emit_to_react(const String &p_event_name, const Dictionary &p_payload) {
	if (!OS::get_singleton()->has_feature("web")) {
		print_line(vformat("[ReactBridge] emit_to_react('%s') skipped — not a web build.", p_event_name));
		return;
	}

	if (!window.is_valid()) {
		return;
	}

	Dictionary event_metadata;
	event_metadata["secret"] = app_key;
	event_metadata["timestamp"] = Time::get_singleton()->get_unix_time_from_system();

	Dictionary envelope;
	envelope["event"] = p_event_name;
	envelope["payload"] = p_payload;
	envelope["metadata"] = event_metadata;

	String json_string = JSON::stringify(envelope);

	Ref<JavaScriptObject> parent = window->get("parent");
	if (parent.is_valid()) {
		parent->call("postMessage", json_string, "*");
	}
}

ReactClient::ReactClient() {
	if (!OS::get_singleton()->has_feature("web")) {
		return;
	}

	JavaScriptBridge *singleton = JavaScriptBridge::get_singleton();
	if (!singleton) {
		ERR_PRINT("JavaScriptBridge singleton is invalid");
		return;
	}

	window = singleton->get_interface("window");
	if (!window.is_valid()) {
		return;
	}

	msg_callback = singleton->create_callback(Callable(this, "_on_message"));
	window->call("addEventListener", "message", msg_callback);
	print_line("[ReactBridge] Listening for postMessage events.");
}

ReactClient::~ReactClient() {
	if (!OS::get_singleton()->has_feature("web")) {
		return;
	}

	if (window.is_valid() && msg_callback.is_valid()) {
		window->call("removeEventListener", "message", msg_callback);
	}
}
