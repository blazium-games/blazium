/**************************************************************************/
/*  fake_address_map.cpp                                                  */
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

#include "fake_address_map.h"
#include "protocol.h"

IPAddress FakeAddressMap::make_fake_ip(uint8_t p_n) {
	return IPAddress(10, 66, 0, p_n);
}

bool FakeAddressMap::is_fake_ip(const IPAddress &p_ip) {
	if (!p_ip.is_valid() || !p_ip.is_ipv4()) {
		return false;
	}
	const uint8_t *b = p_ip.get_ipv4();
	return b[0] == 10 && b[1] == 66 && b[2] == 0 && b[3] >= 1 && b[3] <= 254;
}

void FakeAddressMap::clear() {
	by_signal.clear();
	MutexLock lock(inbound_mutex);
	inbound.clear();
}

void FakeAddressMap::add_peer(const FakePeerEntry &p_entry) {
	by_signal[p_entry.signal_id] = p_entry;
}

void FakeAddressMap::remove_signal(int p_signal_id) {
	by_signal.erase(p_signal_id);
}

void FakeAddressMap::set_link(int p_signal_id, WebRTCLink *p_link) {
	if (by_signal.has(p_signal_id)) {
		by_signal[p_signal_id].link = p_link;
	}
}

void FakeAddressMap::set_relay(int p_signal_id, bool p_relay) {
	if (by_signal.has(p_signal_id)) {
		by_signal[p_signal_id].relay = p_relay;
	}
}

bool FakeAddressMap::lookup_signal(int p_signal_id, FakePeerEntry &r_entry) const {
	if (!by_signal.has(p_signal_id)) {
		return false;
	}
	r_entry = by_signal[p_signal_id];
	return true;
}

bool FakeAddressMap::lookup_addr(const IPAddress &p_ip, uint16_t p_port, FakePeerEntry &r_entry) const {
	for (const KeyValue<int, FakePeerEntry> &E : by_signal) {
		if (E.value.fake_ip == p_ip && E.value.fake_port == p_port) {
			r_entry = E.value;
			return true;
		}
	}
	return false;
}

void FakeAddressMap::push_inbound(const Vector<uint8_t> &p_data, const IPAddress &p_src_ip, uint16_t p_src_port) {
	if (p_data.is_empty() || p_data.size() > GamesEnetWebrtcProtocol::MAX_RELAY_BYTES) {
		return;
	}
	InboundDatagram d;
	d.data = p_data;
	d.src_ip = p_src_ip;
	d.src_port = p_src_port;
	MutexLock lock(inbound_mutex);
	while (inbound.size() >= GamesEnetWebrtcProtocol::MAX_INBOUND_DATAGRAMS) {
		inbound.pop_front();
	}
	inbound.push_back(d);
}

bool FakeAddressMap::pop_inbound(InboundDatagram &r_out) {
	MutexLock lock(inbound_mutex);
	if (inbound.is_empty()) {
		return false;
	}
	r_out = inbound.front()->get();
	inbound.pop_front();
	return true;
}

int FakeAddressMap::inbound_size() {
	MutexLock lock(inbound_mutex);
	return inbound.size();
}
