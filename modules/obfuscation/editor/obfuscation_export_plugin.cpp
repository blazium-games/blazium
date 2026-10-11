/**************************************************************************/
/*  obfuscation_export_plugin.cpp                                         */
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

#ifdef TOOLS_ENABLED

#include "editor/obfuscation_export_plugin.h"

#include "core/config/project_settings.h"
#include "core/io/config_file.h"
#include "core/io/dir_access.h"
#include "core/io/file_access.h"
#include "core/io/image.h"
#include "core/io/json.h"
#include "core/io/resource_loader.h"
#include "core/object/object.h"
#include "core/os/time.h"
#include "core/string/translation.h"
#include "core/templates/list.h"
#include "editor/export/editor_export_platform.h"
#include "editor/export/editor_export_preset.h"
#include "scene/main/node.h"
#include "servers/text/text_server.h"

#include "modules/modules_enabled.gen.h" // For gdscript.

#ifdef MODULE_GDSCRIPT_ENABLED
#include "modules/gdscript/gdscript_tokenizer_buffer.h"
#endif

#include "modules/obfuscation/obfuscation.h"
#include "modules/obfuscation/seeds/output_names.h"
#include "modules/obfuscation/seeds/source_map.h"

// A string from a project setting is rewritten only when the path in it is
// one this export packs, so settings that name missing files keep their text.
static bool _obf_setting_path_exists(const String &p_text) {
	String path = p_text;
	if (path.begins_with("*")) {
		path = path.substr(1); // Autoload singleton marker.
	}
	if (!path.begins_with("res://")) {
		return true; // Paths inside longer text are left to rewrite_pack_paths.
	}
	const int colon = path.find_char(':', 6);
	if (colon >= 0) {
		path = path.substr(0, colon); // Translation remaps: "res://x.png:es".
	}
	return FileAccess::exists(path);
}

Dictionary ObfuscationExportPlugin::last_report;
Callable ObfuscationExportPlugin::report_callback;

Variant ObfuscationExportPlugin::_rewrite_setting(const Variant &p_value) {
	Obfuscation *ob = Obfuscation::get_singleton();
	switch (p_value.get_type()) {
		case Variant::STRING:
		case Variant::STRING_NAME: {
			const String src = p_value;
			if (src.find("res://") < 0 || !_obf_setting_path_exists(src)) {
				return p_value;
			}
			String path = src.begins_with("*") ? src.substr(1) : src;
			if (path.begins_with("res://")) {
				const int colon = path.find_char(':', 6);
				setting_paths.insert(colon >= 0 ? path.substr(0, colon) : path);
			}
			const String rewritten = ob->rewrite_pack_paths(src);
			if (p_value.get_type() == Variant::STRING_NAME) {
				return StringName(rewritten);
			}
			return rewritten;
		}
		case Variant::PACKED_STRING_ARRAY: {
			PackedStringArray arr = p_value;
			for (int i = 0; i < arr.size(); i++) {
				arr.write[i] = _rewrite_setting(arr[i]);
			}
			return arr;
		}
		case Variant::ARRAY: {
			Array arr = Array(p_value).duplicate();
			for (int i = 0; i < arr.size(); i++) {
				arr[i] = _rewrite_setting(arr[i]);
			}
			return arr;
		}
		case Variant::DICTIONARY: {
			const Dictionary src = p_value;
			Dictionary out;
			for (const KeyValue<Variant, Variant> &kv : src) {
				out[_rewrite_setting(kv.key)] = _rewrite_setting(kv.value);
			}
			return out;
		}
		default:
			return p_value;
	}
}

// Scripts go out the way the export preset asks for them. GDScript's own export
// plugin runs after this one and never sees a script this plugin has packed.
void ObfuscationExportPlugin::_add_script(const String &p_dest, const String &p_source) {
	report_scripts++;
#ifdef MODULE_GDSCRIPT_ENABLED
	if (p_dest.get_extension().to_lower() == "gd" && script_mode != EditorExportPreset::MODE_SCRIPT_TEXT) {
		const GDScriptTokenizerBuffer::CompressMode compress = script_mode == EditorExportPreset::MODE_SCRIPT_BINARY_TOKENS_COMPRESSED ? GDScriptTokenizerBuffer::COMPRESS_ZSTD : GDScriptTokenizerBuffer::COMPRESS_NONE;
		const Vector<uint8_t> tokens = GDScriptTokenizerBuffer::parse_code_string(p_source, compress);
		if (!tokens.is_empty()) {
			const String gdc = p_dest.get_basename() + ".gdc";
			add_file(gdc, tokens, false);
			const String remap = "[remap]\n\npath=\"" + gdc.c_escape() + "\"\n";
			add_file(p_dest + ".remap", remap.to_utf8_buffer(), false);
			return;
		}
	}
#endif
	add_file(p_dest, p_source.to_utf8_buffer(), false);
}

