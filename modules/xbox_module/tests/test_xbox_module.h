/**************************************************************************/
/*  test_xbox_module.h                                                    */
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

#include "tests/test_macros.h"

#include "../editor/gdk_toolchain.h"
#include "../editor/microsoft_game_config.h"
#include "../gdk/gdk.h"
#include "../gdk/gdk_result.h"

namespace TestXboxModule {

TEST_CASE("[XboxModule] MicrosoftGameConfig injects TargetDeviceFamily") {
	const String input =
			"<Game><ExecutableList><Executable Name=\"game.exe\" Id=\"Game\" /></ExecutableList></Game>";
	const String output = MicrosoftGameConfig::inject_target_device_family(input, "PC");
	CHECK(output.contains("TargetDeviceFamily=\"PC\""));
}

TEST_CASE("[XboxModule] MicrosoftGameConfig reads executable name") {
	const String content = "<Executable Name=\"MyGame.exe\" TargetDeviceFamily=\"PC\" />";
	CHECK(MicrosoftGameConfig::read_executable_name_from_content(content) == "MyGame.exe");
}

TEST_CASE("[XboxModule] MicrosoftGameConfig template contains executable") {
	const String xml = MicrosoftGameConfig::get_template_xml("test.exe");
	CHECK(xml.contains("Name=\"test.exe\""));
	CHECK(xml.contains("<TitleId>"));
}

TEST_CASE("[XboxModule] GDKToolchain constructs") {
	Ref<GDKToolchain> toolchain = GDKToolchain::create();
	CHECK(toolchain.is_valid());
}

TEST_CASE("[XboxModule] GDK runtime availability and initialize stub") {
	GDK *gdk = GDK::get_singleton();
	CHECK(gdk != nullptr);
#ifdef XBOX_MODULE_GDK_ENABLED
	CHECK(gdk->is_available());
	Ref<GDKResult> init_result = gdk->initialize();
	CHECK(init_result.is_valid());
	gdk->shutdown();
#else
	CHECK_FALSE(gdk->is_available());
	Ref<GDKResult> init_result = gdk->initialize();
	CHECK(init_result.is_valid());
	CHECK_FALSE(init_result->is_ok());
#endif
}

TEST_CASE("[XboxModule] GDK dispatch returns int") {
	GDK *gdk = GDK::get_singleton();
	CHECK(gdk != nullptr);
	CHECK(gdk->dispatch() >= 0);
}

} //namespace TestXboxModule
