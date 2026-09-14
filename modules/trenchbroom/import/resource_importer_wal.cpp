/**************************************************************************/
/*  resource_importer_wal.cpp                                             */
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

#include "resource_importer_wal.h"

#include "core/error/error_macros.h"
#include "core/io/file_access.h"
#include "core/io/resource_loader.h"
#include "core/io/resource_saver.h"
#include "core/string/print_string.h"
#include "modules/trenchbroom/trenchbroom_defaults.h"
#include "quake_palette_file.h"
#include "scene/resources/image_texture.h"

static const int TEXTURE_NAME_LENGTH = 32;
static const int MAX_MIP_LEVELS = 4;

static PackedColorArray _build_fallback_palette() {
	PackedColorArray colors;
	for (int i = 0; i < 256; i++) {
		const float t = i / 255.0f;
		colors.push_back(Color(t, t, t, 1.0f));
	}
	return colors;
}

String ResourceImporterQuakeWal::get_importer_name() const {
	return "blazium.trenchbroom.wal";
}

String ResourceImporterQuakeWal::get_visible_name() const {
	return "Quake 2 WAL";
}

void ResourceImporterQuakeWal::get_recognized_extensions(List<String> *p_extensions) const {
	p_extensions->push_back("wal");
}

String ResourceImporterQuakeWal::get_save_extension() const {
	return "res";
}

String ResourceImporterQuakeWal::get_resource_type() const {
	return "Texture2D";
}

float ResourceImporterQuakeWal::get_priority() const {
	return 1.0f;
}

int ResourceImporterQuakeWal::get_import_order() const {
	return 0;
}

int ResourceImporterQuakeWal::get_preset_count() const {
	return 0;
}

void ResourceImporterQuakeWal::get_import_options(const String &p_path, List<ImportOption> *r_options, int p_preset) const {
	r_options->push_back(ImportOption(PropertyInfo(Variant::STRING, "palette_file", PROPERTY_HINT_FILE, "*.lmp"), String()));
	r_options->push_back(ImportOption(PropertyInfo(Variant::BOOL, "generate_mipmaps"), true));
}

bool ResourceImporterQuakeWal::get_option_visibility(const String &p_path, const String &p_option, const HashMap<StringName, Variant> &p_options) const {
	return true;
}

Error ResourceImporterQuakeWal::import(ResourceUID::ID p_source_id, const String &p_source_file, const String &p_save_path, const HashMap<StringName, Variant> &p_options, List<String> *r_platform_variants, List<String> *r_gen_files, Variant *r_metadata) {
	const String save_path_str = p_save_path + "." + get_save_extension();

	Ref<FileAccess> file = FileAccess::open(p_source_file, FileAccess::READ);
	if (file.is_null()) {
		ERR_PRINT(vformat("Error opening .wal file: %s", FileAccess::get_open_error()));
		return FileAccess::get_open_error();
	}

	file->get_buffer(TEXTURE_NAME_LENGTH);
	const int width = file->get_32();
	const int height = file->get_32();
	PackedInt32Array mip_offsets;
	mip_offsets.resize(MAX_MIP_LEVELS);
	for (int i = 0; i < MAX_MIP_LEVELS; i++) {
		mip_offsets.set(i, file->get_32());
	}
	file->get_buffer(TEXTURE_NAME_LENGTH);
	file->get_32();
	file->get_32();
	file->get_32();

	if (width <= 0 || height <= 0) {
		ERR_PRINT("Error: Invalid .wal dimensions");
		return ERR_INVALID_DATA;
	}
	if (mip_offsets[0] <= 0) {
		ERR_PRINT("Error: Invalid .wal mip offset");
		return ERR_INVALID_DATA;
	}

	file->seek(mip_offsets[0]);
	const int num_pixels = width * height;
	const PackedByteArray pixels = file->get_buffer(num_pixels);
	if (pixels.size() != num_pixels) {
		ERR_PRINT("Error: Unexpected .wal pixel data size");
		return ERR_INVALID_DATA;
	}

	PackedColorArray colors;
	const String palette_path = p_options.has("palette_file") ? String(p_options["palette_file"]) : TrenchbroomDefaults::get_quake2_palette_path();
	if (!palette_path.is_empty()) {
		Ref<QuakePaletteFile> palette_resource = ResourceLoader::load(palette_path);
		if (palette_resource.is_valid() && palette_resource->get_colors().size() >= 256) {
			colors = palette_resource->get_colors();
		}
	}
	if (colors.size() < 256) {
		if (!palette_path.is_empty()) {
			WARN_PRINT(vformat("Invalid palette file for .wal import (%s). Using fallback grayscale palette.", palette_path));
		}
		colors = _build_fallback_palette();
	}

	PackedByteArray pixels_rgba;
	pixels_rgba.resize(num_pixels * 4);
	for (int i = 0; i < num_pixels; i++) {
		const int palette_index = pixels[i];
		const Color color = colors[palette_index];
		const int offset = i * 4;
		pixels_rgba.set(offset, color.get_r8());
		pixels_rgba.set(offset + 1, color.get_g8());
		pixels_rgba.set(offset + 2, color.get_b8());
		if (palette_index == 255 || (color.get_r8() == 255 && color.get_g8() == 0 && color.get_b8() == 255)) {
			pixels_rgba.set(offset + 3, 0);
		} else {
			pixels_rgba.set(offset + 3, 255);
		}
	}

	Ref<Image> image = Image::create_from_data(width, height, false, Image::FORMAT_RGBA8, pixels_rgba);
	bool generate_mipmaps = true;
	if (p_options.has("generate_mipmaps")) {
		generate_mipmaps = bool(p_options["generate_mipmaps"]);
	}
	if (generate_mipmaps) {
		image->generate_mipmaps();
	}

	Ref<ImageTexture> texture = ImageTexture::create_from_image(image);
	return ResourceSaver::save(texture, save_path_str);
}
