/**************************************************************************/
/*  test_obfuscation.cpp                                                  */
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

#include "test_obfuscation.h"

#include "core/io/image.h"
#include "core/math/math_funcs.h"
#include "core/templates/hash_map.h"
#include "core/templates/hash_set.h"
#include "core/variant/dictionary.h"

#include "modules/obfuscation/codec/reed_solomon.h"
#include "modules/obfuscation/codec/seal_matrix.h"
#include "modules/obfuscation/seeds/comment_lattice.h"
#include "modules/obfuscation/seeds/copyright_lattice.h"
#include "modules/obfuscation/seeds/output_names.h"
#include "modules/obfuscation/seeds/source_map.h"
#include "modules/obfuscation/stego/audio_mark.h"
#include "modules/obfuscation/stego/image_mark.h"

void test_obfuscation_reed_solomon_roundtrip() {
	Vector<uint8_t> src;
	src.resize(40);
	for (int i = 0; i < src.size(); i++) {
		src.write[i] = (uint8_t)(i * 7 + 3);
	}
	Vector<uint8_t> encoded;
	ObfuscationReedSolomon::encode(src, encoded);
	CHECK(encoded.size() >= ObfuscationReedSolomon::N);
	Vector<uint8_t> decoded;
	CHECK(ObfuscationReedSolomon::decode(encoded, decoded));
	REQUIRE(decoded.size() >= src.size());
	for (int i = 0; i < src.size(); i++) {
		CHECK(decoded[i] == src[i]);
	}
}

void test_obfuscation_seal_png_roundtrip() {
	Vector<uint8_t> payload;
	payload.resize(64);
	payload.write[0] = 'O';
	payload.write[1] = 'B';
	payload.write[2] = 'F';
	payload.write[3] = '1';
	for (int i = 4; i < payload.size(); i++) {
		payload.write[i] = (uint8_t)(i * 13);
	}
	Ref<Image> img = ObfuscationSealMatrix::encode(payload);
	REQUIRE(img.is_valid());
	CHECK(img->get_width() == 256);
	CHECK(img->get_height() == 256);
	Vector<uint8_t> out;
	CHECK(ObfuscationSealMatrix::decode(img, out));
	REQUIRE(out.size() >= 4);
	CHECK(out[0] == 'O');
	CHECK(out[1] == 'B');
	CHECK(out[2] == 'F');
	CHECK(out[3] == '1');
}

void test_obfuscation_seal_tile_decode() {
	Vector<uint8_t> payload;
	payload.resize(32);
	for (int i = 0; i < payload.size(); i++) {
		payload.write[i] = (uint8_t)i;
	}
	Ref<Image> img = ObfuscationSealMatrix::encode(payload);
	REQUIRE(img.is_valid());
	Ref<Image> tile = img->get_region(Rect2i(0, 0, 64, 64));
	Vector<uint8_t> out;
	bool decoded = ObfuscationSealMatrix::decode_tile(tile, 0, 0, out);
	if (!decoded) {
		decoded = ObfuscationSealMatrix::decode(img, out);
	}
	CHECK(decoded);
	CHECK(out.size() > 0);
}

void test_obfuscation_image_mark_roundtrip() {
	Ref<Image> img = Image::create_empty(256, 256, false, Image::FORMAT_RGBA8);
	for (int y = 0; y < 256; y++) {
		for (int x = 0; x < 256; x++) {
			img->set_pixel(x, y, Color((x % 16) / 15.0f, (y % 16) / 15.0f, 0.4f, 1.0f));
		}
	}
	PackedByteArray key;
	key.resize(16);
	for (int i = 0; i < 16; i++) {
		key.write[i] = (uint8_t)(0xA0 + i);
	}
	PackedByteArray payload = key;
	CHECK(ObfuscationImageMark::can_mark(img, 256));
	Ref<Image> marked = ObfuscationImageMark::embed(img, payload, key, false);
	REQUIRE(marked.is_valid());
	PackedByteArray extracted = ObfuscationImageMark::extract(marked, key, 16 * 8);
	REQUIRE(extracted.size() == 16);
	for (int i = 0; i < 16; i++) {
		CHECK(extracted[i] == payload[i]);
	}
	Ref<Image> tiny = Image::create_empty(32, 32, false, Image::FORMAT_RGBA8);
	CHECK_FALSE(ObfuscationImageMark::can_mark(tiny, 256));
}

