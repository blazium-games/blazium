/**************************************************************************/
/*  enet_webrtc_socket_factory.cpp                                        */
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

#include "enet_webrtc_socket_factory.h"
#include "enet_webrtc_socket.h"
#include "webrtc_enet_session.h"

thread_local FakeAddressMap *ENetWebRTCSocketFactory::map = nullptr;
thread_local WebRTCEnetSession *ENetWebRTCSocketFactory::session = nullptr;

ENetGodotSocket *ENetWebRTCSocketFactory::_create() {
	if (!map || !session) {
		return nullptr;
	}
	return memnew(ENetWebRTCSocket(map, session));
}

void ENetWebRTCSocketFactory::install(FakeAddressMap *p_map, WebRTCEnetSession *p_session) {
	map = p_map;
	session = p_session;
	enet_set_socket_create_fn(&_create);
}

void ENetWebRTCSocketFactory::uninstall() {
	enet_set_socket_create_fn(nullptr);
	map = nullptr;
	session = nullptr;
}

ENetWebRTCSocketFactory::Guard::Guard(FakeAddressMap *p_map, WebRTCEnetSession *p_session) {
	ENetWebRTCSocketFactory::install(p_map, p_session);
}

ENetWebRTCSocketFactory::Guard::~Guard() {
	ENetWebRTCSocketFactory::uninstall();
}
