/**************************************************************************/
/*  register_types.cpp                                                    */
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

#include "register_types.h"

#include "gif_recorder.h"
#include "gif_texture.h"
#include "image_loader_gif.h"
#include "movie_writer_gif.h"
#include "resource_loader_gif.h"
#include "resource_saver_gif.h"

#include "core/config/engine.h"
#include "core/config/project_settings.h"
#include "core/input/input.h"
#include "core/input/input_event.h"
#include "core/io/image.h"
#include "core/io/image_loader.h"
#include "core/io/resource_loader.h"
#include "core/io/resource_saver.h"
#include "core/os/os.h"
#include "scene/main/scene_tree.h"
#include "scene/main/viewport.h"
#include "scene/main/window.h"
#include "servers/movie_writer/movie_writer.h"

#ifdef TOOLS_ENABLED
#include "editor/editor_node.h"
#include "editor/gif_editor_plugin.h"
#include "editor/plugins/editor_plugin.h"
#include "editor/resource_importer_gif.h"
#include "editor/resource_importer_gif_frames.h"
#endif

static Ref<ImageLoaderGIF> image_loader_gif;
static Ref<ResourceSaverGIF> resource_saver_gif;
static Ref<ResourceFormatLoaderGIF> resource_loader_gif;
static MovieWriterGIF *movie_writer_gif = nullptr;
static Ref<GIFRecorder> project_hotkey_recorder;
static bool gif_hotkey_was_down = false;

static void _gif_hotkey_tick() {
	if (Engine::get_singleton()->is_editor_hint()) {
		return;
	}
	const int key = int(GLOBAL_GET("blazium/gif/capture_hotkey"));
	if (key == 0 || !Input::get_singleton()) {
		return;
	}
	const bool down = Input::get_singleton()->is_physical_key_pressed(Key(key));
	if (!down) {
		gif_hotkey_was_down = false;
		return;
	}
	if (gif_hotkey_was_down) {
		return;
	}
	gif_hotkey_was_down = true;
	if (project_hotkey_recorder.is_valid() && project_hotkey_recorder->is_recording()) {
		Ref<GIFTexture> anim = project_hotkey_recorder->stop();
		const String dir = String(GLOBAL_GET("blazium/gif/capture_output_dir"));
		const String path = dir.path_join("capture_" + itos(OS::get_singleton()->get_ticks_msec()) + ".gif");
		if (anim.is_valid()) {
			anim->save_to_path(path);
		}
		project_hotkey_recorder.unref();
		return;
	}
	SceneTree *tree = Object::cast_to<SceneTree>(OS::get_singleton()->get_main_loop());
	ERR_FAIL_NULL(tree);
	project_hotkey_recorder.instantiate();
	const int src = int(GLOBAL_GET("blazium/gif/capture_source"));
	if (src == 1) {
		project_hotkey_recorder->start_window();
	} else if (tree->get_root()) {
		project_hotkey_recorder->start_viewport(tree->get_root());
	}
}

#ifdef TOOLS_ENABLED
static void _editor_init() {
	Ref<ResourceImporterGIF> gif_import;
	gif_import.instantiate();
	ResourceFormatImporter::get_singleton()->add_importer(gif_import);

	Ref<ResourceImporterGIFFrames> gif_frames_import;
	gif_frames_import.instantiate();
	ResourceFormatImporter::get_singleton()->add_importer(gif_frames_import);
}
#endif

void initialize_gif_module(ModuleInitializationLevel p_level) {
	if (p_level == MODULE_INITIALIZATION_LEVEL_SCENE) {
		GLOBAL_DEF(PropertyInfo(Variant::INT, "blazium/gif/max_canvas_pixels", PROPERTY_HINT_RANGE, "65536,268435456,1"), 16777216);
		GLOBAL_DEF(PropertyInfo(Variant::INT, "blazium/gif/max_frames", PROPERTY_HINT_RANGE, "1,65536,1"), 4096);
		GLOBAL_DEF(PropertyInfo(Variant::INT, "blazium/gif/capture_hotkey", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_DEFAULT), 0);
		GLOBAL_DEF(PropertyInfo(Variant::INT, "blazium/gif/capture_source", PROPERTY_HINT_ENUM, "Viewport,Window"), 0);
		GLOBAL_DEF(PropertyInfo(Variant::STRING, "blazium/gif/capture_output_dir", PROPERTY_HINT_DIR), String("user://"));

		GDREGISTER_CLASS(GIFTexture);
		GDREGISTER_CLASS(GIFRecorder);

		image_loader_gif.instantiate();
		ImageLoader::add_image_format_loader(image_loader_gif);

		resource_saver_gif.instantiate();
		ResourceSaver::add_resource_format_saver(resource_saver_gif);

		resource_loader_gif.instantiate();
		ResourceLoader::add_resource_format_loader(resource_loader_gif);

		movie_writer_gif = memnew(MovieWriterGIF);
		MovieWriter::add_writer(movie_writer_gif);

		if (OS::get_singleton() && OS::get_singleton()->get_main_loop()) {
			SceneTree *tree = Object::cast_to<SceneTree>(OS::get_singleton()->get_main_loop());
			if (tree && !tree->is_connected(SNAME("process_frame"), callable_mp_static(_gif_hotkey_tick))) {
				tree->connect(SNAME("process_frame"), callable_mp_static(_gif_hotkey_tick));
			}
		}
	}

#ifdef TOOLS_ENABLED
	if (p_level == MODULE_INITIALIZATION_LEVEL_EDITOR) {
		GDREGISTER_CLASS(ResourceImporterGIF);
		GDREGISTER_CLASS(ResourceImporterGIFFrames);
		EditorPlugins::add_by_type<GIFEditorPlugin>();
		EditorNode::add_init_callback(_editor_init);
	}
#endif
}

void uninitialize_gif_module(ModuleInitializationLevel p_level) {
	if (p_level != MODULE_INITIALIZATION_LEVEL_SCENE) {
		return;
	}

	if (OS::get_singleton() && OS::get_singleton()->get_main_loop()) {
		SceneTree *tree = Object::cast_to<SceneTree>(OS::get_singleton()->get_main_loop());
		if (tree && tree->is_connected(SNAME("process_frame"), callable_mp_static(_gif_hotkey_tick))) {
			tree->disconnect(SNAME("process_frame"), callable_mp_static(_gif_hotkey_tick));
		}
	}

	ImageLoader::remove_image_format_loader(image_loader_gif);
	image_loader_gif.unref();

	ResourceSaver::remove_resource_format_saver(resource_saver_gif);
	resource_saver_gif.unref();

	ResourceLoader::remove_resource_format_loader(resource_loader_gif);
	resource_loader_gif.unref();

	if (movie_writer_gif) {
		memdelete(movie_writer_gif);
		movie_writer_gif = nullptr;
	}

	Image::_gif_mem_loader_func = nullptr;
	Image::save_gif_func = nullptr;
	Image::save_gif_buffer_func = nullptr;

	project_hotkey_recorder.unref();
}
