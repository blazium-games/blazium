/**************************************************************************/
/*  tiled_xml_parser.cpp                                                  */
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

#include "tiled_xml_parser.h"
#include "core/io/file_access.h"

// Note: This implements YATI's `DictionaryFromXml.gd` and `XmlParserCtrl.gd` natively.

TiledXMLParser::TiledXMLParser() {
	_parser.instantiate();
}

Error TiledXMLParser::parse_on() {
	Error err = _parser->read();
	if (err != OK && err != ERR_FILE_EOF) {
		ERR_PRINT("Error parsing file '" + _parsed_file_name + "' (around line " + itos(_parser->get_current_line()) + ").");
	}
	return err;
}

String TiledXMLParser::next_element() {
	Error err = parse_on();
	if (err != OK) {
		return "";
	}

	if (_parser->get_node_type() == XMLParser::NODE_TEXT) {
		String text = _parser->get_node_data().strip_edges();
		if (text.length() > 0) {
			return "<data>";
		}
	}

	while (_parser->get_node_type() != XMLParser::NODE_ELEMENT && _parser->get_node_type() != XMLParser::NODE_ELEMENT_END) {
		err = parse_on();
		if (err != OK) {
			return "";
		}
	}
	return _parser->get_node_name();
}

bool TiledXMLParser::is_end() {
	return _parser->get_node_type() == XMLParser::NODE_ELEMENT_END;
}

bool TiledXMLParser::is_empty() {
	return _parser->is_empty();
}

String TiledXMLParser::get_data() {
	return _parser->get_node_data();
}

Dictionary TiledXMLParser::get_attributes() {
	Dictionary attributes;
	for (int i = 0; i < _parser->get_attribute_count(); ++i) {
		attributes[_parser->get_attribute_name(i)] = _parser->get_attribute_value(i);
	}
	return attributes;
}

void TiledXMLParser::insert_attributes(Dictionary &target_dictionary, const Dictionary &attribs) {
	Array keys = attribs.keys();
	for (int i = 0; i < keys.size(); i++) {
		String key = keys[i];
		Variant attr_val;

		if (key == "infinite" || key == "visible" || key == "wrap") {
			attr_val = String(attribs[key]) == "1";
		} else {
			attr_val = attribs[key];
		}

		if (!key.contains("version")) {
			String s_val = attr_val;
			if (s_val.is_valid_int()) {
				attr_val = s_val.to_int();
			} else if (s_val.is_valid_float()) {
				attr_val = s_val.to_float();
			}
		}

		target_dictionary[key] = attr_val;
	}
}

Error TiledXMLParser::simple_element(const String &element_name, const Dictionary &attribs) {
	String dict_key = element_name;

	if (dict_key == "image") {
		_current_dictionary["image"] = attribs.get("source", "");
		if (attribs.has("width")) {
			_current_dictionary["imagewidth"] = String(attribs["width"]).to_int();
		}
		if (attribs.has("height")) {
			_current_dictionary["imageheight"] = String(attribs["height"]).to_int();
		}
		if (attribs.has("trans")) {
			_current_dictionary["transparentcolor"] = attribs["trans"];
		}
		return OK;
	}
	if (dict_key == "wangcolor") {
		dict_key = "color";
	}
	if (dict_key == "point") {
		_current_dictionary["point"] = true;
		return OK;
	}
	if (dict_key == "ellipse") {
		_current_dictionary["ellipse"] = true;
		return OK;
	}
	if (dict_key == "capsule") {
		_current_dictionary["capsule"] = true;
		return OK;
	}

	if ((dict_key == "objectgroup" && (!_is_map || _in_tileset)) || dict_key == "text" || dict_key == "tileoffset" || dict_key == "grid") {
		Dictionary new_dict;
		_current_dictionary[dict_key] = new_dict;
		// Keep a pointer logically by holding the map
		// In GDScript, dictionaries are passed by reference, so this gets tricky in C++
		// Warning: To match GDScript reference logic, we mutate the dictionary returned by ref.
		if (attribs.size() > 0) {
			insert_attributes(new_dict, attribs);
		}
		// Since C++ Dictionary values are references, we can assign our current ref
		_current_dictionary = new_dict;
	} else {
		if (dict_key == "polygon" || dict_key == "polyline") {
			Array arr;
			PackedStringArray pts = String(attribs.get("points", "")).split(" ");
			for (int i = 0; i < pts.size(); i++) {
				String pt = pts[i];
				if (pt.is_empty()) {
					continue;
				}
				PackedStringArray coords = pt.split(",");
				if (coords.size() == 2) {
					Dictionary dict;
					dict["x"] = coords[0].to_float();
					dict["y"] = coords[1].to_float();
					arr.push_back(dict);
				}
			}
			_current_dictionary[dict_key] = arr;
		} else if (dict_key == "frame" || dict_key == "property") {
			Dictionary dict;
			insert_attributes(dict, attribs);
			_current_array.push_back(dict);
			if (dict.has("type") && String(dict["type"]) == "list") {
				_current_dictionary = dict;
				_current_array = Array();
				_current_dictionary["value"] = _current_array;
			}
		} else {
			Dictionary mutable_attribs = attribs.duplicate();
			if (dict_key == "objectgroup" || dict_key == "imagelayer") {
				mutable_attribs["type"] = dict_key;
				dict_key = "layer";
			}
			if (dict_key == "group") {
				mutable_attribs["type"] = "group";
				if (_current_dictionary.has("layers")) {
					_current_array = _current_dictionary["layers"];
				} else {
					_current_array = Array();
					_current_dictionary["layers"] = _current_array;
				}
				dict_key = "layer";
			}
			if (dict_key != "animation" && dict_key != "properties") {
				dict_key = dict_key + "s";
			}
			if (dict_key != "items") {
				if (_current_dictionary.has(dict_key)) {
					_current_array = _current_dictionary[dict_key];
				} else {
					_current_array = Array();
					_current_dictionary[dict_key] = _current_array;
				}
			}
			if (dict_key != "animation" && dict_key != "properties") {
				_current_dictionary = Dictionary();
				_current_array.push_back(_current_dictionary);
			}
			if (dict_key == "wangtiles") {
				_current_dictionary["tileid"] = String(mutable_attribs["tileid"]).to_int();
				Array arr;
				PackedStringArray wids = String(mutable_attribs.get("wangid", "")).split(",");
				for (int i = 0; i < wids.size(); i++) {
					arr.push_back(wids[i].to_int());
				}
				_current_dictionary["wangid"] = arr;
			} else {
				if (mutable_attribs.size() > 0) {
					insert_attributes(_current_dictionary, mutable_attribs);
				}
			}
		}
	}
	return OK;
}

