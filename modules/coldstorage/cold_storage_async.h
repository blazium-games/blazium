/**************************************************************************/
/*  cold_storage_async.h                                                  */
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

#ifdef TOOLS_ENABLED

#include "cold_storage_settings.h"
#include "core/object/object_id.h"
#include "core/string/ustring.h"

class ColdStorageVCS;

struct ColdStorageConnectRequest {
	enum class Kind {
		CONNECT_JOB,
		TEST_JOB,
		STARTUP_JOB,
	};

	Kind kind = Kind::CONNECT_JOB;
	ColdStorageConnectionConfig cfg;
	String project_path;
	bool validate = true;
	bool auto_pull = false;
	ObjectID caller_id;
	String complete_method; // method(bool ok, String error, int kind) on caller
};

// Runs connect/login/(validate)/(syncAll) on a worker thread using a bare SDK client.
// Completion is delivered via call_deferred to the caller Object.
bool cold_storage_begin_connect_async(const ColdStorageConnectRequest &p_request);

// True while a connect job is in flight (UI/startup busy guard).
bool cold_storage_connect_busy();

// Discard a pending connected client after async completion (main thread only).
void cold_storage_discard_connected_client();

// True if async completion left a connected client ready to adopt.
bool cold_storage_has_pending_client();

// Move a pending connected client into p_vcs (main thread only). Clears busy/pending.
bool cold_storage_adopt_connected_client_into(ColdStorageVCS *p_vcs, const ColdStorageConnectionConfig &p_cfg, const String &p_project_path);

#endif
