/**************************************************************************/
/*  multiuser_editor_security_sink.h                                      */
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

#include "core/string/ustring.h"

namespace multiuser_editor {

// Stable integer aliases for the dock's SecurityEventKind enum, exposed
// here so subsystems (network / interceptor / filesystem-sync / lock-manager)
// can record events without depending on multiuser_editor_dock.h. Values must
// stay byte-equal to MultiuserEditorDock::SecurityEventKind; tests pin this.
static constexpr int kEvtKindOther = 0;
static constexpr int kEvtKindAuthFail = 1;
static constexpr int kEvtKindAuthOk = 2;
static constexpr int kEvtKindThrottled = 3;
static constexpr int kEvtKindDropped = 4;
static constexpr int kEvtKindProtectedPath = 5;
static constexpr int kEvtKindMalformed = 6;
static constexpr int kEvtKindPreAuthDrop = 7;
static constexpr int kEvtKindUnknownAction = 8;
static constexpr int kEvtKindCRDTRefused = 9;
static constexpr int kEvtKindPermissionOverride = 10;
static constexpr int kEvtKindLockEvicted = 11;
static constexpr int kEvtKindReplicationFailed = 12;
static constexpr int kEvtKindPermissionDenied = 13;
static constexpr int kEvtKindInvalidPacket = 14;
static constexpr int kEvtKindRateLimited = 15;
static constexpr int kEvtKindAuthFailed = 16;
static constexpr int kEvtKindAdminKick = 17;

// Stable integer aliases for MultiuserEditorPlugin::LogLevel.
static constexpr int kEvtLogError = 0;
static constexpr int kEvtLogWarn = 1;
static constexpr int kEvtLogInfo = 2;
static constexpr int kEvtLogDebug = 3;

// Stable integer aliases for MultiuserEditorPlugin::LogCategory.
static constexpr int kEvtCatGeneral = 0;
static constexpr int kEvtCatReplication = 1;
static constexpr int kEvtCatFilesystem = 2;
static constexpr int kEvtCatCRDT = 3;
static constexpr int kEvtCatNetwork = 4;
static constexpr int kEvtCatPermissions = 5;

// Lightweight callback owned by the plugin and forwarded to subsystems
// (network / interceptor / filesystem-sync / lock-manager) so they can
// route their own drop/refusal events through the dock's security ring
// without taking a hard dependency on MultiuserEditorPlugin or the dock.
//
// The thunk on the plugin side translates `kind` to MultiuserEditorDock::SecurityEventKind,
// `level` to MultiuserEditorPlugin::LogLevel, and `category` to MultiuserEditorPlugin::LogCategory.
using SecurityEventSinkFn = void (*)(void *user, int kind, int level, int category, const String &message);

struct SecuritySink {
	SecurityEventSinkFn fn = nullptr;
	void *user = nullptr;

	void record(int kind, int level, int category, const String &message) const {
		if (fn) {
			fn(user, kind, level, category, message);
		}
	}
};

} // namespace multiuser_editor

#endif // TOOLS_ENABLED
