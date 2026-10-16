/**************************************************************************/
/*  rasterizer_canvas_ps3.cpp                                             */
/**************************************************************************/
/*                         This file is part of:                          */
/*                             BLAZIUM ENGINE                             */
/*                          https://blazium.app                           */
/**************************************************************************/

#include "rasterizer_canvas_ps3.h"

#include "drivers/software/canvas_blit_ps3.h"
#include "drivers/software/ps3_texture_storage.h"

#include "core/math/geometry_2d.h"
#include "core/math/math_funcs.h"
#include "servers/rendering/dummy/storage/texture_storage.h"
#include "servers/rendering/renderer_compositor.h"

static Vector3 _barycentric(const Point2 &p_point, const Point2 &p_a, const Point2 &p_b, const Point2 &p_c) {
	const Vector2 v0 = p_b - p_a;
	const Vector2 v1 = p_c - p_a;
	const Vector2 v2 = p_point - p_a;
	const float d00 = v0.dot(v0);
	const float d01 = v0.dot(v1);
	const float d11 = v1.dot(v1);
	const float d20 = v2.dot(v0);
	const float d21 = v2.dot(v1);
	const float denom = d00 * d11 - d01 * d01;
	if (Math::is_zero_approx(denom)) {
		return Vector3();
	}
	const float v = (d11 * d20 - d01 * d21) / denom;
	const float w = (d00 * d21 - d01 * d20) / denom;
	const float u = 1.0f - v - w;
	return Vector3(u, v, w);
}

static Rect2i _transform_rect(const Transform2D &p_xform, const Rect2 &p_rect) {
	const Point2 p0 = p_xform.xform(p_rect.position);
	const Point2 p1 = p_xform.xform(p_rect.position + Vector2(p_rect.size.x, 0));
	const Point2 p2 = p_xform.xform(p_rect.position + Vector2(0, p_rect.size.y));
	const Point2 p3 = p_xform.xform(p_rect.position + p_rect.size);
	Rect2 bounds(p0, Size2());
	bounds.expand_to(p1);
	bounds.expand_to(p2);
	bounds.expand_to(p3);
	return Rect2i(bounds.position.floor(), bounds.size.ceil());
}

RendererCanvasRender::PolygonID RasterizerCanvasPS3::request_polygon(const Vector<int> &p_indices, const Vector<Point2> &p_points, const Vector<Color> &p_colors, const Vector<Point2> &p_uvs, const Vector<int> &p_bones, const Vector<float> &p_weights) {
	StoredPolygon *poly = memnew(StoredPolygon);
	poly->indices = p_indices;
	poly->points = p_points;
	poly->colors = p_colors;
	poly->uvs = p_uvs;
	const PolygonID id = next_polygon_id++;
	polygons[id] = poly;
	return id;
}

void RasterizerCanvasPS3::free_polygon(PolygonID p_polygon) {
	StoredPolygon **poly = polygons.getptr(p_polygon);
	if (poly) {
		memdelete(*poly);
		polygons.erase(p_polygon);
	}
}

void RasterizerCanvasPS3::_draw_rect(CanvasBlitPS3::State &p_state, RendererDummy::TextureStorage *p_texture_storage, const Transform2D &p_transform, const Item::CommandRect *p_rect, const Color &p_modulate) {
	const Color color = p_rect->modulate * p_modulate;
	const Rect2i dst = _transform_rect(p_transform, p_rect->rect);

	if (!p_rect->texture.is_valid()) {
		CanvasBlitPS3::fill_rect(p_state, dst, CanvasBlitPS3::color_to_argb(color));
		return;
	}

	const Ref<Image> image = p_texture_storage->texture_2d_get(p_rect->texture);
	if (image.is_null()) {
		CanvasBlitPS3::fill_rect(p_state, dst, CanvasBlitPS3::color_to_argb(color));
		return;
	}

	Rect2i src = Rect2i(p_rect->source.position.floor(), p_rect->source.size.ceil());
	const bool flip_h = (p_rect->flags & CANVAS_RECT_FLIP_H) != 0;
	const bool flip_v = (p_rect->flags & CANVAS_RECT_FLIP_V) != 0;

	if ((p_rect->flags & CANVAS_RECT_TILE) != 0) {
		for (int y = dst.position.y; y < dst.position.y + dst.size.y; y += src.size.y) {
			for (int x = dst.position.x; x < dst.position.x + dst.size.x; x += src.size.x) {
				Rect2i tile_dst(x, y, src.size.x, src.size.y);
				tile_dst = tile_dst.intersection(dst);
				if (tile_dst.size.x > 0 && tile_dst.size.y > 0) {
					CanvasBlitPS3::blit_image_rect(p_state, image, tile_dst, src, color, flip_h, flip_v);
				}
			}
		}
		return;
	}

	CanvasBlitPS3::blit_image_rect(p_state, image, dst, src, color, flip_h, flip_v);
}

