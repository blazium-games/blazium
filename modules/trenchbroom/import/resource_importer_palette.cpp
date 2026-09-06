/**************************************************************************/
/*  resource_importer_palette.cpp                                         */
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

#include "resource_importer_palette.h"

#include "core/error/error_macros.h"
#include "core/io/file_access.h"
#include "core/io/resource_saver.h"
#include "core/string/print_string.h"
#include "quake_palette_file.h"

String ResourceImporterQuakePalette::get_importer_name() const {
	return "blazium.trenchbroom.palette";
}

String ResourceImporterQuakePalette::get_visible_name() const {
	return "Quake Palette";
}

void ResourceImporterQuakePalette::get_recognized_extensions(List<String> *p_extensions) const {
	p_extensions->push_back("lmp");
}

String ResourceImporterQuakePalette::get_save_extension() const {
	return "tres";
}

String ResourceImporterQuakePalette::get_resource_type() const {
	return "QuakePaletteFile";
}

float ResourceImporterQuakePalette::get_priority() const {
	return 1.0f;
}

int ResourceImporterQuakePalette::get_import_order() const {
	return 0;
}

int ResourceImporterQuakePalette::get_preset_count() const {
	return 0;
}

void ResourceImporterQuakePalette::get_import_options(const String &p_path, List<ImportOption> *r_options, int p_preset) const {
}

bool ResourceImporterQuakePalette::get_option_visibility(const String &p_path, const String &p_option, const HashMap<StringName, Variant> &p_options) const {
	return true;
}

Error ResourceImporterQuakePalette::import(ResourceUID::ID p_source_id, const String &p_source_file, const String &p_save_path, const HashMap<StringName, Variant> &p_options, List<String> *r_platform_variants, List<String> *r_gen_files, Variant *r_metadata) {
	const String save_path_str = p_save_path + "." + get_save_extension();

	Ref<FileAccess> file = FileAccess::open(p_source_file, FileAccess::READ);
	if (file.is_null()) {
		ERR_PRINT(vformat("Error opening palette file: %s", FileAccess::get_open_error()));
		return FileAccess::get_open_error();
	}

	PackedColorArray colors;
	while (!file->eof_reached() && colors.size() < 256) {
		const int red = file->get_8();
		const int green = file->get_8();
		const int blue = file->get_8();
		colors.push_back(Color(red / 255.0f, green / 255.0f, blue / 255.0f));
	}

	Ref<QuakePaletteFile> palette_resource;
	palette_resource.instantiate();
	palette_resource->set_colors(colors);
	return ResourceSaver::save(palette_resource, save_path_str);
}