// An imported file is packed as what it imports to, plus a .import file that
// points there -- the same as the export platform does, but at scrambled paths.
void ObfuscationExportPlugin::_add_imported(const String &p_path, const String &p_dest, const HashSet<String> &p_features) {
	Obfuscation *ob = Obfuscation::get_singleton();
	Ref<ConfigFile> config;
	config.instantiate();
	if (config->load(p_path + ".import") != OK) {
		ERR_PRINT("Obfuscation: could not parse '" + p_path + ".import', not exported.");
		return;
	}
	if (String(config->get_value("remap", "importer", String())) == "keep") {
		add_file(p_dest, FileAccess::get_file_as_bytes(p_path), false);
		return;
	}
	const Vector<String> keys = config->get_section_keys("remap");
	for (const String &key : keys) {
		const bool wanted = key == "path" || (key.begins_with("path.") && p_features.has(key.get_slicec('.', 1)));
		if (!wanted) {
			if (key.begins_with("path.")) {
				config->erase_section_key("remap", key);
			}
			continue;
		}
		const String artifact = config->get_value("remap", key);
		const String artifact_dest = ob->scramble_output_path(artifact);
		add_file(artifact_dest, FileAccess::get_file_as_bytes(artifact), false);
		config->set_value("remap", key, artifact_dest);
	}
	if (config->has_section("deps")) {
		config->erase_section("deps");
	}
	if (config->has_section("params")) {
		config->erase_section("params");
	}
	add_file(p_dest + ".import", config->encode_to_text().to_utf8_buffer(), false);
}

String ObfuscationExportPlugin::get_exported_path(const String &p_path) const {
	const String *dest = exported_paths.getptr(p_path);
	return dest ? *dest : p_path;
}

// A project setting that points at a file this plugin never packed (another
// export plugin took it first, or it is excluded) leaves the game unable to
// start. Say so in the export log instead of shipping a broken build.
void ObfuscationExportPlugin::_check_settings() {
	for (const String &path : setting_paths) {
		if (exported_paths.has(path)) {
			continue;
		}
		report_problems++;
		const String msg = vformat("Project settings point at \"%s\", which scramble_names renamed, but the file was not packed under the new name. The exported game will not find it.", path);
		Ref<EditorExportPlatform> platform = get_export_platform();
		if (platform.is_valid()) {
			platform->add_message(EditorExportPlatform::EXPORT_MESSAGE_ERROR, "Obfuscation", msg);
		} else {
			ERR_PRINT(msg);
		}
	}
}

// Scrambling rewrites the translation paths to names that only exist inside the
// exported pack, so the export platform can no longer load the translations to
// see whether their locales need the text server support data (ICU line
// breaking for Thai, Lao, Khmer, Burmese...). Check the real files here, and
// include the data for this export if any of them need it.
void ObfuscationExportPlugin::_keep_text_server_data_check() {
	if (TS.is_null() || !TS->has_feature(TextServer::FEATURE_USE_SUPPORT_DATA)) {
		return;
	}
	const StringName include_setting = "internationalization/locale/include_text_server_data";
	const Ref<EditorExportPreset> preset = get_export_preset();
	if (bool(preset.is_valid() ? preset->get_project_setting(include_setting) : GLOBAL_GET(include_setting))) {
		return;
	}
	const StringName translations_setting = "internationalization/locale/translations";
	const PackedStringArray translations = preset.is_valid() ? preset->get_project_setting(translations_setting) : GLOBAL_GET(translations_setting);
	for (const String &path : translations) {
		if (!ResourceLoader::exists(path)) {
			continue;
		}
		const Ref<Translation> tr = ResourceLoader::load(path);
		if (tr.is_valid() && TS->is_locale_using_support_data(tr->get_locale())) {
			if (!saved_settings.has(include_setting)) {
				saved_settings[include_setting] = GLOBAL_GET(include_setting);
			}
			ProjectSettings::get_singleton()->set(include_setting, true);
			return;
		}
	}
}

