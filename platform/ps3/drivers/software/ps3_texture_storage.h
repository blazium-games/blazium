/**************************************************************************/
/*  ps3_texture_storage.h                                                 */
/**************************************************************************/
/*                         This file is part of:                          */
/*                             BLAZIUM ENGINE                             */
/*                          https://blazium.app                           */
/**************************************************************************/

#pragma once

#include "servers/rendering/dummy/storage/texture_storage.h"

#include "core/templates/rid_owner.h"

class PS3TextureStorage : public RendererDummy::TextureStorage {
	struct RenderTarget {
		Size2i size;
		RID texture;
		Color clear_color = Color(0, 0, 0, 1);
		bool clear_requested = false;
		bool used_in_frame = false;
	};

	mutable RID_Owner<RenderTarget, true> render_target_owner;

	void _update_render_target(RenderTarget *p_rt);

public:
	static PS3TextureStorage *get_singleton();

	Ref<Image> get_render_target_image(RID p_render_target) const;
	void mark_render_target_used(RID p_render_target);

	virtual RID render_target_create() override;
	virtual void render_target_free(RID p_rid) override;
	virtual void render_target_set_size(RID p_render_target, int p_width, int p_height, uint32_t p_view_count) override;
	virtual Size2i render_target_get_size(RID p_render_target) const override;
	virtual bool render_target_was_used(RID p_render_target) const override;
	virtual void render_target_set_as_unused(RID p_render_target) override;
	virtual void render_target_request_clear(RID p_render_target, const Color &p_clear_color) override;
	virtual bool render_target_is_clear_requested(RID p_render_target) override;
	virtual Color render_target_get_clear_request_color(RID p_render_target) override;
	virtual void render_target_disable_clear_request(RID p_render_target) override;
	virtual void render_target_do_clear_request(RID p_render_target) override;
	virtual RID render_target_get_texture(RID p_render_target) override;
};