void test_obfuscation_pcm_mark_roundtrip() {
	const int mix = 44100;
	const int ch = 1;
	const int samples = mix;
	PackedByteArray pcm;
	pcm.resize(samples * (int)sizeof(float));
	float *f = (float *)pcm.ptrw();
	for (int i = 0; i < samples; i++) {
		f[i] = Math::sin(i * 0.05f) * 0.4f;
	}
	PackedByteArray payload;
	payload.resize(16);
	for (int i = 0; i < 16; i++) {
		payload.write[i] = (uint8_t)(i + 1);
	}
	PackedByteArray marked = ObfuscationAudioMark::embed(pcm, mix, ch, payload);
	REQUIRE(marked.size() == pcm.size());
	PackedByteArray extracted = ObfuscationAudioMark::extract(marked, mix, ch, 16);
	REQUIRE(extracted.size() == 16);
	int matches = 0;
	for (int i = 0; i < 16; i++) {
		if (extracted[i] == payload[i]) {
			matches++;
		}
	}
	CHECK(matches >= 12);
}

void test_obfuscation_path_canonicalize() {
	const String a = ObfuscationOutputNames::canonicalize_path("res://Foo/Bar.GD");
	const String b = ObfuscationOutputNames::canonicalize_path("res://foo\\bar.gd");
	CHECK(a == b);
	CHECK(a == String("res://foo/bar.gd"));
	CHECK(ObfuscationOutputNames::ids_match_folded("StudioGame", "studiogame"));
	bool folded_mismatch = ObfuscationOutputNames::ids_match_folded("StudioGame", "other");
	CHECK(!folded_mismatch);
}

void test_obfuscation_scramble_one_way() {
	PackedByteArray key;
	key.resize(32);
	for (int i = 0; i < 32; i++) {
		key.write[i] = (uint8_t)(i * 3 + 1);
	}
	const String logical = "res://.obfuscation/claimkey.png";
	const String scrambled = ObfuscationOutputNames::scramble_output_path(key, logical);
	CHECK(scrambled.begins_with("res://"));
	CHECK(scrambled.get_extension() == "png");
	CHECK(scrambled.find(".obfuscation") < 0);
	CHECK(scrambled.find("claimkey") < 0);
	CHECK(scrambled != ObfuscationOutputNames::scramble_output_path(key, "res://.obfuscation/manifest.bin"));
	CHECK(scrambled == ObfuscationOutputNames::scramble_output_path(key, "RES://.Obfuscation/ClaimKey.PNG"));
	const String abs = ObfuscationOutputNames::to_absolute("C:/out", scrambled);
	CHECK(abs.find("res://") < 0);
}

