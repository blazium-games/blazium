/**************************************************************************/
/*  session.cpp                                                           */
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

#include "common/protocol/session.h"

namespace coldstorage {

Message makeProtocolRequest(const ProtocolInfo &info) {
	Message msg;
	msg.func = "protocol";
	msg.args["version"] = info.version;
	msg.args["bufsize"] = info.bufsize;
	msg.args["capabilities"] = info.capabilities;
	msg.args["client_version"] = info.clientVersion;
	msg.args["type"] = "request";
	return msg;
}

Message makeProtocolAck(const ProtocolInfo &info) {
	Message msg;
	msg.func = "protocol";
	msg.args["version"] = info.version;
	msg.args["bufsize"] = info.bufsize;
	msg.args["capabilities"] = info.capabilities;
	msg.args["server_version"] = coldstorage::version::FULL;
	msg.args["min_client_version"] = coldstorage::version::MIN_CLIENT;
	msg.args["type"] = "ack";
	return msg;
}

} //namespace coldstorage
