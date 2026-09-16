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

#include "modules/obfuscation/obfuscation.h"
#include "modules/obfuscation/seeds/output_names.h"
#include "modules/obfuscation/seeds/source_map.h"

#include "core/config/project_settings.h"
#include "core/io/dir_access.h"
#include "core/io/file_access.h"
#include "core/io/image.h"
#include "core/io/json.h"
#include "core/io/resource_uid.h"
#include "core/object/object.h"
#include "core/templates/list.h"
#include "scene/main/node.h"

static Vector<uint8_t> _obf_rewrite_uid_cache_bytes(const Vector<uint8_t> &p_src, Obfuscation *p_ob) {
	if (p_src.size() < 4 || !p_ob) {
		return p_src;
	}
	const uint8_t *ptr = p_src.ptr();
	const int64_t size = p_src.size();
	int64_t off = 0;
	auto get32 = [&](uint32_t &v) -> bool {
		if (off + 4 > size) {
			return false;
		}
		v = uint32_t(ptr[off]) | (uint32_t(ptr[off + 1]) << 8) | (uint32_t(ptr[off + 2]) << 16) | (uint32_t(ptr[off + 3]) << 24);
		off += 4;
		return true;
	};
	auto get64 = [&](uint64_t &v) -> bool {
		if (off + 8 > size) {
			return false;
		}
		v = 0;
		for (int i = 0; i < 8; i++) {
			v |= uint64_t(ptr[off + i]) << (8 * i);
		}
		off += 8;
		return true;
	};
	uint32_t count = 0;
	if (!get32(count)) {
		return p_src;
	}
	struct Ent {
		uint64_t id = 0;
		String path;
	};
	Vector<Ent> ents;
	ents.resize(count);
	for (uint32_t i = 0; i < count; i++) {
		uint64_t id = 0;
		uint32_t len = 0;
		if (!get64(id) || !get32(len) || off + int64_t(len) > size) {
			return p_src;
		}
		ents.write[i].id = id;
		ents.write[i].path = p_ob->rewrite_pack_paths(String::utf8(reinterpret_cast<const char *>(ptr + off), len));
		off += len;
	}
	auto store32 = [](Vector<uint8_t> &o, uint32_t v) {
		o.push_back(uint8_t(v));
		o.push_back(uint8_t(v >> 8));
		o.push_back(uint8_t(v >> 16));
		o.push_back(uint8_t(v >> 24));
	};
	auto store64 = [](Vector<uint8_t> &o, uint64_t v) {
		for (int i = 0; i < 8; i++) {
			o.push_back(uint8_t(v >> (8 * i)));
		}
	};
	Vector<uint8_t> out;
	store32(out, uint32_t(ents.size()));
	for (int i = 0; i < ents.size(); i++) {
		store64(out, ents[i].id);
		const CharString cs = ents[i].path.utf8();
		store32(out, uint32_t(cs.length()));
		const uint8_t *bp = reinterpret_cast<const uint8_t *>(cs.ptr());
		for (int j = 0; j < cs.length(); j++) {
			out.push_back(bp[j]);
		}
	}
	return out;
}

void ObfuscationExportPlugin::_export_begin(const HashSet<String> &p_features, bool p_debug, const String &p_path, int p_flags) {
	(void)p_debug;
	(void)p_path;
	(void)p_flags;
	enabled = true;
	scene_stamp_remaining = 4;
	stripped_copyright = false;
	scramble_pack = false;
	pack_idents.clear();
	pack_funcs.clear();
	pack_scene_names.clear();
	pack_scripts.clear();
	saved_settings.clear();
	saved_uid_cache.clear();
	uid_cache_path = String();
	uid_cache_patched = false;
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

	scramble_pack = bool(GLOBAL_GET("obfuscation/pack/scramble_names"));
	if (scramble_pack && ob->has_identity()) {
		ob->build_pack_ident_maps(pack_idents, pack_funcs, pack_scene_names);
		List<PropertyInfo> props;
		ProjectSettings::get_singleton()->get_property_list(&props);
		for (const PropertyInfo &pi : props) {
			if (pi.type != Variant::STRING || !(pi.usage & PROPERTY_USAGE_STORAGE)) {
				continue;
			}
			const Variant cur = ProjectSettings::get_singleton()->get(pi.name);
			const String src = cur;
			if (src.find("res://") < 0) {
				continue;
			}
			if (src.begins_with("res://") && !FileAccess::exists(src)) {
				continue;
			}
			const String rewritten = ob->rewrite_pack_paths(src);
			if (rewritten == src) {
				continue;
			}
			saved_settings[pi.name] = cur;
			ProjectSettings::get_singleton()->set(pi.name, rewritten);
		}
		uid_cache_path = ResourceUID::get_cache_file();
		if (FileAccess::exists(uid_cache_path)) {
			saved_uid_cache = FileAccess::get_file_as_bytes(uid_cache_path);
			const Vector<uint8_t> rewritten_uid = _obf_rewrite_uid_cache_bytes(saved_uid_cache, ob);
			Ref<FileAccess> uf = FileAccess::open(uid_cache_path, FileAccess::WRITE);
			if (uf.is_valid()) {
				uf->store_buffer(rewritten_uid.ptr(), rewritten_uid.size());
				uid_cache_patched = true;
			}
		}
	}

	if (ob->has_identity() && (scramble_pack || bool(GLOBAL_GET("obfuscation/scripts/comment_lattice")))) {
		if (pack_idents.is_empty()) {
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
	if (uid_cache_patched && !uid_cache_path.is_empty()) {
		Ref<FileAccess> uf = FileAccess::open(uid_cache_path, FileAccess::WRITE);
		if (uf.is_valid()) {
			if (!saved_uid_cache.is_empty()) {
				uf->store_buffer(saved_uid_cache.ptr(), saved_uid_cache.size());
			}
		}
	}
	saved_uid_cache.clear();
	uid_cache_path = String();
	uid_cache_patched = false;
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
		Vector<uint8_t> bytes;
		if (ObfuscationSourceMap::is_script_ext(ext)) {
			if (pack_scripts.has(p_path)) {
				bytes = pack_scripts[p_path].to_utf8_buffer();
			} else {
				bytes = ob->pack_script_source(FileAccess::get_file_as_string(p_path), p_path, dest, pack_idents, pack_funcs, pack_scene_names).to_utf8_buffer();
			}
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
			add_file(p_path, pack_scripts[p_path].to_utf8_buffer(), false);
			return;
		}
		const String src = FileAccess::get_file_as_string(p_path);
		if (src.is_empty()) {
			return;
		}
		const String injected = ob->inject_script_source(src, p_path);
		if (injected == src) {
			return;
		}
		add_file(p_path, injected.to_utf8_buffer(), true);
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
