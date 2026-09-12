/**************************************************************************/
/*  justamcp_log_levels.cpp                                               */
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

#include "justamcp_log_levels.h"

String justamcp_log_level_normalize(const String &p_level) {
	return p_level.strip_edges().to_lower();
}

int justamcp_log_level_rank(const String &p_level) {
	const String v = justamcp_log_level_normalize(p_level);
	if (v == "debug") {
		return 0;
	}
	if (v == "info") {
		return 1;
	}
	if (v == "notice") {
		return 2;
	}
	if (v == "warning" || v == "warn") {
		return 3;
	}
	if (v == "error" || v == "err") {
		return 4;
	}
	if (v == "critical" || v == "crit") {
		return 5;
	}
	if (v == "alert") {
		return 6;
	}
	if (v == "emergency" || v == "emerg") {
		return 7;
	}
	return -1;
}

bool justamcp_log_level_is_valid(const String &p_level) {
	return justamcp_log_level_rank(p_level) >= 0;
}

String justamcp_log_level_canonical(const String &p_level) {
	const String v = justamcp_log_level_normalize(p_level);
	if (v == "warn") {
		return "warning";
	}
	if (v == "err") {
		return "error";
	}
	if (v == "crit") {
		return "critical";
	}
	if (v == "emerg") {
		return "emergency";
	}
	return v;
}

bool justamcp_log_level_passes(const String &p_message_level, const String &p_min_level) {
	const int message_rank = justamcp_log_level_rank(p_message_level);
	const int min_rank = justamcp_log_level_rank(p_min_level);
	if (message_rank < 0 || min_rank < 0) {
		return false;
	}
	return message_rank >= min_rank;
}
