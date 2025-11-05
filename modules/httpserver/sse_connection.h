/**************************************************************************/
/*  sse_connection.h                                                      */
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

#include "core/io/stream_peer.h"
#include "core/object/ref_counted.h"

class SSEConnection : public RefCounted {
	GDCLASS(SSEConnection, RefCounted);

private:
	Ref<StreamPeer> peer;
	int connection_id = 0;
	String path;
	bool connected = true;

protected:
	static void _bind_methods();

public:
	void set_peer(const Ref<StreamPeer> &p_peer);
	Ref<StreamPeer> get_peer() const;

	void set_connection_id(int p_id);
	int get_connection_id() const;

	void set_path(const String &p_path);
	String get_path() const;

	Error send_event(const String &p_event_type, const String &p_data, const String &p_event_id = "");
	Error send_data(const String &p_data);

	void close_connection();
	bool is_connection_active() const;

	SSEConnection();
	~SSEConnection();
};
