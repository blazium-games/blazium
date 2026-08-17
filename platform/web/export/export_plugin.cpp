/**************************************************************************/
/*  export_plugin.cpp                                                     */
/**************************************************************************/
/*                         This file is part of:                          */
/*                             GODOT ENGINE                               */
/*                        https://godotengine.org                         */
/**************************************************************************/
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

#include "export_plugin.h"

#include "logo_svg.gen.h"
#include "run_icon_svg.gen.h"

#include "core/config/project_settings.h"
#include "core/io/compression.h"
#include "core/io/dir_access.h"
#include "core/io/zip_io.h"
#include "core/os/os.h"
#include "editor/editor_string_names.h"
#include "editor/export/editor_export.h"
#include "editor/file_system/editor_paths.h"
#include "editor/import/resource_importer_texture_settings.h"
#include "editor/settings/editor_settings.h"
#include "editor/themes/editor_scale.h"
#include "scene/resources/image_texture.h"

#include "modules/modules_enabled.gen.h" // IWYU pragma: keep. For mono.
#include "modules/svg/image_loader_svg.h"

Error EditorExportPlatformWeb::_extract_template(const String &p_template, const String &p_dir, const String &p_name, bool pwa, bool p_compress_base_wasm, bool p_keep_uncompressed_wasm) {
	Ref<FileAccess> io_fa;
	zlib_filefunc_def io = zipio_create_io(&io_fa);
	unzFile pkg = unzOpen2(p_template.utf8().get_data(), &io);

	if (!pkg) {
		add_message(EXPORT_MESSAGE_ERROR, TTR("Prepare Templates"), vformat(TTR("Could not open template for export: \"%s\"."), p_template));
		return ERR_FILE_NOT_FOUND;
	}

	if (unzGoToFirstFile(pkg) != UNZ_OK) {
		add_message(EXPORT_MESSAGE_ERROR, TTR("Prepare Templates"), vformat(TTR("Invalid export template: \"%s\"."), p_template));
		unzClose(pkg);
		return ERR_FILE_CORRUPT;
	}

	do {
		//get filename
		unz_file_info info;
		char fname[16384];
		unzGetCurrentFileInfo(pkg, &info, fname, 16384, nullptr, 0, nullptr, 0);

		String file = String::utf8(fname);

		// Skip folders.
		if (file.ends_with("/")) {
			continue;
		}

		// Skip service worker and offline page if not exporting pwa.
		if (!pwa && (file == "blazium.service.worker.js" || file == "blazium.offline.html")) {
			continue;
		}
		Vector<uint8_t> data;
		data.resize(info.uncompressed_size);

		//read
		unzOpenCurrentFile(pkg);
		unzReadCurrentFile(pkg, data.ptrw(), data.size());
		unzCloseCurrentFile(pkg);

		//write
		String dst = p_dir.path_join(file.replace("blazium", p_name));
		Error err;
		if (dst.ends_with(".wasm") && p_compress_base_wasm) {
			err = _write_or_error(data.ptr(), data.size(), dst + ".gz", "Prepare Templates", true);
			if (err == OK && p_keep_uncompressed_wasm) {
				err = _write_or_error(data.ptr(), data.size(), dst, "Prepare Templates", false);
			}
		} else {
			err = _write_or_error(data.ptr(), data.size(), dst, "Prepare Templates", false);
		}
		if (err != OK) {
			unzClose(pkg);
			return err;
		}

	} while (unzGoToNextFile(pkg) == UNZ_OK);
	unzClose(pkg);
	return OK;
}

Error EditorExportPlatformWeb::_write_or_error(const uint8_t *p_content, int p_size, String p_path, String p_stage, bool p_compress) {
	Ref<FileAccess> f = FileAccess::open(p_path, FileAccess::WRITE);
	if (f.is_null()) {
		add_message(EXPORT_MESSAGE_ERROR, TTR(p_stage), vformat(TTR("Could not write file: \"%s\"."), p_path));
		return ERR_FILE_CANT_WRITE;
	}
	if (p_compress) {
		PackedByteArray compressed_data;
		compressed_data.resize(Compression::get_max_compressed_buffer_size(p_size, Compression::MODE_GZIP));
		int compressed_size = Compression::compress(compressed_data.ptrw(), p_content, p_size, Compression::MODE_GZIP);
		if (compressed_size < 0) {
			add_message(EXPORT_MESSAGE_ERROR, TTR(p_stage), vformat(TTR("Could not compress file: \"%s\"."), p_path));
			return ERR_FILE_CANT_WRITE;
		}
		f->store_buffer(compressed_data.ptr(), compressed_size);
	} else {
		f->store_buffer(p_content, p_size);
	}
	return OK;
}

void EditorExportPlatformWeb::_replace_strings(const HashMap<String, String> &p_replaces, Vector<uint8_t> &r_template) {
	String str_template = String::utf8(reinterpret_cast<const char *>(r_template.ptr()), r_template.size());
	String out;
	Vector<String> lines = str_template.split("\n");
	for (int i = 0; i < lines.size(); i++) {
		String current_line = lines[i];
		for (const KeyValue<String, String> &E : p_replaces) {
			current_line = current_line.replace(E.key, E.value);
		}
		out += current_line + "\n";
	}
	CharString cs = out.utf8();
	r_template.resize(cs.length());
	for (int i = 0; i < cs.length(); i++) {
		r_template.write[i] = cs[i];
	}
}

