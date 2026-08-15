/**************************************************************************/
/*  tileson_gd_bindings.cpp                                               */
/**************************************************************************/
/*                         This file is part of:                          */
/*                             BLAZIUM ENGINE                             */
/*                          https://blazium.app                           */
/**************************************************************************/
/* Copyright (c) 2024-present Blazium Engine contributors.                */
/* Copyright (c) 2014-present Godot Engine contributors (see AUTHORS.md). */
/* Copyright (c) 2007-2014 Juan Linietsky, Ariel Manzur.                  */
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

#include "core/object/class_db.h"
#include "tileson_gd_bindings.h"
#include "core/config/project_settings.h"
#include "core/io/file_access.h"

// -------------------------------------------------------------
// TiledTileson
// -------------------------------------------------------------
void TiledTileson::_bind_methods() {
	ClassDB::bind_method(D_METHOD("parse_file", "path"), &TiledTileson::parse_file);
	ClassDB::bind_method(D_METHOD("parse_string", "json"), &TiledTileson::parse_string);
}

TiledTileson::TiledTileson() {}

static String _tileson_status_text(const std::unique_ptr<tson::Map> &p_map) {
	if (!p_map) {
		return "null";
	}
	switch (p_map->getStatus()) {
		case tson::ParseStatus::OK:
			return "OK";
		case tson::ParseStatus::FileNotFound:
			return "FileNotFound";
		case tson::ParseStatus::ParseError:
			return "ParseError";
		case tson::ParseStatus::MissingData:
			return "MissingData";
		case tson::ParseStatus::DecompressionError:
			return "DecompressionError";
		default:
			return "UNKNOWN";
	}
}

static Ref<TiledMap> _tileson_map_or_error(std::unique_ptr<tson::Map> p_parsed_map) {
	if (p_parsed_map && p_parsed_map->getStatus() == tson::ParseStatus::OK) {
		Ref<TiledMap> godot_map;
		godot_map.instantiate();
		godot_map->set_map(std::move(p_parsed_map));
		return godot_map;
	}

	String error;
	if (p_parsed_map) {
		error = to_godot_string(p_parsed_map->getStatusMessage());
	}
	ERR_PRINT("TiledTileson: Failed to parse map (status=" + _tileson_status_text(p_parsed_map) + ", error=" + error + ").");
	return Ref<TiledMap>();
}

static String _tileson_globalize(const String &p_path) {
	if (p_path.begins_with("res://") || p_path.begins_with("user://")) {
		return ProjectSettings::get_singleton()->globalize_path(p_path);
	}
	return p_path;
}

Ref<TiledMap> TiledTileson::parse_file(const String &p_path) {
	const String global_path = _tileson_globalize(p_path);
	if (FileAccess::exists(global_path)) {
		tson::Tileson t;
		return _tileson_map_or_error(t.parse(fs::u8path(to_std_string(global_path))));
	}

	Ref<FileAccess> fa = FileAccess::open(p_path, FileAccess::READ);
	if (fa.is_null()) {
		ERR_PRINT("TiledTileson: Cannot open file " + p_path);
		return Ref<TiledMap>();
	}
	return parse_string_with_dir(fa->get_as_text(), p_path.get_base_dir());
}

Ref<TiledMap> TiledTileson::parse_string(const String &p_json) {
	return parse_string_with_dir(p_json, String());
}

Ref<TiledMap> TiledTileson::parse_string_with_dir(const String &p_json, const String &p_base_dir) {
	tson::Json11 json_parser;
	const CharString utf8 = p_json.utf8();
	if (!json_parser.parse(utf8.get_data(), utf8.length())) {
		ERR_PRINT("TiledTileson: Failed to parse map (status=ParseError, error=JSON parse failed).");
		return Ref<TiledMap>();
	}
	if (!p_base_dir.is_empty()) {
		json_parser.directory(fs::u8path(to_std_string(_tileson_globalize(p_base_dir))));
	}

	tson::Tileson t;
	std::unique_ptr<tson::Map> parsed_map = std::make_unique<tson::Map>();
	parsed_map->parse(json_parser, t.decompressors(), nullptr);
	return _tileson_map_or_error(std::move(parsed_map));
}

// -------------------------------------------------------------
// TiledFrame
// -------------------------------------------------------------
void TiledFrame::_bind_methods() {
	ClassDB::bind_method(D_METHOD("get_tile_id"), &TiledFrame::get_tile_id);
	ClassDB::bind_method(D_METHOD("get_duration"), &TiledFrame::get_duration);
}

int TiledFrame::get_tile_id() const {
	return frame ? frame->getTileId() : 0;
}
int TiledFrame::get_duration() const {
	return frame ? frame->getDuration() : 0;
}

// -------------------------------------------------------------
// TiledAnimation
// -------------------------------------------------------------
void TiledAnimation::_bind_methods() {
	ClassDB::bind_method(D_METHOD("get_frames"), &TiledAnimation::get_frames);
}

Array TiledAnimation::get_frames() const {
	Array arr;
	if (animation) {
		for (auto &frm : animation->getFrames()) {
			Ref<TiledFrame> godot_frm;
			godot_frm.instantiate();
			godot_frm->set_frame(const_cast<tson::Frame *>(&frm));
			arr.push_back(godot_frm);
		}
	}
	return arr;
}

// -------------------------------------------------------------
// TiledTile
// -------------------------------------------------------------
void TiledTile::_bind_methods() {
	ClassDB::bind_method(D_METHOD("get_id"), &TiledTile::get_id);
	ClassDB::bind_method(D_METHOD("get_image"), &TiledTile::get_image);
	ClassDB::bind_method(D_METHOD("get_image_size"), &TiledTile::get_image_size);
	ClassDB::bind_method(D_METHOD("get_tson_type"), &TiledTile::get_tson_type);
	ClassDB::bind_method(D_METHOD("get_class_type"), &TiledTile::get_class_type);
	ClassDB::bind_method(D_METHOD("get_terrain"), &TiledTile::get_terrain);
	ClassDB::bind_method(D_METHOD("get_drawing_rect"), &TiledTile::get_drawing_rect);
	ClassDB::bind_method(D_METHOD("get_sub_rectangle"), &TiledTile::get_sub_rectangle);
	ClassDB::bind_method(D_METHOD("get_flip_flags"), &TiledTile::get_flip_flags);
	ClassDB::bind_method(D_METHOD("get_gid"), &TiledTile::get_gid);
	ClassDB::bind_method(D_METHOD("get_animation"), &TiledTile::get_animation);
	ClassDB::bind_method(D_METHOD("get_objectgroup"), &TiledTile::get_objectgroup);
	ClassDB::bind_method(D_METHOD("get_properties"), &TiledTile::get_properties);
}

int TiledTile::get_id() const {
	return tile ? tile->getId() : 0;
}
String TiledTile::get_image() const {
	return tile ? to_godot_string(tile->getImage().string()) : String();
}
Vector2i TiledTile::get_image_size() const {
	return tile ? Vector2i(tile->getImageSize().x, tile->getImageSize().y) : Vector2i();
}
String TiledTile::get_tson_type() const {
	return tile ? to_godot_string(tile->getType()) : String();
}
String TiledTile::get_class_type() const {
	return tile ? to_godot_string(tile->getClassType()) : String();
}
Array TiledTile::get_terrain() const {
	Array arr;
	if (tile) {
		for (uint32_t val : tile->getTerrain()) {
			arr.push_back(val);
		}
	}
	return arr;
}
Rect2i TiledTile::get_drawing_rect() const {
	return tile ? Rect2i(tile->getDrawingRect().x, tile->getDrawingRect().y, tile->getDrawingRect().width, tile->getDrawingRect().height) : Rect2i();
}
Rect2i TiledTile::get_sub_rectangle() const {
	return tile ? Rect2i(tile->getSubRectangle().x, tile->getSubRectangle().y, tile->getSubRectangle().width, tile->getSubRectangle().height) : Rect2i();
}
int TiledTile::get_flip_flags() const {
	return tile ? (int)tile->getFlipFlags() : 0;
}
int TiledTile::get_gid() const {
	return tile ? tile->getGid() : 0;
}