void RasterizerCanvasPS3::_draw_ninepatch(CanvasBlitPS3::State &p_state, RendererDummy::TextureStorage *p_texture_storage, const Transform2D &p_transform, const Item::CommandNinePatch *p_patch, const Color &p_modulate) {
	if (!p_patch->texture.is_valid()) {
		const Rect2i dst = _transform_rect(p_transform, p_patch->rect);
		CanvasBlitPS3::fill_rect(p_state, dst, CanvasBlitPS3::color_to_argb(p_patch->color * p_modulate));
		return;
	}

	Item::CommandRect rect;
	rect.rect = p_patch->rect;
	rect.source = p_patch->source;
	rect.modulate = p_patch->color;
	rect.texture = p_patch->texture;
	_draw_rect(p_state, p_texture_storage, p_transform, &rect, p_modulate);
}

void RasterizerCanvasPS3::_draw_polygon(CanvasBlitPS3::State &p_state, RendererDummy::TextureStorage *p_texture_storage, const Transform2D &p_transform, const Item::CommandPolygon *p_polygon, const Color &p_modulate) {
	const StoredPolygon *const *poly = polygons.getptr(p_polygon->polygon.polygon_id);
	if (!poly || !*poly) {
		return;
	}

	const StoredPolygon &stored = **poly;
	const int index_count = stored.indices.size();
	if (index_count < 3) {
		return;
	}

	Ref<Image> texture_image;
	if (p_polygon->texture.is_valid()) {
		texture_image = p_texture_storage->texture_2d_get(p_polygon->texture);
	}

	for (int i = 0; i < index_count; i += 3) {
		const int i0 = stored.indices[i];
		const int i1 = stored.indices[i + 1];
		const int i2 = stored.indices[i + 2];
		if (i0 < 0 || i1 < 0 || i2 < 0 || i0 >= stored.points.size() || i1 >= stored.points.size() || i2 >= stored.points.size()) {
			continue;
		}

		const Point2 p0 = p_transform.xform(stored.points[i0]);
		const Point2 p1 = p_transform.xform(stored.points[i1]);
		const Point2 p2 = p_transform.xform(stored.points[i2]);

		Rect2 tri_bounds(p0, Size2());
		tri_bounds.expand_to(p1);
		tri_bounds.expand_to(p2);
		Rect2i bounds = Rect2i(tri_bounds.position.floor(), tri_bounds.size.ceil());
		if (!p_state.has_clip || bounds.intersects(p_state.clip)) {
			if (p_state.has_clip) {
				bounds = bounds.intersection(p_state.clip);
			}
		} else {
			continue;
		}

		Color c0 = stored.colors.is_empty() ? Color(1, 1, 1, 1) : stored.colors[i0];
		Color c1 = stored.colors.is_empty() ? Color(1, 1, 1, 1) : stored.colors[i1];
		Color c2 = stored.colors.is_empty() ? Color(1, 1, 1, 1) : stored.colors[i2];
		c0 *= p_modulate;
		c1 *= p_modulate;
		c2 *= p_modulate;

		for (int y = bounds.position.y; y < bounds.position.y + bounds.size.y; y++) {
			uint32_t *row = p_state.pixels + y * p_state.width;
			for (int x = bounds.position.x; x < bounds.position.x + bounds.size.x; x++) {
				const Vector2 p(x + 0.5f, y + 0.5f);
				if (!Geometry2D::is_point_in_triangle(p, p0, p1, p2)) {
					continue;
				}

				Color pixel_color = c0;
				if (texture_image.is_valid() && stored.uvs.size() == stored.points.size()) {
					const Vector2 uv0 = stored.uvs[i0];
					const Vector2 uv1 = stored.uvs[i1];
					const Vector2 uv2 = stored.uvs[i2];
					const Vector3 bary = _barycentric(p, p0, p1, p2);
					const Vector2 uv = uv0 * bary.x + uv1 * bary.y + uv2 * bary.z;
					const int tx = CLAMP(int(uv.x * texture_image->get_width()), 0, texture_image->get_width() - 1);
					const int ty = CLAMP(int(uv.y * texture_image->get_height()), 0, texture_image->get_height() - 1);
					pixel_color = texture_image->get_pixel(tx, ty) * pixel_color;
				}

				const uint32_t argb = CanvasBlitPS3::color_to_argb(pixel_color);
				const uint8_t a = (argb >> 24) & 0xFF;
				if (a == 255) {
					row[x] = argb;
				} else if (a > 0) {
					const uint8_t sa = a;
					const uint8_t inv_sa = 255 - sa;
					const uint8_t sr = (argb >> 16) & 0xFF;
					const uint8_t sg = (argb >> 8) & 0xFF;
					const uint8_t sb = argb & 0xFF;
					const uint8_t da = (row[x] >> 24) & 0xFF;
					const uint8_t dr = (row[x] >> 16) & 0xFF;
					const uint8_t dg = (row[x] >> 8) & 0xFF;
					const uint8_t db = row[x] & 0xFF;
					const uint8_t na = (uint8_t)MIN(255, sa + (da * inv_sa) / 255);
					const uint8_t nr = (uint8_t)((sr * sa + dr * inv_sa) / 255);
					const uint8_t ng = (uint8_t)((sg * sa + dg * inv_sa) / 255);
					const uint8_t nb = (uint8_t)((sb * sa + db * inv_sa) / 255);
					row[x] = (uint32_t(na) << 24) | (uint32_t(nr) << 16) | (uint32_t(ng) << 8) | uint32_t(nb);
				}
			}
		}
	}
}