void EditorExportPlatformWeb::_fix_html(Vector<uint8_t> &p_html, const Ref<EditorExportPreset> &p_preset, const String &p_name, bool p_debug, BitField<EditorExportPlatform::DebugFlags> p_flags, const Vector<SharedObject> p_shared_objects, const Dictionary &p_file_sizes) {
	// Engine.js config
	Dictionary config;
	Array libs;
	for (int i = 0; i < p_shared_objects.size(); i++) {
		libs.push_back(p_shared_objects[i].path.get_file());
	}
	Vector<String> flags = gen_export_flags(p_flags & (~DEBUG_FLAG_DUMB_CLIENT));
	Array args;
	for (int i = 0; i < flags.size(); i++) {
		args.push_back(flags[i]);
	}
	config["canvasResizePolicy"] = p_preset->get("html/canvas_resize_policy");
	config["experimentalVK"] = p_preset->get("html/experimental_virtual_keyboard");
	config["focusCanvas"] = p_preset->get("html/focus_canvas_on_start");
	config["gdextensionLibs"] = libs;
	config["executable"] = p_name;
	config["args"] = args;
	config["fileSizes"] = p_file_sizes;
	config["ensureCrossOriginIsolationHeaders"] = (bool)p_preset->get("progressive_web_app/ensure_cross_origin_isolation_headers");

	config["godotPoolSize"] = p_preset->get("threads/godot_pool_size");
	config["emscriptenPoolSize"] = p_preset->get("threads/emscripten_pool_size");

	String head_include;
	if (p_preset->get("html/export_icon")) {
		head_include += "<link id=\"-gd-engine-icon\" rel=\"icon\" type=\"image/png\" href=\"" + p_name + ".icon.png\" />\n";
		head_include += "<link rel=\"apple-touch-icon\" href=\"" + p_name + ".apple-touch-icon.png\"/>\n";
	}
	if (p_preset->get("progressive_web_app/enabled")) {
		head_include += "<link rel=\"manifest\" href=\"" + p_name + ".manifest.json\">\n";
		config["serviceWorker"] = p_name + ".service.worker.js";
	}

	String blazium_third_party;
	const bool youtube_playable = p_preset->get("blazium/youtube_playable/enabled");
	const bool discord_embed = p_preset->get("blazium/discord_embed/enabled");
	if (youtube_playable || discord_embed) {
		if (youtube_playable) {
			blazium_third_party += R"(<script src="https://www.youtube.com/game_api/v1"></script>)";
		}
		blazium_third_party += R"(<script src=")" + p_name + ".third.party.js" + R"("></script>)";
	}

	String blazium_header_embeds;
	if (p_preset->get("blazium/web_headers/enabled")) {
		if (p_preset->has("blazium/web_headers/title")) {
			blazium_header_embeds += "<meta property=\"og:title\" content=\"" + String(p_preset->get("blazium/web_headers/title")) + "\"/>\n";
		}
		if (p_preset->has("blazium/web_headers/description")) {
			blazium_header_embeds += "<meta property=\"og:description\" content=\"" + String(p_preset->get("blazium/web_headers/description")) + "\"/>\n";
		}
		if (p_preset->has("blazium/web_headers/url")) {
			blazium_header_embeds += "<meta property=\"og:url\" content=\"" + String(p_preset->get("blazium/web_headers/url")) + "\"/>\n";
		}
		if (p_preset->has("blazium/web_headers/image")) {
			blazium_header_embeds += "<meta property=\"og:image\" content=\"" + String(p_preset->get("blazium/web_headers/image")) + "\"/>\n";
		}
		if (p_preset->has("blazium/web_headers/type")) {
			blazium_header_embeds += "<meta property=\"og:type\" content=\"" + String(p_preset->get("blazium/web_headers/type")) + "\"/>\n";
		}
		if (p_preset->has("blazium/web_headers/site_name")) {
			blazium_header_embeds += "<meta property=\"og:site_name\" content=\"" + String(p_preset->get("blazium/web_headers/site_name")) + "\"/>\n";
		}
	}
	if (p_preset->get("blazium/social_headers/enabled")) {
		if (p_preset->has("blazium/social_headers/title")) {
			blazium_header_embeds += "<meta property=\"twitter:title\" content=\"" + String(p_preset->get("blazium/social_headers/title")) + "\"/>\n";
		}
		if (p_preset->has("blazium/social_headers/description")) {
			blazium_header_embeds += "<meta property=\"twitter:description\" content=\"" + String(p_preset->get("blazium/social_headers/description")) + "\"/>\n";
		}
		if (p_preset->has("blazium/social_headers/url")) {
			blazium_header_embeds += "<meta property=\"twitter:url\" content=\"" + String(p_preset->get("blazium/social_headers/url")) + "\"/>\n";
		}
		if (p_preset->has("blazium/social_headers/image")) {
			blazium_header_embeds += "<meta property=\"twitter:image\" content=\"" + String(p_preset->get("blazium/social_headers/image")) + "\"/>\n";
		}
		if (p_preset->has("blazium/social_headers/site")) {
			blazium_header_embeds += "<meta property=\"twitter:site\" content=\"" + String(p_preset->get("blazium/social_headers/site")) + "\"/>\n";
		}
		if (p_preset->has("blazium/social_headers/card")) {
			blazium_header_embeds += "<meta property=\"twitter:card\" content=\"" + String(p_preset->get("blazium/social_headers/card")) + "\"/>\n";
		}
	}

	// Replaces HTML string
	const String str_config = Variant(config).to_json_string();
	const String custom_head_include = p_preset->get("html/head_include");
	HashMap<String, String> replaces;
	replaces["$BLAZIUM_THIRD_PARTY"] = blazium_third_party;
	replaces["$BLAZIUM_HEADER_EMBEDS"] = blazium_header_embeds;
	replaces["$BLAZIUM_ENGINE_STARTER"] = p_name + ".engine.starter.js";
	replaces["$GODOT_URL"] = p_name + ".js";
	replaces["$GODOT_PROJECT_NAME"] = get_project_setting(p_preset, "application/config/name");
	replaces["$GODOT_HEAD_INCLUDE"] = head_include + custom_head_include;
	replaces["$GODOT_CONFIG"] = str_config;
	replaces["$GODOT_SPLASH_COLOR"] = "#" + Color(get_project_setting(p_preset, "application/boot_splash/bg_color")).to_html(false);

	Vector<String> godot_splash_classes;
	godot_splash_classes.push_back("show-image--" + String(get_project_setting(p_preset, "application/boot_splash/show_image")));
	RSE::SplashStretchMode boot_splash_stretch_mode = get_project_setting(p_preset, "application/boot_splash/stretch_mode");
	godot_splash_classes.push_back("fullsize--" + String(((boot_splash_stretch_mode != RSE::SplashStretchMode::SPLASH_STRETCH_MODE_DISABLED) ? "true" : "false")));
	godot_splash_classes.push_back("use-filter--" + String(get_project_setting(p_preset, "application/boot_splash/use_filter")));
	replaces["$GODOT_SPLASH_CLASSES"] = String(" ").join(godot_splash_classes);
	replaces["$GODOT_SPLASH"] = p_name + ".png";

	if (p_preset->get("variant/thread_support")) {
		replaces["$GODOT_THREADS_ENABLED"] = "true";
	} else {
		replaces["$GODOT_THREADS_ENABLED"] = "false";
	}

	_replace_strings(replaces, p_html);
}