Ref<TiledLayer> TiledTile::get_objectgroup() const {
	if (tile) {
		tson::Layer *l = const_cast<tson::Layer *>(&tile->getObjectgroup());
		if (l->getObjects().size() > 0) {
			Ref<TiledLayer> layer;
			layer.instantiate();
			layer->set_layer(l);
			return layer;
		}
	}
	return Ref<TiledLayer>();
}

Ref<TiledAnimation> TiledTile::get_animation() const {
	if (tile && tile->getAnimation().any()) {
		Ref<TiledAnimation> godot_anim;
		godot_anim.instantiate();
		godot_anim->set_animation(&tile->getAnimation());
		return godot_anim;
	}
	return Ref<TiledAnimation>();
}

Array TiledTile::get_properties() {
	Array arr;
	if (tile) {
		for (auto &prop : tile->getProperties().getProperties()) {
			Ref<TiledProperty> p;
			p.instantiate();
			p->set_property(const_cast<tson::Property *>(&prop.second));
			arr.push_back(p);
		}
	}
	return arr;
}

// -------------------------------------------------------------
// TiledObject
// -------------------------------------------------------------
void TiledObject::_bind_methods() {
	ClassDB::bind_method(D_METHOD("get_id"), &TiledObject::get_id);
	ClassDB::bind_method(D_METHOD("get_name"), &TiledObject::get_name);
	ClassDB::bind_method(D_METHOD("get_tson_type"), &TiledObject::get_tson_type);
	ClassDB::bind_method(D_METHOD("get_position"), &TiledObject::get_position);
	ClassDB::bind_method(D_METHOD("get_size"), &TiledObject::get_size);
	ClassDB::bind_method(D_METHOD("get_rotation"), &TiledObject::get_rotation);
	ClassDB::bind_method(D_METHOD("is_visible"), &TiledObject::is_visible);
	ClassDB::bind_method(D_METHOD("is_ellipse"), &TiledObject::is_ellipse);
	ClassDB::bind_method(D_METHOD("is_point"), &TiledObject::is_point);
	ClassDB::bind_method(D_METHOD("get_polygon"), &TiledObject::get_polygon);
	ClassDB::bind_method(D_METHOD("get_polyline"), &TiledObject::get_polyline);
	ClassDB::bind_method(D_METHOD("get_template"), &TiledObject::get_template);
	ClassDB::bind_method(D_METHOD("get_class_type"), &TiledObject::get_class_type);
	ClassDB::bind_method(D_METHOD("get_object_type"), &TiledObject::get_object_type);
	ClassDB::bind_method(D_METHOD("get_gid"), &TiledObject::get_gid);
	ClassDB::bind_method(D_METHOD("get_properties"), &TiledObject::get_properties);
	ClassDB::bind_method(D_METHOD("get_text"), &TiledObject::get_text);
}

int TiledObject::get_id() const {
	return object ? object->getId() : 0;
}
String TiledObject::get_name() const {
	return object ? to_godot_string(object->getName()) : String();
}
String TiledObject::get_tson_type() const {
	return object ? to_godot_string(object->getType()) : String();
}
Vector2i TiledObject::get_position() const {
	return object ? Vector2i(object->getPosition().x, object->getPosition().y) : Vector2i();
}
Vector2i TiledObject::get_size() const {
	return object ? Vector2i(object->getSize().x, object->getSize().y) : Vector2i();
}
float TiledObject::get_rotation() const {
	return object ? object->getRotation() : 0.0f;
}
bool TiledObject::is_visible() const {
	return object ? object->isVisible() : false;
}
bool TiledObject::is_ellipse() const {
	return object ? object->isEllipse() : false;
}
bool TiledObject::is_point() const {
	return object ? object->isPoint() : false;
}

Array TiledObject::get_polygon() const {
	Array arr;
	if (object) {
		for (auto &pt : object->getPolygons()) {
			arr.push_back(Vector2i(pt.x, pt.y));
		}
	}
	return arr;
}
Array TiledObject::get_polyline() const {
	Array arr;
	if (object) {
		for (auto &pt : object->getPolylines()) {
			arr.push_back(Vector2i(pt.x, pt.y));
		}
	}
	return arr;
}
String TiledObject::get_template() const {
	return object ? to_godot_string(object->getTemplate()) : String();
}
String TiledObject::get_class_type() const {
	return object ? to_godot_string(object->getClassType()) : String();
}
int TiledObject::get_object_type() const {
	return object ? (int)object->getObjectType() : 0;
}
int TiledObject::get_gid() const {
	return object ? object->getGid() : 0;
}

Array TiledObject::get_properties() {
	Array arr;
	if (object) {
		for (auto &prop : object->getProperties().getProperties()) {
			Ref<TiledProperty> p;
			p.instantiate();
			p->set_property(const_cast<tson::Property *>(&prop.second));
			arr.push_back(p);
		}
	}
	return arr;
}

// -------------------------------------------------------------
// TiledChunk
// -------------------------------------------------------------
void TiledChunk::_bind_methods() {
	ClassDB::bind_method(D_METHOD("get_position"), &TiledChunk::get_position);
	ClassDB::bind_method(D_METHOD("get_size"), &TiledChunk::get_size);
	ClassDB::bind_method(D_METHOD("get_data"), &TiledChunk::get_data);
}

Vector2i TiledChunk::get_position() const {
	return chunk ? Vector2i(chunk->getPosition().x, chunk->getPosition().y) : Vector2i();
}
Vector2i TiledChunk::get_size() const {
	return chunk ? Vector2i(chunk->getSize().x, chunk->getSize().y) : Vector2i();
}
Array TiledChunk::get_data() const {
	Array arr;
	if (chunk) {
		for (int val : chunk->getData()) {
			arr.push_back(val);
		}
	}
	return arr;
}

// -------------------------------------------------------------
// TiledLayer
// -------------------------------------------------------------
void TiledLayer::_bind_methods() {
	ClassDB::bind_method(D_METHOD("get_name"), &TiledLayer::get_name);
	ClassDB::bind_method(D_METHOD("get_tson_type"), &TiledLayer::get_tson_type);
	ClassDB::bind_method(D_METHOD("is_visible"), &TiledLayer::is_visible);
	ClassDB::bind_method(D_METHOD("get_objects"), &TiledLayer::get_objects);
	ClassDB::bind_method(D_METHOD("get_chunks"), &TiledLayer::get_chunks);
	ClassDB::bind_method(D_METHOD("get_properties"), &TiledLayer::get_properties);
	ClassDB::bind_method(D_METHOD("get_tile_objects"), &TiledLayer::get_tile_objects);
	ClassDB::bind_method(D_METHOD("get_compression"), &TiledLayer::get_compression);
	ClassDB::bind_method(D_METHOD("get_data_base64"), &TiledLayer::get_data_base64);
	ClassDB::bind_method(D_METHOD("get_base64_data"), &TiledLayer::get_base64_data);
	ClassDB::bind_method(D_METHOD("get_draw_order"), &TiledLayer::get_draw_order);
	ClassDB::bind_method(D_METHOD("get_encoding"), &TiledLayer::get_encoding);
	ClassDB::bind_method(D_METHOD("get_id"), &TiledLayer::get_id);
	ClassDB::bind_method(D_METHOD("get_image"), &TiledLayer::get_image);
	ClassDB::bind_method(D_METHOD("get_offset"), &TiledLayer::get_offset);
	ClassDB::bind_method(D_METHOD("get_opacity"), &TiledLayer::get_opacity);
	ClassDB::bind_method(D_METHOD("get_size"), &TiledLayer::get_size);
	ClassDB::bind_method(D_METHOD("get_transparent_color"), &TiledLayer::get_transparent_color);
	ClassDB::bind_method(D_METHOD("get_parallax"), &TiledLayer::get_parallax);
	ClassDB::bind_method(D_METHOD("has_repeat_x"), &TiledLayer::has_repeat_x);
	ClassDB::bind_method(D_METHOD("has_repeat_y"), &TiledLayer::has_repeat_y);
	ClassDB::bind_method(D_METHOD("get_class_type"), &TiledLayer::get_class_type);
	ClassDB::bind_method(D_METHOD("get_x"), &TiledLayer::get_x);
	ClassDB::bind_method(D_METHOD("get_y"), &TiledLayer::get_y);
	ClassDB::bind_method(D_METHOD("get_tint_color"), &TiledLayer::get_tint_color);
}