void ObfuscationExportPlugin::_export_begin(const HashSet<String> &p_features, bool p_debug, const String &p_path, int p_flags) {
	(void)p_flags;
	enabled = true;
	report_preset = get_export_preset().is_valid() ? get_export_preset()->get_name() : String();
	report_platform = get_export_platform().is_valid() ? get_export_platform()->get_name() : String();
	report_path = p_path;
	report_debug = p_debug;
	report_scripts = 0;
	report_images = 0;
	report_problems = 0;
	report_lattice_skipped = false;
	report_seal = false;
	scene_stamp_remaining = 4;
	stripped_copyright = false;
	scramble_pack = false;
	pack_idents.clear();
	pack_funcs.clear();
	pack_scene_names.clear();
	pack_scripts.clear();
	saved_settings.clear();
	exported_paths.clear();
	setting_paths.clear();
	script_mode = EditorExportPreset::MODE_SCRIPT_BINARY_TOKENS_COMPRESSED;
	if (get_export_preset().is_valid()) {
		script_mode = get_export_preset()->get_script_export_mode();
	}
	if (p_features.has("no_obfuscation")) {
		enabled = false;
		return;
	}
	Obfuscation *ob = Obfuscation::get_singleton();
	if (!ob || !ob->is_enabled()) {
		enabled = false;
		return;
	}
	if (!ob->has_identity()) {
		ob->load_identity();
	}

	// The comment lattice stores its data in comments, and scripts exported as
	// binary tokens keep no comments, so lattice references would come back
	// empty. Use the plain code injection for this export instead.
	if (script_mode != EditorExportPreset::MODE_SCRIPT_TEXT && bool(GLOBAL_GET("obfuscation/scripts/comment_lattice"))) {
		report_lattice_skipped = true;
		saved_settings["obfuscation/scripts/comment_lattice"] = true;
		ProjectSettings::get_singleton()->set("obfuscation/scripts/comment_lattice", false);
		if (get_export_platform().is_valid()) {
			get_export_platform()->add_message(EditorExportPlatform::EXPORT_MESSAGE_INFO, "Obfuscation", "Scripts are exported as binary tokens, which keep no comments, so obfuscation/scripts/comment_lattice was not used for this export.");
		}
	}

	scramble_pack = bool(GLOBAL_GET("obfuscation/pack/scramble_names")) && ob->has_identity();
	if (scramble_pack) {
		_keep_text_server_data_check();
		ob->build_pack_ident_maps(pack_idents, pack_funcs, pack_scene_names);
		List<PropertyInfo> props;
		ProjectSettings::get_singleton()->get_property_list(&props);
		for (const PropertyInfo &pi : props) {
			if (!(pi.usage & PROPERTY_USAGE_STORAGE)) {
				continue;
			}
			const String setting = pi.name;
			if (setting.begins_with("editor_plugins/") || setting.begins_with("editor/")) {
				continue; // Editor-only; those files are not exported.
			}
			// Strings (autoloads, main scene, icon) and lists of them (translations).
			const Variant::Type t = pi.type;
			if (t != Variant::STRING && t != Variant::STRING_NAME && t != Variant::PACKED_STRING_ARRAY && t != Variant::ARRAY && t != Variant::DICTIONARY) {
				continue;
			}
			const Variant cur = ProjectSettings::get_singleton()->get(pi.name);
			const Variant rewritten = _rewrite_setting(cur);
			if (rewritten == cur) {
				continue;
			}
			saved_settings[pi.name] = cur;
			ProjectSettings::get_singleton()->set(pi.name, rewritten);
		}
		// The exported UID cache is rebuilt by the export platform, which asks
		// get_exported_path() where each file went, so it is not patched here.
	}

	if (ob->has_identity() && (scramble_pack || bool(GLOBAL_GET("obfuscation/scripts/comment_lattice")))) {
		if (!scramble_pack) {
			ob->build_pack_ident_maps(pack_idents, pack_funcs, pack_scene_names);
		}
		HashMap<String, String> packed_bodies;
		HashMap<String, String> logical_to_packed;
		Vector<String> files;
		const String root = ProjectSettings::get_singleton()->globalize_path("res://").replace("\\", "/").simplify_path();
		ObfuscationSourceMap::list_files(root, files);
		for (int i = 0; i < files.size(); i++) {
			String path = files[i].replace("\\", "/").simplify_path();
			String rel = path;
			if (path.begins_with(root)) {
				rel = path.substr(root.length());
			}
			while (rel.begins_with("/")) {
				rel = rel.substr(1);
			}
			if (ObfuscationSourceMap::skip_relative_path(rel)) {
				continue;
			}
			if (!ObfuscationSourceMap::is_script_ext(files[i].get_extension().to_lower())) {
				continue;
			}
			const String logical = String("res://") + rel;
			const String packed = ob->output_artifact_path(logical);
			packed_bodies[packed] = ob->pack_script_source(FileAccess::get_file_as_string(files[i]), logical, packed, pack_idents, pack_funcs, pack_scene_names);
			logical_to_packed[logical] = packed;
		}
		if (bool(GLOBAL_GET("obfuscation/scripts/comment_lattice"))) {
			ob->scatter_pack_scripts(packed_bodies);
		}
		for (const KeyValue<String, String> &E : logical_to_packed) {
			pack_scripts[E.key] = packed_bodies[E.value];
		}
	}

	if (bool(GLOBAL_GET("obfuscation/pack/inject_seal")) && ob->has_identity()) {
		Ref<Image> seal = ob->generate_seal_image();
		if (seal.is_valid() && !seal->is_empty()) {
			const Vector<uint8_t> png = seal->save_png_to_buffer();
			if (!png.is_empty()) {
				add_file(ob->output_artifact_path("res://.obfuscation/claimkey.png"), png, false);
				report_seal = true;
			}
		}
		Dictionary man;
		man["owner_id"] = ob->get_owner_id();
		man["project_id"] = ob->get_project_id();
		man["copyright_hash"] = ob->copyright_hash();
		const String json = JSON::stringify(man);
		add_file(ob->output_artifact_path("res://.obfuscation/manifest.bin"), json.to_utf8_buffer(), false);
	}

	saved_copyright_text = GLOBAL_GET("obfuscation/copyright/text");
	saved_copyright_author = GLOBAL_GET("obfuscation/copyright/author");
	saved_copyright_license = GLOBAL_GET("obfuscation/copyright/license");
	ProjectSettings::get_singleton()->set("obfuscation/copyright/text", String());
	ProjectSettings::get_singleton()->set("obfuscation/copyright/author", String());
	ProjectSettings::get_singleton()->set("obfuscation/copyright/license", String());
	stripped_copyright = true;
}

