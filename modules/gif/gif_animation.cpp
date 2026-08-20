/**************************************************************************/
/*  gif_animation.cpp                                                     */
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

#include "gif_animation.h"

#include "gif_decode.h"
#include "gif_encode.h"

#include "core/io/file_access.h"
#include "scene/resources/image_texture.h"
#include "scene/resources/sprite_frames.h"
#include "servers/rendering_server.h"

static int _meta_int(const PackedInt32Array &p_arr, int p_index, int p_def) {
	if (p_arr.is_empty()) {
		return p_def;
	}
	return p_arr[MIN(p_index, p_arr.size() - 1)];
}

static Vector2 _meta_vec2(const PackedVector2Array &p_arr, int p_index) {
	if (p_arr.is_empty()) {
		return Vector2();
	}
	return p_arr[MIN(p_index, p_arr.size() - 1)];
}

static bool _meta_bool(const PackedByteArray &p_arr, int p_index) {
	if (p_arr.is_empty()) {
		return false;
	}
	return p_arr[MIN(p_index, p_arr.size() - 1)] != 0;
}

void GIFAnimation::_clear_frames() {
	source_frames.clear();
	baked_frames.clear();
	baked_textures.clear();
	source_textures.clear();
	delays.clear();
	disposals.clear();
	positions.clear();
	transparent_colors.clear();
	has_transparency.clear();
}

void GIFAnimation::_compact_trailing_duplicates() {
	auto compact_int = [](PackedInt32Array &p_arr) {
		while (p_arr.size() > 1 && p_arr[p_arr.size() - 1] == p_arr[p_arr.size() - 2]) {
			p_arr.resize(p_arr.size() - 1);
		}
	};
	auto compact_byte = [](PackedByteArray &p_arr) {
		while (p_arr.size() > 1 && p_arr[p_arr.size() - 1] == p_arr[p_arr.size() - 2]) {
			p_arr.resize(p_arr.size() - 1);
		}
	};
	auto compact_vec = [](PackedVector2Array &p_arr) {
		while (p_arr.size() > 1 && p_arr[p_arr.size() - 1] == p_arr[p_arr.size() - 2]) {
			p_arr.resize(p_arr.size() - 1);
		}
	};
	compact_int(delays);
	compact_int(disposals);
	compact_int(transparent_colors);
	compact_byte(has_transparency);
	compact_vec(positions);
}

static Ref<Texture2D> _gif_make_texture(const Ref<Image> &p_image) {
	if (p_image.is_null() || p_image->is_empty() || RenderingServer::get_singleton() == nullptr) {
		return Ref<Texture2D>();
	}
	return ImageTexture::create_from_image(p_image);
}

void GIFAnimation::_rebuild_textures() {
	source_textures.clear();
	baked_textures.clear();
}

void GIFAnimation::_ensure_baked() {
	if (display_mode == DISPLAY_BAKED && baked_frames.size() != source_frames.size()) {
		bake_frames();
	}
}

void GIFAnimation::apply_decoded(const GIFDecoded &p_decoded) {
	_clear_frames();
	canvas_size = p_decoded.canvas_size;
	loop_count = p_decoded.loop_count;
	bool needs_bake = false;
	for (int i = 0; i < p_decoded.frames.size(); i++) {
		const GIFDecodedFrame &f = p_decoded.frames[i];
		source_frames.push_back(f.image);
		delays.push_back(f.delay_cs);
		disposals.push_back(f.disposal);
		positions.push_back(f.position);
		transparent_colors.push_back(f.transparent_color);
		has_transparency.push_back(f.has_transparency ? 1 : 0);
		if (f.has_transparency || f.position != Vector2i() || f.image.is_null() || f.image->get_size() != canvas_size) {
			needs_bake = true;
		}
	}
	_compact_trailing_duplicates();
	if (needs_bake) {
		display_mode = DISPLAY_BAKED;
		bake_frames();
	} else {
		display_mode = DISPLAY_SOURCE;
		_rebuild_textures();
	}
	emit_changed();
	emit_signal(SNAME("frames_changed"));
}