Error EditorExportPlatformWeb::_build_js_files(const Ref<EditorExportPreset> &p_preset, const String &p_path, BitField<EditorExportPlatform::DebugFlags> p_flags, const Vector<SharedObject> p_shared_objects, const Dictionary &p_file_sizes) {
	Vector<uint8_t> third_party_js;
	const String dir = p_path.get_base_dir();
	const String name = p_path.get_file().get_basename();

	String discord_embed_path = dir.path_join(name + ".discord.embed.js");
	const bool discord_embed = p_preset->get("blazium/discord_embed/enabled");
	if (discord_embed) {
		Vector<uint8_t> discord_embed_js;
		Ref<FileAccess> f = FileAccess::open(discord_embed_path, FileAccess::READ);
		if (f.is_null()) {
			add_message(EXPORT_MESSAGE_ERROR, TTR("BUILD_JS_FILES"), vformat(TTR("Could not read file: \"%s\"."), discord_embed_path));
			return ERR_FILE_CANT_READ;
		}
		discord_embed_js.resize(f->get_length());
		f->get_buffer(discord_embed_js.ptrw(), discord_embed_js.size());

		HashMap<String, String> replaces;
		replaces["$BLAZIUM_DISCORD_AUTODETECT"] = p_preset->get("blazium/discord_embed/autodetect") ? "true" : "false";
		_replace_strings(replaces, discord_embed_js);
		third_party_js.append_array(discord_embed_js);
	}
	DirAccess::remove_file_or_error(discord_embed_path);

	String youtube_playables_path = dir.path_join(name + ".youtube.playables.js");
	const bool youtube_playable = p_preset->get("blazium/youtube_playable/enabled");
	if (youtube_playable) {
		Vector<uint8_t> youtube_playables_js;
		Ref<FileAccess> f = FileAccess::open(youtube_playables_path, FileAccess::READ);
		if (f.is_null()) {
			add_message(EXPORT_MESSAGE_ERROR, TTR("BUILD_JS_FILES"), vformat(TTR("Could not read file: \"%s\"."), youtube_playables_path));
			return ERR_FILE_CANT_READ;
		}
		youtube_playables_js.resize(f->get_length());
		f->get_buffer(youtube_playables_js.ptrw(), youtube_playables_js.size());
		third_party_js.append_array(youtube_playables_js);
	}
	DirAccess::remove_file_or_error(youtube_playables_path);

	if (youtube_playable || discord_embed) {
		Error err = _write_or_error(third_party_js.ptr(), third_party_js.size(), dir.path_join(name + ".third.party.js"), "Export", false);
		if (err != OK) {
			return err;
		}
	}

	String engine_starter_path = dir.path_join(name + ".engine.starter.js");
	Vector<uint8_t> engine_starter_js;
	{
		Ref<FileAccess> f = FileAccess::open(engine_starter_path, FileAccess::READ);
		if (f.is_null()) {
			add_message(EXPORT_MESSAGE_ERROR, TTR("BUILD_JS_FILES"), vformat(TTR("Could not read file: \"%s\"."), engine_starter_path));
			return ERR_FILE_CANT_READ;
		}
		engine_starter_js.resize(f->get_length());
		f->get_buffer(engine_starter_js.ptrw(), engine_starter_js.size());
	}

	Dictionary engine_config;
	engine_config["canvasResizePolicy"] = p_preset->get("html/canvas_resize_policy");
	engine_config["experimentalVK"] = p_preset->get("html/experimental_virtual_keyboard");
	engine_config["focusCanvas"] = p_preset->get("html/focus_canvas_on_start");
	engine_config["executable"] = name;
	engine_config["fileSizes"] = p_file_sizes;
	engine_config["ensureCrossOriginIsolationHeaders"] = (bool)p_preset->get("progressive_web_app/ensure_cross_origin_isolation_headers");
	engine_config["godotPoolSize"] = p_preset->get("threads/godot_pool_size");
	engine_config["emscriptenPoolSize"] = p_preset->get("threads/emscripten_pool_size");

	Array libs;
	for (int i = 0; i < p_shared_objects.size(); i++) {
		libs.push_back(p_shared_objects[i].path.get_file());
	}
	engine_config["gdextensionLibs"] = libs;

	Array args;
	Vector<String> flags = gen_export_flags(p_flags & (~DEBUG_FLAG_DUMB_CLIENT));
	for (int i = 0; i < flags.size(); i++) {
		args.push_back(flags[i]);
	}
	engine_config["args"] = args;

	if (p_preset->get("progressive_web_app/enabled")) {
		engine_config["serviceWorker"] = name + ".service.worker.js";
	}

	HashMap<String, String> replaces;
	replaces["$GODOT_CONFIG"] = Variant(engine_config).to_json_string();
	replaces["$GODOT_THREADS_ENABLED"] = p_preset->get("variant/thread_support") ? "true" : "false";
	_replace_strings(replaces, engine_starter_js);

	return _write_or_error(engine_starter_js.ptr(), engine_starter_js.size(), engine_starter_path, "Export", false);
}

Error EditorExportPlatformWeb::_add_manifest_icon(const Ref<EditorExportPreset> &p_preset, const String &p_path, const String &p_icon, int p_size, Array &r_arr) {
	const String name = p_path.get_file().get_basename();
	const String icon_name = vformat("%s.%dx%d.png", name, p_size, p_size);
	const String icon_dest = p_path.get_base_dir().path_join(icon_name);

	Ref<Image> icon;
	if (!p_icon.is_empty()) {
		Error err = OK;
		icon = _load_icon_or_splash_image(p_icon, &err);
		if (err != OK || icon.is_null() || icon->is_empty()) {
			add_message(EXPORT_MESSAGE_ERROR, TTR("Icon Creation"), vformat(TTR("Could not read file: \"%s\"."), p_icon));
			return err;
		}
		if (icon->get_width() != p_size || icon->get_height() != p_size) {
			icon->resize(p_size, p_size);
		}
	} else {
		icon = _get_project_icon(p_preset);
		icon->resize(p_size, p_size);
	}
	const Error err = icon->save_png(icon_dest);
	if (err != OK) {
		add_message(EXPORT_MESSAGE_ERROR, TTR("Icon Creation"), vformat(TTR("Could not write file: \"%s\"."), icon_dest));
		return err;
	}
	Dictionary icon_dict;
	icon_dict["sizes"] = vformat("%dx%d", p_size, p_size);
	icon_dict["type"] = "image/png";
	icon_dict["src"] = icon_name;
	r_arr.push_back(icon_dict);
	return err;
}

