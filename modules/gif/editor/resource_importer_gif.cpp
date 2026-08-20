/**************************************************************************/
/*  resource_importer_gif.cpp                                             */
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

#include "resource_importer_gif.h"

#include "modules/gif/gif_animation.h"

#include "core/config/project_settings.h"
#include "core/io/resource_saver.h"
#include "scene/resources/image_texture.h"
#include "scene/resources/sprite_frames.h"

String ResourceImporterGIF::get_importer_name() const {
	return "gif";
}

String ResourceImporterGIF::get_visible_name() const {
	return "GIF";
}

void ResourceImporterGIF::get_recognized_extensions(List<String> *p_extensions) const {
	p_extensions->push_back("gif");
}

String ResourceImporterGIF::get_save_extension() const {
	return "res";
}

String ResourceImporterGIF::get_resource_type() const {
	return "GIFAnimation";
}

float ResourceImporterGIF::get_priority() const {
	return 2.0f;
}

int ResourceImporterGIF::get_preset_count() const {
	return 0;
}

String ResourceImporterGIF::get_preset_name(int p_idx) const {
	return String();
}

void ResourceImporterGIF::get_import_options(const String &p_path, List<ImportOption> *r_options, int p_preset) const {
	int default_mode = 0;
	if (ProjectSettings::get_singleton() && ProjectSettings::get_singleton()->has_setting("blazium/gif/default_import_mode")) {
		default_mode = int(GLOBAL_GET("blazium/gif/default_import_mode"));
	}
	r_options->push_back(ImportOption(PropertyInfo(Variant::INT, "import_mode", PROPERTY_HINT_ENUM, "GIF Animation,Sprite Frames,First Frame Texture"), default_mode));
	r_options->push_back(ImportOption(PropertyInfo(Variant::INT, "bake_storage", PROPERTY_HINT_ENUM, "Store,Generate On Load"), 1));
	r_options->push_back(ImportOption(PropertyInfo(Variant::BOOL, "dither"), true));
	r_options->push_back(ImportOption(PropertyInfo(Variant::BOOL, "bake_compress"), false));
}

bool ResourceImporterGIF::get_option_visibility(const String &p_path, const String &p_option, const HashMap<StringName, Variant> &p_options) const {
	return true;
}

Error ResourceImporterGIF::import(ResourceUID::ID p_source_id, const String &p_source_file, const String &p_save_path, const HashMap<StringName, Variant> &p_options, List<String> *r_platform_variants, List<String> *r_gen_files, Variant *r_metadata) {
	Ref<GIFAnimation> anim;
	anim.instantiate();
	const Error err = anim->load_from_path(p_source_file);
	ERR_FAIL_COND_V_MSG(err != OK, err, vformat("Failed to decode GIF: '%s'.", p_source_file));

	const int mode = p_options.has("import_mode") ? int(p_options["import_mode"]) : 0;
	anim->set_bake_storage(GIFAnimation::BakeStorage(int(p_options["bake_storage"])));
	anim->set_dither(bool(p_options["dither"]));
	anim->set_bake_compress(bool(p_options["bake_compress"]));

	if (mode == MODE_SPRITE_FRAMES) {
		Ref<SpriteFrames> frames = anim->to_sprite_frames("default");
		return ResourceSaver::save(frames, p_save_path + ".res");
	}
	if (mode == MODE_FIRST_FRAME_TEXTURE) {
		Ref<Image> img = anim->get_source_image(0);
		ERR_FAIL_COND_V(img.is_null(), ERR_FILE_CORRUPT);
		Ref<ImageTexture> tex = ImageTexture::create_from_image(img);
		return ResourceSaver::save(tex, p_save_path + ".res");
	}
	return ResourceSaver::save(anim, p_save_path + ".res");
}
