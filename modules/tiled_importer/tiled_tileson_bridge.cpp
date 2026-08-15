/**************************************************************************/
/*  tiled_tileson_bridge.cpp                                              */
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

#include "tiled_tileson_bridge.h"
#include "core/io/json.h"
#include "core/variant/array.h"
#include "core/variant/variant.h"
#include "tiled_common.h"

// Thirdparty tileson parser library
#include "tileson.hpp"

#define POCKETLZMA_LZMA_C_DEFINE
#include "extras/pocketlzma.hpp"

Dictionary TiledTilesonBridge::parse_world(const String &p_path) {
	Dictionary result;
	result["status"] = String("ERROR");

	PackedByteArray content = DataLoader::get_tiled_file_content(p_path, p_path.get_base_dir());
	if (content.is_empty()) {
		return result;
	}

	Ref<JSON> json = memnew(JSON);
	Error err = json->parse(String::utf8((const char *)content.ptr(), content.size()));
	if (err != OK) {
		ERR_PRINT("JSON parse error on " + p_path + ": " + json->get_error_message());
		return result;
	}

	Dictionary world_data = json->get_data();
	if (!world_data.has("maps")) {
		return result;
	}

	Array maps = world_data["maps"];
	Array maps_array;

	for (int i = 0; i < maps.size(); i++) {
		Dictionary map_dict = maps[i];
		Dictionary result_map;
		result_map["file_name"] = map_dict.get("fileName", "");
		result_map["pos_x"] = map_dict.get("x", 0);
		result_map["pos_y"] = map_dict.get("y", 0);
		result_map["width"] = map_dict.get("width", 0);
		result_map["height"] = map_dict.get("height", 0);
		maps_array.push_back(result_map);
	}

	result["maps"] = maps_array;
	if (maps_array.size() > 0) {
		result["status"] = String("OK");
	}

	return result;
}

PackedByteArray TiledTilesonBridge::decompress_lzma(const PackedByteArray &p_buffer, int p_expected_size) {
	PackedByteArray decompressed;

	if (p_buffer.is_empty()) {
		return decompressed;
	}

	std::vector<uint8_t> input_data;
	input_data.resize(p_buffer.size());
	memcpy(input_data.data(), p_buffer.ptr(), p_buffer.size());

	plz::PocketLzma p;
	std::vector<uint8_t> output_data;
	plz::StatusCode status = p.decompress(input_data, output_data);

	if (status == plz::StatusCode::Ok) {
		decompressed.resize(output_data.size());
		memcpy(decompressed.ptrw(), output_data.data(), output_data.size());
	} else {
		ERR_PRINT(String("PocketLzma decompression failed: ") + String::num_int64(static_cast<int>(status)));
	}

	return decompressed;
}
