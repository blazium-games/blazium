/**************************************************************************/
/*  react_client.h                                                        */
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

#pragma once

#include "../third_party_client.h"
#include "platform/web/api/javascript_bridge_singleton.h"

class ReactClient : public ThirdPartyClient {
	GDCLASS(ReactClient, ThirdPartyClient);

	String app_key = "";
	String allowed_origin = "*";

	Ref<JavaScriptObject> msg_callback;
	Ref<JavaScriptObject> window;

protected:
	static void _bind_methods();

public:
	void _on_message(Array p_args);
	void emit_to_react(const String &p_event_name, const Dictionary &p_payload = Dictionary());

	String get_app_key() const { return app_key; }
	void set_app_key(const String &p_app_key) { app_key = p_app_key; }

	String get_allowed_origin() const { return allowed_origin; }
	void set_allowed_origin(const String &p_allowed_origin) { allowed_origin = p_allowed_origin; }

	ReactClient();
	~ReactClient();
};
