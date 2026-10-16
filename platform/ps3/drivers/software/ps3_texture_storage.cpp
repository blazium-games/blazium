/**************************************************************************/
/*  ps3_texture_storage.cpp                                               */
/**************************************************************************/
/*                         This file is part of:                          */
/*                             BLAZIUM ENGINE                             */
/*                          https://blazium.app                           */
/**************************************************************************/

#include "ps3_texture_storage.h"

#include "drivers/software/canvas_blit_ps3.h"

PS3TextureStorage *PS3TextureStorage::get_singleton() {
	return static_cast<PS3TextureStorage *>(RendererDummy::TextureStorage::get_singleton());
}

void PS3TextureStorage::_update_render_target(RenderTarget *p_rt) {
	ERR_FAIL_NULL(p_rt);

	if (p_rt->size.x < 1 || p_rt->size.y < 1) {
		return;
	}

	Ref<Image> image = Image::create_empty(p_rt->size.x, p_rt->size.y, false, Image::FORMAT_RGBA8);
	image->fill(p_rt->clear_color);

	if (p_rt->texture.is_valid()) {
		texture_2d_update(p_rt->texture, image);
	} else {
		p_rt->texture = texture_allocate();
		texture_2d_initialize(p_rt->texture, image);
	}
}

Ref<Image> PS3TextureStorage::get_render_target_image(RID p_render_target) const {
	const RenderTarget *rt = render_target_owner.get_or_null(p_render_target);
	ERR_FAIL_NULL_V(rt, Ref<Image>());
	return texture_2d_get(rt->texture);
}

RID PS3TextureStorage::render_target_create() {
	RenderTarget rt;
	rt.used_in_frame = false;
	rt.clear_requested = false;
	return render_target_owner.make_rid(rt);
}

void PS3TextureStorage::render_target_free(RID p_rid) {
	RenderTarget *rt = render_target_owner.get_or_null(p_rid);
	ERR_FAIL_NULL(rt);
	if (rt->texture.is_valid()) {
		texture_free(rt->texture);
	}
	render_target_owner.free(p_rid);
}

void PS3TextureStorage::render_target_set_size(RID p_render_target, int p_width, int p_height, uint32_t p_view_count) {
	RenderTarget *rt = render_target_owner.get_or_null(p_render_target);
	ERR_FAIL_NULL(rt);
	if (rt->size.x != p_width || rt->size.y != p_height) {
		rt->size = Size2i(p_width, p_height);
		_update_render_target(rt);
	}
}

Size2i PS3TextureStorage::render_target_get_size(RID p_render_target) const {
	const RenderTarget *rt = render_target_owner.get_or_null(p_render_target);
	ERR_FAIL_NULL_V(rt, Size2i());
	return rt->size;
}

bool PS3TextureStorage::render_target_was_used(RID p_render_target) const {
	const RenderTarget *rt = render_target_owner.get_or_null(p_render_target);
	ERR_FAIL_NULL_V(rt, false);
	return rt->used_in_frame;
}

void PS3TextureStorage::render_target_set_as_unused(RID p_render_target) {
	RenderTarget *rt = render_target_owner.get_or_null(p_render_target);
	ERR_FAIL_NULL(rt);
	rt->used_in_frame = false;
}

void PS3TextureStorage::render_target_request_clear(RID p_render_target, const Color &p_clear_color) {
	RenderTarget *rt = render_target_owner.get_or_null(p_render_target);
	ERR_FAIL_NULL(rt);
	rt->clear_color = p_clear_color;
	rt->clear_requested = true;
}

bool PS3TextureStorage::render_target_is_clear_requested(RID p_render_target) {
	const RenderTarget *rt = render_target_owner.get_or_null(p_render_target);
	ERR_FAIL_NULL_V(rt, false);
	return rt->clear_requested;
}

Color PS3TextureStorage::render_target_get_clear_request_color(RID p_render_target) {
	const RenderTarget *rt = render_target_owner.get_or_null(p_render_target);
	ERR_FAIL_NULL_V(rt, Color());
	return rt->clear_color;
}

void PS3TextureStorage::render_target_disable_clear_request(RID p_render_target) {
	RenderTarget *rt = render_target_owner.get_or_null(p_render_target);
	ERR_FAIL_NULL(rt);
	rt->clear_requested = false;
}

void PS3TextureStorage::render_target_do_clear_request(RID p_render_target) {
	RenderTarget *rt = render_target_owner.get_or_null(p_render_target);
	ERR_FAIL_NULL(rt);
	if (!rt->clear_requested) {
		return;
	}

	Ref<Image> image = texture_2d_get(rt->texture);
	if (image.is_valid()) {
		image->fill(rt->clear_color);
	}
	rt->clear_requested = false;
}

RID PS3TextureStorage::render_target_get_texture(RID p_render_target) {
	const RenderTarget *rt = render_target_owner.get_or_null(p_render_target);
	ERR_FAIL_NULL_V(rt, RID());
	return rt->texture;
}

void PS3TextureStorage::mark_render_target_used(RID p_render_target) {
	RenderTarget *rt = render_target_owner.get_or_null(p_render_target);
	ERR_FAIL_NULL(rt);
	rt->used_in_frame = true;
}