String TiledLayer::get_name() const {
	return layer ? to_godot_string(layer->getName()) : String();
}
String TiledLayer::get_tson_type() const {
	if (!layer) {
		return String();
	}
	switch (layer->getType()) {
		case tson::LayerType::TileLayer:
			return "TileLayer";
		case tson::LayerType::ObjectGroup:
			return "ObjectGroup";
		case tson::LayerType::ImageLayer:
			return "ImageLayer";
		case tson::LayerType::Group:
			return "Group";
		default:
			return "Undefined";
	}
}
bool TiledLayer::is_visible() const {
	return layer ? layer->isVisible() : false;
}

String TiledLayer::get_compression() const {
	return layer ? to_godot_string(layer->getCompression()) : String();
}
Array TiledLayer::get_data_base64() const {
	Array arr;
	if (layer) {
		for (uint32_t val : layer->getData()) {
			arr.push_back(val);
		}
	}
	return arr;
}
String TiledLayer::get_base64_data() const {
	return layer ? to_godot_string(layer->getBase64Data()) : String();
}
String TiledLayer::get_draw_order() const {
	return layer ? to_godot_string(layer->getDrawOrder()) : String();
}
String TiledLayer::get_encoding() const {
	return layer ? to_godot_string(layer->getEncoding()) : String();
}
int TiledLayer::get_id() const {
	return layer ? layer->getId() : 0;
}
String TiledLayer::get_image() const {
	return layer ? to_godot_string(layer->getImage()) : String();
}
Vector2 TiledLayer::get_offset() const {
	return layer ? Vector2(layer->getOffset().x, layer->getOffset().y) : Vector2();
}
float TiledLayer::get_opacity() const {
	return layer ? layer->getOpacity() : 1.0f;
}
Vector2i TiledLayer::get_size() const {
	return layer ? Vector2i(layer->getSize().x, layer->getSize().y) : Vector2i();
}

Color TiledLayer::get_transparent_color() const {
	if (!layer) {
		return Color();
	}
	tson::Colori col = layer->getTransparentColor();
	return Color(col.r / 255.0f, col.g / 255.0f, col.b / 255.0f, col.a / 255.0f);
}

Vector2 TiledLayer::get_parallax() const {
	return layer ? Vector2(layer->getParallax().x, layer->getParallax().y) : Vector2();
}
bool TiledLayer::has_repeat_x() const {
	return layer ? layer->hasRepeatX() : false;
}
bool TiledLayer::has_repeat_y() const {
	return layer ? layer->hasRepeatY() : false;
}
String TiledLayer::get_class_type() const {
	return layer ? to_godot_string(layer->getClassType()) : String();
}
int TiledLayer::get_x() const {
	return layer ? layer->getX() : 0;
}
int TiledLayer::get_y() const {
	return layer ? layer->getY() : 0;
}
Color TiledLayer::get_tint_color() const {
	if (!layer) {
		return Color();
	}
	tson::Colori col = layer->getTintColor();
	return Color(col.r / 255.0f, col.g / 255.0f, col.b / 255.0f, col.a / 255.0f);
}

Array TiledLayer::get_objects() {
	Array arr;
	if (layer) {
		for (auto &obj : layer->getObjects()) {
			Ref<TiledObject> godot_obj;
			godot_obj.instantiate();
			godot_obj->set_object(&obj); // Vector stores them directly, reference inside is stable as long as Map survives
			arr.push_back(godot_obj);
		}
	}
	return arr;
}

Array TiledLayer::get_chunks() {
	Array arr;
	if (layer) {
		for (auto &chk : layer->getChunks()) {
			Ref<TiledChunk> godot_chk;
			godot_chk.instantiate();
			godot_chk->set_chunk(&chk);
			arr.push_back(godot_chk);
		}
	}
	return arr;
}

Array TiledLayer::get_properties() {
	Array arr;
	if (layer) {
		for (auto &prop : layer->getProperties().getProperties()) {
			Ref<TiledProperty> p;
			p.instantiate();
			p->set_property(const_cast<tson::Property *>(&prop.second));
			arr.push_back(p);
		}
	}
	return arr;
}

Array TiledLayer::get_tile_objects() {
	Array arr;
	if (layer) {
		for (auto &to : layer->getTileObjects()) {
			Ref<TiledTileObject> t;
			t.instantiate();
			t->set_tile_object(const_cast<tson::TileObject *>(&to.second));
			arr.push_back(t);
		}
	}
	return arr;
}

// -------------------------------------------------------------
// TiledTileset
// -------------------------------------------------------------
void TiledTileset::_bind_methods() {
	ClassDB::bind_method(D_METHOD("get_name"), &TiledTileset::get_name);
	ClassDB::bind_method(D_METHOD("get_image"), &TiledTileset::get_image);
	ClassDB::bind_method(D_METHOD("get_tiles"), &TiledTileset::get_tiles);
	ClassDB::bind_method(D_METHOD("get_properties"), &TiledTileset::get_properties);
	ClassDB::bind_method(D_METHOD("get_wang_sets"), &TiledTileset::get_wang_sets);
	ClassDB::bind_method(D_METHOD("get_grid"), &TiledTileset::get_grid);
	ClassDB::bind_method(D_METHOD("get_terrains"), &TiledTileset::get_terrains);
	ClassDB::bind_method(D_METHOD("get_transformations"), &TiledTileset::get_transformations);
	ClassDB::bind_method(D_METHOD("get_columns"), &TiledTileset::get_columns);
	ClassDB::bind_method(D_METHOD("get_first_gid"), &TiledTileset::get_first_gid);
	ClassDB::bind_method(D_METHOD("get_image_path"), &TiledTileset::get_image_path);
	ClassDB::bind_method(D_METHOD("get_full_image_path"), &TiledTileset::get_full_image_path);
	ClassDB::bind_method(D_METHOD("get_image_size"), &TiledTileset::get_image_size);
	ClassDB::bind_method(D_METHOD("get_margin"), &TiledTileset::get_margin);
	ClassDB::bind_method(D_METHOD("get_spacing"), &TiledTileset::get_spacing);
	ClassDB::bind_method(D_METHOD("get_tile_count"), &TiledTileset::get_tile_count);
	ClassDB::bind_method(D_METHOD("get_transparent_color"), &TiledTileset::get_transparent_color);
	ClassDB::bind_method(D_METHOD("get_type_str"), &TiledTileset::get_type_str);
	ClassDB::bind_method(D_METHOD("get_class_type"), &TiledTileset::get_class_type);
	ClassDB::bind_method(D_METHOD("get_tile_offset"), &TiledTileset::get_tile_offset);
	ClassDB::bind_method(D_METHOD("get_tile_render_size"), &TiledTileset::get_tile_render_size);
	ClassDB::bind_method(D_METHOD("get_fill_mode"), &TiledTileset::get_fill_mode);
	ClassDB::bind_method(D_METHOD("get_object_alignment"), &TiledTileset::get_object_alignment);
}