Error EditorExportPlatformWeb::_build_pwa(const Ref<EditorExportPreset> &p_preset, const String p_path, const Vector<SharedObject> &p_shared_objects) {
	String proj_name = get_project_setting(p_preset, "application/config/name");
	if (proj_name.is_empty()) {
		proj_name = "Blazium Game";
	}

	// Service worker
	const String dir = p_path.get_base_dir();
	const String name = p_path.get_file().get_basename();
	bool extensions = (bool)p_preset->get("variant/extensions_support");
	bool ensure_crossorigin_isolation_headers = (bool)p_preset->get("progressive_web_app/ensure_cross_origin_isolation_headers");
	HashMap<String, String> replaces;
	replaces["___GODOT_VERSION___"] = String::num_int64(OS::get_singleton()->get_unix_time()) + "|" + String::num_int64(OS::get_singleton()->get_ticks_usec());
	replaces["___GODOT_NAME___"] = proj_name.substr(0, 16);
	replaces["___GODOT_OFFLINE_PAGE___"] = name + ".offline.html";
	replaces["___GODOT_ENSURE_CROSSORIGIN_ISOLATION_HEADERS___"] = ensure_crossorigin_isolation_headers ? "true" : "false";

	// Files cached during worker install.
	Array cache_files = {
		name + ".html",
		name + ".js",
		name + ".offline.html"
	};
	if (p_preset->get("html/export_icon")) {
		cache_files.push_back(name + ".icon.png");
		cache_files.push_back(name + ".apple-touch-icon.png");
	}

	cache_files.push_back(name + ".audio.worklet.js");
	cache_files.push_back(name + ".audio.position.worklet.js");
	replaces["___GODOT_CACHE___"] = Variant(cache_files).to_json_string();

	// Heavy files that are cached on demand.
	Array opt_cache_files = {
		name + ".wasm",
		name + ".pck"
	};
	if (extensions) {
		opt_cache_files.push_back(name + ".side.wasm");
		for (int i = 0; i < p_shared_objects.size(); i++) {
			opt_cache_files.push_back(p_shared_objects[i].path.get_file());
		}
	}
	replaces["___GODOT_OPT_CACHE___"] = Variant(opt_cache_files).to_json_string();

	const String sw_path = dir.path_join(name + ".service.worker.js");
	Vector<uint8_t> sw;
	{
		Ref<FileAccess> f = FileAccess::open(sw_path, FileAccess::READ);
		if (f.is_null()) {
			add_message(EXPORT_MESSAGE_ERROR, TTR("PWA"), vformat(TTR("Could not read file: \"%s\"."), sw_path));
			return ERR_FILE_CANT_READ;
		}
		sw.resize(f->get_length());
		f->get_buffer(sw.ptrw(), sw.size());
	}
	_replace_strings(replaces, sw);
	Error err = _write_or_error(sw.ptr(), sw.size(), dir.path_join(name + ".service.worker.js"));
	if (err != OK) {
		// Message is supplied by the subroutine method.
		return err;
	}

	// Custom offline page
	const String offline_page = p_preset->get("progressive_web_app/offline_page");
	if (!offline_page.is_empty()) {
		Ref<DirAccess> da = DirAccess::create(DirAccess::ACCESS_FILESYSTEM);
		const String offline_dest = dir.path_join(name + ".offline.html");
		err = da->copy(ProjectSettings::get_singleton()->globalize_path(offline_page), offline_dest);
		if (err != OK) {
			add_message(EXPORT_MESSAGE_ERROR, TTR("PWA"), vformat(TTR("Could not read file: \"%s\"."), offline_dest));
			return err;
		}
	}

	// Manifest
	const char *modes[4] = { "fullscreen", "standalone", "minimal-ui", "browser" };
	const char *orientations[3] = { "any", "landscape", "portrait" };
	const int display = CLAMP(int(p_preset->get("progressive_web_app/display")), 0, 4);
	const int orientation = CLAMP(int(p_preset->get("progressive_web_app/orientation")), 0, 3);

	Dictionary manifest;
	manifest["name"] = proj_name;
	manifest["start_url"] = "./" + name + ".html";
	manifest["display"] = String::utf8(modes[display]);
	manifest["orientation"] = String::utf8(orientations[orientation]);
	manifest["background_color"] = "#" + p_preset->get("progressive_web_app/background_color").operator Color().to_html(false);

	Array icons_arr;
	const String icon144_path = p_preset->get("progressive_web_app/icon_144x144");
	err = _add_manifest_icon(p_preset, p_path, icon144_path, 144, icons_arr);
	if (err != OK) {
		// Message is supplied by the subroutine method.
		return err;
	}
	const String icon180_path = p_preset->get("progressive_web_app/icon_180x180");
	err = _add_manifest_icon(p_preset, p_path, icon180_path, 180, icons_arr);
	if (err != OK) {
		// Message is supplied by the subroutine method.
		return err;
	}
	const String icon512_path = p_preset->get("progressive_web_app/icon_512x512");
	err = _add_manifest_icon(p_preset, p_path, icon512_path, 512, icons_arr);
	if (err != OK) {
		// Message is supplied by the subroutine method.
		return err;
	}
	manifest["icons"] = icons_arr;

	CharString cs = Variant(manifest).to_json_string().utf8();
	err = _write_or_error((const uint8_t *)cs.get_data(), cs.length(), dir.path_join(name + ".manifest.json"));
	if (err != OK) {
		// Message is supplied by the subroutine method.
		return err;
	}

	return OK;
}

void EditorExportPlatformWeb::get_preset_features(const Ref<EditorExportPreset> &p_preset, List<String> *r_features) const {
	if (p_preset->get("vram_texture_compression/for_desktop")) {
		r_features->push_back("s3tc");
		r_features->push_back("bptc");
	}
	if (p_preset->get("vram_texture_compression/for_mobile")) {
		r_features->push_back("etc2");
		r_features->push_back("astc");
	}
	if (p_preset->get("variant/thread_support").operator bool()) {
		r_features->push_back("threads");
	} else {
		r_features->push_back("nothreads");
	}
	if (p_preset->get("variant/extensions_support").operator bool()) {
		r_features->push_back("web_extensions");
	} else {
		r_features->push_back("web_noextensions");
	}
	r_features->push_back("wasm32");
}