void RasterizerCanvasPS3::_draw_primitive(CanvasBlitPS3::State &p_state, RendererDummy::TextureStorage *p_texture_storage, const Transform2D &p_transform, const Item::CommandPrimitive *p_primitive, const Color &p_modulate) {
	if (p_primitive->point_count < 3) {
		return;
	}

	Item::CommandPolygon poly;
	poly.primitive = RS::PRIMITIVE_TRIANGLES;
	poly.texture = p_primitive->texture;

	Vector<Point2> points;
	Vector<Color> colors;
	Vector<Point2> uvs;
	Vector<int> indices;
	points.resize(p_primitive->point_count);
	colors.resize(p_primitive->point_count);
	uvs.resize(p_primitive->point_count);
	for (uint32_t i = 0; i < p_primitive->point_count; i++) {
		points.write[i] = p_primitive->points[i];
		colors.write[i] = p_primitive->colors[i] * p_modulate;
		uvs.write[i] = p_primitive->uvs[i];
	}
	if (p_primitive->point_count == 3) {
		indices.push_back(0);
		indices.push_back(1);
		indices.push_back(2);
	} else {
		indices.push_back(0);
		indices.push_back(1);
		indices.push_back(2);
		indices.push_back(0);
		indices.push_back(2);
		indices.push_back(3);
	}

	const PolygonID pid = request_polygon(indices, points, colors, uvs);
	poly.polygon.polygon_id = pid;
	_draw_polygon(p_state, p_texture_storage, p_transform, &poly, Color(1, 1, 1, 1));
	free_polygon(pid);
}