Error GIFAnimation::load_from_path(const String &p_path) {
	Error err = OK;
	Ref<FileAccess> f = FileAccess::open(p_path, FileAccess::READ, &err);
	ERR_FAIL_COND_V_MSG(err != OK || f.is_null(), err == OK ? ERR_CANT_OPEN : err, vformat("Cannot open GIF: '%s'.", p_path));
	const uint64_t len = f->get_length();
	ERR_FAIL_COND_V(len == 0 || len > 0x7FFFFFFF, ERR_FILE_CORRUPT);
	PackedByteArray buf;
	buf.resize(int(len));
	f->get_buffer(buf.ptrw(), len);
	return load_from_buffer(buf);
}

Error GIFAnimation::load_from_buffer(const PackedByteArray &p_buffer) {
	ERR_FAIL_COND_V(p_buffer.is_empty(), ERR_INVALID_PARAMETER);
	GIFDecoded decoded;
	const Error err = gif_decode_buffer(p_buffer.ptr(), p_buffer.size(), decoded, false);
	if (err != OK) {
		return err;
	}
	apply_decoded(decoded);
	return OK;
}

Error GIFAnimation::save_to_path(const String &p_path) const {
	return gif_encode_write_file(p_path, save_to_buffer());
}

PackedByteArray GIFAnimation::save_to_buffer() const {
	Vector<GIFEncodeFrame> frames;
	const_cast<GIFAnimation *>(this)->_ensure_baked();
	const bool use_baked = display_mode == DISPLAY_BAKED && !baked_frames.is_empty();
	const int count = get_frame_count();
	frames.resize(count);
	for (int i = 0; i < count; i++) {
		Ref<Image> img = use_baked ? get_baked_image(i) : get_source_image(i);
		if (img.is_null()) {
			return PackedByteArray();
		}
		if (!use_baked && (img->get_size() != canvas_size || get_frame_position(i) != Vector2i())) {
			Ref<Image> canvas = Image::create_empty(canvas_size.x, canvas_size.y, false, Image::FORMAT_RGBA8);
			canvas->fill(Color(0, 0, 0, 0));
			canvas->blit_rect(img, Rect2i(Vector2i(), img->get_size()), get_frame_position(i));
			img = canvas;
		}
		frames.write[i].image = img;
		frames.write[i].delay_cs = get_frame_delay(i);
	}
	PackedByteArray out;
	if (gif_encode_frames(frames, loop_count, dither, true, out) != OK) {
		return PackedByteArray();
	}
	return out;
}

Error GIFAnimation::bake_frames() {
	baked_frames.clear();
	baked_textures.clear();
	const int count = source_frames.size();
	if (count == 0 || canvas_size.x <= 0 || canvas_size.y <= 0) {
		return ERR_UNCONFIGURED;
	}

	Ref<Image> canvas = Image::create_empty(canvas_size.x, canvas_size.y, false, Image::FORMAT_RGBA8);
	canvas->fill(Color(0, 0, 0, 0));
	Ref<Image> previous = canvas->duplicate();

	for (int i = 0; i < count; i++) {
		const Disposal disposal = get_frame_disposal(i);
		if (disposal == DISPOSAL_RESTORE_PREVIOUS) {
			previous = canvas->duplicate();
		}

		const Ref<Image> src = source_frames[i];
		const Vector2i pos = get_frame_position(i);
		if (src.is_valid()) {
			Ref<Image> src_rgba = src->duplicate();
			if (src_rgba->is_compressed()) {
				src_rgba->decompress();
			}
			if (src_rgba->get_format() != Image::FORMAT_RGBA8) {
				src_rgba->convert(Image::FORMAT_RGBA8);
			}
			const Rect2i src_rect(Vector2i(), src_rgba->get_size());
			if (get_frame_has_transparency(i)) {
				canvas->blend_rect(src_rgba, src_rect, pos);
			} else {
				canvas->blit_rect(src_rgba, src_rect, pos);
			}
		}

		Ref<Image> baked = canvas->duplicate();
		if (bake_compress) {
			baked->compress(Image::COMPRESS_S3TC);
		}
		baked_frames.push_back(baked);

		if (disposal == DISPOSAL_RESTORE_BACKGROUND) {
			const Vector2i fpos = get_frame_position(i);
			const Vector2i fsize = src.is_valid() ? src->get_size() : canvas_size;
			canvas->fill_rect(Rect2i(fpos, fsize), Color(0, 0, 0, 0));
		} else if (disposal == DISPOSAL_RESTORE_PREVIOUS) {
			canvas = previous->duplicate();
		}
	}

	_rebuild_textures();
	emit_signal(SNAME("rebaked"));
	return OK;
}

