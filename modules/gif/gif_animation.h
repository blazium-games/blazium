/**************************************************************************/
/*  gif_animation.h                                                       */
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

#pragma once

#include "core/io/resource.h"
#include "core/math/vector2i.h"
#include "scene/resources/texture.h"

class ImageTexture;
class SpriteFrames;

class GIFAnimation : public Resource {
	GDCLASS(GIFAnimation, Resource);

public:
	enum DisplayMode {
		DISPLAY_SOURCE,
		DISPLAY_BAKED,
	};

	enum BakeStorage {
		BAKE_STORE,
		BAKE_GENERATE_ON_LOAD,
	};

	enum Disposal {
		DISPOSAL_NONE,
		DISPOSAL_DO_NOT_DISPOSE,
		DISPOSAL_RESTORE_BACKGROUND,
		DISPOSAL_RESTORE_PREVIOUS,
	};

private:
	Vector2i canvas_size;
	int loop_count = 0;
	DisplayMode display_mode = DISPLAY_SOURCE;
	BakeStorage bake_storage = BAKE_GENERATE_ON_LOAD;
	bool bake_compress = false;
	bool dither = true;

	Vector<Ref<Image>> source_frames;
	Vector<Ref<Image>> baked_frames;
	Vector<Ref<Texture2D>> baked_textures;
	Vector<Ref<Texture2D>> source_textures;

	PackedInt32Array delays;
	PackedInt32Array disposals;
	PackedVector2Array positions;
	PackedInt32Array transparent_colors;
	PackedByteArray has_transparency;

	void _compact_trailing_duplicates();
	bool _source_needs_composition() const;
	void _ensure_baked();
	void _rebuild_textures();
	void _clear_frames();

protected:
	static void _bind_methods();
	void _get_property_list(List<PropertyInfo> *p_list) const;
	bool _set(const StringName &p_name, const Variant &p_value);
	bool _get(const StringName &p_name, Variant &r_ret) const;

public:
	Error load_from_path(const String &p_path);
	Error load_from_buffer(const PackedByteArray &p_buffer);
	Error save_to_path(const String &p_path) const;
	PackedByteArray save_to_buffer() const;

	Error bake_frames();
	int get_frame_count() const;
	int wrap_frame(int p_frame) const;
	Ref<Texture2D> get_active_texture(int p_frame);
	Ref<Image> get_source_image(int p_frame) const;
	Ref<Image> get_baked_image(int p_frame) const;

	int get_frame_delay(int p_frame) const;
	double get_frame_delay_sec(int p_frame) const;
	Disposal get_frame_disposal(int p_frame) const;
	Vector2i get_frame_position(int p_frame) const;
	int get_frame_transparent_color(int p_frame) const;
	bool get_frame_has_transparency(int p_frame) const;

	void set_canvas_size(const Vector2i &p_size);
	Vector2i get_canvas_size() const;
	void set_loop_count(int p_count);
	int get_loop_count() const;
	void set_display_mode(DisplayMode p_mode);
	DisplayMode get_display_mode() const;
	void set_bake_storage(BakeStorage p_storage);
	BakeStorage get_bake_storage() const;
	void set_bake_compress(bool p_compress);
	bool get_bake_compress() const;
	void set_dither(bool p_dither);
	bool get_dither() const;

	void add_source_frame(const Ref<Image> &p_image, int p_delay_cs = 10, Disposal p_disposal = DISPOSAL_NONE, const Vector2i &p_position = Vector2i());

	Ref<SpriteFrames> to_sprite_frames(const StringName &p_animation_name = "default");
	static Ref<GIFAnimation> from_sprite_frames(const Ref<SpriteFrames> &p_frames, const StringName &p_animation_name = "default");

	TypedArray<Image> _get_source_frames() const;
	void _set_source_frames(const TypedArray<Image> &p_frames);
	TypedArray<Image> _get_baked_frames() const;
	void _set_baked_frames(const TypedArray<Image> &p_frames);

	void apply_decoded(const struct GIFDecoded &p_decoded);
};

VARIANT_ENUM_CAST(GIFAnimation::DisplayMode);
VARIANT_ENUM_CAST(GIFAnimation::BakeStorage);
VARIANT_ENUM_CAST(GIFAnimation::Disposal);
