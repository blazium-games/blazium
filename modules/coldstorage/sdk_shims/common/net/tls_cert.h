/**************************************************************************/
/*  tls_cert.h                                                            */
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
#include <ctime>
#include <optional>
#include <string>

namespace coldstorage {

std::string normalizeFingerprint(const std::string &fp);
std::string getCertFingerprint(const std::string &certPath);

bool generateSelfSignedCert(const std::string &certPath, const std::string &keyPath,
		const std::string &commonName, int validDays);

bool generateCaCert(const std::string &caCertPath, const std::string &caKeyPath, int validDays);
bool generateSignedCert(const std::string &caCertPath, const std::string &caKeyPath,
		const std::string &subjectCn, const std::string &certPath,
		const std::string &keyPath, int validDays, bool isClientCert);

std::optional<std::time_t> getCertExpiry(const std::string &certPath);
int daysUntilExpiry(const std::string &certPath);
bool isCertExpired(const std::string &certPath);
std::string formatCertExpiryDate(const std::string &certPath);

} //namespace coldstorage
