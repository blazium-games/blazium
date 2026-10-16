/**************************************************************************/
/*  rasterizer_canvas_ps3.h                                               */
/**************************************************************************/
/*                         This file is part of:                          */
/*                             BLAZIUM ENGINE                             */
/*                          https://blazium.app                           */
/**************************************************************************/

#pragma once

#include "drivers/software/canvas_blit_ps3.h"

#include "core/templates/hash_map.h"
#include "servers/rendering/dummy/storage/texture_storage.h"
#include "servers/rendering/renderer_canvas_render.h"

class RasterizerCanvasPS3 : public RendererCanvasRender {
	struct StoredPolygon {
		Vector<int> indices;
		Vector<Point2> points;
		Vector<Color> colors;
		Vector<Point2> uvs;
	};

	HashMap<PolygonID, StoredPolygon *> polygons;
	PolygonID next_polygon_id = 1;

	void _draw_rect(CanvasBlitPS3::State &p_state, RendererDummy::TextureStorage *p_texture_storage, const Transform2D &p_transform, const Item::CommandRect *p_rect, const Color &p_modulate);
	void _draw_ninepatch(CanvasBlitPS3::State &p_state, RendererDummy::TextureStorage *p_texture_storage, const Transform2D &p_transform, const Item::CommandNinePatch *p_patch, const Color &p_modulate);
	void _draw_polygon(CanvasBlitPS3::State &p_state, RendererDummy::TextureStorage *p_texture_storage, const Transform2D &p_transform, const Item::CommandPolygon *p_polygon, const Color &p_modulate);
	void _draw_primitive(CanvasBlitPS3::State &p_state, RendererDummy::TextureStorage *p_texture_storage, const Transform2D &p_transform, const Item::CommandPrimitive *p_primitive, const Color &p_modulate);

public:
	PolygonID request_polygon(const Vector<int> &p_indices, const Vector<Point2> &p_points, const Vector<Color> &p_colors, const Vector<Point2> &p_uvs = Vector<Point2>(), const Vector<int> &p_bones = Vector<int>(), const Vector<float> &p_weights = Vector<float>()) override;
	void free_polygon(PolygonID p_polygon) override;

	void canvas_render_items(RID p_to_render_target, Item *p_item_list, const Color &p_modulate, Light *p_light_list, Light *p_directional_list, const Transform2D &p_canvas_transform, RS::CanvasItemTextureFilter p_default_filter, RS::CanvasItemTextureRepeat p_default_repeat, bool p_snap_2d_vertices_to_pixel, bool &r_sdf_used, RenderingMethod::RenderInfo *r_render_info = nullptr) override;

	RID light_create() override { return RID(); }
	void light_set_texture(RID p_rid, RID p_texture) override {}
	void light_set_use_shadow(RID p_rid, bool p_enable) override {}
	void light_update_shadow(RID p_rid, int p_shadow_index, const Transform2D &p_light_xform, int p_light_mask, float p_near, float p_far, LightOccluderInstance *p_occluders) override {}
	void light_update_directional_shadow(RID p_rid, int p_shadow_index, const Transform2D &p_light_xform, int p_light_mask, float p_cull_distance, const Rect2 &p_clip_rect, LightOccluderInstance *p_occluders) override {}

	void render_sdf(RID p_render_target, LightOccluderInstance *p_occluders) override {}
	RID occluder_polygon_create() override { return RID(); }
	void occluder_polygon_set_shape(RID p_occluder, const Vector<Vector2> &p_points, bool p_closed) override {}
	void occluder_polygon_set_cull_mode(RID p_occluder, RS::CanvasOccluderPolygonCullMode p_mode) override {}
	void set_shadow_texture_size(int p_size) override {}

	bool free(RID p_rid) override { return true; }
	void update() override {}
	virtual void set_debug_redraw(bool p_enabled, double p_time, const Color &p_color) override {}

	~RasterizerCanvasPS3();
};