String TiledTileset::get_name() const {
	return tileset ? to_godot_string(tileset->getName()) : String();
}
String TiledTileset::get_image() const {
	return tileset ? to_godot_string(tileset->getImage().string()) : String();
}
int TiledTileset::get_columns() const {
	return tileset ? tileset->getColumns() : 0;
}
int TiledTileset::get_first_gid() const {
	return tileset ? tileset->getFirstgid() : 0;
}
String TiledTileset::get_image_path() const {
	return tileset ? to_godot_string(tileset->getImagePath().string()) : String();
}
String TiledTileset::get_full_image_path() const {
	return tileset ? to_godot_string(tileset->getFullImagePath().string()) : String();
}
Vector2i TiledTileset::get_image_size() const {
	return tileset ? Vector2i(tileset->getImageSize().x, tileset->getImageSize().y) : Vector2i();
}
int TiledTileset::get_margin() const {
	return tileset ? tileset->getMargin() : 0;
}
int TiledTileset::get_spacing() const {
	return tileset ? tileset->getSpacing() : 0;
}
int TiledTileset::get_tile_count() const {
	return tileset ? tileset->getTileCount() : 0;
}

Color TiledTileset::get_transparent_color() const {
	if (!tileset) {
		return Color();
	}
	tson::Colori col = tileset->getTransparentColor();
	return Color(col.r / 255.0f, col.g / 255.0f, col.b / 255.0f, col.a / 255.0f);
}

String TiledTileset::get_type_str() const {
	return tileset ? to_godot_string(tileset->getTypeStr()) : String();
}
String TiledTileset::get_class_type() const {
	return tileset ? to_godot_string(tileset->getClassType()) : String();
}
Vector2i TiledTileset::get_tile_offset() const {
	return tileset ? Vector2i(tileset->getTileOffset().x, tileset->getTileOffset().y) : Vector2i();
}
int TiledTileset::get_tile_render_size() const {
	return tileset ? (int)tileset->getTileRenderSize() : 0;
}
int TiledTileset::get_fill_mode() const {
	return tileset ? (int)tileset->getFillMode() : 0;
}
int TiledTileset::get_object_alignment() const {
	return tileset ? (int)tileset->getObjectAlignment() : 0;
}

Array TiledTileset::get_tiles() const {
	Array arr;
	if (tileset) {
		for (auto &tile : tileset->getTiles()) {
			Ref<TiledTile> godot_tile;
			godot_tile.instantiate();
			godot_tile->set_tile(const_cast<tson::Tile *>(&tile));
			arr.push_back(godot_tile);
		}
	}
	return arr;
}

Array TiledTileset::get_properties() {
	Array arr;
	if (tileset) {
		for (auto &prop : tileset->getProperties().getProperties()) {
			Ref<TiledProperty> p;
			p.instantiate();
			p->set_property(const_cast<tson::Property *>(&prop.second));
			arr.push_back(p);
		}
	}
	return arr;
}

Array TiledTileset::get_wang_sets() const {
	Array arr;
	if (tileset) {
		for (auto &w : tileset->getWangsets()) {
			Ref<TiledWangSet> godot_w;
			godot_w.instantiate();
			godot_w->set_wang_set(const_cast<tson::WangSet *>(&w));
			arr.push_back(godot_w);
		}
	}
	return arr;
}

// -------------------------------------------------------------
// TiledMap
// -------------------------------------------------------------
void TiledMap::_bind_methods() {
	ClassDB::bind_method(D_METHOD("get_status"), &TiledMap::get_status);
	ClassDB::bind_method(D_METHOD("get_size"), &TiledMap::get_size);
	ClassDB::bind_method(D_METHOD("get_tile_size"), &TiledMap::get_tile_size);
	ClassDB::bind_method(D_METHOD("get_layers"), &TiledMap::get_layers);
	ClassDB::bind_method(D_METHOD("get_tilesets"), &TiledMap::get_tilesets);
	ClassDB::bind_method(D_METHOD("get_properties"), &TiledMap::get_properties);
	ClassDB::bind_method(D_METHOD("get_background_color"), &TiledMap::get_background_color);
	ClassDB::bind_method(D_METHOD("get_hexside_length"), &TiledMap::get_hexside_length);
	ClassDB::bind_method(D_METHOD("is_infinite"), &TiledMap::is_infinite);
	ClassDB::bind_method(D_METHOD("get_next_layer_id"), &TiledMap::get_next_layer_id);
	ClassDB::bind_method(D_METHOD("get_next_object_id"), &TiledMap::get_next_object_id);
	ClassDB::bind_method(D_METHOD("get_orientation"), &TiledMap::get_orientation);
	ClassDB::bind_method(D_METHOD("get_render_order"), &TiledMap::get_render_order);
	ClassDB::bind_method(D_METHOD("get_stagger_axis"), &TiledMap::get_stagger_axis);
	ClassDB::bind_method(D_METHOD("get_stagger_index"), &TiledMap::get_stagger_index);
	ClassDB::bind_method(D_METHOD("get_tiled_version"), &TiledMap::get_tiled_version);
	ClassDB::bind_method(D_METHOD("get_tson_type"), &TiledMap::get_tson_type);
	ClassDB::bind_method(D_METHOD("get_class_type"), &TiledMap::get_class_type);
	ClassDB::bind_method(D_METHOD("get_parallax_origin"), &TiledMap::get_parallax_origin);
	ClassDB::bind_method(D_METHOD("get_compression_level"), &TiledMap::get_compression_level);
}

String TiledMap::get_status() const {
	if (!map) {
		return "NOT_INITIALIZED";
	}
	switch (map->getStatus()) {
		case tson::ParseStatus::OK:
			return "OK";
		case tson::ParseStatus::FileNotFound:
			return "FileNotFound";
		case tson::ParseStatus::ParseError:
			return "ParseError";
		case tson::ParseStatus::MissingData:
			return "MissingData";
		case tson::ParseStatus::DecompressionError:
			return "DecompressionError";
	}
	return "UNKNOWN";
}

Vector2i TiledMap::get_size() const {
	return map ? Vector2i(map->getSize().x, map->getSize().y) : Vector2i();
}
Vector2i TiledMap::get_tile_size() const {
	return map ? Vector2i(map->getTileSize().x, map->getTileSize().y) : Vector2i();
}

Color TiledMap::get_background_color() const {
	if (!map) {
		return Color();
	}
	tson::Colori col = map->getBackgroundColor();
	return Color(col.r / 255.0f, col.g / 255.0f, col.b / 255.0f, col.a / 255.0f);
}
int TiledMap::get_hexside_length() const {
	return map ? map->getHexsideLength() : 0;
}
bool TiledMap::is_infinite() const {
	return map ? map->isInfinite() : false;
}
int TiledMap::get_next_layer_id() const {
	return map ? map->getNextLayerId() : 0;
}
int TiledMap::get_next_object_id() const {
	return map ? map->getNextObjectId() : 0;
}
String TiledMap::get_orientation() const {
	return map ? to_godot_string(map->getOrientation()) : String();
}
String TiledMap::get_render_order() const {
	return map ? to_godot_string(map->getRenderOrder()) : String();
}
String TiledMap::get_stagger_axis() const {
	return map ? to_godot_string(map->getStaggerAxis()) : String();
}
String TiledMap::get_stagger_index() const {
	return map ? to_godot_string(map->getStaggerIndex()) : String();
}
String TiledMap::get_tiled_version() const {
	return map ? to_godot_string(map->getTiledVersion()) : String();
}
String TiledMap::get_tson_type() const {
	return map ? to_godot_string(map->getType()) : String();
}
String TiledMap::get_class_type() const {
	return map ? to_godot_string(map->getClassType()) : String();
}
Vector2 TiledMap::get_parallax_origin() const {
	return map ? Vector2(map->getParallaxOrigin().x, map->getParallaxOrigin().y) : Vector2();
}
int TiledMap::get_compression_level() const {
	return map ? map->getCompressionLevel() : 0;
}

