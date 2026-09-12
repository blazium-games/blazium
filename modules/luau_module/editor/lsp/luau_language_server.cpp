/**************************************************************************/
/*  luau_language_server.cpp                                              */
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

#ifdef TOOLS_ENABLED

#include "editor/lsp/luau_language_server.h"

#include "luau_script_language.h"

#include "core/config/engine.h"
#include "core/os/os.h"
#include "editor/editor_node.h"

void LuauLanguageServerPlugin::_bind_methods() {
}

void LuauLanguageServerPlugin::start_server() {
	if (started) {
		return;
	}
#ifndef LUAU_NO_LSP
	if (protocol.start(port, IPAddress("127.0.0.1")) == OK) {
		started = true;
		set_process_internal(true);
	}
#endif
}

void LuauLanguageServerPlugin::stop_server() {
#ifndef LUAU_NO_LSP
	protocol.stop();
#endif
	started = false;
	set_process_internal(false);
}

void LuauLanguageServerPlugin::_notification(int p_what) {
	switch (p_what) {
		case NOTIFICATION_ENTER_TREE: {
			if (EditorNode::get_singleton()) {
				start_server();
			}
		} break;
		case NOTIFICATION_EXIT_TREE: {
			stop_server();
		} break;
		case NOTIFICATION_INTERNAL_PROCESS: {
#ifndef LUAU_NO_LSP
			if (started) {
				protocol.poll(8000);
			}
#endif
		} break;
	}
}

LuauLanguageServerPlugin::LuauLanguageServerPlugin() {
}

LuauLanguageServerPlugin::~LuauLanguageServerPlugin() {
}

#endif
