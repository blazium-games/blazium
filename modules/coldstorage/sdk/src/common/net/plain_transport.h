/**************************************************************************/
/*  plain_transport.h                                                     */
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

#include "common/net/stream_transport.h"

namespace coldstorage {

class PlainSocketTransport : public StreamTransport {
public:
	explicit PlainSocketTransport(socket_t sock);
	~PlainSocketTransport() override;

	PlainSocketTransport(const PlainSocketTransport &) = delete;
	PlainSocketTransport &operator=(const PlainSocketTransport &) = delete;

	bool readAll(uint8_t *data, size_t len) override;
	bool writeAll(const uint8_t *data, size_t len) override;
	void close() override;
	bool setRecvTimeout(int seconds) override;
	socket_t rawSocket() const override { return sock_; }

	socket_t release();

private:
	socket_t sock_;
};

TransportPtr makePlainTransport(socket_t sock);

} //namespace coldstorage