int GIFAnimation::get_frame_count() const {
	return source_frames.size();
}

int GIFAnimation::wrap_frame(int p_frame) const {
	const int count = get_frame_count();
	if (count <= 0) {
		return 0;
	}
	int f = p_frame % count;
	if (f < 0) {
		f += count;
	}
	return f;
}

Ref<Texture2D> GIFAnimation::get_active_texture(int p_frame) {
	_ensure_baked();
	const int i = wrap_frame(p_frame);
	if (display_mode == DISPLAY_BAKED) {
		if (i < 0 || i >= baked_frames.size()) {
			return Ref<Texture2D>();
		}
		if (baked_textures.size() != baked_frames.size()) {
			baked_textures.resize(baked_frames.size());
		}
		if (baked_textures[i].is_null()) {
			baked_textures.write[i] = _gif_make_texture(baked_frames[i]);
		}
		return baked_textures[i];
	}
	if (i < 0 || i >= source_frames.size()) {
		return Ref<Texture2D>();
	}
	if (source_textures.size() != source_frames.size()) {
		source_textures.resize(source_frames.size());
	}
	if (source_textures[i].is_null()) {
		source_textures.write[i] = _gif_make_texture(source_frames[i]);
	}
	return source_textures[i];
}

Ref<Image> GIFAnimation::get_source_image(int p_frame) const {
	ERR_FAIL_INDEX_V(p_frame, source_frames.size(), Ref<Image>());
	return source_frames[p_frame];
}

Ref<Image> GIFAnimation::get_baked_image(int p_frame) const {
	if (p_frame < 0 || p_frame >= baked_frames.size()) {
		return Ref<Image>();
	}
	return baked_frames[p_frame];
}

int GIFAnimation::get_frame_delay(int p_frame) const {
	return MAX(1, _meta_int(delays, p_frame, 10));
}

double GIFAnimation::get_frame_delay_sec(int p_frame) const {
	return MAX(0.01, get_frame_delay(p_frame) / 100.0);
}

GIFAnimation::Disposal GIFAnimation::get_frame_disposal(int p_frame) const {
	return Disposal(CLAMP(_meta_int(disposals, p_frame, 0), 0, 3));
}

Vector2i GIFAnimation::get_frame_position(int p_frame) const {
	return Vector2i(_meta_vec2(positions, p_frame));
}

int GIFAnimation::get_frame_transparent_color(int p_frame) const {
	return _meta_int(transparent_colors, p_frame, -1);
}

bool GIFAnimation::get_frame_has_transparency(int p_frame) const {
	return _meta_bool(has_transparency, p_frame);
}

void GIFAnimation::set_canvas_size(const Vector2i &p_size) {
	canvas_size = p_size;
}

Vector2i GIFAnimation::get_canvas_size() const {
	return canvas_size;
}

void GIFAnimation::set_loop_count(int p_count) {
	loop_count = MAX(0, p_count);
}

int GIFAnimation::get_loop_count() const {
	return loop_count;
}

void GIFAnimation::set_display_mode(DisplayMode p_mode) {
	display_mode = p_mode;
	if (p_mode == DISPLAY_BAKED) {
		_ensure_baked();
	}
}

GIFAnimation::DisplayMode GIFAnimation::get_display_mode() const {
	return display_mode;
}

void GIFAnimation::set_bake_storage(BakeStorage p_storage) {
	bake_storage = p_storage;
}

GIFAnimation::BakeStorage GIFAnimation::get_bake_storage() const {
	return bake_storage;
}

void GIFAnimation::set_bake_compress(bool p_compress) {
	bake_compress = p_compress;
}

bool GIFAnimation::get_bake_compress() const {
	return bake_compress;
}

void GIFAnimation::set_dither(bool p_dither) {
	dither = p_dither;
}

bool GIFAnimation::get_dither() const {
	return dither;
}