Array TiledMap::get_layers() {
	Array arr;
	if (map) {
		for (auto &layer : map->getLayers()) {
			Ref<TiledLayer> l;
			l.instantiate();
			l->set_layer(&layer);
			arr.push_back(l);
		}
	}
	return arr;
}

Array TiledMap::get_tilesets() {
	Array arr;
	if (map) {
		for (auto &tileset : map->getTilesets()) {
			Ref<TiledTileset> t;
			t.instantiate();
			t->set_tileset(&tileset);
			arr.push_back(t);
		}
	}
	return arr;
}

Array TiledMap::get_properties() {
	Array arr;
	if (map) {
		for (auto &prop : map->getProperties().getProperties()) {
			Ref<TiledProperty> p;
			p.instantiate();
			p->set_property(const_cast<tson::Property *>(&prop.second));
			arr.push_back(p);
		}
	}
	return arr;
}

// -------------------------------------------------------------
// TiledProperty
// -------------------------------------------------------------
void TiledProperty::_bind_methods() {
	ClassDB::bind_method(D_METHOD("get_name"), &TiledProperty::get_name);
	ClassDB::bind_method(D_METHOD("get_tson_type"), &TiledProperty::get_tson_type);
	ClassDB::bind_method(D_METHOD("get_property_type"), &TiledProperty::get_property_type);
	ClassDB::bind_method(D_METHOD("get_value"), &TiledProperty::get_value);
}

String TiledProperty::get_name() const {
	return property ? to_godot_string(property->getName()) : String();
}
int TiledProperty::get_tson_type() const {
	return property ? (int)property->getType() : 0;
}
String TiledProperty::get_property_type() const {
	return property ? to_godot_string(property->getPropertyType()) : String();
}

Variant TiledProperty::get_value() const {
	if (!property) {
		return Variant();
	}

	switch (property->getType()) {
		case tson::Type::Boolean:
			return property->getValue<bool>();
		case tson::Type::Int:
			return property->getValue<int>();
		case tson::Type::Float:
			return property->getValue<float>();
		case tson::Type::String:
			return to_godot_string(property->getValue<std::string>());
		case tson::Type::File:
			return to_godot_string(property->getValue<std::string>());
		case tson::Type::Color: {
			tson::Colori col = property->getValue<tson::Colori>();
			return Color(col.r / 255.0f, col.g / 255.0f, col.b / 255.0f, col.a / 255.0f);
		}
		case tson::Type::Object:
			return property->getValue<int>();
		case tson::Type::Class:
			return to_godot_string(property->getPropertyType());
		default:
			return Variant();
	}
}

// -------------------------------------------------------------
// TiledProjectFolder
// -------------------------------------------------------------
void TiledProjectFolder::_bind_methods() {
	ClassDB::bind_method(D_METHOD("get_path"), &TiledProjectFolder::get_path);
	ClassDB::bind_method(D_METHOD("has_world_file"), &TiledProjectFolder::has_world_file);
	ClassDB::bind_method(D_METHOD("get_sub_folders"), &TiledProjectFolder::get_sub_folders);
	ClassDB::bind_method(D_METHOD("get_files"), &TiledProjectFolder::get_files);
}

String TiledProjectFolder::get_path() const {
	return folder ? to_godot_string(folder->getPath().string()) : String();
}
bool TiledProjectFolder::has_world_file() const {
	return folder ? folder->hasWorldFile() : false;
}

Array TiledProjectFolder::get_sub_folders() const {
	Array arr;
	if (folder) {
		for (auto &f : folder->getSubFolders()) {
			Ref<TiledProjectFolder> godot_f;
			godot_f.instantiate();
			godot_f->set_folder(const_cast<tson::ProjectFolder *>(&f));
			arr.push_back(godot_f);
		}
	}
	return arr;
}

Array TiledProjectFolder::get_files() const {
	Array arr;
	if (folder) {
		for (auto &f : folder->getFiles()) {
			arr.push_back(to_godot_string(f.string()));
		}
	}
	return arr;
}

// -------------------------------------------------------------
// TiledProjectData
// -------------------------------------------------------------
void TiledProjectData::_bind_methods() {
	ClassDB::bind_method(D_METHOD("get_base_path"), &TiledProjectData::get_base_path);
	ClassDB::bind_method(D_METHOD("get_automapping_rules_file"), &TiledProjectData::get_automapping_rules_file);
	ClassDB::bind_method(D_METHOD("get_commands"), &TiledProjectData::get_commands);
	ClassDB::bind_method(D_METHOD("get_extensions_path"), &TiledProjectData::get_extensions_path);
	ClassDB::bind_method(D_METHOD("get_folders"), &TiledProjectData::get_folders);
	ClassDB::bind_method(D_METHOD("get_object_types_file"), &TiledProjectData::get_object_types_file);
	ClassDB::bind_method(D_METHOD("get_project_property_types"), &TiledProjectData::get_project_property_types);
}

String TiledProjectData::get_base_path() const {
	return project_data ? to_godot_string(project_data->basePath.string()) : String();
}
String TiledProjectData::get_automapping_rules_file() const {
	return project_data ? to_godot_string(project_data->automappingRulesFile) : String();
}

Array TiledProjectData::get_commands() const {
	Array arr;
	if (project_data) {
		for (auto &c : project_data->commands) {
			arr.push_back(to_godot_string(c));
		}
	}
	return arr;
}

String TiledProjectData::get_extensions_path() const {
	return project_data ? to_godot_string(project_data->extensionsPath) : String();
}

Array TiledProjectData::get_folders() const {
	Array arr;
	if (project_data) {
		for (auto &f : project_data->folderPaths) {
			Ref<TiledProjectFolder> godot_f;
			godot_f.instantiate();
			godot_f->set_folder(const_cast<tson::ProjectFolder *>(&f));
			arr.push_back(godot_f);
		}
	}
	return arr;
}

String TiledProjectData::get_object_types_file() const {
	return project_data ? to_godot_string(project_data->objectTypesFile) : String();
}

Ref<TiledProjectPropertyTypes> TiledProjectData::get_project_property_types() const {
	if (project_data) {
		Ref<TiledProjectPropertyTypes> godot;
		godot.instantiate();
		godot->set_project_property_types(&project_data->projectPropertyTypes);
		return godot;
	}
	return Ref<TiledProjectPropertyTypes>();
}

// -------------------------------------------------------------
// TiledProject
// -------------------------------------------------------------
void TiledProject::_bind_methods() {
	ClassDB::bind_method(D_METHOD("parse", "path"), &TiledProject::parse);
	ClassDB::bind_method(D_METHOD("get_path"), &TiledProject::get_path);
	ClassDB::bind_method(D_METHOD("get_data"), &TiledProject::get_data);
	ClassDB::bind_method(D_METHOD("get_folders"), &TiledProject::get_folders);
	ClassDB::bind_method(D_METHOD("get_tiled_class", "name"), &TiledProject::get_tiled_class);
	ClassDB::bind_method(D_METHOD("get_enum_definition", "name"), &TiledProject::get_enum_definition);
}

bool TiledProject::parse(const String &p_path) {
	project = std::make_unique<tson::Project>();
	return project->parse(std::string(p_path.utf8().get_data()));
}

String TiledProject::get_path() const {
	return project ? to_godot_string(project->getPath().string()) : String();
}

Ref<TiledProjectData> TiledProject::get_data() const {
	if (project) {
		Ref<TiledProjectData> data;
		data.instantiate();
		data->set_data(&project->getData());
		return data;
	}
	return Ref<TiledProjectData>();
}

Array TiledProject::get_folders() const {
	Array arr;
	if (project) {
		for (auto &f : project->getFolders()) {
			Ref<TiledProjectFolder> godot_f;
			godot_f.instantiate();
			godot_f->set_folder(const_cast<tson::ProjectFolder *>(&f));
			arr.push_back(godot_f);
		}
	}
	return arr;
}

