/**************************************************************************/
/*  gdk_xbox_services.h                                                   */
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

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifdef XBOX_MODULE_GDK_ENABLED
#include "gdk_windows.h"
#endif

#include "gdk_gdk_stubs.h"

#include <vector>

#include "core/string/ustring.h"
#include "core/variant/variant.h"

#ifdef XBOX_MODULE_GDK_ENABLED
#include <XGame.h>
#include <XUser.h>
#include <xsapi-c/services_c.h>
#endif

class GDKResult;
class GDKUser;

class GDKXboxServices {
	struct UserContextState {
		XUserLocalId local_id = {};
		uint64_t xbox_user_id = 0;
		XblContextHandle context = nullptr;
	};

	bool m_initialized = false;
	uint32_t m_title_id = 0;
	String m_scid;
	std::vector<UserContextState> m_user_contexts;

	static String _build_default_scid(uint32_t p_title_id);
	static String _extract_scid_override(const Variant &p_config);
	UserContextState *_find_user_context(XUserLocalId p_local_id);
	HRESULT _ensure_user_context(const Ref<GDKUser> &p_user, UserContextState **r_context_state);

public:
	GDKXboxServices() = default;
	~GDKXboxServices();

	Ref<GDKResult> initialize(XTaskQueueHandle p_queue, const Variant &p_config);
	void shutdown();

	bool is_initialized() const;
	uint32_t get_title_id() const;
	String get_scid() const;

	HRESULT get_xbox_user_id(const Ref<GDKUser> &p_user, uint64_t *r_xbox_user_id);
	HRESULT duplicate_context_for_user(const Ref<GDKUser> &p_user, XblContextHandle *r_context, uint64_t *r_xbox_user_id = nullptr);
	void forget_user(XUserLocalId p_local_id);
};
