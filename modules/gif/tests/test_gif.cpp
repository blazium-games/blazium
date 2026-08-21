/**************************************************************************/
/*  test_gif.cpp                                                          */
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

#include "test_gif.h"

#include "modules/gif/gif_animation.h"
#include "modules/gif/gif_decode.h"
#include "modules/gif/gif_encode.h"
#include "modules/gif/gif_recorder.h"

#include "core/config/project_settings.h"
#include "core/io/image.h"
#include "scene/resources/image_texture.h"
#include "scene/resources/sprite_frames.h"
#include "servers/rendering_server.h"

#include <string.h>

static PackedByteArray _make_solid_gif89a_1x1_red() {
	const uint8_t bytes[] = {
		0x47, 0x49, 0x46, 0x38, 0x39, 0x61,
		0x01, 0x00, 0x01, 0x00,
		0x80, 0x00, 0x00,
		0xFF, 0x00, 0x00,
		0x00, 0x00, 0x00,
		0x21, 0xFF, 0x0B,
		0x4E, 0x45, 0x54, 0x53, 0x43, 0x41, 0x50, 0x45, 0x32, 0x2E, 0x30,
		0x03, 0x01, 0x00, 0x00, 0x00,
		0x21, 0xF9, 0x04, 0x01, 0x0A, 0x00, 0x01, 0x00,
		0x2C, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x01, 0x00, 0x00,
		0x02, 0x02, 0x44, 0x01, 0x00,
		0x3B
	};
	PackedByteArray out;
	out.resize(sizeof(bytes));
	memcpy(out.ptrw(), bytes, sizeof(bytes));
	return out;
}

static PackedByteArray _make_interlaced_2x4() {
	Ref<Image> img = Image::create_empty(2, 4, false, Image::FORMAT_RGBA8);
	img->fill(Color(1, 0, 0));
	img->set_pixel(0, 1, Color(0, 0, 1));
	img->set_pixel(1, 1, Color(0, 0, 1));
	img->set_pixel(0, 3, Color(0, 0, 1));
	img->set_pixel(1, 3, Color(0, 0, 1));
	Vector<uint8_t> encoded;
	gif_encode_still(img, encoded, false, true);
	return encoded;
}

void test_gif_decode_basic() {
	const PackedByteArray buf = _make_solid_gif89a_1x1_red();
	GIFDecoded decoded;
	CHECK(gif_decode_buffer(buf.ptr(), buf.size(), decoded) == OK);
	REQUIRE(decoded.frames.size() == 1);
	CHECK(decoded.canvas_size == Vector2i(1, 1));
	CHECK(decoded.loop_count == 0);
	CHECK(decoded.frames[0].delay_cs == 10);
	CHECK(decoded.frames[0].has_transparency);
	REQUIRE(decoded.frames[0].image.is_valid());
	const Color c = decoded.frames[0].image->get_pixel(0, 0);
	CHECK(c.r > 0.9f);
	CHECK(c.g < 0.1f);
	CHECK(c.b < 0.1f);
}

void test_gif_encode_roundtrip() {
	Ref<Image> still = Image::create_empty(4, 3, false, Image::FORMAT_RGBA8);
	REQUIRE(still.is_valid());
	still->fill(Color(0.2f, 0.4f, 0.8f));
	Vector<uint8_t> still_buf;
	const Error enc_err = gif_encode_still(still, still_buf, false);
	CHECK(enc_err == OK);
	CHECK(still_buf.size() > 0);

	Ref<Image> loaded;
	CHECK(gif_decode_first_frame(still_buf.ptr(), still_buf.size(), loaded) == OK);
	REQUIRE(loaded.is_valid());
	CHECK(loaded->get_width() == 4);
	CHECK(loaded->get_height() == 3);

	Ref<Image> a = Image::create_empty(3, 2, false, Image::FORMAT_RGBA8);
	a->fill(Color(1, 0, 0));
	Ref<Image> b = Image::create_empty(3, 2, false, Image::FORMAT_RGBA8);
	b->fill(Color(0, 1, 0));
	Vector<GIFEncodeFrame> frames;
	GIFEncodeFrame fa;
	fa.image = a;
	fa.delay_cs = 20;
	GIFEncodeFrame fb;
	fb.image = b;
	fb.delay_cs = 30;
	frames.push_back(fa);
	frames.push_back(fb);
	Vector<uint8_t> anim_buf;
	CHECK(gif_encode_frames(frames, 0, false, true, anim_buf) == OK);

	Ref<GIFAnimation> anim;
	anim.instantiate();
	CHECK(anim->load_from_buffer(anim_buf) == OK);
	CHECK(anim->get_frame_count() == 2);
	CHECK(anim->get_frame_delay(0) >= 1);
	CHECK(anim->get_loop_count() == 0);
}

