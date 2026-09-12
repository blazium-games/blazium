/**************************************************************************/
/*  net_trace.h                                                           */
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

#include <atomic>
#include <chrono>
#include <cstdlib>
#include <iostream>
#include <sstream>
#include <string>
#include <thread>

#ifdef _WIN32
#include <winsock2.h>
#else
#include <unistd.h>
#include <fstream>
#endif

namespace coldstorage {

inline bool csNetTraceEnabled() {
	static const bool enabled = []() {
		const char *value = std::getenv("COLDSTORAGE_TEST_TRACE");
		if (value == nullptr) {
			return false;
		}
		return value[0] != '0' && value[0] != '\0';
	}();
	return enabled;
}

inline std::string csNetTraceThreadId() {
	std::ostringstream oss;
	oss << std::this_thread::get_id();
	return oss.str();
}

inline long long csNetTraceElapsedMs() {
	using clock = std::chrono::steady_clock;
	static const clock::time_point start = clock::now();
	return std::chrono::duration_cast<std::chrono::milliseconds>(clock::now() - start).count();
}

inline int csNetTraceProcessThreadCount() {
#ifdef _WIN32
	return -1;
#else
	std::ifstream status("/proc/self/status");
	if (!status.is_open()) {
		return -1;
	}
	std::string line;
	while (std::getline(status, line)) {
		if (line.rfind("Threads:", 0) == 0) {
			return std::stoi(line.substr(8));
		}
	}
	return -1;
#endif
}

inline void csNetTraceLine(const char *component, const std::string &message) {
	if (!csNetTraceEnabled()) {
		return;
	}
	const int threads = csNetTraceProcessThreadCount();
	std::cerr << "[TRACE +" << csNetTraceElapsedMs() << "ms"
			  << " component=" << component
			  << " tid=" << csNetTraceThreadId();
	if (threads >= 0) {
		std::cerr << " threads=" << threads;
	}
	std::cerr << "] " << message << std::endl;
}

inline void csNetTraceThreadSnapshot(const char *component) {
	if (!csNetTraceEnabled()) {
		return;
	}
#ifndef _WIN32
	std::ostringstream oss;
	oss << "thread snapshot";
	const int self = static_cast<int>(::getpid());
	int listed = 0;
	for (int tid = 1; tid < 100000; ++tid) {
		std::ifstream comm("/proc/self/task/" + std::to_string(tid) + "/comm");
		std::ifstream wchan("/proc/self/task/" + std::to_string(tid) + "/wchan");
		if (!comm.is_open()) {
			continue;
		}
		std::string name;
		std::getline(comm, name);
		std::string wait;
		if (wchan.is_open()) {
			std::getline(wchan, wait);
		}
		oss << " {tid=" << tid << " comm=" << name << " wchan=" << wait << "}";
		++listed;
		if (listed >= 8) {
			break;
		}
	}
	oss << " pid=" << self;
	csNetTraceLine(component, oss.str());
#else
	csNetTraceLine(component, "thread snapshot unavailable on Windows");
#endif
}

class CsNetTraceWatchdog {
public:
	explicit CsNetTraceWatchdog(const char *component) :
			component_(component), stop_(false) {
		if (!csNetTraceEnabled()) {
			return;
		}
		worker_ = std::thread([this]() {
			while (!stop_.load(std::memory_order_acquire)) {
				csNetTraceThreadSnapshot(component_);
				std::this_thread::sleep_for(std::chrono::seconds(1));
			}
		});
	}

	~CsNetTraceWatchdog() {
		stop_.store(true, std::memory_order_release);
		if (worker_.joinable()) {
			worker_.join();
		}
	}

	CsNetTraceWatchdog(const CsNetTraceWatchdog &) = delete;
	CsNetTraceWatchdog &operator=(const CsNetTraceWatchdog &) = delete;

private:
	const char *component_;
	std::atomic<bool> stop_;
	std::thread worker_;
};

#define CS_NET_TRACE(component, expr)                                      \
	do {                                                                   \
		if (::coldstorage::csNetTraceEnabled()) {                          \
			std::ostringstream _cs_trace_oss;                              \
			_cs_trace_oss << expr;                                         \
			::coldstorage::csNetTraceLine(component, _cs_trace_oss.str()); \
		}                                                                  \
	} while (0)

} //namespace coldstorage
