/**************************************************************************/
/*  tiled_xml_parser.h                                                    */
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

#pragma once

#include "core/io/xml_parser.h"

// Equivalent to DictionaryFromXml.gd and XmlParserCtrl.gd

class TiledXMLParser {
private:
	Ref<XMLParser> _parser;
	String _parsed_file_name;
	String _current_element;
	int _current_group_level = 0;
	Dictionary _result;
	Dictionary _current_dictionary;
	Array _current_array;
	bool _csv_encoded = true;
	bool _is_map = false;
	bool _in_tileset = false;

	String next_element();
	bool is_end();
	bool is_empty();
	String get_data();
	Dictionary get_attributes();
	Error parse_on();

	Error simple_element(const String &element_name, const Dictionary &attribs);
	Error nested_element(const String &element_name, const Dictionary &attribs);
	void insert_attributes(Dictionary &target_dictionary, const Dictionary &attribs);

public:
	Dictionary parse_xml_file(const String &p_path, Error *r_err = nullptr);
	Dictionary parse_xml_buffer(const PackedByteArray &p_buffer, const String &p_path, Error *r_err = nullptr);

	TiledXMLParser();
};