void test_obfuscation_identifier_rewrite() {
	PackedByteArray key;
	key.resize(32);
	for (int i = 0; i < 32; i++) {
		key.write[i] = (uint8_t)(i * 3 + 1);
	}
	CHECK(ObfuscationSourceMap::is_reserved("_CK"));
	CHECK(ObfuscationSourceMap::is_reserved("_CI"));
	CHECK(ObfuscationSourceMap::is_reserved("_CR"));
	CHECK(ObfuscationSourceMap::is_reserved("_obfuscation_seed_salt"));
	CHECK(ObfuscationSourceMap::is_reserved("_ready"));
	CHECK(ObfuscationSourceMap::is_reserved("_initialize"));
	CHECK(ObfuscationSourceMap::is_reserved("preload"));
	CHECK(ObfuscationSourceMap::is_reserved("Node"));
	CHECK(!ObfuscationSourceMap::is_reserved("next_roll"));
	CHECK(ObfuscationSourceMap::keep_original_path("res://project.godot"));
	CHECK(!ObfuscationSourceMap::keep_original_path("res://scripts/gameplay.gd"));

	const String src = "extends Node\n\nvar rng := 1\nfunc next_roll() -> int:\n\treturn rng\nfunc _ready() -> void:\n\tprint(\"next_roll\")\n\tpreload(\"res://scripts/gameplay.gd\")\n\tx.next_roll()\n";
	HashSet<String> names;
	ObfuscationSourceMap::collect_identifiers(src, names);
	CHECK(names.has("rng"));
	CHECK(names.has("next_roll"));
	CHECK(!names.has("_ready"));
	CHECK(!names.has("Node"));
	CHECK(!names.has("_CK"));
	const HashMap<String, String> idents = ObfuscationSourceMap::map_identifiers(key, names);
	REQUIRE(idents.has("next_roll"));
	REQUIRE(idents.has("rng"));
	const String *rolled_ptr = idents.getptr("next_roll");
	const String *rng_ptr = idents.getptr("rng");
	REQUIRE(rolled_ptr != nullptr);
	REQUIRE(rng_ptr != nullptr);
	const String rolled = *rolled_ptr;
	const String rng_name = *rng_ptr;
	HashSet<String> funcs;
	funcs.insert("next_roll");
	CHECK(rolled.begins_with("_"));
	CHECK(rolled.length() == 13);
	CHECK(rolled == ObfuscationOutputNames::scramble_identifier(key, "next_roll"));
	CHECK(rolled != ObfuscationOutputNames::scramble_identifier(key, "rng"));
	const String out = ObfuscationSourceMap::rewrite_script(src, idents, funcs, key, true);
	CHECK(out.find("func next_roll") < 0);
	CHECK(out.find("var rng") < 0);
	CHECK(out.find("print(\"next_roll\")") < 0);
	CHECK(out.find(String("x.") + rolled) >= 0);
	CHECK(out.find("extends Node") >= 0);
	CHECK(out.find("func _ready") >= 0);
	CHECK(out.find(rolled) >= 0);
	CHECK(out.find(rng_name) >= 0);
	CHECK(out.find("gameplay") < 0);
	CHECK(out.find("scripts") < 0);
	CHECK(out.find("res://") >= 0);
	const String paths = ObfuscationSourceMap::rewrite_paths("run/main_scene=\"res://scripts/gameplay.gd\"\nconfig=\"res://project.godot\"", key);
	CHECK(paths.find("gameplay") < 0);
	CHECK(paths.find("res://project.godot") >= 0);
}

