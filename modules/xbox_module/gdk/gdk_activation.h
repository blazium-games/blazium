/**************************************************************************/
/*  gdk_activation.h                                                      */
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

#include "gdk_gdk_stubs.h"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifdef XBOX_MODULE_GDK_ENABLED
#include "gdk_windows.h"
#endif

#include <cstdint>
#include <functional>
#include <vector>

#include "core/object/class_db.h"
#include "core/object/ref_counted.h"
#include "core/string/ustring.h"
#include "core/variant/dictionary.h"

#ifdef XBOX_MODULE_GDK_ENABLED
#include <XGameActivation.h>
#endif

class GDK;
class GDKResult;
class GDKRuntime;

class GDKActivation : public RefCounted {
	GDCLASS(GDKActivation, RefCounted);

	struct ActivationListener {
		uint64_t id = 0;
		std::function<void(const Dictionary &)> callback;
	};

	GDK *m_owner = nullptr;
	bool m_runtime_ready = false;
	bool m_activation_registered = false;
	uint64_t m_next_activation_listener_id = 1;
	XTaskQueueRegistrationToken m_activation_token = {};
	std::vector<ActivationListener> m_activation_listeners;

	static void CALLBACK _activation_callback(void *p_context, const XGameActivationInfo *p_activation_info);
	void notify_activation_listeners_internal(const Dictionary &p_info);

protected:
	static void _bind_methods();

public:
	enum ActivationType {
		ACTIVATION_TYPE_PROTOCOL = static_cast<uint32_t>(XGameActivationType::Protocol),
		ACTIVATION_TYPE_FILE = static_cast<uint32_t>(XGameActivationType::File),
		ACTIVATION_TYPE_PENDING_GAME_INVITE = static_cast<uint32_t>(XGameActivationType::PendingGameInvite),
		ACTIVATION_TYPE_ACCEPTED_GAME_INVITE = static_cast<uint32_t>(XGameActivationType::AcceptedGameInvite),
	};

	void set_owner(GDK *p_owner);

	Ref<GDKResult> on_runtime_initialized();
	void shutdown();

	Ref<GDKResult> accept_pending_invite(const String &p_invite_uri);

	void handle_activation_internal(const XGameActivationInfo *p_activation_info);
	uint64_t add_activation_listener(std::function<void(const Dictionary &)> p_callback);
	void remove_activation_listener(uint64_t p_listener_id);

	static Dictionary make_invite_dictionary_internal(const String &p_uri, const String &p_activation_type);
};

VARIANT_ENUM_CAST(GDKActivation::ActivationType);
