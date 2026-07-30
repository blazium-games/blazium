/**************************************************************************/
/*  framing.cpp                                                           */
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

#include "common/protocol/framing.h"
#include "common/net/plain_transport.h"
#include "common/util/net_trace.h"

#include <climits>
#include <cstring>
#include <iostream>

namespace coldstorage {

bool writeFrame(StreamTransport &transport, const std::vector<uint8_t> &payload,
		size_t maxFrameSize) {
	CS_NET_TRACE("Framing", "writeFrame begin bytes=" << payload.size());
	if (payload.size() > maxFrameSize) {
		std::cerr << "[framing] writeFrame rejected: payload size "
				  << payload.size() << " exceeds maxFrameSize " << maxFrameSize
				  << ". Use chunked transfer for large payloads." << std::endl;
		return false;
	}
	if (payload.size() > UINT32_MAX) {
		std::cerr << "[framing] writeFrame rejected: payload size "
				  << payload.size() << " exceeds UINT32_MAX (4GB frame limit). "
				  << "Use chunked transfer for large payloads." << std::endl;
		return false;
	}

	uint32_t len = static_cast<uint32_t>(payload.size());
	uint8_t header[4];
	header[0] = static_cast<uint8_t>((len >> 24) & 0xFF);
	header[1] = static_cast<uint8_t>((len >> 16) & 0xFF);
	header[2] = static_cast<uint8_t>((len >> 8) & 0xFF);
	header[3] = static_cast<uint8_t>((len) & 0xFF);

	if (!transport.writeAll(header, 4)) {
		CS_NET_TRACE("Framing", "writeFrame header failed");
		return false;
	}
	if (!payload.empty() && !transport.writeAll(payload.data(), payload.size())) {
		CS_NET_TRACE("Framing", "writeFrame payload failed");
		return false;
	}
	CS_NET_TRACE("Framing", "writeFrame ok bytes=" << payload.size());
	return true;
}

bool readFrame(StreamTransport &transport, std::vector<uint8_t> &payload,
		size_t maxFrameSize) {
	CS_NET_TRACE("Framing", "readFrame begin max=" << maxFrameSize);
	uint8_t header[4];
	if (!transport.readAll(header, 4)) {
		CS_NET_TRACE("Framing", "readFrame header failed");
		return false;
	}

	uint32_t len = (static_cast<uint32_t>(header[0]) << 24) |
			(static_cast<uint32_t>(header[1]) << 16) |
			(static_cast<uint32_t>(header[2]) << 8) |
			(static_cast<uint32_t>(header[3]));

	if (static_cast<size_t>(len) > maxFrameSize) {
		std::cerr << "[framing] readFrame rejected: frame length "
				  << len << " bytes exceeds maxFrameSize "
				  << maxFrameSize << " bytes" << std::endl;
		return false;
	}

	payload.resize(len);
	if (len > 0 && !transport.readAll(payload.data(), len)) {
		CS_NET_TRACE("Framing", "readFrame payload failed len=" << len);
		return false;
	}
	CS_NET_TRACE("Framing", "readFrame ok bytes=" << len);
	return true;
}

bool writeFrame(socket_t sock, const std::vector<uint8_t> &payload, size_t maxFrameSize) {
	PlainSocketTransport transport(sock);
	return writeFrame(static_cast<StreamTransport &>(transport), payload, maxFrameSize);
}

bool readFrame(socket_t sock, std::vector<uint8_t> &payload, size_t maxFrameSize) {
	PlainSocketTransport transport(sock);
	return readFrame(static_cast<StreamTransport &>(transport), payload, maxFrameSize);
}

} //namespace coldstorage
