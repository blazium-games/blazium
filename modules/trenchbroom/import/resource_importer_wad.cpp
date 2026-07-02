/**************************************************************************/
/*  resource_importer_wad.cpp                                             */
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

#include "resource_importer_wad.h"

#include "core/error/error_macros.h"
#include "core/io/file_access.h"
#include "core/io/resource_loader.h"
#include "core/io/resource_saver.h"
#include "core/string/print_string.h"
#include "modules/trenchbroom/trenchbroom_defaults.h"
#include "quake_palette_file.h"
#include "quake_wad_file.h"
#include "scene/resources/image_texture.h"

enum {
	WAD_FORMAT_QUAKE,
	WAD_FORMAT_HALFLIFE,
};

enum {
	QUAKE_WAD_ENTRY_MIPS_TEXTURE = 0x44,
	HALFLIFE_WAD_ENTRY_MIPS_TEXTURE = 0x43,
};

static const int TEXTURE_NAME_LENGTH = 16;
static const int MAX_MIP_LEVELS = 4;

static String _buffer_to_ascii(const Vector<uint8_t> &p_buffer) {
	return String((const char *)p_buffer.ptr(), p_buffer.size());
}

String ResourceImporterQuakeWad::get_importer_name() const {
	return "blazium.trenchbroom.wad";
}

String ResourceImporterQuakeWad::get_visible_name() const {
	return "Quake WAD";
}

void ResourceImporterQuakeWad::get_recognized_extensions(List<String> *p_extensions) const {
	p_extensions->push_back("wad");
}

String ResourceImporterQuakeWad::get_save_extension() const {
	return "res";
}

String ResourceImporterQuakeWad::get_resource_type() const {
	return "QuakeWadFile";
}

float ResourceImporterQuakeWad::get_priority() const {
	return 1.0f;
}

int ResourceImporterQuakeWad::get_import_order() const {
	return 0;
}

int ResourceImporterQuakeWad::get_preset_count() const {
	return 0;
}

void ResourceImporterQuakeWad::get_import_options(const String &p_path, List<ImportOption> *r_options, int p_preset) const {
	r_options->push_back(ImportOption(PropertyInfo(Variant::STRING, "palette_file", PROPERTY_HINT_FILE, "*.lmp"), String()));
	r_options->push_back(ImportOption(PropertyInfo(Variant::BOOL, "generate_mipmaps"), true));
}

bool ResourceImporterQuakeWad::get_option_visibility(const String &p_path, const String &p_option, const HashMap<StringName, Variant> &p_options) const {
	return true;
}

