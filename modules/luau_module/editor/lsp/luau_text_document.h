/**************************************************************************/
/*  luau_text_document.h                                                  */
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
#include "core/templates/hash_map.h"
#include "core/variant/array.h"
#include "core/variant/dictionary.h"

class LuauScriptLanguage;

class LuauTextDocument : public RefCounted {
	GDCLASS(LuauTextDocument, RefCounted);

	HashMap<String, String> open_documents;

protected:
	static void _bind_methods();

public:
	void didOpen(const Dictionary &p_params);
	void didClose(const Dictionary &p_params);
	void didChange(const Dictionary &p_params);
	void didSave(const Dictionary &p_params);
	Array completion(const Dictionary &p_params);
	Array definition(const Dictionary &p_params);
	Variant hover(const Dictionary &p_params);
	Array documentSymbol(const Dictionary &p_params);
	Array formatting(const Dictionary &p_params);

	void initialize();
	void did_open(const String &p_uri, const String &p_text);
	void did_change(const String &p_uri, const String &p_text);
	void did_save(const String &p_uri, const String &p_text);
	void did_close(const String &p_uri);
	void push_diagnostics(const String &p_uri);

	String get_text(const String &p_uri) const;
	Array publish_diagnostics(const String &p_uri) const;
	Array complete_at(const String &p_uri, int p_line, int p_character) const;
	Dictionary lookup_definition(const String &p_uri, const String &p_symbol) const;
};
