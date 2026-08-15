/**************************************************************************/
/*  tiled_common.cpp                                                      */
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

#include "tiled_common.h"
#include "core/io/file_access.h"
#include "core/io/resource_loader.h"
#include "scene/resources/image_texture.h"
#include "scene/resources/texture.h"
#include "tiled_tileson_bridge.h"

PackedByteArray DataLoader::get_tiled_file_content(const String &p_file_name, const String &p_base_path) {
	String checked_file = p_file_name;
	if (!FileAccess::exists(checked_file)) {
		checked_file = p_base_path.path_join(p_file_name);
	}
	if (!FileAccess::exists(checked_file)) {
		return PackedByteArray();
	}

	Ref<FileAccess> file = FileAccess::open(checked_file, FileAccess::READ);
	if (file.is_valid()) {
		PackedByteArray bytes = file->get_buffer(file->get_length());
		if (checked_file.get_extension() == "lzma") {
			return TiledTilesonBridge::decompress_lzma(bytes, bytes.size() * 10);
		}
		return bytes;
	}
	return PackedByteArray();
}

Ref<Texture2D> DataLoader::load_image(const String &p_file_name, const String &p_base_path) {
	String checked_file = p_file_name;
	if (!FileAccess::exists(checked_file)) {
		checked_file = p_base_path.path_join(p_file_name);
	}
	if (!FileAccess::exists(checked_file)) {
		ERR_PRINT("ERROR: Image file '" + p_file_name + "' not found.");
		return Ref<Texture2D>();
	}

	if (ResourceLoader::exists(checked_file, "Image")) {
		return ResourceLoader::load(checked_file);
	}

	Ref<Image> image = Image::load_from_file(checked_file);
	if (image.is_valid()) {
		return ImageTexture::create_from_image(image);
	}
	return Ref<Texture2D>();
}

Ref<Resource> DataLoader::load_resource_from_file(const String &p_resource_file, const String &p_base_path) {
	String checked_file = p_resource_file;
	if (!FileAccess::exists(checked_file)) {
		checked_file = p_base_path.path_join(p_resource_file);
	}
	if (FileAccess::exists(checked_file)) {
		return ResourceLoader::load(checked_file);
	}

	ERR_PRINT("ERROR: Resource file '" + p_resource_file + "' not found.");
	return Ref<Resource>();
}

uint32_t CommonUtils::get_bitmask_integer_from_string(const String &p_mask_string, int p_max_len) {
	uint32_t ret = 0;
	PackedStringArray s1_arr = p_mask_string.split(",", false);
	for (int j = 0; j < s1_arr.size(); j++) {
		String s1 = s1_arr[j].strip_edges();
		if (s1.contains("-")) {
			PackedStringArray s2_arr = s1.split("-", false, 1);
			int i1 = s2_arr[0].strip_edges().is_valid_int() ? s2_arr[0].strip_edges().to_int() : 0;
			int i2 = s2_arr[1].strip_edges().is_valid_int() ? s2_arr[1].strip_edges().to_int() : 0;
			if (i1 == 0 || i2 == 0 || i1 > i2) {
				continue;
			}
			for (int i = i1; i <= i2; i++) {
				if (i <= p_max_len) {
					ret |= (1 << (i - 1));
				}
			}
		} else if (s1.is_valid_int()) {
			int i = s1.to_int();
			if (i <= p_max_len) {
				ret |= (1 << (i - 1));
			}
		}
	}
	return ret;
}
