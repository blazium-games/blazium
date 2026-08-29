/**************************************************************************/
/*  autorun_inf.h                                                         */
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

#include "core/object/ref_counted.h"
#include "core/string/ustring.h"
#include "core/variant/dictionary.h"

class AutorunInf : public RefCounted {
	GDCLASS(AutorunInf, RefCounted);

	String label;
	String icon;
	String open;
	String action;
	String shell;
	Dictionary shell_verbs;

protected:
	static void _bind_methods();

public:
	void set_label(const String &p_label) { label = p_label; }
	String get_label() const { return label; }
	void set_icon(const String &p_icon) { icon = p_icon; }
	String get_icon() const { return icon; }
	void set_open(const String &p_open) { open = p_open; }
	String get_open() const { return open; }
	void set_action(const String &p_action) { action = p_action; }
	String get_action() const { return action; }
	void set_shell(const String &p_shell) { shell = p_shell; }
	String get_shell() const { return shell; }
	void set_shell_verbs(const Dictionary &p_verbs) { shell_verbs = p_verbs; }
	Dictionary get_shell_verbs() const { return shell_verbs; }

	String build() const;
	static String usb_note_text();
};
