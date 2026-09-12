/**************************************************************************/
/*  register_types.cpp                                                    */
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

#include "register_types.h"

#include "core/config/project_settings.h"
#include "rcon_client.h"
#include "rcon_packet.h"
#include "rcon_server.h"

void initialize_rcon_module(ModuleInitializationLevel p_level) {
	if (p_level == MODULE_INITIALIZATION_LEVEL_SCENE) {
		// RCONPacket is always available for low-level packet utilities
		GDREGISTER_CLASS(RCONPacket);

#ifdef TOOLS_ENABLED
		// Editor builds get both client and server for testing/development
		GDREGISTER_CLASS(RCONServer);
		GDREGISTER_CLASS(RCONClient);
#else
		// Runtime/template builds: check for dedicated_server custom feature
		bool is_dedicated_server = ProjectSettings::get_singleton()->has_custom_feature("dedicated_server");

		if (is_dedicated_server) {
			// Dedicated server builds only get RCONServer (Windows/Linux only)
#if defined(WINDOWS_ENABLED) || defined(LINUXBSD_ENABLED)
			GDREGISTER_CLASS(RCONServer);
#endif
		} else {
			// Client builds only get RCONClient
			GDREGISTER_CLASS(RCONClient);
		}
#endif
	}
}

void uninitialize_rcon_module(ModuleInitializationLevel p_level) {
	if (p_level == MODULE_INITIALIZATION_LEVEL_SCENE) {
		// Cleanup if needed
	}
}