void RasterizerCanvasPS3::canvas_render_items(RID p_to_render_target, Item *p_item_list, const Color &p_modulate, Light *p_light_list, Light *p_directional_list, const Transform2D &p_canvas_transform, RS::CanvasItemTextureFilter p_default_filter, RS::CanvasItemTextureRepeat p_default_repeat, bool p_snap_2d_vertices_to_pixel, bool &r_sdf_used, RenderingMethod::RenderInfo *r_render_info) {
	r_sdf_used = false;

	PS3TextureStorage *texture_storage = PS3TextureStorage::get_singleton();
	ERR_FAIL_NULL(texture_storage);

	Ref<Image> target = texture_storage->get_render_target_image(p_to_render_target);
	ERR_FAIL_COND(target.is_null());

	CanvasBlitPS3::State state;
	state.pixels = reinterpret_cast<uint32_t *>(target->ptrw());
	state.width = target->get_width();
	state.height = target->get_height();

	RendererDummy::TextureStorage *dummy_textures = static_cast<RendererDummy::TextureStorage *>(RendererCompositor::get_singleton()->get_texture_storage());

	LocalVector<Transform2D> transform_stack;
	transform_stack.push_back(Transform2D());
	LocalVector<Rect2i> clip_stack;
	LocalVector<bool> clip_enabled_stack;
	clip_enabled_stack.push_back(false);
	bool clip_ignore = false;

	Item *item = p_item_list;
	while (item) {
		if (!item->visible) {
			item = item->next;
			continue;
		}

		const Transform2D item_transform = p_canvas_transform * item->final_transform;
		const Color item_modulate = p_modulate * item->final_modulate;

		if (item->clip && !clip_ignore) {
			Rect2i clip = Rect2i(item->final_clip_rect.position.floor(), item->final_clip_rect.size.ceil());
			clip_stack.push_back(clip);
			clip_enabled_stack.push_back(true);
			CanvasBlitPS3::set_clip(state, clip);
		}

		Transform2D draw_transform = item_transform * transform_stack[transform_stack.size() - 1];
		Item::Command *cmd = item->commands;
		while (cmd) {
			switch (cmd->type) {
				case Item::Command::TYPE_TRANSFORM: {
					const Item::CommandTransform *t = static_cast<const Item::CommandTransform *>(cmd);
					transform_stack.push_back(transform_stack[transform_stack.size() - 1] * t->xform);
					draw_transform = item_transform * transform_stack[transform_stack.size() - 1];
				} break;
				case Item::Command::TYPE_CLIP_IGNORE: {
					const Item::CommandClipIgnore *c = static_cast<const Item::CommandClipIgnore *>(cmd);
					clip_ignore = c->ignore;
				} break;
				case Item::Command::TYPE_RECT: {
					const Item::CommandRect *r = static_cast<const Item::CommandRect *>(cmd);
					_draw_rect(state, dummy_textures, draw_transform, r, item_modulate);
				} break;
				case Item::Command::TYPE_NINEPATCH: {
					const Item::CommandNinePatch *n = static_cast<const Item::CommandNinePatch *>(cmd);
					_draw_ninepatch(state, dummy_textures, draw_transform, n, item_modulate);
				} break;
				case Item::Command::TYPE_POLYGON: {
					const Item::CommandPolygon *poly_cmd = static_cast<const Item::CommandPolygon *>(cmd);
					_draw_polygon(state, dummy_textures, draw_transform, poly_cmd, item_modulate);
				} break;
				case Item::Command::TYPE_PRIMITIVE: {
					const Item::CommandPrimitive *prim = static_cast<const Item::CommandPrimitive *>(cmd);
					_draw_primitive(state, dummy_textures, draw_transform, prim, item_modulate);
				} break;
				default:
					break;
			}
			cmd = cmd->next;
		}

		if (item->clip && !clip_ignore) {
			clip_stack.remove_at(clip_stack.size() - 1);
			clip_enabled_stack.remove_at(clip_enabled_stack.size() - 1);
			if (clip_enabled_stack.size() > 0 && clip_enabled_stack[clip_enabled_stack.size() - 1]) {
				CanvasBlitPS3::set_clip(state, clip_stack[clip_stack.size() - 1]);
			} else {
				CanvasBlitPS3::clear_clip(state);
			}
		}

		item = item->next;
	}

	texture_storage->mark_render_target_used(p_to_render_target);
}

RasterizerCanvasPS3::~RasterizerCanvasPS3() {
	for (const KeyValue<PolygonID, StoredPolygon *> &E : polygons) {
		memdelete(E.value);
	}
	polygons.clear();
}