void EditorExportPlatformWeb::get_export_options(List<ExportOption> *r_options) const {
	r_options->push_back(ExportOption(PropertyInfo(Variant::STRING, "custom_template/debug", PROPERTY_HINT_GLOBAL_FILE, "*.zip"), ""));
	r_options->push_back(ExportOption(PropertyInfo(Variant::STRING, "custom_template/release", PROPERTY_HINT_GLOBAL_FILE, "*.zip"), ""));

	r_options->push_back(ExportOption(PropertyInfo(Variant::BOOL, "variant/extensions_support"), false)); // GDExtension support.
	r_options->push_back(ExportOption(PropertyInfo(Variant::BOOL, "variant/thread_support"), false, true)); // Thread support (i.e. run with or without COEP/COOP headers).
	r_options->push_back(ExportOption(PropertyInfo(Variant::BOOL, "vram_texture_compression/for_desktop"), true)); // S3TC
	r_options->push_back(ExportOption(PropertyInfo(Variant::BOOL, "vram_texture_compression/for_mobile"), false)); // ETC or ETC2, depending on renderer

	r_options->push_back(ExportOption(PropertyInfo(Variant::BOOL, "html/export_icon"), true));
	r_options->push_back(ExportOption(PropertyInfo(Variant::STRING, "html/custom_html_shell", PROPERTY_HINT_FILE, "*.html"), ""));
	r_options->push_back(ExportOption(PropertyInfo(Variant::STRING, "html/head_include", PROPERTY_HINT_MULTILINE_TEXT, "monospace,no_wrap"), ""));
	r_options->push_back(ExportOption(PropertyInfo(Variant::INT, "html/canvas_resize_policy", PROPERTY_HINT_ENUM, "None,Project,Adaptive"), 2));
	r_options->push_back(ExportOption(PropertyInfo(Variant::BOOL, "html/focus_canvas_on_start"), true));
	r_options->push_back(ExportOption(PropertyInfo(Variant::BOOL, "html/experimental_virtual_keyboard"), false));
	r_options->push_back(ExportOption(PropertyInfo(Variant::BOOL, "progressive_web_app/enabled"), false));
	r_options->push_back(ExportOption(PropertyInfo(Variant::BOOL, "progressive_web_app/ensure_cross_origin_isolation_headers"), true));
	r_options->push_back(ExportOption(PropertyInfo(Variant::STRING, "progressive_web_app/offline_page", PROPERTY_HINT_FILE, "*.html"), ""));
	r_options->push_back(ExportOption(PropertyInfo(Variant::INT, "progressive_web_app/display", PROPERTY_HINT_ENUM, "Fullscreen,Standalone,Minimal UI,Browser"), 1));
	r_options->push_back(ExportOption(PropertyInfo(Variant::INT, "progressive_web_app/orientation", PROPERTY_HINT_ENUM, "Any,Landscape,Portrait"), 0));
	r_options->push_back(ExportOption(PropertyInfo(Variant::STRING, "progressive_web_app/icon_144x144", PROPERTY_HINT_FILE, "*.png,*.webp,*.svg"), ""));
	r_options->push_back(ExportOption(PropertyInfo(Variant::STRING, "progressive_web_app/icon_180x180", PROPERTY_HINT_FILE, "*.png,*.webp,*.svg"), ""));
	r_options->push_back(ExportOption(PropertyInfo(Variant::STRING, "progressive_web_app/icon_512x512", PROPERTY_HINT_FILE, "*.png,*.webp,*.svg"), ""));
	r_options->push_back(ExportOption(PropertyInfo(Variant::COLOR, "progressive_web_app/background_color", PROPERTY_HINT_COLOR_NO_ALPHA), Color()));

	r_options->push_back(ExportOption(PropertyInfo(Variant::INT, "threads/emscripten_pool_size"), 8));
	r_options->push_back(ExportOption(PropertyInfo(Variant::INT, "threads/godot_pool_size"), 4));

	r_options->push_back(ExportOption(PropertyInfo(Variant::BOOL, "blazium/web_headers/enabled"), false, true));
	r_options->push_back(ExportOption(PropertyInfo(Variant::STRING, "blazium/web_headers/title", PROPERTY_HINT_PLACEHOLDER_TEXT, "Web Title"), ""));
	r_options->push_back(ExportOption(PropertyInfo(Variant::STRING, "blazium/web_headers/description", PROPERTY_HINT_PLACEHOLDER_TEXT, "Web Description"), ""));
	r_options->push_back(ExportOption(PropertyInfo(Variant::STRING, "blazium/web_headers/url", PROPERTY_HINT_PLACEHOLDER_TEXT, "Web URL"), ""));
	r_options->push_back(ExportOption(PropertyInfo(Variant::STRING, "blazium/web_headers/image", PROPERTY_HINT_PLACEHOLDER_TEXT, "Image URL"), ""));
	r_options->push_back(ExportOption(PropertyInfo(Variant::STRING, "blazium/web_headers/type", PROPERTY_HINT_PLACEHOLDER_TEXT, "Web Type"), "website"));
	r_options->push_back(ExportOption(PropertyInfo(Variant::STRING, "blazium/web_headers/site_name", PROPERTY_HINT_PLACEHOLDER_TEXT, "Site Name"), ""));

	r_options->push_back(ExportOption(PropertyInfo(Variant::BOOL, "blazium/social_headers/enabled"), false, true));
	r_options->push_back(ExportOption(PropertyInfo(Variant::STRING, "blazium/social_headers/title", PROPERTY_HINT_PLACEHOLDER_TEXT, "Social Title"), ""));
	r_options->push_back(ExportOption(PropertyInfo(Variant::STRING, "blazium/social_headers/description", PROPERTY_HINT_PLACEHOLDER_TEXT, "Social Description"), ""));
	r_options->push_back(ExportOption(PropertyInfo(Variant::STRING, "blazium/social_headers/url", PROPERTY_HINT_PLACEHOLDER_TEXT, "Social URL"), ""));
	r_options->push_back(ExportOption(PropertyInfo(Variant::STRING, "blazium/social_headers/image", PROPERTY_HINT_PLACEHOLDER_TEXT, "Social Image URL"), ""));
	r_options->push_back(ExportOption(PropertyInfo(Variant::STRING, "blazium/social_headers/site", PROPERTY_HINT_PLACEHOLDER_TEXT, "Social Site"), ""));
	r_options->push_back(ExportOption(PropertyInfo(Variant::STRING, "blazium/social_headers/card", PROPERTY_HINT_PLACEHOLDER_TEXT, "Site Card"), ""));

	r_options->push_back(ExportOption(PropertyInfo(Variant::BOOL, "blazium/discord_embed/enabled"), false, true));
	r_options->push_back(ExportOption(PropertyInfo(Variant::BOOL, "blazium/discord_embed/autodetect"), false));

	r_options->push_back(ExportOption(PropertyInfo(Variant::BOOL, "blazium/youtube_playable/enabled"), false));

	r_options->push_back(ExportOption(PropertyInfo(Variant::BOOL, "blazium/export_gzip_compressed_wasm/enabled"), false, true));
	r_options->push_back(ExportOption(PropertyInfo(Variant::BOOL, "blazium/export_gzip_compressed_wasm/keep_uncompressed_wasm"), false));
}

bool EditorExportPlatformWeb::get_export_option_visibility(const EditorExportPreset *p_preset, const String &p_option) const {
	bool advanced_options_enabled = p_preset->are_advanced_options_enabled();
	if (p_option == "custom_template/debug" || p_option == "custom_template/release") {
		return advanced_options_enabled;
	}

	if (p_option == "threads/godot_pool_size" || p_option == "threads/emscripten_pool_size") {
		return p_preset->get("variant/thread_support").operator bool();
	}
	if (p_option.begins_with("blazium/web_headers") && p_option != "blazium/web_headers/enabled") {
		return p_preset->get("blazium/web_headers/enabled");
	}
	if (p_option.begins_with("blazium/social_headers") && p_option != "blazium/social_headers/enabled") {
		return p_preset->get("blazium/social_headers/enabled");
	}
	if (p_option.begins_with("blazium/discord_embed") && p_option != "blazium/discord_embed/enabled") {
		return p_preset->get("blazium/discord_embed/enabled");
	}
	if (p_option.begins_with("blazium/export_gzip_compressed_wasm") && p_option != "blazium/export_gzip_compressed_wasm/enabled") {
		return p_preset->get("blazium/export_gzip_compressed_wasm/enabled");
	}

	return true;
}

