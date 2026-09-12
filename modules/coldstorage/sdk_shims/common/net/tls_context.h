/**************************************************************************/
/*  tls_context.h                                                         */
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
#include <string>

namespace coldstorage {

class TLSContext {
public:
	enum class Mode { Server,
		Client };

	TLSContext();
	~TLSContext();

	TLSContext(const TLSContext &) = delete;
	TLSContext &operator=(const TLSContext &) = delete;

	static void initLibraries();

	bool configureServer(const std::string &certFile, const std::string &keyFile,
			const std::string &caFile = "");
	bool configureClient(const std::string &caFile, bool verifyPeer,
			const std::string &certFile = "",
			const std::string &keyFile = "");

	void *native() const { return conf_; }
	Mode mode() const { return mode_; }

	const std::string &caFile() const { return ca_file_; }
	bool verifyPeer() const { return verify_peer_; }
	const std::string &certFile() const { return cert_file_; }
	const std::string &keyFile() const { return key_file_; }

private:
	void freeContext();

	void *conf_ = nullptr;
	void *cacert_ = nullptr;
	void *clicert_ = nullptr;
	void *pkey_ = nullptr;
	Mode mode_ = Mode::Server;
	bool verify_peer_ = true;
	std::string ca_file_;
	std::string cert_file_;
	std::string key_file_;
};

} //namespace coldstorage