// -------------------------------------------------------------
// TiledWorldMapData
// -------------------------------------------------------------
void TiledWorldMapData::_bind_methods() {
	ClassDB::bind_method(D_METHOD("get_folder"), &TiledWorldMapData::get_folder);
	ClassDB::bind_method(D_METHOD("get_path"), &TiledWorldMapData::get_path);
	ClassDB::bind_method(D_METHOD("get_file_name"), &TiledWorldMapData::get_file_name);
	ClassDB::bind_method(D_METHOD("get_size"), &TiledWorldMapData::get_size);
	ClassDB::bind_method(D_METHOD("get_position"), &TiledWorldMapData::get_position);
}

String TiledWorldMapData::get_folder() const {
	return world_map_data ? to_godot_string(world_map_data->folder.string()) : String();
}
String TiledWorldMapData::get_path() const {
	return world_map_data ? to_godot_string(world_map_data->path.string()) : String();
}
String TiledWorldMapData::get_file_name() const {
	return world_map_data ? to_godot_string(world_map_data->fileName) : String();
}
Vector2i TiledWorldMapData::get_size() const {
	return world_map_data ? Vector2i(world_map_data->size.x, world_map_data->size.y) : Vector2i();
}
Vector2i TiledWorldMapData::get_position() const {
	return world_map_data ? Vector2i(world_map_data->position.x, world_map_data->position.y) : Vector2i();
}

// -------------------------------------------------------------
// TiledWorld
// -------------------------------------------------------------
void TiledWorld::_bind_methods() {
	ClassDB::bind_method(D_METHOD("parse", "path"), &TiledWorld::parse);
	ClassDB::bind_method(D_METHOD("get_path"), &TiledWorld::get_path);
	ClassDB::bind_method(D_METHOD("get_folder"), &TiledWorld::get_folder);
	ClassDB::bind_method(D_METHOD("get_tson_type"), &TiledWorld::get_tson_type);
	ClassDB::bind_method(D_METHOD("only_show_adjacent_maps"), &TiledWorld::only_show_adjacent_maps);
	ClassDB::bind_method(D_METHOD("get_map_data"), &TiledWorld::get_map_data);
}

void TiledWorld::parse(const String &p_path) {
	world = std::make_unique<tson::World>();
	world->parse(std::string(p_path.utf8().get_data()));
}

String TiledWorld::get_path() const {
	return world ? to_godot_string(world->getPath().string()) : String();
}
String TiledWorld::get_folder() const {
	return world ? to_godot_string(world->getFolder().string()) : String();
}
String TiledWorld::get_tson_type() const {
	return world ? to_godot_string(world->getType()) : String();
}
bool TiledWorld::only_show_adjacent_maps() const {
	return world ? world->onlyShowAdjacentMaps() : false;
}

Array TiledWorld::get_map_data() const {
	Array arr;
	if (world) {
		for (auto &data : world->getMapData()) {
			Ref<TiledWorldMapData> godot_data;
			godot_data.instantiate();
			godot_data->set_data(const_cast<tson::WorldMapData *>(&data));
			arr.push_back(godot_data);
		}
	}
	return arr;
}

// -------------------------------------------------------------
// TiledWangColor
// -------------------------------------------------------------
void TiledWangColor::_bind_methods() {
	ClassDB::bind_method(D_METHOD("get_color"), &TiledWangColor::get_color);
	ClassDB::bind_method(D_METHOD("get_name"), &TiledWangColor::get_name);
	ClassDB::bind_method(D_METHOD("get_probability"), &TiledWangColor::get_probability);
	ClassDB::bind_method(D_METHOD("get_tile"), &TiledWangColor::get_tile);
	ClassDB::bind_method(D_METHOD("get_class_type"), &TiledWangColor::get_class_type);
	ClassDB::bind_method(D_METHOD("get_properties"), &TiledWangColor::get_properties);
}

Color TiledWangColor::get_color() const {
	if (!wang_color) {
		return Color();
	}
	tson::Colori col = wang_color->getColor();
	return Color(col.r / 255.0f, col.g / 255.0f, col.b / 255.0f, col.a / 255.0f);
}
String TiledWangColor::get_name() const {
	return wang_color ? to_godot_string(wang_color->getName()) : String();
}
float TiledWangColor::get_probability() const {
	return wang_color ? wang_color->getProbability() : 0.0f;
}
int TiledWangColor::get_tile() const {
	return wang_color ? wang_color->getTile() : 0;
}
String TiledWangColor::get_class_type() const {
	return wang_color ? to_godot_string(wang_color->getClassType()) : String();
}

Array TiledWangColor::get_properties() {
	Array arr;
	if (wang_color) {
		for (auto &prop : wang_color->getProperties().getProperties()) {
			Ref<TiledProperty> p;
			p.instantiate();
			p->set_property(const_cast<tson::Property *>(&prop.second));
			arr.push_back(p);
		}
	}
	return arr;
}

// -------------------------------------------------------------
// TiledWangTile
// -------------------------------------------------------------
void TiledWangTile::_bind_methods() {
	ClassDB::bind_method(D_METHOD("get_tile_id"), &TiledWangTile::get_tile_id);
	ClassDB::bind_method(D_METHOD("has_d_flip"), &TiledWangTile::has_d_flip);
	ClassDB::bind_method(D_METHOD("has_h_flip"), &TiledWangTile::has_h_flip);
	ClassDB::bind_method(D_METHOD("has_v_flip"), &TiledWangTile::has_v_flip);
	ClassDB::bind_method(D_METHOD("get_wang_id"), &TiledWangTile::get_wang_id);
}

int TiledWangTile::get_tile_id() const {
	return wang_tile ? wang_tile->getTileid() : 0;
}
bool TiledWangTile::has_d_flip() const {
	return wang_tile ? wang_tile->hasDFlip() : false;
}
bool TiledWangTile::has_h_flip() const {
	return wang_tile ? wang_tile->hasHFlip() : false;
}
bool TiledWangTile::has_v_flip() const {
	return wang_tile ? wang_tile->hasVFlip() : false;
}

Array TiledWangTile::get_wang_id() const {
	Array arr;
	if (wang_tile) {
		for (uint32_t val : wang_tile->getWangIds()) {
			arr.push_back(val);
		}
	}
	return arr;
}

// -------------------------------------------------------------
// TiledWangSet
// -------------------------------------------------------------
void TiledWangSet::_bind_methods() {
	ClassDB::bind_method(D_METHOD("get_name"), &TiledWangSet::get_name);
	ClassDB::bind_method(D_METHOD("get_tile"), &TiledWangSet::get_tile);
	ClassDB::bind_method(D_METHOD("get_class_type"), &TiledWangSet::get_class_type);
	ClassDB::bind_method(D_METHOD("get_wang_tiles"), &TiledWangSet::get_wang_tiles);
	ClassDB::bind_method(D_METHOD("get_colors"), &TiledWangSet::get_colors);
	ClassDB::bind_method(D_METHOD("get_properties"), &TiledWangSet::get_properties);
}

String TiledWangSet::get_name() const {
	return wang_set ? to_godot_string(wang_set->getName()) : String();
}
int TiledWangSet::get_tile() const {
	return wang_set ? wang_set->getTile() : 0;
}
String TiledWangSet::get_class_type() const {
	return wang_set ? to_godot_string(wang_set->getClassType()) : String();
}

Array TiledWangSet::get_wang_tiles() const {
	Array arr;
	if (wang_set) {
		for (auto &t : wang_set->getWangTiles()) {
			Ref<TiledWangTile> godot_t;
			godot_t.instantiate();
			godot_t->set_wang_tile(const_cast<tson::WangTile *>(&t));
			arr.push_back(godot_t);
		}
	}
	return arr;
}

