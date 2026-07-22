/**************************************************************************/
/*  justamcp_editor_filesystem.cpp                                        */
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

#include "justamcp_editor_filesystem.h"

#ifdef TOOLS_ENABLED
#include "core/os/thread.h"
#include "editor/editor_file_system.h"
#endif

namespace JustAMCPEditorFilesystem {

void refresh_path(const String &p_path) {
#ifdef TOOLS_ENABLED
	if (p_path.is_empty() || !EditorFileSystem::get_singleton()) {
		return;
	}
	// EditorFileSystem is a Node; never call it off the main thread.
	if (Thread::is_main_thread()) {
		EditorFileSystem::get_singleton()->update_file(p_path);
	} else {
		EditorFileSystem::get_singleton()->call_deferred(SNAME("update_file"), p_path);
	}
#else
	(void)p_path;
#endif
}

} //namespace JustAMCPEditorFilesystem