void test_gif_disposal_compose() {
	Ref<GIFAnimation> anim;
	anim.instantiate();
	anim->set_canvas_size(Vector2i(2, 2));

	Ref<Image> first = Image::create_empty(2, 2, false, Image::FORMAT_RGBA8);
	first->fill(Color(1, 0, 0, 1));
	anim->add_source_frame(first, 10, GIFAnimation::DISPOSAL_RESTORE_BACKGROUND, Vector2i());

	Ref<Image> second = Image::create_empty(1, 1, false, Image::FORMAT_RGBA8);
	second->fill(Color(0, 1, 0, 1));
	anim->add_source_frame(second, 10, GIFAnimation::DISPOSAL_NONE, Vector2i(0, 0));

	CHECK(anim->bake_frames() == OK);
	Ref<Image> baked0 = anim->get_baked_image(0);
	Ref<Image> baked1 = anim->get_baked_image(1);
	REQUIRE(baked0.is_valid());
	REQUIRE(baked1.is_valid());
	CHECK(baked0->get_pixel(0, 0).r > 0.9f);
	CHECK(baked1->get_pixel(0, 0).g > 0.9f);
	CHECK(baked1->get_pixel(1, 1).a < 0.1f);

	Ref<GIFAnimation> prev;
	prev.instantiate();
	prev->set_canvas_size(Vector2i(2, 2));
	Ref<Image> p0 = Image::create_empty(2, 2, false, Image::FORMAT_RGBA8);
	p0->fill(Color(0, 0, 1, 1));
	prev->add_source_frame(p0, 10, GIFAnimation::DISPOSAL_DO_NOT_DISPOSE, Vector2i());
	Ref<Image> p1 = Image::create_empty(2, 2, false, Image::FORMAT_RGBA8);
	p1->fill(Color(1, 1, 0, 1));
	prev->add_source_frame(p1, 10, GIFAnimation::DISPOSAL_RESTORE_PREVIOUS, Vector2i());
	Ref<Image> p2 = Image::create_empty(1, 1, false, Image::FORMAT_RGBA8);
	p2->fill(Color(1, 0, 1, 1));
	prev->add_source_frame(p2, 10, GIFAnimation::DISPOSAL_NONE, Vector2i(0, 0));
	CHECK(prev->bake_frames() == OK);
	Ref<Image> b2 = prev->get_baked_image(2);
	REQUIRE(b2.is_valid());
	CHECK(b2->get_pixel(1, 1).b > 0.9f);
}

void test_gif_interlaced_decode() {
	const PackedByteArray buf = _make_interlaced_2x4();
	GIFDecoded decoded;
	const Error err = gif_decode_buffer(buf.ptr(), buf.size(), decoded);
	CHECK(err == OK);
	if (err == OK) {
		CHECK(decoded.frames.size() >= 1);
		CHECK(decoded.canvas_size.x == 2);
	}
}

void test_gif_corrupt_buffer() {
	PackedByteArray bad;
	bad.resize(4);
	bad.write[0] = 'G';
	bad.write[1] = 'I';
	bad.write[2] = 'F';
	bad.write[3] = '8';
	GIFDecoded decoded;
	CHECK(gif_decode_buffer(bad.ptr(), bad.size(), decoded) == ERR_FILE_CORRUPT);

	Ref<GIFAnimation> anim;
	anim.instantiate();
	CHECK(anim->load_from_buffer(bad) == ERR_FILE_CORRUPT);
}