void test_obfuscation_scene_rewrite() {
	PackedByteArray key;
	key.resize(32);
	for (int i = 0; i < 32; i++) {
		key.write[i] = (uint8_t)(i * 3 + 1);
	}
	const String tscn = "[gd_scene load_steps=2 format=3]\n\n[ext_resource type=\"Script\" path=\"res://scripts/main.gd\" id=\"1_main\"]\n\n[sub_resource type=\"StyleBoxFlat\" id=\"StyleBoxFlat_abc\"]\n\n[node name=\"HarborHero\" type=\"Control\" parent=\"Margin/VBox\" groups=[\"harbor_party\"]]\nunique_id=42\ntext = \"BlazeSeal\"\nscript = ExtResource(\"1_main\")\nstyle = SubResource(\"StyleBoxFlat_abc\")\nanim = NodePath(\"HarborHero:frame\")\n";
	const String godot = "[application]\nconfig/name=\"Harbor Quest\"\nrun/main_scene=\"res://scenes/main.tscn\"\n\n[autoload]\nHarborQuestState=\"*res://scripts/quest.gd\"\n\n[input]\njump={\n\"deadzone\": 0.5\n}\n";
	const String gd = "extends Control\nfunc _ready() -> void:\n\t$HarborHero.visible = true\n\tvar n = get_node(\"HarborHero\")\n\tadd_to_group(\"harbor_party\")\n\tif Input.is_action_pressed(\"jump\"):\n\t\tpass\n\t$\"Margin/VBox\"\n\tprint(\"BlazeSeal\")\n";
	HashSet<String> scene;
	ObfuscationSourceMap::collect_scene_names(tscn, scene);
	ObfuscationSourceMap::collect_scene_names(godot, scene);
	CHECK(scene.has("HarborHero"));
	CHECK(scene.has("harbor_party"));
	CHECK(scene.has("1_main"));
	CHECK(scene.has("StyleBoxFlat_abc"));
	CHECK(scene.has("Margin"));
	CHECK(scene.has("VBox"));
	CHECK(scene.has("HarborQuestState"));
	CHECK(scene.has("jump"));
	CHECK(!scene.has("Control"));
	CHECK(!scene.has("BlazeSeal"));
	CHECK(!scene.has("42"));
	HashSet<String> funcs;
	HashSet<String> vars;
	ObfuscationSourceMap::collect_split(gd, funcs, vars);
	HashSet<String> names(scene);
	for (const String &n : funcs) {
		names.insert(n);
	}
	for (const String &n : vars) {
		names.insert(n);
	}
	const HashMap<String, String> idents = ObfuscationSourceMap::map_identifiers(key, names);
	REQUIRE(idents.has("HarborHero"));
	REQUIRE(idents.has("harbor_party"));
	REQUIRE(idents.has("1_main"));
	REQUIRE(idents.has("HarborQuestState"));
	REQUIRE(idents.has("jump"));
	const String hero = *idents.getptr("HarborHero");
	const String group = *idents.getptr("harbor_party");
	const String ext_id = *idents.getptr("1_main");
	const String auto_n = *idents.getptr("HarborQuestState");
	const String jump_n = *idents.getptr("jump");
	const String out_scene = ObfuscationSourceMap::rewrite_scene(tscn, idents, key);
	CHECK(out_scene.find("HarborHero") < 0);
	CHECK(out_scene.find("harbor_party") < 0);
	CHECK(out_scene.find("1_main") < 0);
	CHECK(out_scene.find("StyleBoxFlat_abc") < 0);
	CHECK(out_scene.find("type=\"Control\"") >= 0);
	CHECK(out_scene.find("text = \"BlazeSeal\"") >= 0);
	CHECK(out_scene.find("unique_id=42") >= 0);
	CHECK(out_scene.find("scripts") < 0);
	CHECK(out_scene.find(hero) >= 0);
	CHECK(out_scene.find(group) >= 0);
	CHECK(out_scene.find(ext_id) >= 0);
	CHECK(out_scene.find(String("NodePath(\"") + hero + ":frame\")") >= 0);
	const String out_godot = ObfuscationSourceMap::rewrite_scene(godot, idents, key);
	CHECK(out_godot.find("HarborQuestState") < 0);
	CHECK(out_godot.find("jump=") < 0);
	CHECK(out_godot.find("config/name=\"Harbor Quest\"") >= 0);
	CHECK(out_godot.find(auto_n) >= 0);
	CHECK(out_godot.find(jump_n + "={") >= 0);
	CHECK(out_godot.find("quest") < 0);
	const String out_gd = ObfuscationSourceMap::rewrite_script(gd, idents, funcs, scene, key, true);
	CHECK(out_gd.find("HarborHero") < 0);
	CHECK(out_gd.find("harbor_party") < 0);
	CHECK(out_gd.find("\"jump\"") < 0);
	CHECK(out_gd.find("extends Control") >= 0);
	CHECK(out_gd.find("print(\"BlazeSeal\")") >= 0);
	CHECK(out_gd.find(String("$") + hero) >= 0);
	CHECK(out_gd.find(group) >= 0);
	CHECK(out_gd.find(jump_n) >= 0);
}

