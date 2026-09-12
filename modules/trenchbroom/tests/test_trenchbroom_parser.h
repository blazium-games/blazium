/**************************************************************************/
/*  test_trenchbroom_parser.h                                             */
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

#include "modules/trenchbroom/core/data.h"
#include "modules/trenchbroom/core/entity_assembler.h"
#include "modules/trenchbroom/core/geometry_generator.h"
#include "modules/trenchbroom/core/map_parser.h"
#include "modules/trenchbroom/fgd/blazium_fgd_entity_class.h"
#include "modules/trenchbroom/fgd/blazium_fgd_file.h"
#include "modules/trenchbroom/fgd/blazium_fgd_solid_class.h"
#include "modules/trenchbroom/import/quake_map_file.h"
#ifdef TOOLS_ENABLED
#include "modules/trenchbroom/import/resource_importer_map.h"
#endif
#include "modules/trenchbroom/netradiant/netradiant_custom_gamepack_config.h"
#include "modules/trenchbroom/trenchbroom/trenchbroom_game_config.h"
#include "modules/trenchbroom/trenchbroom_defaults.h"
#include "modules/trenchbroom/trenchbroom_local_config.h"
#include "modules/trenchbroom/trenchbroom_map_settings.h"
#include "modules/trenchbroom/util/trenchbroom_util.h"

#include "core/config/project_settings.h"
#include "core/io/file_access.h"
#include "core/io/json.h"
#include "core/io/resource_loader.h"
#include "core/math/transform_3d.h"
#include "core/os/os.h"
#include "core/templates/hash_set.h"
#include "core/templates/vector.h"
#include "scene/3d/node_3d.h"
#include "scene/resources/material.h"
#include "tests/test_macros.h"
#include "tests/test_utils.h"
#ifdef TOOLS_ENABLED
#include "modules/gdscript/gdscript.h"
#endif

