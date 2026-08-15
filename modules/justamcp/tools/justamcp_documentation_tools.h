/**************************************************************************/
/*  justamcp_documentation_tools.h                                        */
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

#include "core/doc_data.h"
#include "core/object/object.h"

class DocTools;

class JustAMCPDocumentationTools : public Object {
	GDCLASS(JustAMCPDocumentationTools, Object);

	DocTools *_get_doc_tools() const;
	bool _resolve_class_name(const String &p_class_name, String &r_class_name) const;
	bool _matches_query(const String &p_value, const String &p_query) const;
	Dictionary _class_summary(const DocData::ClassDoc &p_doc) const;
	Dictionary _class_details(const DocData::ClassDoc &p_doc, bool p_include_members) const;
	Dictionary _find_member(const DocData::ClassDoc &p_doc, const String &p_member_type, const String &p_member_name) const;

protected:
	static void _bind_methods();

public:
	Dictionary execute_tool(const String &p_tool_name, const Dictionary &p_args);

	Dictionary list_classes(const Dictionary &p_args);
	Dictionary search_documentation(const Dictionary &p_args);
	Dictionary get_class_documentation(const Dictionary &p_args);
	Dictionary get_member_documentation(const Dictionary &p_args);

	JustAMCPDocumentationTools();
	~JustAMCPDocumentationTools();
};

#endif
