/**************************************************************************/
/*  fake_address_map.h                                                    */
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

#include "core/io/ip_address.h"
#include "core/os/mutex.h"
#include "core/templates/hash_map.h"
#include "core/templates/list.h"
#include "core/templates/vector.h"

class WebRTCLink;

struct FakePeerEntry {
	int signal_id = 0;
	IPAddress fake_ip;
	uint16_t fake_port = 1;
	String role;
	String name;
	WebRTCLink *link = nullptr;
	bool relay = false;
};

struct InboundDatagram {
	Vector<uint8_t> data;
	IPAddress src_ip;
	uint16_t src_port = 0;
};

class FakeAddressMap {
	HashMap<int, FakePeerEntry> by_signal;
	Mutex inbound_mutex;
	List<InboundDatagram> inbound;

public:
	static IPAddress make_fake_ip(uint8_t p_n);
	static bool is_fake_ip(const IPAddress &p_ip);

	void clear();
	void add_peer(const FakePeerEntry &p_entry);
	void remove_signal(int p_signal_id);
	void set_link(int p_signal_id, WebRTCLink *p_link);
	void set_relay(int p_signal_id, bool p_relay);

	bool lookup_signal(int p_signal_id, FakePeerEntry &r_entry) const;
	bool lookup_addr(const IPAddress &p_ip, uint16_t p_port, FakePeerEntry &r_entry) const;

	void push_inbound(const Vector<uint8_t> &p_data, const IPAddress &p_src_ip, uint16_t p_src_port);
	bool pop_inbound(InboundDatagram &r_out);
	int inbound_size();
};