String EditorExportPlatformWeb::get_name() const {
	return "Web";
}

String EditorExportPlatformWeb::get_os_name() const {
	return "Web";
}

Ref<Texture2D> EditorExportPlatformWeb::get_logo() const {
	return logo;
}

bool EditorExportPlatformWeb::has_valid_export_configuration(const Ref<EditorExportPreset> &p_preset, String &r_error, bool &r_missing_templates, bool p_debug) const {
#ifdef MODULE_MONO_ENABLED
	// Don't check for additional errors, as this particular error cannot be resolved.
	r_error += TTR("Exporting to Web is currently not supported in Blazium when using C#/.NET.") + "\n";
	r_error += TTR("If this project does not use C#, use a non-C# editor build to export the project.") + "\n";
	return false;
#else

	String err;
	bool valid = false;
	bool extensions = (bool)p_preset->get("variant/extensions_support");
	bool thread_support = (bool)p_preset->get("variant/thread_support");

	// Look for export templates (first official, and if defined custom templates).
	bool dvalid = exists_export_template(_get_template_name(extensions, thread_support, true), &err);
	bool rvalid = exists_export_template(_get_template_name(extensions, thread_support, false), &err);

	if (p_preset->get("custom_template/debug") != "") {
		dvalid = FileAccess::exists(p_preset->get("custom_template/debug"));
		if (!dvalid) {
			err += TTR("Custom debug template not found.") + "\n";
		}
	}
	if (p_preset->get("custom_template/release") != "") {
		rvalid = FileAccess::exists(p_preset->get("custom_template/release"));
		if (!rvalid) {
			err += TTR("Custom release template not found.") + "\n";
		}
	}

	valid = dvalid || rvalid;
	r_missing_templates = !valid;

	if (!err.is_empty()) {
		r_error = err;
	}

	return valid;
#endif // !MODULE_MONO_ENABLED
}

bool EditorExportPlatformWeb::has_valid_project_configuration(const Ref<EditorExportPreset> &p_preset, String &r_error) const {
	String err;
	bool valid = true;

	// Validate the project configuration.

	if (p_preset->get("vram_texture_compression/for_mobile")) {
		if (!ResourceImporterTextureSettings::should_import_etc2_astc()) {
			valid = false;
		}
	}

	if (!err.is_empty()) {
		r_error = err;
	}

	return valid;
}

List<String> EditorExportPlatformWeb::get_binary_extensions(const Ref<EditorExportPreset> &p_preset) const {
	List<String> list;
	list.push_back("html");
	return list;
}

Error EditorExportPlatformWeb::export_project(const Ref<EditorExportPreset> &p_preset, bool p_debug, const String &p_path, BitField<EditorExportPlatform::DebugFlags> p_flags, bool p_notify) {
	ExportNotifier notifier(*this, p_preset, p_debug, p_path, p_flags, p_notify);

	const String custom_debug = p_preset->get("custom_template/debug");
	const String custom_release = p_preset->get("custom_template/release");
	const String custom_html = p_preset->get("html/custom_html_shell");
	const bool export_icon = p_preset->get("html/export_icon");
	const bool pwa = p_preset->get("progressive_web_app/enabled");

	const String base_dir = p_path.get_base_dir();
	const String base_path = p_path.get_basename();
	const String base_name = p_path.get_file().get_basename();

	if (!DirAccess::exists(base_dir)) {
		add_message(EXPORT_MESSAGE_ERROR, TTR("Export"), vformat(TTR("Target folder does not exist or is inaccessible: \"%s\""), base_dir));
		return ERR_FILE_BAD_PATH;
	}

	// Find the correct template
	String template_path = p_debug ? custom_debug : custom_release;
	template_path = template_path.strip_edges();
	if (template_path.is_empty()) {
		bool extensions = (bool)p_preset->get("variant/extensions_support");
		bool thread_support = (bool)p_preset->get("variant/thread_support");
		template_path = find_export_template(_get_template_name(extensions, thread_support, p_debug));
	}

	if (!template_path.is_empty() && !FileAccess::exists(template_path)) {
		add_message(EXPORT_MESSAGE_ERROR, TTR("Prepare Templates"), vformat(TTR("Template file not found: \"%s\"."), template_path));
		return ERR_FILE_NOT_FOUND;
	}

	// Export pck and shared objects
	Vector<SharedObject> shared_objects;
	String pck_path = base_path + ".pck";
	Error error = save_pack(p_preset, p_debug, pck_path, &shared_objects);
	if (error != OK) {
		add_message(EXPORT_MESSAGE_ERROR, TTR("Export"), vformat(TTR("Could not write file: \"%s\"."), pck_path));
		return error;
	}

	{
		Ref<DirAccess> da = DirAccess::create(DirAccess::ACCESS_FILESYSTEM);
		for (int i = 0; i < shared_objects.size(); i++) {
			String dst = base_dir.path_join(shared_objects[i].path.get_file());
			error = da->copy(shared_objects[i].path, dst);
			if (error != OK) {
				add_message(EXPORT_MESSAGE_ERROR, TTR("Export"), vformat(TTR("Could not write file: \"%s\"."), shared_objects[i].path.get_file()));
				return error;
			}
		}
	}

	// Extract templates.
	const bool compress_base_wasm = p_preset->get("blazium/export_gzip_compressed_wasm/enabled");
	bool keep_uncompressed_wasm = false;
	if (p_preset->has("blazium/export_gzip_compressed_wasm/keep_uncompressed_wasm")) {
		keep_uncompressed_wasm = p_preset->get("blazium/export_gzip_compressed_wasm/keep_uncompressed_wasm");
	}
	error = _extract_template(template_path, base_dir, base_name, pwa, compress_base_wasm, keep_uncompressed_wasm);
	if (error) {
		// Message is supplied by the subroutine method.
		return error;
	}

	// Parse generated file sizes (pck and wasm, to help show a meaningful loading bar).
	Dictionary file_sizes;
	Ref<FileAccess> f = FileAccess::open(pck_path, FileAccess::READ);
	if (f.is_valid()) {
		file_sizes[pck_path.get_file()] = (uint64_t)f->get_length();
	}
	String wasm_path = base_path + ".wasm";
	if (compress_base_wasm && !keep_uncompressed_wasm) {
		wasm_path += ".gz";
	}
	f = FileAccess::open(wasm_path, FileAccess::READ);
	if (f.is_valid()) {
		file_sizes[base_name + ".wasm"] = (uint64_t)f->get_length();
	}

	// Read the HTML shell file (custom or from template).
	const String html_path = custom_html.is_empty() ? base_path + ".html" : custom_html;
	Vector<uint8_t> html;
	f = FileAccess::open(html_path, FileAccess::READ);
	if (f.is_null()) {
		add_message(EXPORT_MESSAGE_ERROR, TTR("Export"), vformat(TTR("Could not read HTML shell: \"%s\"."), html_path));
		return ERR_FILE_CANT_READ;
	}
	html.resize(f->get_length());
	f->get_buffer(html.ptrw(), html.size());
	f.unref(); // close file.

	// Generate HTML file with replaced strings.
	_fix_html(html, p_preset, base_name, p_debug, p_flags, shared_objects, file_sizes);
	Error err = _write_or_error(html.ptr(), html.size(), p_path);
	if (err != OK) {
		// Message is supplied by the subroutine method.
		return err;
	}
	html.resize(0);

	err = _build_js_files(p_preset, p_path, p_flags, shared_objects, file_sizes);
	if (err != OK) {
		return err;
	}

	// Export splash (why?)
	Ref<Image> splash = _get_project_splash(p_preset);
	const String splash_png_path = base_path + ".png";
	if (splash->save_png(splash_png_path) != OK) {
		add_message(EXPORT_MESSAGE_ERROR, TTR("Export"), vformat(TTR("Could not write file: \"%s\"."), splash_png_path));
		return ERR_FILE_CANT_WRITE;
	}

	// Save a favicon that can be accessed without waiting for the project to finish loading.
	// This way, the favicon can be displayed immediately when loading the page.
	if (export_icon) {
		Ref<Image> favicon = _get_project_icon(p_preset);
		const String favicon_png_path = base_path + ".icon.png";
		if (favicon->save_png(favicon_png_path) != OK) {
			add_message(EXPORT_MESSAGE_ERROR, TTR("Export"), vformat(TTR("Could not write file: \"%s\"."), favicon_png_path));
			return ERR_FILE_CANT_WRITE;
		}
		favicon->resize(180, 180);
		const String apple_icon_png_path = base_path + ".apple-touch-icon.png";
		if (favicon->save_png(apple_icon_png_path) != OK) {
			add_message(EXPORT_MESSAGE_ERROR, TTR("Export"), vformat(TTR("Could not write file: \"%s\"."), apple_icon_png_path));
			return ERR_FILE_CANT_WRITE;
		}
	}

	// Generate the PWA worker and manifest
	if (pwa) {
		err = _build_pwa(p_preset, p_path, shared_objects);
		if (err != OK) {
			// Message is supplied by the subroutine method.
			return err;
		}
	}

	return OK;
}

