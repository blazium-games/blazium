/**************************************************************************/
/*  enet_godot_socket.h                                                   */
/**************************************************************************/
/*                         This file is part of:                          */
/*                             GODOT ENGINE                               */
/*                        https://godotengine.org                         */
/**************************************************************************/
/* Copyright (c) 2014-present Godot Engine contributors (see AUTHORS.md). */
/* Copyright (c) 2007-2014 Juan Linietsky, Ariel Manzur.                  */
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

#include "core/error/error_list.h"
#include "core/io/ip_address.h"

/// Abstract ENet interface for UDP/DTLS/custom backends.
/// Intentionally does not include enet.h (Winsock) so module code can include this
/// without breaking Object/ConnectFlags.
class ENetGodotSocket {
public:
	virtual Error bind(IPAddress p_ip, uint16_t p_port) = 0;
	virtual Error get_socket_address(IPAddress *r_ip, uint16_t *r_port) = 0;
	virtual Error sendto(const uint8_t *p_buffer, int p_len, int &r_sent, IPAddress p_ip, uint16_t p_port) = 0;
	virtual Error recvfrom(uint8_t *p_buffer, int p_len, int &r_read, IPAddress &r_ip, uint16_t &r_port) = 0;
	virtual int set_option(int p_option, int p_value) = 0;
	virtual void close() = 0;
	virtual void set_refuse_new_connections(bool p_enable) {} /* Only used by dtls server */
	virtual bool can_upgrade() { return false; } /* Only true in ENetUDP */
	virtual ~ENetGodotSocket() {}
};

typedef ENetGodotSocket *(*ENetSocketCreateFn)();

/// Thread-local socket factory. nullptr restores the default UDP socket.
/// Does not change behavior unless a caller installs a function for the
/// duration of one enet_host_create.
void enet_set_socket_create_fn(ENetSocketCreateFn p_fn);
ENetSocketCreateFn enet_get_socket_create_fn();
