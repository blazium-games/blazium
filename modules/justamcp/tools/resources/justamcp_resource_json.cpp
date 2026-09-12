/**************************************************************************/
/*  justamcp_resource_json.cpp                                            */
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

#include "justamcp_resource_json.h"

#include "core/io/json.h"

namespace JustAMCPResourceJson {

Dictionary make_json_contents(const String &p_uri, const Dictionary &p_payload) {
	Dictionary result;
	result["ok"] = true;
	Array contents;
	Dictionary content;
	content["uri"] = p_uri;
	content["mimeType"] = "application/json";
	content["text"] = JSON::stringify(p_payload, "\t");
	contents.push_back(content);
	result["contents"] = contents;
	return result;
}

Dictionary make_json_error(const String &p_uri, const String &p_error) {
	Dictionary result;
	result["ok"] = false;
	result["error"] = p_error;
	result["uri"] = p_uri;
	return result;
}

Dictionary make_text_contents(const String &p_uri, const String &p_text, const String &p_mime_type) {
	Dictionary result;
	result["ok"] = true;
	Array contents;
	Dictionary content;
	content["uri"] = p_uri;
	content["mimeType"] = p_mime_type;
	content["text"] = p_text;
	contents.push_back(content);
	result["contents"] = contents;
	return result;
}

} //namespace JustAMCPResourceJson

#endif
