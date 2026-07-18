/**************************************************************************/
/*  justamcp_request_router.h                                             */
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

#include "core/os/mutex.h"
#include "core/string/ustring.h"
#include "core/templates/hash_map.h"
#include "core/variant/variant.h"

class JustAMCPRequestRouter {
	struct RouteEntry {
		String session_id;
		int connection_id = -1;
		uint64_t created_usec = 0;
	};

	mutable Mutex mutex;
	HashMap<String, RouteEntry> routes;

public:
	static String request_id_key(const Variant &p_request_id);
	static bool request_ids_equal(const Variant &p_a, const Variant &p_b);

	void bind(const Variant &p_request_id, const String &p_session_id, int p_connection_id);
	bool lookup(const Variant &p_request_id, String &r_session_id, int &r_connection_id) const;
	void clear(const Variant &p_request_id);
	void clear_for_connection(int p_connection_id);
	void prune_expired(uint64_t p_ttl_usec = 3600000000);
	void clear_all();

#ifdef TESTS_ENABLED
	void test_backdate_route(const Variant &p_request_id, uint64_t p_age_usec);
	void test_set_route_created_usec(const Variant &p_request_id, uint64_t p_created_usec);
#endif
};
