/**************************************************************************/
/*  tiled_dictionary_builder.cpp                                          */
/**************************************************************************/
/*                         This file is part of:                          */
/*                             BLAZIUM ENGINE                             */
/*                          https://blazium.app                           */
/**************************************************************************/
/* Copyright (c) 2024-present Blazium Engine contributors.                */
/* Copyright (c) 2014-present Godot Engine contributors (see AUTHORS.md). */
/* Copyright (c) 2007-2014 Juan Linietsky, Ariel Manzur.                  */
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

#include "tiled_dictionary_builder.h"
#include "core/io/json.h"
#include "tiled_xml_parser.h"

Dictionary TiledDictionaryBuilder::get_dictionary(const PackedByteArray &p_tiled_file_content, const String &p_source_file) {
	String extension = p_source_file.get_extension().to_lower();
	bool is_xml = false;
	bool is_json = false;

	if (extension == "tmx" || extension == "tsx" || extension == "xml" || extension == "tx") {
		is_xml = true;
	} else if (extension == "tmj" || extension == "tsj" || extension == "json" || extension == "tj" || extension == "tiled-project") {
		is_json = true;
	} else {
		PackedByteArray sliced = p_tiled_file_content.slice(0, 50);
		String chunk = String::utf8((const char *)sliced.ptr(), sliced.size());
		if (chunk.begins_with("<?xml")) {
			is_xml = true;
		} else if (chunk.begins_with("{")) {
			is_json = true;
		} else {
			print_line("DEBUG CHUNK: '" + chunk + "'");
		}
	}

	if (is_xml) {
		TiledXMLParser parser;
		return parser.parse_xml_buffer(p_tiled_file_content, p_source_file);
	} else if (is_json) {
		Ref<JSON> json = memnew(JSON);
		if (json->parse(String::utf8((const char *)p_tiled_file_content.ptr(), p_tiled_file_content.size())) == OK) {
			return json->get_data();
		}
	} else {
		ERR_PRINT("ERROR: File '" + p_source_file + "' has an unknown type. -> Continuing but result may be unusable");
	}

	return Dictionary();
}