void test_gif_image_hooks() {
	Ref<Image> src = Image::create_empty(2, 2, false, Image::FORMAT_RGBA8);
	src->fill(Color(0.1f, 0.2f, 0.3f));
	const PackedByteArray buf = src->save_gif_to_buffer();
	CHECK(buf.size() > 0);
	Ref<Image> dst;
	dst.instantiate();
	CHECK(dst->load_gif_from_buffer(buf) == OK);
	CHECK(dst->get_width() == 2);
	CHECK(dst->get_height() == 2);

	Ref<Image> clear = Image::create_empty(4, 4, false, Image::FORMAT_RGBA8);
	const PackedByteArray clear_buf = clear->save_gif_to_buffer();
	CHECK(clear_buf.size() > 0);
	Ref<Image> clear_dst;
	clear_dst.instantiate();
	CHECK(clear_dst->load_gif_from_buffer(clear_buf) == OK);
	CHECK(clear_dst->get_width() == 4);
	CHECK(clear_dst->get_height() == 4);
}

void test_gif_sprite_frames_convert() {
	Ref<GIFAnimation> anim;
	anim.instantiate();
	Ref<Image> img = Image::create_empty(2, 2, false, Image::FORMAT_RGBA8);
	img->fill(Color(1, 0, 0));
	anim->add_source_frame(img, 15);
	Ref<Image> img2 = Image::create_empty(2, 2, false, Image::FORMAT_RGBA8);
	img2->fill(Color(0, 0, 1));
	anim->add_source_frame(img2, 25);
	Ref<SpriteFrames> sf = anim->to_sprite_frames("default");
	REQUIRE(sf.is_valid());
	CHECK(sf->get_frame_count("default") == 2);
	if (RenderingServer::get_singleton() && sf->get_frame_texture("default", 0).is_valid()) {
		Ref<GIFAnimation> back = GIFAnimation::from_sprite_frames(sf, "default");
		REQUIRE(back.is_valid());
		CHECK(back->get_frame_count() == 2);
	}
}

void test_gif_decode_caps() {
	if (!ProjectSettings::get_singleton()) {
		return;
	}
	const Variant old_frames = ProjectSettings::get_singleton()->get_setting("blazium/gif/max_frames");
	ProjectSettings::get_singleton()->set_setting("blazium/gif/max_frames", 1);

	Ref<Image> a = Image::create_empty(2, 2, false, Image::FORMAT_RGBA8);
	a->fill(Color(1, 0, 0));
	Ref<Image> b = Image::create_empty(2, 2, false, Image::FORMAT_RGBA8);
	b->fill(Color(0, 1, 0));
	Vector<GIFEncodeFrame> frames;
	GIFEncodeFrame fa;
	fa.image = a;
	GIFEncodeFrame fb;
	fb.image = b;
	frames.push_back(fa);
	frames.push_back(fb);
	Vector<uint8_t> buf;
	REQUIRE(gif_encode_frames(frames, 0, false, false, buf) == OK);
	GIFDecoded decoded;
	CHECK(gif_decode_buffer(buf.ptr(), buf.size(), decoded) == ERR_OUT_OF_MEMORY);

	ProjectSettings::get_singleton()->set_setting("blazium/gif/max_frames", old_frames);
}

void test_gif_active_texture_without_rebake() {
	Ref<GIFAnimation> anim;
	anim.instantiate();
	CHECK(anim->load_from_buffer(_make_solid_gif89a_1x1_red()) == OK);
	CHECK(anim->get_frame_count() >= 1);

	anim->set_bake_storage(GIFAnimation::BAKE_GENERATE_ON_LOAD);
	anim->set_display_mode(GIFAnimation::DISPLAY_BAKED);
	anim->_set_source_frames(anim->_get_source_frames());

	if (!RenderingServer::get_singleton()) {
		return;
	}
	Ref<Texture2D> tex = anim->get_active_texture(0);
	REQUIRE(tex.is_valid());
	CHECK(tex->get_width() == anim->get_canvas_size().x);
	CHECK(tex->get_height() == anim->get_canvas_size().y);
}

void test_gif_recorder_add_frame() {
	Ref<GIFRecorder> rec;
	rec.instantiate();
	rec->set_fps(10);
	Ref<Image> a = Image::create_empty(8, 8, false, Image::FORMAT_RGBA8);
	a->fill(Color(1, 0, 0));
	Ref<Image> b = Image::create_empty(8, 8, false, Image::FORMAT_RGBA8);
	b->fill(Color(0, 0, 1));
	CHECK(rec->add_frame(a) == OK);
	CHECK(rec->add_frame(b) == OK);
	Ref<GIFAnimation> anim = rec->stop();
	REQUIRE(anim.is_valid());
	CHECK(anim->get_frame_count() == 2);
}
