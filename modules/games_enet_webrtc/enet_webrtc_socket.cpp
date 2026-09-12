/**************************************************************************/
/*  enet_webrtc_socket.cpp                                                */
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

#include "enet_webrtc_socket.h"
#include "webrtc_enet_session.h"

#include "fake_address_map.h"
#include "protocol.h"

#include <cstring>

ENetWebRTCSocket::ENetWebRTCSocket(FakeAddressMap *p_map, WebRTCEnetSession *p_session) :
		map(p_map),
		session(p_session) {
}

Error ENetWebRTCSocket::bind(IPAddress p_ip, uint16_t p_port) {
	bound_ip = p_ip;
	bound_port = p_port;
	bound = true;
	return OK;
}

Error ENetWebRTCSocket::get_socket_address(IPAddress *r_ip, uint16_t *r_port) {
	if (!bound) {
		return ERR_UNCONFIGURED;
	}
	if (r_ip) {
		*r_ip = bound_ip;
	}
	if (r_port) {
		*r_port = bound_port;
	}
	return OK;
}

Error ENetWebRTCSocket::sendto(const uint8_t *p_buffer, int p_len, int &r_sent, IPAddress p_ip, uint16_t p_port) {
	r_sent = 0;
	ERR_FAIL_NULL_V(session, ERR_UNCONFIGURED);
	ERR_FAIL_COND_V(p_buffer == nullptr || p_len <= 0 || p_len > GamesEnetWebrtcProtocol::MAX_RELAY_BYTES, ERR_INVALID_PARAMETER);
	const Error err = session->send_datagram(p_ip, p_port, p_buffer, p_len);
	if (err != OK) {
		return err;
	}
	r_sent = p_len;
	return OK;
}

Error ENetWebRTCSocket::recvfrom(uint8_t *p_buffer, int p_len, int &r_read, IPAddress &r_ip, uint16_t &r_port) {
	r_read = 0;
	ERR_FAIL_NULL_V(map, ERR_UNCONFIGURED);
	ERR_FAIL_COND_V(p_buffer == nullptr || p_len <= 0, ERR_INVALID_PARAMETER);
	InboundDatagram d;
	if (!map->pop_inbound(d)) {
		return ERR_BUSY;
	}
	const int copy = MIN(p_len, d.data.size());
	if (copy > 0) {
		memcpy(p_buffer, d.data.ptr(), copy);
	}
	r_read = copy;
	r_ip = d.src_ip;
	r_port = d.src_port;
	if (d.data.size() > p_len) {
		return ERR_OUT_OF_MEMORY;
	}
	return OK;
}

int ENetWebRTCSocket::set_option(int p_option, int p_value) {
	(void)p_value;
	switch (p_option) {
		case ENET_SOCKOPT_NONBLOCK:
		case ENET_SOCKOPT_RCVBUF:
		case ENET_SOCKOPT_SNDBUF:
			return 0;
		default:
			return 0;
	}
}

void ENetWebRTCSocket::close() {
	bound = false;
	bound_ip.clear();
	bound_port = 0;
}