Array TiledWangSet::get_colors() const {
	Array arr;
	if (wang_set) {
		for (auto &c : wang_set->getColors()) {
			Ref<TiledWangColor> godot_c;
			godot_c.instantiate();
			godot_c->set_wang_color(const_cast<tson::WangColor *>(&c));
			arr.push_back(godot_c);
		}
	}
	return arr;
}

Array TiledWangSet::get_properties() {
	Array arr;
	if (wang_set) {
		for (auto &prop : wang_set->getProperties().getProperties()) {
			Ref<TiledProperty> p;
			p.instantiate();
			p->set_property(const_cast<tson::Property *>(&prop.second));
			arr.push_back(p);
		}
	}
	return arr;
}

// -------------------------------------------------------------
// TiledGrid
// -------------------------------------------------------------
void TiledGrid::_bind_methods() {
	ClassDB::bind_method(D_METHOD("get_orientation"), &TiledGrid::get_orientation);
	ClassDB::bind_method(D_METHOD("get_size"), &TiledGrid::get_size);
}

String TiledGrid::get_orientation() const {
	return grid ? to_godot_string(grid->getOrientation()) : String();
}
Vector2i TiledGrid::get_size() const {
	return grid ? Vector2i(grid->getSize().x, grid->getSize().y) : Vector2i();
}

// -------------------------------------------------------------
// TiledTerrain
// -------------------------------------------------------------
void TiledTerrain::_bind_methods() {
	ClassDB::bind_method(D_METHOD("get_name"), &TiledTerrain::get_name);
	ClassDB::bind_method(D_METHOD("get_tile"), &TiledTerrain::get_tile);
	ClassDB::bind_method(D_METHOD("get_properties"), &TiledTerrain::get_properties);
}

String TiledTerrain::get_name() const {
	return terrain ? to_godot_string(terrain->getName()) : String();
}
int TiledTerrain::get_tile() const {
	return terrain ? terrain->getTile() : 0;
}

Array TiledTerrain::get_properties() {
	Array arr;
	if (terrain) {
		for (auto &prop : terrain->getProperties().getProperties()) {
			Ref<TiledProperty> p;
			p.instantiate();
			p->set_property(const_cast<tson::Property *>(&prop.second));
			arr.push_back(p);
		}
	}
	return arr;
}

// -------------------------------------------------------------
// TiledText
// -------------------------------------------------------------
void TiledText::_bind_methods() {
	ClassDB::bind_method(D_METHOD("get_text"), &TiledText::get_text);
	ClassDB::bind_method(D_METHOD("get_color"), &TiledText::get_color);
	ClassDB::bind_method(D_METHOD("is_wrap"), &TiledText::is_wrap);
	ClassDB::bind_method(D_METHOD("is_bold"), &TiledText::is_bold);
	ClassDB::bind_method(D_METHOD("get_font_family"), &TiledText::get_font_family);
	ClassDB::bind_method(D_METHOD("is_italic"), &TiledText::is_italic);
	ClassDB::bind_method(D_METHOD("is_kerning"), &TiledText::is_kerning);
	ClassDB::bind_method(D_METHOD("get_pixel_size"), &TiledText::get_pixel_size);
	ClassDB::bind_method(D_METHOD("is_strikeout"), &TiledText::is_strikeout);
	ClassDB::bind_method(D_METHOD("is_underline"), &TiledText::is_underline);
	ClassDB::bind_method(D_METHOD("get_horizontal_alignment"), &TiledText::get_horizontal_alignment);
	ClassDB::bind_method(D_METHOD("get_vertical_alignment"), &TiledText::get_vertical_alignment);
}

String TiledText::get_text() const {
	return text ? to_godot_string(text->text) : String();
}
Color TiledText::get_color() const {
	if (!text) {
		return Color();
	}
	tson::Colori col = text->color;
	return Color(col.r / 255.0f, col.g / 255.0f, col.b / 255.0f, col.a / 255.0f);
}
bool TiledText::is_wrap() const {
	return text ? text->wrap : false;
}
bool TiledText::is_bold() const {
	return text ? text->bold : false;
}
String TiledText::get_font_family() const {
	return text ? to_godot_string(text->fontFamily) : String("sans-serif");
}
bool TiledText::is_italic() const {
	return text ? text->italic : false;
}
bool TiledText::is_kerning() const {
	return text ? text->kerning : true;
}
int TiledText::get_pixel_size() const {
	return text ? text->pixelSize : 16;
}
bool TiledText::is_strikeout() const {
	return text ? text->strikeout : false;
}
bool TiledText::is_underline() const {
	return text ? text->underline : false;
}
int TiledText::get_horizontal_alignment() const {
	return text ? (int)text->horizontalAlignment : 0;
}
int TiledText::get_vertical_alignment() const {
	return text ? (int)text->verticalAlignment : 0;
}

// -------------------------------------------------------------
// TiledTransformations
// -------------------------------------------------------------
void TiledTransformations::_bind_methods() {
	ClassDB::bind_method(D_METHOD("allow_hflip"), &TiledTransformations::allow_hflip);
	ClassDB::bind_method(D_METHOD("allow_preferuntransformed"), &TiledTransformations::allow_preferuntransformed);
	ClassDB::bind_method(D_METHOD("allow_rotation"), &TiledTransformations::allow_rotation);
	ClassDB::bind_method(D_METHOD("allow_vflip"), &TiledTransformations::allow_vflip);
}

bool TiledTransformations::allow_hflip() const {
	return transformations ? transformations->allowHflip() : false;
}
bool TiledTransformations::allow_preferuntransformed() const {
	return transformations ? transformations->allowPreferuntransformed() : false;
}
bool TiledTransformations::allow_rotation() const {
	return transformations ? transformations->allowRotation() : false;
}
bool TiledTransformations::allow_vflip() const {
	return transformations ? transformations->allowVflip() : false;
}

// -------------------------------------------------------------
// TiledClass
// -------------------------------------------------------------
void TiledClass::_bind_methods() {
	ClassDB::bind_method(D_METHOD("get_id"), &TiledClass::get_id);
	ClassDB::bind_method(D_METHOD("get_name"), &TiledClass::get_name);
	ClassDB::bind_method(D_METHOD("get_tson_type"), &TiledClass::get_tson_type);
	ClassDB::bind_method(D_METHOD("get_members"), &TiledClass::get_members);
}

int TiledClass::get_id() const {
	return tiled_class ? tiled_class->getId() : 0;
}
String TiledClass::get_name() const {
	return tiled_class ? to_godot_string(tiled_class->getName()) : String();
}
String TiledClass::get_tson_type() const {
	return tiled_class ? to_godot_string(tiled_class->getType()) : String();
}

Array TiledClass::get_members() {
	Array arr;
	if (tiled_class) {
		for (auto &prop : tiled_class->getMembers().getProperties()) {
			Ref<TiledProperty> p;
			p.instantiate();
			p->set_property(const_cast<tson::Property *>(&prop.second));
			arr.push_back(p);
		}
	}
	return arr;
}

// -------------------------------------------------------------
// TiledEnumDefinition
// -------------------------------------------------------------
void TiledEnumDefinition::_bind_methods() {
	ClassDB::bind_method(D_METHOD("get_id"), &TiledEnumDefinition::get_id);
	ClassDB::bind_method(D_METHOD("get_max_value"), &TiledEnumDefinition::get_max_value);
	ClassDB::bind_method(D_METHOD("get_storage_type"), &TiledEnumDefinition::get_storage_type);
	ClassDB::bind_method(D_METHOD("get_name"), &TiledEnumDefinition::get_name);
	ClassDB::bind_method(D_METHOD("has_values_as_flags"), &TiledEnumDefinition::has_values_as_flags);
	ClassDB::bind_method(D_METHOD("has_value", "str"), &TiledEnumDefinition::has_value);
	ClassDB::bind_method(D_METHOD("has_value_id", "num"), &TiledEnumDefinition::has_value_id);
	ClassDB::bind_method(D_METHOD("get_values", "num"), &TiledEnumDefinition::get_values);
}