bool EditorExportPlatformWeb::poll_export() {
	Ref<EditorExportPreset> preset = EditorExport::get_singleton()->get_runnable_preset_for_platform(this);

	RemoteDebugState prev_remote_debug_state = remote_debug_state;
	remote_debug_state = REMOTE_DEBUG_STATE_UNAVAILABLE;

	if (preset.is_valid()) {
		const bool debug = true;
		// Throwaway variables to pass to validation functions.
		String err;
		bool missing_templates;

		bool valid = has_valid_export_configuration(preset, err, missing_templates, debug) &&
				has_valid_project_configuration(preset, err);

		if (valid) {
			if (server->is_listening()) {
				remote_debug_state = REMOTE_DEBUG_STATE_SERVING;
			} else {
				remote_debug_state = REMOTE_DEBUG_STATE_AVAILABLE;
			}
		}
	}

	if (remote_debug_state != REMOTE_DEBUG_STATE_SERVING && server->is_listening()) {
		server->stop();
	}

	return remote_debug_state != prev_remote_debug_state;
}

Ref<Texture2D> EditorExportPlatformWeb::get_option_icon(int p_index) const {
	Ref<Texture2D> play_icon = EditorExportPlatform::get_option_icon(p_index);

	switch (remote_debug_state) {
		case REMOTE_DEBUG_STATE_UNAVAILABLE: {
			return nullptr;
		} break;

		case REMOTE_DEBUG_STATE_AVAILABLE: {
			switch (p_index) {
				case 0:
				case 1:
					return play_icon;
				default:
					ERR_FAIL_V(nullptr);
			}
		} break;

		case REMOTE_DEBUG_STATE_SERVING: {
			switch (p_index) {
				case 0:
					return play_icon;
				case 1:
					return restart_icon;
				case 2:
					return stop_icon;
				default:
					ERR_FAIL_V(nullptr);
			}
		} break;
	}

	return nullptr;
}

int EditorExportPlatformWeb::get_options_count() const {
	switch (remote_debug_state) {
		case REMOTE_DEBUG_STATE_UNAVAILABLE: {
			return 0;
		} break;

		case REMOTE_DEBUG_STATE_AVAILABLE: {
			return 2;
		} break;

		case REMOTE_DEBUG_STATE_SERVING: {
			return 3;
		} break;
	}

	return 0;
}

String EditorExportPlatformWeb::get_option_label(int p_index) const {
	String run_in_browser = TTR("Run in Browser");
	String start_http_server = TTR("Start HTTP Server");
	String reexport_project = TTR("Re-export Project");
	String stop_http_server = TTR("Stop HTTP Server");

	switch (remote_debug_state) {
		case REMOTE_DEBUG_STATE_UNAVAILABLE:
			return "";

		case REMOTE_DEBUG_STATE_AVAILABLE: {
			switch (p_index) {
				case 0:
					return run_in_browser;
				case 1:
					return start_http_server;
				default:
					ERR_FAIL_V("");
			}
		} break;

		case REMOTE_DEBUG_STATE_SERVING: {
			switch (p_index) {
				case 0:
					return run_in_browser;
				case 1:
					return reexport_project;
				case 2:
					return stop_http_server;
				default:
					ERR_FAIL_V("");
			}
		} break;
	}

	return "";
}

String EditorExportPlatformWeb::get_option_tooltip(int p_index) const {
	String run_in_browser = TTR("Run exported HTML in the system's default browser.");
	String start_http_server = TTR("Start the HTTP server.");
	String reexport_project = TTR("Export project again to account for updates.");
	String stop_http_server = TTR("Stop the HTTP server.");

	switch (remote_debug_state) {
		case REMOTE_DEBUG_STATE_UNAVAILABLE:
			return "";

		case REMOTE_DEBUG_STATE_AVAILABLE: {
			switch (p_index) {
				case 0:
					return run_in_browser;
				case 1:
					return start_http_server;
				default:
					ERR_FAIL_V("");
			}
		} break;

		case REMOTE_DEBUG_STATE_SERVING: {
			switch (p_index) {
				case 0:
					return run_in_browser;
				case 1:
					return reexport_project;
				case 2:
					return stop_http_server;
				default:
					ERR_FAIL_V("");
			}
		} break;
	}

	return "";
}