Error ResourceImporterQuakeWad::import(ResourceUID::ID p_source_id, const String &p_source_file, const String &p_save_path, const HashMap<StringName, Variant> &p_options, List<String> *r_platform_variants, List<String> *r_gen_files, Variant *r_metadata) {
	const String save_path_str = p_save_path + "." + get_save_extension();

	Ref<FileAccess> file = FileAccess::open(p_source_file, FileAccess::READ);
	if (file.is_null()) {
		ERR_PRINT(vformat("Error opening WAD file: %s", FileAccess::get_open_error()));
		return FileAccess::get_open_error();
	}

	const String magic_string = _buffer_to_ascii(file->get_buffer(4));
	int wad_format = WAD_FORMAT_QUAKE;
	if (magic_string == "WAD3") {
		wad_format = WAD_FORMAT_HALFLIFE;
	} else if (magic_string != "WAD2") {
		ERR_PRINT("Error: Invalid WAD magic");
		return ERR_INVALID_DATA;
	}

	const String palette_path = p_options.has("palette_file") ? String(p_options["palette_file"]) : TrenchbroomDefaults::get_default_palette_path();
	Ref<QuakePaletteFile> palette_file;
	if (wad_format == WAD_FORMAT_QUAKE) {
		if (palette_path.is_empty()) {
			ERR_PRINT("Error: Quake WAD import requires a palette file");
			return ERR_CANT_ACQUIRE_RESOURCE;
		}
		palette_file = ResourceLoader::load(palette_path);
		if (palette_file.is_null()) {
			ERR_PRINT("Error: Invalid Quake palette file");
			return ERR_CANT_ACQUIRE_RESOURCE;
		}
	}

	const int num_entries = file->get_32();
	const int dir_offset = file->get_32();

	struct WadEntry {
		int offset = 0;
		int in_wad_size = 0;
		int size = 0;
		int type = 0;
		int compression = 0;
		String name;
	};

	LocalVector<WadEntry> entries;
	file->seek(dir_offset);
	for (int entry_idx = 0; entry_idx < num_entries; entry_idx++) {
		WadEntry entry;
		entry.offset = file->get_32();
		entry.in_wad_size = file->get_32();
		entry.size = file->get_32();
		entry.type = file->get_8();
		entry.compression = file->get_8();
		file->get_16();
		entry.name = _buffer_to_ascii(file->get_buffer(TEXTURE_NAME_LENGTH)).get_slice("\0", 0);

		if ((wad_format == WAD_FORMAT_QUAKE && entry.type == QUAKE_WAD_ENTRY_MIPS_TEXTURE) ||
				(wad_format == WAD_FORMAT_HALFLIFE && entry.type == HALFLIFE_WAD_ENTRY_MIPS_TEXTURE)) {
			entries.push_back(entry);
		}
	}

	struct TextureData {
		String name;
		int width = 0;
		int height = 0;
		PackedByteArray pixels;
		PackedColorArray palette_colors;
	};

	LocalVector<TextureData> texture_data_array;
	for (const WadEntry &entry : entries) {
		file->seek(entry.offset);
		file->get_buffer(TEXTURE_NAME_LENGTH);

		TextureData texture_data;
		texture_data.name = entry.name;
		texture_data.width = file->get_32();
		texture_data.height = file->get_32();

		LocalVector<int> mip_offsets;
		for (int idx = 0; idx < MAX_MIP_LEVELS; idx++) {
			mip_offsets.push_back(file->get_32());
		}

		const int num_pixels = texture_data.width * texture_data.height;
		Vector<uint8_t> raw_pixels = file->get_buffer(num_pixels);
		texture_data.pixels.resize(raw_pixels.size());
		for (int i = 0; i < raw_pixels.size(); i++) {
			texture_data.pixels.set(i, raw_pixels[i]);
		}

		if (wad_format == WAD_FORMAT_QUAKE) {
			texture_data_array.push_back(texture_data);
			continue;
		}

		file->seek(entry.offset + mip_offsets[mip_offsets.size() - 1] + (texture_data.width / 8) * (texture_data.height / 8));
		file->get_16();
		for (int idx = 0; idx < 256; idx++) {
			const int red = file->get_8();
			const int green = file->get_8();
			const int blue = file->get_8();
			texture_data.palette_colors.push_back(Color(red / 255.0f, green / 255.0f, blue / 255.0f));
		}
		texture_data_array.push_back(texture_data);
	}

	bool generate_mipmaps = true;
	if (p_options.has("generate_mipmaps")) {
		generate_mipmaps = bool(p_options["generate_mipmaps"]);
	}
	Dictionary textures;
	for (const TextureData &texture_data : texture_data_array) {
		PackedByteArray pixels_rgb;
		Ref<Image> texture_image;

		if (wad_format == WAD_FORMAT_HALFLIFE) {
			for (int i = 0; i < texture_data.pixels.size(); i++) {
				const uint8_t palette_color = texture_data.pixels[i];
				const Color rgb_color = texture_data.palette_colors[palette_color];
				pixels_rgb.push_back(rgb_color.get_r8());
				pixels_rgb.push_back(rgb_color.get_g8());
				pixels_rgb.push_back(rgb_color.get_b8());
				if (rgb_color.b == 1 && rgb_color.r == 0 && rgb_color.b == 0) {
					pixels_rgb.push_back(0);
				} else {
					pixels_rgb.push_back(255);
				}
			}
			texture_image = Image::create_from_data(texture_data.width, texture_data.height, false, Image::FORMAT_RGBA8, pixels_rgb);
		} else {
			const PackedColorArray &colors = palette_file->get_colors();
			for (int i = 0; i < texture_data.pixels.size(); i++) {
				const uint8_t palette_color = texture_data.pixels[i];
				const Color rgb_color = colors[palette_color];
				pixels_rgb.push_back(rgb_color.get_r8());
				pixels_rgb.push_back(rgb_color.get_g8());
				pixels_rgb.push_back(rgb_color.get_b8());
				if (palette_color != 255) {
					pixels_rgb.push_back(255);
				} else {
					pixels_rgb.push_back(0);
				}
			}
			texture_image = Image::create_from_data(texture_data.width, texture_data.height, false, Image::FORMAT_RGBA8, pixels_rgb);
		}

		if (generate_mipmaps) {
			texture_image->generate_mipmaps();
		}

		Ref<ImageTexture> texture = ImageTexture::create_from_image(texture_image);
		textures[texture_data.name.to_lower()] = texture;
	}

	Ref<QuakeWadFile> wad_resource;
	wad_resource.instantiate();
	wad_resource->set_textures(textures);
	return ResourceSaver::save(wad_resource, save_path_str);
}
