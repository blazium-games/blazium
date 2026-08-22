/**************************************************************************/
/*  justamcp_animation_tools_sprite_frames.cpp                            */
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

#include "justamcp_agent_helpers.h"
#include "justamcp_animation_tools.h"
#include "justamcp_scene_file_io.h"

#include "core/io/file_access.h"
#include "core/io/json.h"
#include "core/io/resource_loader.h"
#include "scene/2d/animated_sprite_2d.h"
#include "scene/resources/atlas_texture.h"
#include "scene/resources/image_texture.h"
#include "scene/resources/sprite_frames.h"

static Dictionary _load_atlas_metadata(const String &p_atlas_path, Dictionary &r_meta) {
	String meta_path = p_atlas_path + ".metadata.json";
	if (!FileAccess::exists(meta_path)) {
		meta_path = p_atlas_path.get_basename() + ".metadata.json";
	}
	if (!FileAccess::exists(meta_path)) {
		Dictionary err;
		err["ok"] = false;
		err["error"] = "generated_atlas is missing metadata JSON next to " + p_atlas_path;
		return err;
	}
	Ref<FileAccess> file = FileAccess::open(meta_path, FileAccess::READ);
	if (file.is_null()) {
		Dictionary err;
		err["ok"] = false;
		err["error"] = "Failed to read metadata: " + meta_path;
		return err;
	}
	Ref<JSON> json;
	json.instantiate();
	if (json->parse(file->get_as_utf8_string()) != OK || json->get_data().get_type() != Variant::DICTIONARY) {
		Dictionary err;
		err["ok"] = false;
		err["error"] = "Invalid metadata JSON: " + meta_path;
		return err;
	}
	r_meta = json->get_data();
	return Dictionary();
}

Dictionary JustAMCPAnimationTools::configure_sprite_frames(const Dictionary &p_args) {
	const String file_path = justamcp_resolve_project_path(p_args.get("file_path", p_args.get("scene_path", "")));
	const String node_path = p_args.get("node_path", "");
	Array animations = p_args.get("animations", Array());
	if (file_path.is_empty() || node_path.is_empty()) {
		Dictionary ret;
		ret["ok"] = false;
		ret["error"] = "file_path and node_path are required.";
		return ret;
	}
	if (animations.is_empty() && p_args.get("animations", Variant()).get_type() == Variant::STRING) {
		Ref<JSON> json;
		json.instantiate();
		if (json->parse(String(p_args["animations"])) == OK && json->get_data().get_type() == Variant::ARRAY) {
			animations = json->get_data();
		}
	}
	if (animations.is_empty()) {
		Dictionary ret;
		ret["ok"] = false;
		ret["error"] = "animations must be a JSON array.";
		return ret;
	}

	Node *root = nullptr;
	Dictionary load_err = justamcp_load_scene_root(file_path, &root);
	if (!load_err.is_empty()) {
		return load_err;
	}
	Node *node = justamcp_find_node_in_root(root, node_path);
	AnimatedSprite2D *sprite = Object::cast_to<AnimatedSprite2D>(node);
	if (!sprite) {
		memdelete(root);
		Dictionary ret;
		ret["ok"] = false;
		ret["error"] = "Node is not an AnimatedSprite2D: " + node_path;
		return ret;
	}

	Ref<SpriteFrames> frames;
	frames.instantiate();
	int total_frames = 0;
	int anim_count = 0;
	for (int i = 0; i < animations.size(); i++) {
		if (animations[i].get_type() != Variant::DICTIONARY) {
			memdelete(root);
			Dictionary ret;
			ret["ok"] = false;
			ret["error"] = vformat("Each animation must be a JSON object. Got type at index %d", i);
			return ret;
		}
		Dictionary anim = animations[i];
		const String name = anim.get("name", "");
		const String mode = String(anim.get("mode", "")).to_lower();
		if (name.is_empty() || (mode != "generated_atlas" && mode != "frames")) {
			memdelete(root);
			Dictionary ret;
			ret["ok"] = false;
			ret["error"] = vformat("Animation '%s' is missing or has invalid 'mode'. Use \"generated_atlas\" or \"frames\".", name);
			return ret;
		}
		if (!frames->has_animation(name)) {
			frames->add_animation(name);
		}
		frames->set_animation_speed(name, double(anim.get("speed", 8)));
		frames->set_animation_loop(name, bool(anim.get("loop", true)));
		anim_count++;

		if (mode == "frames") {
			Array frame_paths = anim.get("frames", Array());
			if (frame_paths.is_empty()) {
				memdelete(root);
				Dictionary ret;
				ret["ok"] = false;
				ret["error"] = vformat("Animation '%s' with mode:\"frames\" must provide at least one frame path.", name);
				return ret;
			}
			for (int f = 0; f < frame_paths.size(); f++) {
				const String frame_path = justamcp_resolve_project_path(String(frame_paths[f]));
				Ref<Texture2D> tex = ResourceLoader::load(frame_path);
				if (tex.is_null()) {
					memdelete(root);
					Dictionary ret;
					ret["ok"] = false;
					ret["error"] = vformat("Animation '%s': failed to load frame '%s'", name, frame_path);
					return ret;
				}
				frames->add_frame(name, tex);
				total_frames++;
			}
			continue;
		}

		const String atlas_path = justamcp_resolve_project_path(anim.get("atlas", ""));
		if (atlas_path.is_empty()) {
			memdelete(root);
			Dictionary ret;
			ret["ok"] = false;
			ret["error"] = vformat("Animation '%s' with mode:\"generated_atlas\" is missing 'atlas'.", name);
			return ret;
		}
		Ref<Texture2D> atlas_tex = ResourceLoader::load(atlas_path);
		if (atlas_tex.is_null()) {
			memdelete(root);
			Dictionary ret;
			ret["ok"] = false;
			ret["error"] = vformat("Animation '%s': failed to load atlas '%s'", name, atlas_path);
			return ret;
		}
		Dictionary meta = anim;
		if (!anim.has("frame_width") || !anim.has("columns")) {
			Dictionary meta_err = _load_atlas_metadata(atlas_path, meta);
			if (!meta_err.is_empty()) {
				memdelete(root);
				return meta_err;
			}
		}
		const int frame_w = int(meta.get("frame_width", 0));
		const int frame_h = int(meta.get("frame_height", 0));
		const int columns = int(meta.get("columns", 0));
		const int rows = int(meta.get("rows", 0));
		if (frame_w <= 0 || frame_h <= 0 || columns <= 0 || rows <= 0) {
			memdelete(root);
			Dictionary ret;
			ret["ok"] = false;
			ret["error"] = vformat("Animation '%s': metadata must include positive frame_width, frame_height, columns, and rows.", name);
			return ret;
		}
		for (int row = 0; row < rows; row++) {
			for (int col = 0; col < columns; col++) {
				Ref<AtlasTexture> region;
				region.instantiate();
				region->set_atlas(atlas_tex);
				region->set_region(Rect2(col * frame_w, row * frame_h, frame_w, frame_h));
				frames->add_frame(name, region);
				total_frames++;
			}
		}
	}

	sprite->set_sprite_frames(frames);
	Dictionary save_err = justamcp_save_scene_root(root, file_path, true);
	if (!save_err.is_empty()) {
		return save_err;
	}
	Dictionary ret;
	ret["ok"] = true;
	ret["message"] = vformat("SpriteFrames configured with %d animation(s) and %d total frame(s).", anim_count, total_frames);
	ret["animations"] = anim_count;
	ret["frames"] = total_frames;
	return ret;
}