namespace TestTrenchbroomParser {

class MapParserTestAccess : public TrenchbroomMapParser {
public:
	static void transform_patch(PatchData &p_patch, const Transform3D &p_xform) {
		_transform_patch(p_patch, p_xform);
	}
};

static void _append_u32_le(Vector<uint8_t> &p_buffer, uint32_t p_value) {
	p_buffer.push_back(p_value & 0xFF);
	p_buffer.push_back((p_value >> 8) & 0xFF);
	p_buffer.push_back((p_value >> 16) & 0xFF);
	p_buffer.push_back((p_value >> 24) & 0xFF);
}

static bool _write_trenchbroom_user_config(const Dictionary &p_config_data) {
	DirAccess::make_dir_recursive_absolute(OS::get_singleton()->get_user_data_dir());
	Ref<FileAccess> file = FileAccess::open("user://trenchbroom_config.json", FileAccess::WRITE);
	if (file.is_null()) {
		return false;
	}
	file->store_string(JSON::stringify(p_config_data));
	return true;
}

static String _write_trenchbroom_minimal_wad2() {
	const String wad_path = TestUtils::get_temp_path("trenchbroom_minimal.wad");
	const int width = 16;
	const int height = 16;
	static const char texture_name[] = "test";
	Vector<uint8_t> texture;
	for (int i = 0; i < 16; i++) {
		const uint8_t name_byte = (i < 4) ? uint8_t(texture_name[i]) : 0;
		texture.push_back(name_byte);
	}
	_append_u32_le(texture, width);
	_append_u32_le(texture, height);
	for (int i = 0; i < 4; i++) {
		texture.push_back(0);
	}
	for (int i = 0; i < width * height; i++) {
		texture.push_back(i % 256);
	}

	Ref<FileAccess> wad_file = FileAccess::open(wad_path, FileAccess::WRITE);
	if (wad_file.is_null()) {
		return String();
	}
	wad_file->store_buffer((const uint8_t *)"WAD2", 4);
	wad_file->store_32(1);
	wad_file->store_32(12 + texture.size());
	wad_file->store_buffer(texture.ptr(), texture.size());
	wad_file->store_32(12);
	wad_file->store_32(texture.size());
	wad_file->store_32(texture.size());
	wad_file->store_8(0x44);
	wad_file->store_8(0);
	wad_file->store_8(0);
	for (int i = 0; i < 16; i++) {
		const uint8_t name_byte = (i < 4) ? uint8_t(texture_name[i]) : 0;
		wad_file->store_8(name_byte);
	}
	return wad_path;
}

static String _write_trenchbroom_halflife_wad3() {
	const String wad_path = TestUtils::get_temp_path("trenchbroom_halflife.wad");
	const int width = 16;
	const int height = 16;
	static const char texture_name[] = "hltest";
	Vector<uint8_t> texture;
	for (int i = 0; i < 16; i++) {
		const uint8_t name_byte = (i < 6) ? uint8_t(texture_name[i]) : 0;
		texture.push_back(name_byte);
	}
	_append_u32_le(texture, width);
	_append_u32_le(texture, height);
	const uint32_t mip0_offset = 40;
	const uint32_t mip1_offset = mip0_offset + width * height;
	const uint32_t mip2_offset = mip1_offset + (width / 2) * (height / 2);
	const uint32_t mip3_offset = mip2_offset + (width / 4) * (height / 4);
	_append_u32_le(texture, mip0_offset);
	_append_u32_le(texture, mip1_offset);
	_append_u32_le(texture, mip2_offset);
	_append_u32_le(texture, mip3_offset);
	for (int i = 0; i < width * height; i++) {
		texture.push_back(i % 256);
	}
	for (int level = 1; level < 4; level++) {
		const int count = (width >> level) * (height >> level);
		for (int i = 0; i < count; i++) {
			texture.push_back(0);
		}
	}
	texture.push_back(0);
	texture.push_back(0);
	for (int i = 0; i < 256; i++) {
		texture.push_back(i);
		texture.push_back((i * 2) % 256);
		texture.push_back((i * 3) % 256);
	}

	Ref<FileAccess> wad_file = FileAccess::open(wad_path, FileAccess::WRITE);
	if (wad_file.is_null()) {
		return String();
	}
	wad_file->store_buffer((const uint8_t *)"WAD3", 4);
	wad_file->store_32(1);
	wad_file->store_32(12 + texture.size());
	wad_file->store_buffer(texture.ptr(), texture.size());
	wad_file->store_32(12);
	wad_file->store_32(texture.size());
	wad_file->store_32(texture.size());
	wad_file->store_8(0x43);
	wad_file->store_8(0);
	wad_file->store_8(0);
	for (int i = 0; i < 16; i++) {
		const uint8_t name_byte = (i < 6) ? uint8_t(texture_name[i]) : 0;
		wad_file->store_8(name_byte);
	}
	return wad_path;
}

TEST_CASE("[Trenchbroom] MapSettings inverse scale factor") {
	Ref<TrenchbroomMapSettings> settings;
	settings.instantiate();
	settings->set_inverse_scale_factor(32.0);
	CHECK(settings->get_inverse_scale_factor() == doctest::Approx(32.0));
	CHECK(settings->get_scale_factor() == doctest::Approx(1.0 / 32.0));
}

TEST_CASE("[Trenchbroom] QuakeMapFile stores map data") {
	Ref<QuakeMapFile> map_file;
	map_file.instantiate();
	map_file->set_map_data("{\n\"worldspawn\"\n{\n\"classname\" \"worldspawn\"\n}\n}\n");
	CHECK(map_file->get_map_data().contains("worldspawn"));
	CHECK(map_file->get_revision() == 0);
}

TEST_CASE("[Trenchbroom] MapParser parses minimal Quake map") {
	const String map_path = TestUtils::get_temp_path("trenchbroom_test.map");
	Ref<FileAccess> file = FileAccess::open(map_path, FileAccess::WRITE);
	REQUIRE(file.is_valid());
	file->store_string(
			"{\n"
			"\"worldspawn\"\n"
			"{\n"
			"\"classname\" \"worldspawn\"\n"
			"}\n"
			"}\n");
	file.unref();

	Ref<TrenchbroomMapSettings> settings;
	settings.instantiate();
	settings->set_inverse_scale_factor(32.0);

	Ref<TrenchbroomMapParser> parser;
	parser.instantiate();
	const ParseData parse_data = parser->parse_map_data(map_path, settings.ptr());
	CHECK(parse_data.entities.size() >= 1);
}

TEST_CASE("[Trenchbroom] Default game config includes brush tags") {
	Ref<TrenchbroomGameConfig> config = TrenchbroomDefaults::create_default_game_config();
	REQUIRE(config.is_valid());
	CHECK(config->get_brush_tags().size() >= 2);
	CHECK(config->get_brushface_tags().size() >= 3);
	CHECK(!config->get_game_name().is_empty());
}

TEST_CASE("[Trenchbroom] NetRadiant gamepack defaults") {
	Ref<NetRadiantCustomGamePackConfig> config;
	config.instantiate();
	config->set_fgd_file(TrenchbroomDefaults::create_default_fgd());
	CHECK(config->get_netradiant_custom_shaders().size() >= 3);
	CHECK(config->get_game_name() == "Blazium");
}

TEST_CASE("[Trenchbroom] Patch-only entity parses as solid") {
	const String map_path = TestUtils::get_temp_path("trenchbroom_patch.map");
	Ref<FileAccess> file = FileAccess::open(map_path, FileAccess::WRITE);
	REQUIRE(file.is_valid());
	file->store_string(
			"{\n"
			"\"classname\" \"worldspawn\"\n"
			"{\n"
			"patchDef3\n"
			"{\n"
			"\"patch/patch\"\n"
			"( 3 3 0 0 0 )\n"
			"(\n"
			"( 0 0 0 0 0 )\n"
			"( 32 0 0 0.25 0 )\n"
			"( 64 0 0 0.5 0 )\n"
			")\n"
			"(\n"
			"( 0 32 32 0 0.25 )\n"
			"( 32 32 48 0.25 0.25 )\n"
			"( 64 32 32 0.5 0.25 )\n"
			")\n"
			"(\n"
			"( 0 64 0 0 0.5 )\n"
			"( 32 64 0 0.25 0.5 )\n"
			"( 64 64 0 0.5 0.5 )\n"
			")\n"
			"}\n"
			"}\n"
			"}\n");
	file.unref();

	Ref<TrenchbroomMapSettings> settings;
	settings.instantiate();
	settings->set_inverse_scale_factor(32.0);

	Ref<BlaziumFGDSolidClass> worldspawn;
	worldspawn.instantiate();
	worldspawn->set_classname("worldspawn");
	worldspawn->set_node_class("StaticBody3D");
	worldspawn->set_spawn_type(BlaziumFGDSolidClass::SPAWN_WORLDSPAWN);

	Ref<BlaziumFGDFile> fgd;
	fgd.instantiate();
	Array defs;
	defs.push_back(worldspawn);
	fgd->set_entity_definitions(defs);
	settings->set_entity_fgd(fgd);
	REQUIRE(settings.is_valid());

	Ref<TrenchbroomMapParser> parser;
	parser.instantiate();
	const ParseData parse_data = parser->parse_map_data(map_path, settings.ptr());
	REQUIRE(parse_data.entities.size() >= 1);
	CHECK(parse_data.entities[0].patches.size() >= 1);
}

TEST_CASE("[Trenchbroom] LocalConfig set/get round-trip") {
	Ref<TrenchbroomLocalConfig> config;
	config.instantiate();
	REQUIRE(config.is_valid());

	Dictionary config_data;
	config_data["FGD_OUTPUT_FOLDER"] = String("/tmp/trenchbroom_fgd_test");
	config_data["TRENCHBROOM_GAME_CONFIG_FOLDER"] = String("/tmp/trenchbroom_tb_test");
	REQUIRE(_write_trenchbroom_user_config(config_data));

	config->reload_trenchbroom_settings();
	CHECK(config->get_fgd_output_folder() == String("/tmp/trenchbroom_fgd_test"));
	CHECK(config->get_trenchbroom_game_config_folder() == String("/tmp/trenchbroom_tb_test"));
}

TEST_CASE("[Trenchbroom] GameConfig export_file smoke") {
	const String export_dir = TestUtils::get_temp_path("trenchbroom_tb_export");
	DirAccess::make_dir_recursive_absolute(export_dir);

	Dictionary config_data;
	config_data["TRENCHBROOM_GAME_CONFIG_FOLDER"] = export_dir.replace("\\", "/");
	REQUIRE(_write_trenchbroom_user_config(config_data));

	Ref<TrenchbroomGameConfig> config = TrenchbroomDefaults::create_default_game_config();
	REQUIRE(config.is_valid());
	config->export_file();

	CHECK(FileAccess::exists(export_dir.path_join("GameConfig.cfg")));
}

TEST_CASE("[Trenchbroom] Defaults dir resolves from project setting") {
	if (ProjectSettings::get_singleton()) {
		ProjectSettings::get_singleton()->set_setting("blazium/trenchbroom/defaults_path", "res://trenchbroom_defaults");
	}
	const String defaults_dir = TrenchbroomDefaults::get_defaults_dir();
	CHECK(defaults_dir == String("res://trenchbroom_defaults"));
}

#ifdef TOOLS_ENABLED

TEST_CASE("[Trenchbroom] EntityAssembler sets func_godot_properties") {
	Ref<TrenchbroomMapSettings> settings;
	settings.instantiate();
	TrenchbroomEntityAssembler assembler(settings.ptr());

	Ref<GDScript> script;
	script.instantiate();
	script->set_source_code("extends Node3D\n@export var func_godot_properties: Dictionary = {}");
	const Error reload_err = script->reload();
	if (reload_err != OK) {
		return;
	}

	Node3D *node = memnew(Node3D);
	node->set_script(script);

	EntityData entity;
	entity.properties["classname"] = "test_entity";
	entity.properties["targetname"] = "receiver_test";
	assembler.apply_entity_properties(node, entity);

	const Dictionary props = node->get("func_godot_properties");
	CHECK(props.has("targetname"));
	CHECK(String(props["targetname"]) == String("receiver_test"));

	memdelete(node);
}

#endif // TOOLS_ENABLED

TEST_CASE("[Trenchbroom] MapParser respects profile flag") {
	const String map_path = TestUtils::get_temp_path("trenchbroom_profile.map");
	Ref<FileAccess> file = FileAccess::open(map_path, FileAccess::WRITE);
	REQUIRE(file.is_valid());
	file->store_string(
			"{\n"
			"\"worldspawn\"\n"
			"{\n"
			"\"classname\" \"worldspawn\"\n"
			"}\n"
			"}\n");
	file.unref();

	Ref<TrenchbroomMapSettings> settings;
	settings.instantiate();

	Ref<TrenchbroomMapParser> parser;
	parser.instantiate();
	const ParseData silent = parser->parse_map_data(map_path, settings.ptr(), false);
	const ParseData verbose = parser->parse_map_data(map_path, settings.ptr(), true);
	CHECK(silent.entities.size() >= 1);
	CHECK(verbose.entities.size() == silent.entities.size());
}

TEST_CASE("[Trenchbroom] build_texture_map registers textures") {
	Ref<TrenchbroomMapSettings> settings;
	settings.instantiate();
	LocalVector<EntityData> entities;

	const Array texture_map = TrenchbroomUtil::build_texture_map(entities, settings.ptr());
	REQUIRE(texture_map.size() >= 2);
	const Dictionary materials = texture_map[0];
	const Dictionary sizes = texture_map[1];
	CHECK(materials.size() == 0);
	CHECK(sizes.size() == 0);
}

TEST_CASE("[Trenchbroom] minimal brush map parses six-face brush") {
	const String map_path = TestUtils::get_temp_path("trenchbroom_minimal_brush.map");
	Ref<FileAccess> file = FileAccess::open(map_path, FileAccess::WRITE);
	REQUIRE(file.is_valid());
	file->store_string(
			"{\n"
			"\"classname\" \"worldspawn\"\n"
			"{\n"
			"( 64 0 0 ) ( 0 0 0 ) ( 0 64 0 ) dev/-0 0 0 0 1 1\n"
			"( 0 0 64 ) ( 64 0 64 ) ( 64 64 64 ) dev/-0 0 0 0 1 1\n"
			"( 0 0 0 ) ( 0 0 64 ) ( 0 64 64 ) dev/-0 0 0 0 1 1\n"
			"( 64 0 0 ) ( 64 64 0 ) ( 64 64 64 ) dev/-0 0 0 0 1 1\n"
			"( 0 64 0 ) ( 64 64 0 ) ( 64 64 64 ) dev/-0 0 0 0 1 1\n"
			"( 0 0 0 ) ( 64 0 0 ) ( 64 0 64 ) dev/-0 0 0 0 1 1\n"
			"}\n"
			"}\n");
	file.unref();

	Ref<TrenchbroomMapSettings> settings;
	settings.instantiate();
	settings->set_inverse_scale_factor(32.0);

	Ref<BlaziumFGDSolidClass> worldspawn;
	worldspawn.instantiate();
	worldspawn->set_classname("worldspawn");

	Ref<BlaziumFGDFile> fgd;
	fgd.instantiate();
	Array defs;
	defs.push_back(worldspawn);
	fgd->set_entity_definitions(defs);
	settings->set_entity_fgd(fgd);

	Ref<TrenchbroomMapParser> parser;
	parser.instantiate();
	const ParseData parse_data = parser->parse_map_data(map_path, settings.ptr());
	REQUIRE(parse_data.entities.size() >= 1);
	REQUIRE(parse_data.entities[0].brushes.size() >= 1);
	CHECK(parse_data.entities[0].brushes[0].faces.size() == 6);
}

TEST_CASE("[Trenchbroom] compute_interior_faces_to_cull marks shared opposite faces") {
	Ref<TrenchbroomGeometryGenerator> generator = memnew(TrenchbroomGeometryGenerator(nullptr, 512.0));

	FaceData inner_face;
	inner_face.plane = Plane(Vector3(1, 0, 0), 2.0);
	inner_face.vertices = PackedVector3Array{
		Vector3(2, 0, 0),
		Vector3(2, 0, 2),
		Vector3(2, 2, 2),
		Vector3(2, 2, 0),
	};
	inner_face.indices = PackedInt32Array{ 0, 1, 2, 0, 2, 3 };
	inner_face.wind();
	inner_face.index_vertices();

	FaceData outer_face;
	outer_face.plane = Plane(Vector3(-1, 0, 0), -2.0);
	outer_face.vertices = PackedVector3Array{
		Vector3(2, 0, 0),
		Vector3(2, 2, 0),
		Vector3(2, 2, 2),
		Vector3(2, 0, 2),
	};
	outer_face.indices = PackedInt32Array{ 0, 1, 2, 0, 2, 3 };
	outer_face.wind();
	outer_face.index_vertices();

	LocalVector<FaceData *> faces;
	faces.push_back(&inner_face);
	faces.push_back(&outer_face);

	HashMap<FaceData *, uint32_t> face_brush_index;
	face_brush_index[&inner_face] = 0;
	face_brush_index[&outer_face] = 1;

	const HashSet<FaceData *> culled = generator->compute_interior_faces_to_cull(faces, face_brush_index, TrenchbroomUtil::VERTEX_EPSILON);
	CHECK((culled.has(&inner_face) || culled.has(&outer_face)));
}

TEST_CASE("[Trenchbroom] collect_texture_names gathers unique visual textures") {
	Ref<TrenchbroomMapSettings> settings;
	settings.instantiate();
	settings->set_clip_texture("clip");
	settings->set_skip_texture("skip");
	settings->set_origin_texture("origin");

	Ref<BlaziumFGDSolidClass> solid;
	solid.instantiate();
	solid->set_build_visuals(true);

	EntityData entity;
	entity.definition = solid;

	BrushData brush;
	FaceData face_a;
	face_a.texture = "tex_a";
	brush.faces.push_back(face_a);
	FaceData face_b;
	face_b.texture = "tex_b";
	brush.faces.push_back(face_b);
	FaceData face_skip;
	face_skip.texture = "skip";
	brush.faces.push_back(face_skip);
	entity.brushes.push_back(brush);

	PatchData patch;
	patch.texture = "tex_a";
	entity.patches.push_back(patch);

	LocalVector<EntityData> entities;
	entities.push_back(entity);

	HashSet<String> names;
	TrenchbroomUtil::collect_texture_names(entities, settings.ptr(), names);
	CHECK(names.size() == 2);
	CHECK(names.has("tex_a"));
	CHECK(names.has("tex_b"));
}

TEST_CASE("[Trenchbroom] EDITOR_OTHER FGD export produces generic Hammer output") {
	Ref<BlaziumFGDFile> fgd = TrenchbroomDefaults::create_default_fgd();
	const String text = fgd->build_class_text(BlaziumFGDFile::EDITOR_OTHER);
	REQUIRE(!text.is_empty());
	CHECK_FALSE(text.contains("{\"path\""));
	CHECK(text.contains("@SolidClass"));
}

TEST_CASE("[Trenchbroom] MapParser parses VMF dispinfo distance rows") {
	const String map_path = TestUtils::get_temp_path("trenchbroom_dispdist.vmf");
	Ref<FileAccess> file = FileAccess::open(map_path, FileAccess::WRITE);
	REQUIRE(file.is_valid());
	file->store_string(
			"world\n"
			"{\n"
			"\"classname\" \"worldspawn\"\n"
			"solid\n"
			"{\n"
			"side\n"
			"{\n"
			"\"plane\" \"(64 0 0) (0 0 0) (0 64 0)\"\n"
			"\"material\" \"dev/-0\"\n"
			"\"uaxis\" \"[1 0 0 0] 0.03125\"\n"
			"\"vaxis\" \"[0 -1 0 0] 0.03125\"\n"
			"dispinfo\n"
			"{\n"
			"\"power\" \"2\"\n"
			"\"elevation\" \"0\"\n"
			"\"startposition\" \"[0 0 0]\"\n"
			"distances\n"
			"{\n"
			"\"row0\" \"0 0 0 0 0\"\n"
			"\"row1\" \"0 8 16 8 0\"\n"
			"\"row2\" \"0 16 32 16 0\"\n"
			"\"row3\" \"0 8 16 8 0\"\n"
			"\"row4\" \"0 0 0 0 0\"\n"
			"}\n"
			"}\n"
			"}\n"
			"}\n"
			"}\n");
	file.unref();

	Ref<TrenchbroomMapSettings> settings;
	settings.instantiate();
	settings->set_inverse_scale_factor(32.0);

	Ref<TrenchbroomMapParser> parser;
	parser.instantiate();
	const ParseData parse_data = parser->parse_map_data(map_path, settings.ptr());
	REQUIRE(parse_data.entities.size() >= 1);
	REQUIRE(parse_data.entities[0].brushes.size() >= 1);
	REQUIRE(parse_data.entities[0].brushes[0].faces.size() >= 1);

	const DispInfoData &disp = parse_data.entities[0].brushes[0].faces[0].disp;
	CHECK(disp.valid);
	CHECK(disp.get_grid_size() == 5);
	CHECK(disp.has_distance_grid());
	CHECK(disp.distances.size() == 25);
	CHECK(disp.distances[12] == doctest::Approx(1.0));
	CHECK(disp.startposition.is_equal_approx(Vector3(0, 0, 0)));
}

TEST_CASE("[Trenchbroom] MapParser VMF dispinfo scope does not leak to other sides") {
	const String map_path = TestUtils::get_temp_path("trenchbroom_disp_two_side.vmf");
	Ref<FileAccess> file = FileAccess::open(map_path, FileAccess::WRITE);
	REQUIRE(file.is_valid());
	file->store_string(
			"world\n"
			"{\n"
			"\"classname\" \"worldspawn\"\n"
			"solid\n"
			"{\n"
			"side\n"
			"{\n"
			"\"plane\" \"(64 0 0) (0 0 0) (0 64 0)\"\n"
			"\"material\" \"dev/-0\"\n"
			"dispinfo\n"
			"{\n"
			"\"power\" \"2\"\n"
			"\"elevation\" \"8\"\n"
			"}\n"
			"}\n"
			"side\n"
			"{\n"
			"\"plane\" \"(0 0 64) (64 0 64) (64 64 64)\"\n"
			"\"material\" \"dev/-0\"\n"
			"}\n"
			"}\n"
			"}\n");
	file.unref();

	Ref<TrenchbroomMapSettings> settings;
	settings.instantiate();
	settings->set_inverse_scale_factor(32.0);

	Ref<TrenchbroomMapParser> parser;
	parser.instantiate();
	const ParseData parse_data = parser->parse_map_data(map_path, settings.ptr());
	REQUIRE(parse_data.entities.size() >= 1);
	REQUIRE(parse_data.entities[0].brushes.size() >= 1);
	REQUIRE(parse_data.entities[0].brushes[0].faces.size() >= 2);

	CHECK(parse_data.entities[0].brushes[0].faces[0].disp.valid);
	CHECK_FALSE(parse_data.entities[0].brushes[0].faces[1].disp.valid);
}

TEST_CASE("[Trenchbroom] MapParser parses VMF dispinfo offsets") {
	const String map_path = TestUtils::get_temp_path("trenchbroom_disp_offset.vmf");
	Ref<FileAccess> file = FileAccess::open(map_path, FileAccess::WRITE);
	REQUIRE(file.is_valid());
	file->store_string(
			"world\n"
			"{\n"
			"\"classname\" \"worldspawn\"\n"
			"solid\n"
			"{\n"
			"side\n"
			"{\n"
			"\"plane\" \"(64 0 0) (0 0 0) (0 64 0)\"\n"
			"\"material\" \"dev/-0\"\n"
			"dispinfo\n"
			"{\n"
			"\"power\" \"2\"\n"
			"offsets\n"
			"{\n"
			"\"row0\" \"0 0 0 0 0 0 0 0 0 0 0 0 0 0 0\"\n"
			"\"row2\" \"0 0 32 0 0 0 0 0 0 0 0 0 0 0 0\"\n"
			"}\n"
			"}\n"
			"}\n"
			"}\n"
			"}\n");
	file.unref();

	Ref<TrenchbroomMapSettings> settings;
	settings.instantiate();
	settings->set_inverse_scale_factor(32.0);

	Ref<TrenchbroomMapParser> parser;
	parser.instantiate();
	const ParseData parse_data = parser->parse_map_data(map_path, settings.ptr());
	REQUIRE(parse_data.entities.size() >= 1);
	const DispInfoData &disp = parse_data.entities[0].brushes[0].faces[0].disp;
	CHECK(disp.valid);
	CHECK(disp.offsets.size() >= 5);
	real_t max_offset_length = 0.0;
	for (int i = 0; i < disp.offsets.size(); i++) {
		max_offset_length = MAX(max_offset_length, disp.offsets[i].length());
	}
	CHECK(max_offset_length == doctest::Approx(1.0));
}

#ifdef TOOLS_ENABLED

TEST_CASE("[Trenchbroom] ResourceImporterQuakeMap imports VMF fixture") {
	const String vmf_path = TestUtils::get_temp_path("trenchbroom_import.vmf");
	Ref<FileAccess> file = FileAccess::open(vmf_path, FileAccess::WRITE);
	REQUIRE(file.is_valid());
	const String vmf_text =
			"world\n"
			"{\n"
			"\"classname\" \"worldspawn\"\n"
			"}\n";
	file->store_string(vmf_text);
	file.unref();

	ResourceImporterQuakeMap importer;
	const String save_base = TestUtils::get_temp_path("trenchbroom_imported_map");
	const Error import_err = importer.import(0, vmf_path, save_base, HashMap<StringName, Variant>(), nullptr, nullptr, nullptr);
	CHECK(import_err == OK);
	CHECK(FileAccess::exists(save_base + ".tres"));

	Ref<QuakeMapFile> reference;
	reference.instantiate();
	reference->set_map_data(vmf_text);
	CHECK(reference->get_map_data().contains("worldspawn"));
}

#endif // TOOLS_ENABLED

TEST_CASE("[Trenchbroom] MapParser VMF side rotation changes UV axes") {
	const String map_path = TestUtils::get_temp_path("trenchbroom_rotation.vmf");
	Ref<FileAccess> file = FileAccess::open(map_path, FileAccess::WRITE);
	REQUIRE(file.is_valid());
	file->store_string(
			"world\n"
			"{\n"
			"\"classname\" \"worldspawn\"\n"
			"solid\n"
			"{\n"
			"side\n"
			"{\n"
			"\"plane\" \"(64 0 0) (0 0 0) (0 64 0)\"\n"
			"\"material\" \"dev/-0\"\n"
			"\"uaxis\" \"[1 0 0 0] 0.03125\"\n"
			"\"vaxis\" \"[0 -1 0 0] 0.03125\"\n"
			"\"rotation\" \"45\"\n"
			"}\n"
			"side\n"
			"{\n"
			"\"plane\" \"(0 0 64) (64 0 64) (64 64 64)\"\n"
			"\"material\" \"dev/-0\"\n"
			"\"uaxis\" \"[1 0 0 0] 0.03125\"\n"
			"\"vaxis\" \"[0 -1 0 0] 0.03125\"\n"
			"\"rotation\" \"0\"\n"
			"}\n"
			"}\n"
			"}\n");
	file.unref();

	Ref<TrenchbroomMapSettings> settings;
	settings.instantiate();
	settings->set_inverse_scale_factor(32.0);

	Ref<TrenchbroomMapParser> parser;
	parser.instantiate();
	const ParseData parse_data = parser->parse_map_data(map_path, settings.ptr());
	REQUIRE(parse_data.entities.size() >= 1);
	REQUIRE(parse_data.entities[0].brushes.size() >= 1);
	REQUIRE(parse_data.entities[0].brushes[0].faces.size() >= 2);

	const FaceData &rotated = parse_data.entities[0].brushes[0].faces[0];
	const FaceData &flat = parse_data.entities[0].brushes[0].faces[1];
	CHECK_FALSE(rotated.uv.columns[0].is_equal_approx(flat.uv.columns[0]));
	CHECK_FALSE(rotated.uv.columns[1].is_equal_approx(flat.uv.columns[1]));
}

TEST_CASE("[Trenchbroom] instance patch transform rotates control points") {
	PatchData patch;
	patch.points.push_back(Vector3(0, 0, 0));
	patch.points.push_back(Vector3(64, 0, 0));
	const Transform3D xform(Basis::from_euler(Vector3(0, Math::deg_to_rad(90.0), 0)), Vector3(10, 0, 0));
	MapParserTestAccess::transform_patch(patch, xform);
	CHECK(patch.points[0].is_equal_approx(Vector3(10, 0, 0)));
	CHECK(patch.points[1].distance_to(Vector3(74, 0, 0)) > 0.01);
	CHECK(Math::abs(patch.points[1].z) > 0.01);
}

TEST_CASE("[Trenchbroom] MapParser parses VMF dispinfo offset_normals") {
	const String map_path = TestUtils::get_temp_path("trenchbroom_disp_offset_normals.vmf");
	Ref<FileAccess> file = FileAccess::open(map_path, FileAccess::WRITE);
	REQUIRE(file.is_valid());
	file->store_string(
			"world\n"
			"{\n"
			"\"classname\" \"worldspawn\"\n"
			"solid\n"
			"{\n"
			"side\n"
			"{\n"
			"\"plane\" \"(64 0 0) (0 0 0) (0 64 0)\"\n"
			"\"material\" \"dev/-0\"\n"
			"dispinfo\n"
			"{\n"
			"\"power\" \"2\"\n"
			"offsets\n"
			"{\n"
			"\"row2\" \"0 0 32 0 0 0 0 0 0 0 0 0 0 0 0\"\n"
			"}\n"
			"offset_normals\n"
			"{\n"
			"\"row2\" \"1 0 0 1 0 0 1 0 0 1 0 0 1 0 0\"\n"
			"}\n"
			"}\n"
			"}\n"
			"}\n"
			"}\n");
	file.unref();

	Ref<TrenchbroomMapSettings> settings;
	settings.instantiate();
	settings->set_inverse_scale_factor(32.0);

	Ref<TrenchbroomMapParser> parser;
	parser.instantiate();
	const ParseData parse_data = parser->parse_map_data(map_path, settings.ptr());
	REQUIRE(parse_data.entities.size() >= 1);
	const DispInfoData &disp = parse_data.entities[0].brushes[0].faces[0].disp;
	CHECK(disp.valid);
	CHECK(disp.offset_normals.size() >= 5);
	CHECK(disp.offset_normals[2].is_equal_approx(Vector3(1, 0, 0)));
}

TEST_CASE("[Trenchbroom] ResourceImporterQuakeWad generates valid WAD3 bytes") {
	const String wad_path = _write_trenchbroom_halflife_wad3();
	REQUIRE(!wad_path.is_empty());

	Ref<FileAccess> header = FileAccess::open(wad_path, FileAccess::READ);
	REQUIRE(header.is_valid());
	const Vector<uint8_t> magic = header->get_buffer(4);
	CHECK(magic.size() == 4);
	CHECK(magic[0] == 'W');
	CHECK(magic[1] == 'A');
	CHECK(magic[2] == 'D');
	CHECK(magic[3] == '3');
	CHECK(header->get_length() > 12);
}

TEST_CASE("[Trenchbroom] ResourceImporterQuakeWad generates valid WAD2 bytes") {
	const String wad_path = _write_trenchbroom_minimal_wad2();
	REQUIRE(!wad_path.is_empty());

	Ref<FileAccess> header = FileAccess::open(wad_path, FileAccess::READ);
	REQUIRE(header.is_valid());
	const Vector<uint8_t> magic = header->get_buffer(4);
	CHECK(magic.size() == 4);
	CHECK(magic[0] == 'W');
	CHECK(magic[1] == 'A');
	CHECK(magic[2] == 'D');
	CHECK(magic[3] == '2');
	CHECK(header->get_length() > 12);
}

} //namespace TestTrenchbroomParser
