/**************************************************************************/
/*  youtube_playables_response.h                                          */
/**************************************************************************/
/*                         This file is part of:                          */
/*                             BLAZIUM ENGINE                             */
/*                          https://blazium.app                           */
/**************************************************************************/
/* Copyright (c) 2024-present Blazium Engine contributors.                */
/* Copyright (c) 2025 Nicholas Santos Shiden.                             */
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

#include "core/io/json.h"
#include "core/object/ref_counted.h"
#include "platform/web/api/javascript_bridge_singleton.h"

class YoutubePlayablesResponse : public RefCounted {
	GDCLASS(YoutubePlayablesResponse, RefCounted);

	Ref<JavaScriptObject> signal_finish_callback;

protected:
	static void _bind_methods() {
		ClassDB::bind_method(D_METHOD("_signal_finish"), &YoutubePlayablesResponse::_signal_finish);

		ADD_SIGNAL(MethodInfo("finished", PropertyInfo(Variant::OBJECT, "result", PROPERTY_HINT_RESOURCE_TYPE, "YoutubePlayablesResult")));
	}

public:
	class YoutubePlayablesResult : public RefCounted {
		GDCLASS(YoutubePlayablesResult, RefCounted);

		String error;
		String data;

	protected:
		static void _bind_methods() {
			ClassDB::bind_method(D_METHOD("get_data"), &YoutubePlayablesResult::get_data);
			ClassDB::bind_method(D_METHOD("has_error"), &YoutubePlayablesResult::has_error);
			ClassDB::bind_method(D_METHOD("get_error"), &YoutubePlayablesResult::get_error);

			ADD_PROPERTY(PropertyInfo(Variant::DICTIONARY, "data"), "", "get_data");
			ADD_PROPERTY(PropertyInfo(Variant::STRING, "error"), "", "get_error");
		}

	public:
		void set_error(String p_error) { this->error = p_error; }
		bool has_error() const { return !error.is_empty(); }
		String get_error() const { return error; }

		void set_data(String p_data) { this->data = p_data; }
		String get_data() const { return data; }
	};

	void _signal_finish(Array p_input) {
		ERR_FAIL_COND(p_input.is_empty());

		Dictionary dict = JSON::parse_string(p_input[0]);
		Ref<YoutubePlayablesResult> result = Ref<YoutubePlayablesResult>();
		result.instantiate();
		result->set_error(dict.get("error", ""));
		result->set_data(dict.get("data", ""));
		emit_signal("finished", result);
	}

	void _signal_finish_error(String p_input) {
		Ref<YoutubePlayablesResult> result = Ref<YoutubePlayablesResult>();
		result.instantiate();
		result->set_error(p_input);
		emit_signal("finished", result);
	}

	void create_signal_finish_callback() {
		JavaScriptBridge *singleton = JavaScriptBridge::get_singleton();
		if (!singleton) {
			ERR_PRINT("JavaScriptBridge singleton is invalid");
			return;
		}
		signal_finish_callback = singleton->create_callback(Callable(this, "_signal_finish"));
	}

	Ref<JavaScriptObject> get_signal_finish_callback() {
		return signal_finish_callback;
	}
};