void GIFAnimation::add_source_frame(const Ref<Image> &p_image, int p_delay_cs, Disposal p_disposal, const Vector2i &p_position) {
	ERR_FAIL_COND(p_image.is_null());
	if (canvas_size == Vector2i()) {
		canvas_size = p_image->get_size();
	}
	source_frames.push_back(p_image);
	delays.push_back(MAX(1, p_delay_cs));
	disposals.push_back(int(p_disposal));
	positions.push_back(p_position);
	transparent_colors.push_back(-1);
	has_transparency.push_back(p_image->detect_alpha() != Image::ALPHA_NONE ? 1 : 0);
	_rebuild_textures();
	emit_changed();
	emit_signal(SNAME("frames_changed"));
}

Ref<SpriteFrames> GIFAnimation::to_sprite_frames(const StringName &p_animation_name) {
	_ensure_baked();
	Ref<SpriteFrames> frames;
	frames.instantiate();
	if (!frames->has_animation(p_animation_name)) {
		frames->add_animation(p_animation_name);
	} else {
		frames->clear(p_animation_name);
	}
	frames->set_animation_loop(p_animation_name, loop_count == 0);
	frames->set_animation_speed(p_animation_name, 1.0);
	const int count = get_frame_count();
	for (int i = 0; i < count; i++) {
		Ref<Texture2D> tex = get_active_texture(i);
		if (tex.is_null()) {
			Ref<Image> img = display_mode == DISPLAY_BAKED ? get_baked_image(i) : get_source_image(i);
			tex = _gif_make_texture(img);
		}
		frames->add_frame(p_animation_name, tex, float(get_frame_delay_sec(i)));
	}
	return frames;
}

Ref<GIFAnimation> GIFAnimation::from_sprite_frames(const Ref<SpriteFrames> &p_frames, const StringName &p_animation_name) {
	Ref<GIFAnimation> anim;
	anim.instantiate();
	ERR_FAIL_COND_V(p_frames.is_null() || !p_frames->has_animation(p_animation_name), anim);
	const int count = p_frames->get_frame_count(p_animation_name);
	anim->set_loop_count(p_frames->get_animation_loop(p_animation_name) ? 0 : 1);
	for (int i = 0; i < count; i++) {
		Ref<Texture2D> tex = p_frames->get_frame_texture(p_animation_name, i);
		ERR_CONTINUE(tex.is_null());
		Ref<Image> img = tex->get_image();
		ERR_CONTINUE(img.is_null());
		const int delay_cs = MAX(1, int(Math::round(p_frames->get_frame_duration(p_animation_name, i) * 100.0)));
		anim->add_source_frame(img, delay_cs);
	}
	return anim;
}

TypedArray<Image> GIFAnimation::_get_source_frames() const {
	TypedArray<Image> arr;
	arr.resize(source_frames.size());
	for (int i = 0; i < source_frames.size(); i++) {
		arr[i] = source_frames[i];
	}
	return arr;
}

void GIFAnimation::_set_source_frames(const TypedArray<Image> &p_frames) {
	source_frames.clear();
	for (int i = 0; i < p_frames.size(); i++) {
		source_frames.push_back(p_frames[i]);
	}
	if (bake_storage == BAKE_GENERATE_ON_LOAD) {
		baked_frames.clear();
	}
	_rebuild_textures();
}

TypedArray<Image> GIFAnimation::_get_baked_frames() const {
	TypedArray<Image> arr;
	if (bake_storage != BAKE_STORE) {
		return arr;
	}
	arr.resize(baked_frames.size());
	for (int i = 0; i < baked_frames.size(); i++) {
		arr[i] = baked_frames[i];
	}
	return arr;
}

void GIFAnimation::_set_baked_frames(const TypedArray<Image> &p_frames) {
	baked_frames.clear();
	for (int i = 0; i < p_frames.size(); i++) {
		baked_frames.push_back(p_frames[i]);
	}
	_rebuild_textures();
}

