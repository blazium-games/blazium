/**************************************************************************/
/*  autorun_inf.cpp                                                       */
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

#include "autorun_inf.h"

#include "core/object/class_db.h"
#include "core/variant/variant.h"

void AutorunInf::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_label", "label"), &AutorunInf::set_label);
	ClassDB::bind_method(D_METHOD("get_label"), &AutorunInf::get_label);
	ClassDB::bind_method(D_METHOD("set_icon", "icon"), &AutorunInf::set_icon);
	ClassDB::bind_method(D_METHOD("get_icon"), &AutorunInf::get_icon);
	ClassDB::bind_method(D_METHOD("set_open", "open"), &AutorunInf::set_open);
	ClassDB::bind_method(D_METHOD("get_open"), &AutorunInf::get_open);
	ClassDB::bind_method(D_METHOD("set_action", "action"), &AutorunInf::set_action);
	ClassDB::bind_method(D_METHOD("get_action"), &AutorunInf::get_action);
	ClassDB::bind_method(D_METHOD("set_shell", "shell"), &AutorunInf::set_shell);
	ClassDB::bind_method(D_METHOD("get_shell"), &AutorunInf::get_shell);
	ClassDB::bind_method(D_METHOD("set_shell_verbs", "verbs"), &AutorunInf::set_shell_verbs);
	ClassDB::bind_method(D_METHOD("get_shell_verbs"), &AutorunInf::get_shell_verbs);
	ClassDB::bind_method(D_METHOD("build"), &AutorunInf::build);
	ClassDB::bind_static_method("AutorunInf", D_METHOD("usb_note_text"), &AutorunInf::usb_note_text);

	ADD_PROPERTY(PropertyInfo(Variant::STRING, "label"), "set_label", "get_label");
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "icon"), "set_icon", "get_icon");
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "open"), "set_open", "get_open");
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "action"), "set_action", "get_action");
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "shell"), "set_shell", "get_shell");
	ADD_PROPERTY(PropertyInfo(Variant::DICTIONARY, "shell_verbs"), "set_shell_verbs", "get_shell_verbs");
}

String AutorunInf::build() const {
	String text = "[autorun]\r\n";
	if (!open.is_empty()) {
		text += "open=" + open + "\r\n";
	}
	if (!icon.is_empty()) {
		text += "icon=" + icon + "\r\n";
	}
	if (!action.is_empty()) {
		text += "action=" + action + "\r\n";
	}
	if (!label.is_empty()) {
		text += "label=" + label + "\r\n";
	}
	if (!shell.is_empty()) {
		text += "shell=" + shell + "\r\n";
	}

	const Array keys = shell_verbs.keys();
	for (int i = 0; i < keys.size(); i++) {
		const String verb = String(keys[i]);
		const String command = String(shell_verbs.get(verb, String()));
		if (verb.is_empty() || command.is_empty()) {
			continue;
		}
		text += "shell\\" + verb + "=" + verb + "\r\n";
		text += "shell\\" + verb + "\\command=" + command + "\r\n";
	}
	return text;
}

String AutorunInf::usb_note_text() {
	return String(
			"This disc/drive was packaged with autorun.inf.\r\n"
			"\r\n"
			"CD/DVD: Windows may still honor OPEN= and launch the listed program.\r\n"
			"USB flash drives on Windows 7 and later ignore OPEN= and ACTION=.\r\n"
			"LABEL= and ICON= still apply. Start the program from the drive root in Explorer.\r\n");
}
