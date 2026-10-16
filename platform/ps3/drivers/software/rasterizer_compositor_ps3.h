/**************************************************************************/
/*  rasterizer_compositor_ps3.h                                           */
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

#include "drivers/software/ps3_texture_storage.h"
#include "drivers/software/rasterizer_canvas_ps3.h"

#include "servers/rendering/dummy/environment/fog.h"
#include "servers/rendering/dummy/environment/gi.h"
#include "servers/rendering/dummy/rasterizer_scene_dummy.h"
#include "servers/rendering/dummy/storage/light_storage.h"
#include "servers/rendering/dummy/storage/material_storage.h"
#include "servers/rendering/dummy/storage/mesh_storage.h"
#include "servers/rendering/dummy/storage/particles_storage.h"
#include "servers/rendering/dummy/storage/utilities.h"
#include "servers/rendering/renderer_compositor.h"

class RasterizerCompositorPS3 : public RendererCompositor {
private:
	uint64_t frame = 1;
	double delta = 0;
	double time = 0;
	bool blitted_this_frame = false;

protected:
	RasterizerCanvasPS3 canvas;
	RendererDummy::Utilities utilities;
	RendererDummy::LightStorage light_storage;
	RendererDummy::MaterialStorage material_storage;
	RendererDummy::MeshStorage mesh_storage;
	RendererDummy::ParticlesStorage particles_storage;
	PS3TextureStorage texture_storage;
	RendererDummy::GI gi;
	RendererDummy::Fog fog;
	RasterizerSceneDummy scene;

	void _blit_image_to_framebuffer(const Ref<Image> &p_image, const Rect2i &p_dst);

public:
	RendererUtilities *get_utilities() override { return &utilities; }
	RendererLightStorage *get_light_storage() override { return &light_storage; }
	RendererMaterialStorage *get_material_storage() override { return &material_storage; }
	RendererMeshStorage *get_mesh_storage() override { return &mesh_storage; }
	RendererParticlesStorage *get_particles_storage() override { return &particles_storage; }
	RendererTextureStorage *get_texture_storage() override { return &texture_storage; }
	RendererGI *get_gi() override { return &gi; }
	RendererFog *get_fog() override { return &fog; }
	RendererCanvasRender *get_canvas() override { return &canvas; }
	RendererSceneRender *get_scene() override { return &scene; }

	void set_boot_image(const Ref<Image> &p_image, const Color &p_color, bool p_scale, bool p_use_filter = true) override {}

	void initialize() override {}
	void begin_frame(double frame_step) override {
		frame++;
		delta = frame_step;
		time += frame_step;
		blitted_this_frame = false;
	}

	void blit_render_targets_to_screen(int p_screen, const BlitToScreen *p_render_targets, int p_amount) override;
	void gl_end_frame(bool p_swap_buffers) override {}
	void end_frame(bool p_swap_buffers) override;
	void finalize() override {}

	static RendererCompositor *_create_current() {
		return memnew(RasterizerCompositorPS3);
	}

	static void make_current() {
		_create_func = _create_current;
		low_end = true;
	}

	uint64_t get_frame_number() const override { return frame; }
	double get_frame_delta_time() const override { return delta; }
	double get_total_time() const override { return time; }
};
