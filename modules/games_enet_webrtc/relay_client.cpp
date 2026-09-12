/**************************************************************************/
/*  relay_client.cpp                                                      */
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

#include "relay_client.h"

#include "protocol.h"
#include "signal_client.h"

#include "core/crypto/crypto_core.h"

#include <cstring>

Error RelayClient::send_datagram(int p_to_signal_id, const uint8_t *p_buffer, int p_len) {
	ERR_FAIL_NULL_V(signal, ERR_UNCONFIGURED);
	ERR_FAIL_COND_V(!GamesEnetWebrtcProtocol::is_valid_signal_id(p_to_signal_id), ERR_INVALID_PARAMETER);
	ERR_FAIL_COND_V(p_len <= 0 || p_len > GamesEnetWebrtcProtocol::MAX_RELAY_BYTES, ERR_INVALID_PARAMETER);
	Vector<uint8_t> raw;
	raw.resize(p_len);
	if (p_len > 0) {
		memcpy(raw.ptrw(), p_buffer, p_len);
	}
	Dictionary data;
	data["to"] = p_to_signal_id;
	data["payload"] = CryptoCore::b64_encode_str(raw.ptr(), raw.size());
	return signal->send_message(GamesEnetWebrtcProtocol::MSG_RELAY_DATAGRAM, data, p_to_signal_id);
}

bool RelayClient::decode_datagram(const Dictionary &p_data, int &r_from, Vector<uint8_t> &r_payload) {
	r_from = int(p_data.get("from", 0));
	if (r_from == 0) {
		r_from = int(p_data.get("id", 0));
	}
	const String b64 = String(p_data.get("payload", ""));
	if (b64.is_empty()) {
		return false;
	}
	const CharString cs = b64.ascii();
	size_t decoded_size = 0;
	r_payload.resize(cs.length());
	if (CryptoCore::b64_decode(r_payload.ptrw(), r_payload.size(), &decoded_size, (const uint8_t *)cs.get_data(), cs.length()) != OK) {
		r_payload.clear();
		return false;
	}
	r_payload.resize((int)decoded_size);
	if (!GamesEnetWebrtcProtocol::is_valid_signal_id(r_from) || !GamesEnetWebrtcProtocol::is_valid_relay_payload(r_payload)) {
		r_payload.clear();
		return false;
	}
	return true;
}