Error EditorExportPlatformWeb::run(const Ref<EditorExportPreset> &p_preset, int p_option, BitField<EditorExportPlatform::DebugFlags> p_debug_flags) {
	const uint16_t bind_port = EDITOR_GET("export/web/http_port");
	// Resolve host if needed.
	const String bind_host = EDITOR_GET("export/web/http_host");
	const bool use_tls = EDITOR_GET("export/web/use_tls");

	switch (remote_debug_state) {
		case REMOTE_DEBUG_STATE_UNAVAILABLE: {
			return FAILED;
		} break;

		case REMOTE_DEBUG_STATE_AVAILABLE: {
			switch (p_option) {
				// Run in Browser.
				case 0: {
					Error err = _export_project(p_preset, p_debug_flags);
					if (err != OK) {
						return err;
					}
					err = _start_server(bind_host, bind_port, use_tls);
					if (err != OK) {
						return err;
					}
					return _launch_browser(bind_host, bind_port, use_tls);
				} break;

				// Start HTTP Server.
				case 1: {
					Error err = _export_project(p_preset, p_debug_flags);
					if (err != OK) {
						return err;
					}
					return _start_server(bind_host, bind_port, use_tls);
				} break;

				default: {
					ERR_FAIL_V_MSG(FAILED, vformat(R"(Invalid option "%s" for the current state.)", p_option));
				}
			}
		} break;

		case REMOTE_DEBUG_STATE_SERVING: {
			switch (p_option) {
				// Run in Browser.
				case 0: {
					Error err = _export_project(p_preset, p_debug_flags);
					if (err != OK) {
						return err;
					}
					return _launch_browser(bind_host, bind_port, use_tls);
				} break;

				// Re-export Project.
				case 1: {
					return _export_project(p_preset, p_debug_flags);
				} break;

				// Stop HTTP Server.
				case 2: {
					return _stop_server();
				} break;

				default: {
					ERR_FAIL_V_MSG(FAILED, vformat(R"(Invalid option "%s" for the current state.)", p_option));
				}
			}
		} break;
	}

	return FAILED;
}

Error EditorExportPlatformWeb::_export_project(const Ref<EditorExportPreset> &p_preset, int p_debug_flags) {
	const String dest = EditorPaths::get_singleton()->get_temp_dir().path_join("web");
	Ref<DirAccess> da = DirAccess::create(DirAccess::ACCESS_FILESYSTEM);
	if (!da->dir_exists(dest)) {
		Error err = da->make_dir_recursive(dest);
		if (err != OK) {
			add_message(EXPORT_MESSAGE_ERROR, TTR("Run"), vformat(TTR("Could not create HTTP server directory: %s."), dest));
			return err;
		}
	}

	const String basepath = dest.path_join("tmp_js_export");
	Error err = export_project(p_preset, true, basepath + ".html", p_debug_flags);
	if (err != OK) {
		// Export generates several files, clean them up on failure.
		DirAccess::remove_file_or_error(basepath + ".html");
		DirAccess::remove_file_or_error(basepath + ".offline.html");
		DirAccess::remove_file_or_error(basepath + ".js");
		DirAccess::remove_file_or_error(basepath + ".audio.worklet.js");
		DirAccess::remove_file_or_error(basepath + ".audio.position.worklet.js");
		DirAccess::remove_file_or_error(basepath + ".service.worker.js");
		DirAccess::remove_file_or_error(basepath + ".pck");
		DirAccess::remove_file_or_error(basepath + ".png");
		DirAccess::remove_file_or_error(basepath + ".side.wasm");
		DirAccess::remove_file_or_error(basepath + ".wasm");
		DirAccess::remove_file_or_error(basepath + ".icon.png");
		DirAccess::remove_file_or_error(basepath + ".apple-touch-icon.png");
		DirAccess::remove_file_or_error(basepath + ".engine.starter.js");
		DirAccess::remove_file_or_error(basepath + ".discord.embed.js");
		DirAccess::remove_file_or_error(basepath + ".youtube.playables.js");
		DirAccess::remove_file_or_error(basepath + ".third.party.js");
	}
	return err;
}

Error EditorExportPlatformWeb::_launch_browser(const String &p_bind_host, const uint16_t p_bind_port, const bool p_use_tls) {
	OS::get_singleton()->shell_open(String((p_use_tls ? "https://" : "http://") + p_bind_host + ":" + itos(p_bind_port) + "/tmp_js_export.html"));
	// FIXME: Find out how to clean up export files after running the successfully
	// exported game. Might not be trivial.
	return OK;
}

Error EditorExportPlatformWeb::_start_server(const String &p_bind_host, const uint16_t p_bind_port, const bool p_use_tls) {
	IPAddress bind_ip;
	if (p_bind_host.is_valid_ip_address()) {
		bind_ip = p_bind_host;
	} else {
		bind_ip = IP::get_singleton()->resolve_hostname(p_bind_host);
	}
	ERR_FAIL_COND_V_MSG(!bind_ip.is_valid(), ERR_INVALID_PARAMETER, "Invalid editor setting 'export/web/http_host': '" + p_bind_host + "'. Try using '127.0.0.1'.");

	const String tls_key = EDITOR_GET("export/web/tls_key");
	const String tls_cert = EDITOR_GET("export/web/tls_certificate");

	// Restart server.
	server->stop();
	Error err = server->listen(p_bind_port, bind_ip, p_use_tls, tls_key, tls_cert);
	if (err != OK) {
		add_message(EXPORT_MESSAGE_ERROR, TTR("Run"), vformat(TTR("Error starting HTTP server: %d."), err));
	}
	return err;
}

Error EditorExportPlatformWeb::_stop_server() {
	server->stop();
	return OK;
}

Ref<Texture2D> EditorExportPlatformWeb::get_run_icon() const {
	return run_icon;
}

void EditorExportPlatformWeb::initialize() {
	if (EditorNode::get_singleton()) {
		server.instantiate();

		Ref<Image> img = memnew(Image);
		const bool upsample = !Math::is_equal_approx(Math::round(EDSCALE), EDSCALE);

		ImageLoaderSVG::create_image_from_string(img, _web_logo_svg, EDSCALE, upsample, false);
		logo = ImageTexture::create_from_image(img);

		ImageLoaderSVG::create_image_from_string(img, _web_run_icon_svg, EDSCALE, upsample, false);
		run_icon = ImageTexture::create_from_image(img);

		Ref<Theme> theme = EditorNode::get_singleton()->get_editor_theme();
		if (theme.is_valid()) {
			stop_icon = theme->get_icon(SNAME("Stop"), EditorStringName(EditorIcons));
			restart_icon = theme->get_icon(SNAME("Reload"), EditorStringName(EditorIcons));
		} else {
			stop_icon.instantiate();
			restart_icon.instantiate();
		}
	}
}

EditorExportPlatformWeb::~EditorExportPlatformWeb() {
}
