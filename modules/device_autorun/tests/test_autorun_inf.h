/**************************************************************************/
/*  test_autorun_inf.h                                                    */
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

#include "modules/device_autorun/autorun_inf.h"
#include "tests/test_macros.h"

TEST_CASE("[Modules][DeviceAutorun] builds autorun.inf") {
	Ref<AutorunInf> inf;
	inf.instantiate();
	inf->set_open("MyGame.exe");
	inf->set_icon("MyGame.exe,0");
	inf->set_action("Start My Game");
	inf->set_label("My Game");
	inf->set_shell("play");
	Dictionary verbs;
	verbs["play"] = "MyGame.exe";
	inf->set_shell_verbs(verbs);

	const String text = inf->build();
	CHECK(text.contains("[autorun]"));
	CHECK(text.contains("open=MyGame.exe"));
	CHECK(text.contains("icon=MyGame.exe,0"));
	CHECK(text.contains("action=Start My Game"));
	CHECK(text.contains("label=My Game"));
	CHECK(text.contains("shell=play"));
	CHECK(text.contains("shell\\play\\command=MyGame.exe"));
	CHECK(AutorunInf::usb_note_text().contains("Windows 7"));
}
