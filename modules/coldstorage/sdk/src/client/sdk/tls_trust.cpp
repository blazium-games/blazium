/**************************************************************************/
/*  tls_trust.cpp                                                         */
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

#include "client/sdk/tls_trust.h"
#include "common/net/tls_cert.h"

#include <iostream>

#ifdef _WIN32
#include <io.h>
#define isatty _isatty
#define fileno _fileno
#else
#include <unistd.h>
#endif

namespace coldstorage {

namespace {

bool fingerprintsMatch(const std::string &a, const std::string &b) {
	return normalizeFingerprint(a) == normalizeFingerprint(b);
}

} //namespace

TrustDecision verifyServerTrust(const std::string &host, int port,
		const std::string &fingerprint,
		const TrustOptions &opts,
		TrustStore &store,
		std::string *messageOut) {
	if (!opts.pinningEnabled || fingerprint.empty()) {
		return TrustDecision::Trusted;
	}

	if (!opts.expectedFingerprint.empty()) {
		if (!fingerprintsMatch(fingerprint, opts.expectedFingerprint)) {
			if (messageOut) {
				*messageOut = "Server fingerprint does not match --accept-fingerprint value";
			}
			return TrustDecision::Mismatch;
		}
		store.save(host, port, fingerprint);
		return TrustDecision::AcceptedNew;
	}

	auto existing = store.lookup(host, port);
	if (existing) {
		if (!fingerprintsMatch(fingerprint, existing->fingerprint)) {
			if (messageOut) {
				*messageOut = "Server fingerprint changed! Possible MITM.\n"
							  "  Expected: " +
						existing->fingerprint + "\n"
												"  Got:      " +
						fingerprint + "\n"
									  "Remove with: cstorage trust remove " +
						TrustStore::makeKey(host, port);
			}
			return TrustDecision::Mismatch;
		}
		store.save(host, port, fingerprint);
		return TrustDecision::Trusted;
	}

	if (opts.acceptNew) {
		store.save(host, port, fingerprint);
		return TrustDecision::AcceptedNew;
	}

	bool canPrompt = opts.interactive && isatty(fileno(stdin)) != 0;
	if (!canPrompt) {
		if (messageOut) {
			*messageOut = "Unknown server fingerprint. Use --accept-new or --accept-fingerprint for non-interactive use.";
		}
		return TrustDecision::Rejected;
	}

	std::string key = TrustStore::makeKey(host, port);
	std::cerr << "WARNING: Unknown server " << key << "\n";
	std::cerr << "TLS fingerprint: " << fingerprint << "\n";
	std::cerr << "Trust this server? [y]es / [n]o / [a]ccept and remember: ";
	std::cerr.flush();

	std::string line;
	if (!std::getline(std::cin, line)) {
		return TrustDecision::Rejected;
	}
	if (line.empty()) {
		return TrustDecision::Rejected;
	}

	char c = static_cast<char>(std::tolower(static_cast<unsigned char>(line[0])));
	if (c == 'y') {
		store.save(host, port, fingerprint);
		return TrustDecision::AcceptedNew;
	}
	if (c == 'a') {
		store.save(host, port, fingerprint);
		return TrustDecision::AcceptedNew;
	}
	return TrustDecision::Rejected;
}

} //namespace coldstorage