int TiledEnumDefinition::get_id() const {
	return enum_def ? enum_def->getId() : 0;
}
int TiledEnumDefinition::get_max_value() const {
	return enum_def ? enum_def->getMaxValue() : 0;
}
int TiledEnumDefinition::get_storage_type() const {
	return enum_def ? (int)enum_def->getStorageType() : 0;
}
String TiledEnumDefinition::get_name() const {
	return enum_def ? to_godot_string(enum_def->getName()) : String();
}
bool TiledEnumDefinition::has_values_as_flags() const {
	return enum_def ? enum_def->hasValuesAsFlags() : false;
}
bool TiledEnumDefinition::has_value(const String &str) const {
	return enum_def ? enum_def->exists(std::string(str.utf8().get_data())) : false;
}
bool TiledEnumDefinition::has_value_id(uint32_t num) const {
	return enum_def ? enum_def->exists(num) : false;
}

Array TiledEnumDefinition::get_values(int p_num) {
	Array arr;
	if (enum_def) {
		for (auto &v : enum_def->getValues(p_num)) {
			arr.push_back(to_godot_string(v));
		}
	}
	return arr;
}

// -------------------------------------------------------------
// TiledEnumValue
// -------------------------------------------------------------
void TiledEnumValue::_bind_methods() {
	ClassDB::bind_method(D_METHOD("get_value"), &TiledEnumValue::get_value);
	ClassDB::bind_method(D_METHOD("get_value_name"), &TiledEnumValue::get_value_name);
	ClassDB::bind_method(D_METHOD("contains_value_name", "name"), &TiledEnumValue::contains_value_name);
	ClassDB::bind_method(D_METHOD("get_value_names"), &TiledEnumValue::get_value_names);
	ClassDB::bind_method(D_METHOD("get_definition"), &TiledEnumValue::get_definition);
}

int TiledEnumValue::get_value() const {
	return enum_val ? enum_val->getValue() : 0;
}
String TiledEnumValue::get_value_name() const {
	return enum_val ? to_godot_string(enum_val->getValueName()) : String();
}
bool TiledEnumValue::contains_value_name(const String &name) const {
	return enum_val ? enum_val->containsValueName(std::string(name.utf8().get_data())) : false;
}

Array TiledEnumValue::get_value_names() const {
	Array arr;
	if (enum_val) {
		for (auto &v : enum_val->getValueNames()) {
			arr.push_back(to_godot_string(v));
		}
	}
	return arr;
}

Ref<TiledEnumDefinition> TiledEnumValue::get_definition() const {
	if (enum_val && enum_val->getDefinition()) {
		Ref<TiledEnumDefinition> d;
		d.instantiate();
		d->set_definition(enum_val->getDefinition());
		return d;
	}
	return Ref<TiledEnumDefinition>();
}

Ref<TiledGrid> TiledTileset::get_grid() const {
	if (tileset) {
		Ref<TiledGrid> godot;
		godot.instantiate();
		godot->set_grid(&tileset->getGrid());
		return godot;
	}
	return Ref<TiledGrid>();
}

Array TiledTileset::get_terrains() const {
	Array arr;
	if (tileset) {
		for (auto &t : tileset->getTerrains()) {
			Ref<TiledTerrain> godot;
			godot.instantiate();
			godot->set_terrain(const_cast<tson::Terrain *>(&t));
			arr.push_back(godot);
		}
	}
	return arr;
}

Ref<TiledTransformations> TiledTileset::get_transformations() const {
	if (tileset) {
		Ref<TiledTransformations> godot;
		godot.instantiate();
		godot->set_transformations(&tileset->getTransformations());
		return godot;
	}
	return Ref<TiledTransformations>();
}

Ref<TiledText> TiledObject::get_text() const {
	if (object && object->getText().text != "") {
		Ref<TiledText> godot;
		godot.instantiate();
		godot->set_text(&object->getText());
		return godot;
	}
	return Ref<TiledText>();
}

Ref<TiledClass> TiledProject::get_tiled_class(const String &p_name) const {
	if (project) {
		tson::TiledClass *cls = project->getClass(std::string(p_name.utf8().get_data()));
		if (cls) {
			Ref<TiledClass> godot;
			godot.instantiate();
			godot->set_class(cls);
			return godot;
		}
	}
	return Ref<TiledClass>();
}

Ref<TiledEnumDefinition> TiledProject::get_enum_definition(const String &p_name) const {
	if (project) {
		tson::EnumDefinition *enm = project->getEnumDefinition(std::string(p_name.utf8().get_data()));
		if (enm) {
			Ref<TiledEnumDefinition> godot;
			godot.instantiate();
			godot->set_definition(enm);
			return godot;
		}
	}
	return Ref<TiledEnumDefinition>();
}

// -------------------------------------------------------------
// TiledTileObject
// -------------------------------------------------------------
void TiledTileObject::_bind_methods() {
	ClassDB::bind_method(D_METHOD("get_position_in_tile_units"), &TiledTileObject::get_position_in_tile_units);
	ClassDB::bind_method(D_METHOD("get_position"), &TiledTileObject::get_position);
}

Vector2i TiledTileObject::get_position_in_tile_units() const {
	return tile_object ? Vector2i(tile_object->getPositionInTileUnits().x, tile_object->getPositionInTileUnits().y) : Vector2i();
}
Vector2 TiledTileObject::get_position() const {
	return tile_object ? Vector2(tile_object->getPosition().x, tile_object->getPosition().y) : Vector2();
}

// -------------------------------------------------------------
// TiledProjectPropertyTypes
// -------------------------------------------------------------
void TiledProjectPropertyTypes::_bind_methods() {
	ClassDB::bind_method(D_METHOD("get_enums"), &TiledProjectPropertyTypes::get_enums);
	ClassDB::bind_method(D_METHOD("get_classes"), &TiledProjectPropertyTypes::get_classes);
	ClassDB::bind_method(D_METHOD("get_enum_definition", "name"), &TiledProjectPropertyTypes::get_enum_definition);
	ClassDB::bind_method(D_METHOD("get_tiled_class", "name"), &TiledProjectPropertyTypes::get_tiled_class);
}

Array TiledProjectPropertyTypes::get_enums() const {
	Array arr;
	if (prop_types) {
		for (auto &e : prop_types->getEnums()) {
			Ref<TiledEnumDefinition> ed;
			ed.instantiate();
			ed->set_definition(const_cast<tson::EnumDefinition *>(&e));
			arr.push_back(ed);
		}
	}
	return arr;
}

Array TiledProjectPropertyTypes::get_classes() const {
	Array arr;
	if (prop_types) {
		for (auto &c : prop_types->getClasses()) {
			Ref<TiledClass> tc;
			tc.instantiate();
			tc->set_class(const_cast<tson::TiledClass *>(&c));
			arr.push_back(tc);
		}
	}
	return arr;
}

Ref<TiledEnumDefinition> TiledProjectPropertyTypes::get_enum_definition(const String &name) const {
	if (prop_types) {
		tson::EnumDefinition *e = const_cast<tson::ProjectPropertyTypes *>(prop_types)->getEnumDefinition(std::string(name.utf8().get_data()));
		if (e) {
			Ref<TiledEnumDefinition> ed;
			ed.instantiate();
			ed->set_definition(e);
			return ed;
		}
	}
	return Ref<TiledEnumDefinition>();
}

Ref<TiledClass> TiledProjectPropertyTypes::get_tiled_class(const String &name) const {
	if (prop_types) {
		tson::TiledClass *c = const_cast<tson::ProjectPropertyTypes *>(prop_types)->getClass(std::string(name.utf8().get_data()));
		if (c) {
			Ref<TiledClass> tc;
			tc.instantiate();
			tc->set_class(c);
			return tc;
		}
	}
	return Ref<TiledClass>();
}