void GIFAnimation::_get_property_list(List<PropertyInfo> *p_list) const {
	p_list->push_back(PropertyInfo(Variant::PACKED_INT32_ARRAY, "delays", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_NO_EDITOR | PROPERTY_USAGE_STORAGE));
	p_list->push_back(PropertyInfo(Variant::PACKED_INT32_ARRAY, "disposals", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_NO_EDITOR | PROPERTY_USAGE_STORAGE));
	p_list->push_back(PropertyInfo(Variant::PACKED_VECTOR2_ARRAY, "positions", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_NO_EDITOR | PROPERTY_USAGE_STORAGE));
	p_list->push_back(PropertyInfo(Variant::PACKED_INT32_ARRAY, "transparent_colors", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_NO_EDITOR | PROPERTY_USAGE_STORAGE));
	p_list->push_back(PropertyInfo(Variant::PACKED_BYTE_ARRAY, "has_transparency", PROPERTY_HINT_NONE, "", PROPERTY_USAGE_NO_EDITOR | PROPERTY_USAGE_STORAGE));
}

bool GIFAnimation::_set(const StringName &p_name, const Variant &p_value) {
	if (p_name == SNAME("delays")) {
		delays = p_value;
		return true;
	}
	if (p_name == SNAME("disposals")) {
		disposals = p_value;
		return true;
	}
	if (p_name == SNAME("positions")) {
		positions = p_value;
		return true;
	}
	if (p_name == SNAME("transparent_colors")) {
		transparent_colors = p_value;
		return true;
	}
	if (p_name == SNAME("has_transparency")) {
		has_transparency = p_value;
		return true;
	}
	return false;
}

bool GIFAnimation::_get(const StringName &p_name, Variant &r_ret) const {
	if (p_name == SNAME("delays")) {
		r_ret = delays;
		return true;
	}
	if (p_name == SNAME("disposals")) {
		r_ret = disposals;
		return true;
	}
	if (p_name == SNAME("positions")) {
		r_ret = positions;
		return true;
	}
	if (p_name == SNAME("transparent_colors")) {
		r_ret = transparent_colors;
		return true;
	}
	if (p_name == SNAME("has_transparency")) {
		r_ret = has_transparency;
		return true;
	}
	return false;
}