void test_obfuscation_luau_rewrite() {
	PackedByteArray key;
	key.resize(32);
	for (int i = 0; i < 32; i++) {
		key.write[i] = (uint8_t)(i * 3 + 1);
	}
	CHECK(ObfuscationSourceMap::is_script_ext("luau"));
	CHECK(ObfuscationSourceMap::is_script_ext("lua"));
	CHECK(ObfuscationSourceMap::script_lang_from_path("res://scripts/harbor_hero.luau") == ObfuscationSourceMap::SCRIPT_LUAU);
	const String src = "--!strict\nlocal HarborHero = {\n\textends = \"Node\",\n\t_ready = function(self)\n\t\tself:add_to_group(\"harbor_party\")\n\tend,\n\tnext_wave = function(self)\n\t\treturn require(\"res://scripts/harbor_hero.luau\")\n\tend,\n}\nreturn gdclass(HarborHero)\n";
	HashSet<String> funcs;
	HashSet<String> vars;
	ObfuscationSourceMap::collect_split(src, funcs, vars, ObfuscationSourceMap::SCRIPT_LUAU);
	CHECK(funcs.has("next_wave"));
	CHECK(vars.has("HarborHero"));
	CHECK(!funcs.has("_ready"));
	CHECK(!vars.has("self"));
	HashSet<String> scene;
	scene.insert("harbor_party");
	HashSet<String> names(vars);
	for (const String &n : funcs) {
		names.insert(n);
	}
	for (const String &n : scene) {
		names.insert(n);
	}
	const HashMap<String, String> idents = ObfuscationSourceMap::map_identifiers(key, names);
	REQUIRE(idents.has("next_wave"));
	REQUIRE(idents.has("HarborHero"));
	REQUIRE(idents.has("harbor_party"));
	const String wave = *idents.getptr("next_wave");
	const String hero = *idents.getptr("HarborHero");
	const String group = *idents.getptr("harbor_party");
	const String out = ObfuscationSourceMap::rewrite_script(src, idents, funcs, scene, key, true, ObfuscationSourceMap::SCRIPT_LUAU);
	CHECK(out.find("next_wave") < 0);
	CHECK(out.find("HarborHero") < 0);
	CHECK(out.find("harbor_party") < 0);
	CHECK(out.find("harbor_hero") < 0);
	CHECK(out.find("scripts") < 0);
	CHECK(out.find("--!strict") >= 0);
	CHECK(out.find("gdclass") >= 0);
	CHECK(out.find("function(self)") >= 0);
	CHECK(out.find(wave) >= 0);
	CHECK(out.find(hero) >= 0);
	CHECK(out.find(group) >= 0);
}