void ObfuscationExportPlugin::_export_end() {
	if (scramble_pack) {
		_check_settings();
	}
	{
		Dictionary report;
		report["time"] = Time::get_singleton()->get_datetime_string_from_system(false, true);
		report["preset"] = report_preset;
		report["platform"] = report_platform;
		report["path"] = report_path;
		report["debug"] = report_debug;
		Obfuscation *ob = Obfuscation::get_singleton();
		int renamed = 0;
		for (const KeyValue<String, String> &E : exported_paths) {
			if (E.key != E.value) {
				renamed++;
			}
		}
		report["protected"] = enabled && ob && ob->has_identity();
		report["scripts"] = report_scripts;
		report["renamed"] = scramble_pack ? renamed : 0;
		report["scramble_names"] = scramble_pack;
		report["images"] = report_images;
		report["scenes_stamped"] = enabled ? MAX(0, 4 - scene_stamp_remaining) : 0;
		report["seal"] = report_seal;
		report["lattice_skipped"] = report_lattice_skipped;
		report["lattice_used"] = enabled && !report_lattice_skipped && bool(GLOBAL_GET("obfuscation/scripts/comment_lattice"));
		report["problems"] = report_problems;
		last_report = report;
		if (report_callback.is_valid()) {
			report_callback.call_deferred(report);
		}
	}
	exported_paths.clear();
	setting_paths.clear();
	pack_idents.clear();
	pack_funcs.clear();
	pack_scene_names.clear();
	pack_scripts.clear();
	scramble_pack = false;
	if (ProjectSettings::get_singleton()) {
		for (const KeyValue<StringName, Variant> &E : saved_settings) {
			ProjectSettings::get_singleton()->set(E.key, E.value);
		}
	}
	saved_settings.clear();
	if (!stripped_copyright || !ProjectSettings::get_singleton()) {
		return;
	}
	ProjectSettings::get_singleton()->set("obfuscation/copyright/text", saved_copyright_text);
	ProjectSettings::get_singleton()->set("obfuscation/copyright/author", saved_copyright_author);
	ProjectSettings::get_singleton()->set("obfuscation/copyright/license", saved_copyright_license);
	stripped_copyright = false;
}

static String _obf_strip_copyright_project(const String &p_src) {
	PackedStringArray lines = p_src.split("\n");
	PackedStringArray kept;
	for (int i = 0; i < lines.size(); i++) {
		const String line = lines[i];
		if (line.find("obfuscation/copyright/text") >= 0 || line.find("obfuscation/copyright/author") >= 0 || line.find("obfuscation/copyright/license") >= 0) {
			continue;
		}
		kept.push_back(line);
	}
	return String("\n").join(kept);
}

