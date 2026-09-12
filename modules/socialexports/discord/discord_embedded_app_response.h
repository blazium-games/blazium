/**************************************************************************/
/*  discord_embedded_app_response.h                                       */
/**************************************************************************/
/*                         This file is part of:                          */
/*                             BLAZIUM ENGINE                             */
/*                          https://blazium.app                           */
/**************************************************************************/
/* Copyright (c) 2024-present Blazium Engine contributors.                */
/* Copyright (c) 2024 Dragos Daian, Randolph William Aarseth II.          */
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

#include "core/object/ref_counted.h"

class DiscordEmbeddedAppResponse : public RefCounted {
	GDCLASS(DiscordEmbeddedAppResponse, RefCounted);

protected:
	static void _bind_methods() {
		ADD_SIGNAL(MethodInfo("finished", PropertyInfo(Variant::OBJECT, "result", PROPERTY_HINT_RESOURCE_TYPE, "DiscordEmbeddedAppResult")));
	}

public:
	class DiscordEmbeddedAppResult : public RefCounted {
		GDCLASS(DiscordEmbeddedAppResult, RefCounted);
		String error = "";
		Dictionary data;

	protected:
		static void _bind_methods() {
			ClassDB::bind_method(D_METHOD("get_data"), &DiscordEmbeddedAppResult::get_data);
			ClassDB::bind_method(D_METHOD("has_error"), &DiscordEmbeddedAppResult::has_error);
			ClassDB::bind_method(D_METHOD("get_error"), &DiscordEmbeddedAppResult::get_error);
			ADD_PROPERTY(PropertyInfo(Variant::DICTIONARY, "data"), "", "get_data");
			ADD_PROPERTY(PropertyInfo(Variant::STRING, "error"), "", "get_error");
		}

	public:
		void set_error(String p_error) { this->error = p_error; }
		bool has_error() const { return !error.is_empty(); }
		String get_error() const { return error; }

		void set_data(Dictionary p_data) { this->data = p_data; }
		Dictionary get_data() const { return data; }
	};

	void signal_finish(String p_error) {
		Ref<DiscordEmbeddedAppResult> result;
		result.instantiate();
		result->set_error(p_error);
		emit_signal("finished", result);
	}
};