Error TiledXMLParser::nested_element(const String &element_name, const Dictionary &attribs) {
	Error err = OK;
	if (element_name == "wangsets") {
		return OK;
	} else if (element_name == "data") {
		_current_dictionary["type"] = "tilelayer";
		if (attribs.has("encoding")) {
			_current_dictionary["encoding"] = attribs["encoding"];
			_csv_encoded = String(attribs["encoding"]) == "csv";
		}
		if (attribs.has("compression")) {
			_current_dictionary["compression"] = attribs["compression"];
		}
		return OK;
	} else if (element_name == "tileset") {
		_in_tileset = true;
	}

	Dictionary dictionary_bookmark_1 = _current_dictionary;
	Array array_bookmark_1 = _current_array;
	err = simple_element(element_name, attribs);

	String base_element = _current_element;
	int base_group_level = _current_group_level;

	while (err == OK && (!is_end() || _current_element != base_element || (base_element == "group" && _current_element == "group" && _current_group_level == base_group_level))) {
		_current_element = next_element();
		if (_current_element.is_empty()) {
			return ERR_PARSE_ERROR;
		}

		if (is_end()) {
			if (_current_element == "group") {
				_current_group_level -= 1;
			}
			continue;
		}
		if (_current_element == "group" && !is_empty()) {
			_current_group_level += 1;
		}

		if (_current_element == "<data>") {
			String data = get_data();
			if (base_element == "text") {
				_current_dictionary[base_element] = data.replace("\r", "");
			} else if (base_element == "property") {
				Dictionary last_prop = _current_array[_current_array.size() - 1];
				last_prop["value"] = data.replace("\r", "");
			} else {
				data = data.strip_edges();
				if (_csv_encoded) {
					Array arr;
					PackedStringArray parts = data.split(",");
					for (int i = 0; i < parts.size(); i++) {
						arr.push_back(parts[i].strip_edges().to_int());
					}
					Dictionary last_prop = _current_array[_current_array.size() - 1];
					last_prop["data"] = arr;
				} else {
					Dictionary last_prop = _current_array[_current_array.size() - 1];
					last_prop["data"] = data;
				}
			}
			continue;
		}

		Dictionary c_attributes = get_attributes();
		Dictionary dictionary_bookmark_2 = _current_dictionary;
		Array array_bookmark_2 = _current_array;

		if (is_empty()) {
			err = simple_element(_current_element, c_attributes);
		} else {
			err = nested_element(_current_element, c_attributes);
		}
		_current_dictionary = dictionary_bookmark_2;
		_current_array = array_bookmark_2;
	}

	_current_dictionary = dictionary_bookmark_1;
	_current_array = array_bookmark_1;
	if (base_element == "tileset") {
		_in_tileset = false;
	}

	return err;
}

Dictionary TiledXMLParser::parse_xml_buffer(const PackedByteArray &p_buffer, const String &p_path, Error *r_err) {
	_parsed_file_name = p_path;
	Error err = _parser->open_buffer(p_buffer);
	if (r_err) {
		*r_err = err;
	}
	if (err != OK) {
		return Dictionary();
	}

	_current_element = next_element();
	_current_dictionary = _result;
	Dictionary base_attributes = get_attributes();

	_current_dictionary["type"] = _current_element;
	insert_attributes(_current_dictionary, base_attributes);
	_is_map = _current_element == "map";

	String base_element = _current_element;
	int base_group_level = _current_group_level;

	while (err == OK && (!is_end() || _current_element != base_element || (base_element == "group" && _current_element == "group" && _current_group_level == base_group_level))) {
		_current_element = next_element();
		if (_current_element.is_empty()) {
			err = ERR_PARSE_ERROR;
			break;
		}
		if (is_end()) {
			if (_current_element == "group") {
				_current_group_level -= 1;
			}
			continue;
		}
		if (_current_element == "group" && !is_empty()) {
			_current_group_level += 1;
		}

		Dictionary c_attributes = get_attributes();
		Dictionary dictionary_bookmark = _current_dictionary;

		if (is_empty()) {
			err = simple_element(_current_element, c_attributes);
		} else {
			err = nested_element(_current_element, c_attributes);
		}
		_current_dictionary = dictionary_bookmark;
	}

	if (err == OK) {
		return _result;
	}
	return Dictionary();
}

Dictionary TiledXMLParser::parse_xml_file(const String &p_path, Error *r_err) {
	String checked_file = p_path;
	Ref<FileAccess> file = FileAccess::open(checked_file, FileAccess::READ);
	if (!file.is_valid()) {
		if (r_err) {
			*r_err = ERR_FILE_NOT_FOUND;
		}
		return Dictionary();
	}
	PackedByteArray buffer = file->get_buffer(file->get_length());
	return parse_xml_buffer(buffer, p_path, r_err);
}