void ObfuscationExportPlugin::_export_file(const String &p_path, const String &p_type, const HashSet<String> &p_features) {
	(void)p_type;
	(void)p_features;
	if (!enabled) {
		return;
	}
	Obfuscation *ob = Obfuscation::get_singleton();
	if (!ob) {
		return;
	}
	const String ext = p_path.get_extension().to_lower();
	const String file = p_path.get_file();
	if (scramble_pack) {
		String rel = p_path.replace("\\", "/");
		if (rel.begins_with("res://")) {
			rel = rel.substr(6);
		}
		if (ObfuscationSourceMap::skip_relative_path(rel)) {
			skip();
			return;
		}
		const String dest = ObfuscationSourceMap::keep_original_path(p_path) ? p_path : ob->scramble_output_path(p_path);
		exported_paths[p_path] = dest;
		if (FileAccess::exists(p_path + ".import")) {
			skip();
			_add_imported(p_path, dest, p_features);
			return;
		}
		Vector<uint8_t> bytes;
		if (ObfuscationSourceMap::is_script_ext(ext)) {
			const String src = pack_scripts.has(p_path) ? pack_scripts[p_path] : ob->pack_script_source(FileAccess::get_file_as_string(p_path), p_path, dest, pack_idents, pack_funcs, pack_scene_names);
			skip();
			_add_script(dest, src);
			return;
		} else if (ext == "tscn" || ext == "tres" || ext == "godot" || file == "project.godot") {
			String src = FileAccess::get_file_as_string(p_path);
			if (file == "project.godot") {
				src = _obf_strip_copyright_project(src);
			}
			bytes = ob->rewrite_pack_scene(src, pack_idents).to_utf8_buffer();
		} else if (ext == "png") {
			Ref<Image> img;
			img.instantiate();
			if (img->load(p_path) == OK && ob->can_watermark_image(img)) {
				img = ob->watermark_image(img);
				bytes = img->save_png_to_buffer();
				report_images++;
			} else {
				bytes = FileAccess::get_file_as_bytes(p_path);
			}
		} else {
			bytes = FileAccess::get_file_as_bytes(p_path);
		}
		skip();
		if (!bytes.is_empty() || ObfuscationSourceMap::is_script_ext(ext) || file == "project.godot") {
			add_file(dest, bytes, false);
		}
		return;
	}
	if (ObfuscationSourceMap::is_script_ext(ext)) {
		if (pack_scripts.has(p_path)) {
			skip();
			_add_script(p_path, pack_scripts[p_path]);
			return;
		}
		const String src = FileAccess::get_file_as_string(p_path);
		if (src.is_empty()) {
			return;
		}
		const String injected = ob->inject_script_source(src, p_path);
		if (injected == src) {
			return; // GDScript's export plugin packs it as usual.
		}
		skip();
		_add_script(p_path, injected);
		return;
	}
	if (file == "project.godot") {
		add_file(p_path, _obf_strip_copyright_project(FileAccess::get_file_as_string(p_path)).to_utf8_buffer(), true);
	}
}

bool ObfuscationExportPlugin::_begin_customize_scenes(const Ref<EditorExportPlatform> &p_platform, const Vector<String> &p_features) {
	(void)p_platform;
	if (!enabled || p_features.has("no_obfuscation")) {
		return false;
	}
	Obfuscation *ob = Obfuscation::get_singleton();
	return ob && ob->is_enabled() && bool(GLOBAL_GET("obfuscation/scripts/inject_copyright"));
}

Node *ObfuscationExportPlugin::_customize_scene(Node *p_root, const String &p_path) {
	if (!p_root || scene_stamp_remaining <= 0) {
		return nullptr;
	}
	Obfuscation *ob = Obfuscation::get_singleton();
	if (!ob) {
		return nullptr;
	}
	PackedStringArray shards = ob->split_copyright_shards();
	if (shards.is_empty()) {
		return nullptr;
	}
	const int idx = (int)((uint64_t)ob->derive_seed(p_path) % (uint64_t)shards.size());
	p_root->set_meta("obfuscation_ci", idx);
	p_root->set_meta("obfuscation_cr", shards[idx]);
	scene_stamp_remaining--;
	return p_root;
}

uint64_t ObfuscationExportPlugin::_get_customization_configuration_hash() const {
	return 0x4F424631u;
}

#endif