void GIFAnimation::_bind_methods() {
	ClassDB::bind_method(D_METHOD("load_from_path", "path"), &GIFAnimation::load_from_path);
	ClassDB::bind_method(D_METHOD("load_from_buffer", "buffer"), &GIFAnimation::load_from_buffer);
	ClassDB::bind_method(D_METHOD("save_to_path", "path"), &GIFAnimation::save_to_path);
	ClassDB::bind_method(D_METHOD("save_to_buffer"), &GIFAnimation::save_to_buffer);
	ClassDB::bind_method(D_METHOD("bake_frames"), &GIFAnimation::bake_frames);
	ClassDB::bind_method(D_METHOD("get_frame_count"), &GIFAnimation::get_frame_count);
	ClassDB::bind_method(D_METHOD("wrap_frame", "frame"), &GIFAnimation::wrap_frame);
	ClassDB::bind_method(D_METHOD("get_active_texture", "frame"), &GIFAnimation::get_active_texture);
	ClassDB::bind_method(D_METHOD("get_source_image", "frame"), &GIFAnimation::get_source_image);
	ClassDB::bind_method(D_METHOD("get_baked_image", "frame"), &GIFAnimation::get_baked_image);
	ClassDB::bind_method(D_METHOD("get_frame_delay", "frame"), &GIFAnimation::get_frame_delay);
	ClassDB::bind_method(D_METHOD("get_frame_delay_sec", "frame"), &GIFAnimation::get_frame_delay_sec);
	ClassDB::bind_method(D_METHOD("get_frame_disposal", "frame"), &GIFAnimation::get_frame_disposal);
	ClassDB::bind_method(D_METHOD("get_frame_position", "frame"), &GIFAnimation::get_frame_position);
	ClassDB::bind_method(D_METHOD("get_frame_transparent_color", "frame"), &GIFAnimation::get_frame_transparent_color);
	ClassDB::bind_method(D_METHOD("get_frame_has_transparency", "frame"), &GIFAnimation::get_frame_has_transparency);
	ClassDB::bind_method(D_METHOD("add_source_frame", "image", "delay_cs", "disposal", "position"), &GIFAnimation::add_source_frame, DEFVAL(10), DEFVAL(DISPOSAL_NONE), DEFVAL(Vector2i()));
	ClassDB::bind_method(D_METHOD("to_sprite_frames", "animation_name"), &GIFAnimation::to_sprite_frames, DEFVAL(StringName("default")));
	ClassDB::bind_static_method("GIFAnimation", D_METHOD("from_sprite_frames", "frames", "animation_name"), &GIFAnimation::from_sprite_frames, DEFVAL(StringName("default")));

	ClassDB::bind_method(D_METHOD("set_canvas_size", "size"), &GIFAnimation::set_canvas_size);
	ClassDB::bind_method(D_METHOD("get_canvas_size"), &GIFAnimation::get_canvas_size);
	ClassDB::bind_method(D_METHOD("set_loop_count", "count"), &GIFAnimation::set_loop_count);
	ClassDB::bind_method(D_METHOD("get_loop_count"), &GIFAnimation::get_loop_count);
	ClassDB::bind_method(D_METHOD("set_display_mode", "mode"), &GIFAnimation::set_display_mode);
	ClassDB::bind_method(D_METHOD("get_display_mode"), &GIFAnimation::get_display_mode);
	ClassDB::bind_method(D_METHOD("set_bake_storage", "storage"), &GIFAnimation::set_bake_storage);
	ClassDB::bind_method(D_METHOD("get_bake_storage"), &GIFAnimation::get_bake_storage);
	ClassDB::bind_method(D_METHOD("set_bake_compress", "compress"), &GIFAnimation::set_bake_compress);
	ClassDB::bind_method(D_METHOD("get_bake_compress"), &GIFAnimation::get_bake_compress);
	ClassDB::bind_method(D_METHOD("set_dither", "dither"), &GIFAnimation::set_dither);
	ClassDB::bind_method(D_METHOD("get_dither"), &GIFAnimation::get_dither);
	ClassDB::bind_method(D_METHOD("_get_source_frames"), &GIFAnimation::_get_source_frames);
	ClassDB::bind_method(D_METHOD("_set_source_frames", "frames"), &GIFAnimation::_set_source_frames);
	ClassDB::bind_method(D_METHOD("_get_baked_frames"), &GIFAnimation::_get_baked_frames);
	ClassDB::bind_method(D_METHOD("_set_baked_frames", "frames"), &GIFAnimation::_set_baked_frames);

	ADD_PROPERTY(PropertyInfo(Variant::VECTOR2I, "canvas_size"), "set_canvas_size", "get_canvas_size");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "loop_count", PROPERTY_HINT_RANGE, "0,65535,1"), "set_loop_count", "get_loop_count");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "display_mode", PROPERTY_HINT_ENUM, "Source,Baked"), "set_display_mode", "get_display_mode");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "bake_storage", PROPERTY_HINT_ENUM, "Store,Generate On Load"), "set_bake_storage", "get_bake_storage");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "bake_compress"), "set_bake_compress", "get_bake_compress");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "dither"), "set_dither", "get_dither");
	ADD_PROPERTY(PropertyInfo(Variant::ARRAY, "source_frames", PROPERTY_HINT_ARRAY_TYPE, "Image", PROPERTY_USAGE_NO_EDITOR | PROPERTY_USAGE_STORAGE), "_set_source_frames", "_get_source_frames");
	ADD_PROPERTY(PropertyInfo(Variant::ARRAY, "baked_frames", PROPERTY_HINT_ARRAY_TYPE, "Image", PROPERTY_USAGE_NO_EDITOR | PROPERTY_USAGE_STORAGE), "_set_baked_frames", "_get_baked_frames");

	ADD_SIGNAL(MethodInfo("frames_changed"));
	ADD_SIGNAL(MethodInfo("rebaked"));

	BIND_ENUM_CONSTANT(DISPLAY_SOURCE);
	BIND_ENUM_CONSTANT(DISPLAY_BAKED);
	BIND_ENUM_CONSTANT(BAKE_STORE);
	BIND_ENUM_CONSTANT(BAKE_GENERATE_ON_LOAD);
	BIND_ENUM_CONSTANT(DISPOSAL_NONE);
	BIND_ENUM_CONSTANT(DISPOSAL_DO_NOT_DISPOSE);
	BIND_ENUM_CONSTANT(DISPOSAL_RESTORE_BACKGROUND);
	BIND_ENUM_CONSTANT(DISPOSAL_RESTORE_PREVIOUS);
}