void test_obfuscation_comment_lattice() {
	PackedByteArray key;
	key.resize(32);
	for (int i = 0; i < 32; i++) {
		key.write[i] = (uint8_t)(i * 5 + 2);
	}
	const String gd = "extends Node\n\n# TODO harbor map\nfunc _ready() -> void:\n\tvar hero := $HarborHero\n\thero.add_to_group(\"harbor_party\")\n\tload(\"res://scripts/gameplay.gd\")\n";
	const String mini = ObfuscationSourceMap::strip_minify(gd, ObfuscationSourceMap::SCRIPT_GDSCRIPT);
	CHECK(mini.find("TODO") < 0);
	CHECK(mini.find("# ") < 0);
	CHECK(mini.find("extends Node") >= 0);
	CHECK(mini.find("func _ready") >= 0);
	CHECK(mini.find("load(\"res://scripts/gameplay.gd\")") >= 0);
	const String luau = "--!strict\n-- debug next_wave\nlocal HarborHero = {\n\textends = \"Node\",\n}\nreturn gdclass(HarborHero)\n";
	const String luau_mini = ObfuscationSourceMap::strip_minify(luau, ObfuscationSourceMap::SCRIPT_LUAU);
	CHECK(luau_mini.find("--!strict") < 0);
	CHECK(luau_mini.find("debug next_wave") < 0);
	CHECK(luau_mini.find("return gdclass") >= 0);
	const String packed = "res://aaaaaaaa/bbbbbbbb/cccccccccccccccc.gd";
	const String line = ObfuscationCommentLattice::encode_line(key, packed, "k", 'k', "42", ObfuscationSourceMap::SCRIPT_GDSCRIPT);
	CHECK(line.begins_with("# ~ k "));
	const String src = mini + line + "\n" + ObfuscationCommentLattice::make_decoy(1, 0, ObfuscationSourceMap::SCRIPT_GDSCRIPT) + "\n";
	Dictionary parsed = ObfuscationCommentLattice::parse(src, packed, key);
	const int64_t kv = parsed["k"];
	CHECK(kv == 42);
	CHECK(parsed.size() == 1);
	Dictionary wrong = ObfuscationCommentLattice::parse(src, "res://other.gd", key);
	CHECK(!wrong.has("k"));
	String stripped;
	PackedStringArray lines = src.split("\n");
	for (int i = 0; i < lines.size(); i++) {
		if (!ObfuscationCommentLattice::is_lattice_line(lines[i])) {
			stripped += lines[i] + "\n";
		}
	}
	CHECK(ObfuscationCommentLattice::parse(stripped, packed, key).is_empty());
	HashSet<String> funcs;
	HashSet<String> vars;
	ObfuscationSourceMap::collect_split(gd, funcs, vars);
	HashSet<String> scene;
	scene.insert("HarborHero");
	scene.insert("harbor_party");
	HashSet<String> names(vars);
	for (const String &n : funcs) {
		names.insert(n);
	}
	for (const String &n : scene) {
		names.insert(n);
	}
	const HashMap<String, String> idents = ObfuscationSourceMap::map_identifiers(key, names);
	const String rewritten = ObfuscationSourceMap::rewrite_script(mini, idents, funcs, scene, key, true);
	Vector<String> crefs;
	const String with_cref = ObfuscationCommentLattice::apply_cref(rewritten, key, packed, ObfuscationSourceMap::SCRIPT_GDSCRIPT, idents, funcs, scene, crefs);
	CHECK(with_cref.find("_obfuscation_cref") >= 0);
	CHECK(with_cref.find("HarborHero") < 0);
	CHECK(with_cref.find("harbor_party") < 0);
	CHECK(with_cref.find("res://scripts") < 0);
	CHECK(!crefs.is_empty());
	const String planted = ObfuscationCommentLattice::append_lines(with_cref, crefs);
	CHECK(planted.find("# ~ ") >= 0);
	CHECK(planted.find("TODO") < 0);
	const String luau_help = ObfuscationCommentLattice::inject_helpers("local HarborHero = {\n}\nreturn gdclass(HarborHero)\n", ObfuscationSourceMap::SCRIPT_LUAU, true, false);
	const int ret_at = luau_help.find("return gdclass");
	const int help_at = luau_help.find("local function _obfuscation_cref");
	CHECK(help_at >= 0);
	CHECK(help_at < ret_at);
	const String luau_line = ObfuscationCommentLattice::encode_line(key, "res://x.luau", "n0", 'n', "_abc", ObfuscationSourceMap::SCRIPT_LUAU);
	CHECK(luau_line.begins_with("-- ~ "));
	CHECK(ObfuscationCopyrightLattice::to_base64(String()).is_empty());
	const String b64 = ObfuscationCopyrightLattice::to_base64("Example Studio|MIT|notice");
	CHECK(!b64.is_empty());
}
