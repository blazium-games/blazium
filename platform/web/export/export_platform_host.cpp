/**************************************************************************/
/*  export_platform_host.cpp                                              */
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

#include "export_platform_host.h"

#include "portal_scripts.inc"

#include "core/config/project_settings.h"
#include "core/io/compression.h"
#include "core/io/dir_access.h"
#include "core/io/file_access.h"
#include "core/io/json.h"

#include "modules/modules_enabled.gen.h"

namespace {

String project_name() {
	String name = GLOBAL_GET("application/config/name");
	if (name.is_empty()) {
		name = "Blazium Game";
	}
	return name;
}

const char *script_body(const char *p_name) {
	for (int i = 0; i < portal_script_count; i++) {
		if (String::utf8(portal_script_names[i]) == String::utf8(p_name)) {
			return portal_script_bodies[i];
		}
	}
	return nullptr;
}

Error write_text(const String &p_path, const String &p_text) {
	Ref<FileAccess> file = FileAccess::open(p_path, FileAccess::WRITE);
	if (file.is_null()) {
		return ERR_FILE_CANT_WRITE;
	}
	file->store_string(p_text);
	return OK;
}

Error write_bytes(const String &p_path, const Vector<uint8_t> &p_bytes) {
	Ref<FileAccess> file = FileAccess::open(p_path, FileAccess::WRITE);
	if (file.is_null()) {
		return ERR_FILE_CANT_WRITE;
	}
	file->store_buffer(p_bytes.ptr(), p_bytes.size());
	return OK;
}

Error ensure_dir(const String &p_path) {
	Ref<DirAccess> dir = DirAccess::create(DirAccess::ACCESS_FILESYSTEM);
	if (dir.is_null()) {
		return ERR_CANT_CREATE;
	}
	return dir->make_dir_recursive(p_path);
}

String orientation_of(const Ref<EditorExportPreset> &p_preset) {
	String orientation = p_preset->get("export/orientation");
	if (orientation != "portrait" && orientation != "landscape") {
		orientation = "landscape";
	}
	return orientation;
}

void measure_tree(const String &p_dir, int64_t &r_total, int64_t &r_main) {
	Ref<DirAccess> dir = DirAccess::open(p_dir);
	if (dir.is_null()) {
		return;
	}
	dir->list_dir_begin();
	for (String name = dir->get_next(); !name.is_empty(); name = dir->get_next()) {
		if (name == "." || name == "..") {
			continue;
		}
		const String path = p_dir.path_join(name);
		if (dir->current_is_dir()) {
			measure_tree(path, r_total, r_main);
			continue;
		}
		Ref<FileAccess> file = FileAccess::open(path, FileAccess::READ);
		if (file.is_null()) {
			continue;
		}
		const int64_t size = file->get_length();
		r_total += size;
		const String normalized = path.replace("\\", "/");
		if (!normalized.contains("/engine/")) {
			r_main += size;
		}
		if (name.ends_with(".js")) {
			// Scanned by the caller when it needs the eval token check.
		}
	}
	dir->list_dir_end();
}

bool tree_contains_eval(const String &p_dir) {
	Ref<DirAccess> dir = DirAccess::open(p_dir);
	if (dir.is_null()) {
		return false;
	}
	dir->list_dir_begin();
	for (String name = dir->get_next(); !name.is_empty(); name = dir->get_next()) {
		if (name == "." || name == "..") {
			continue;
		}
		const String path = p_dir.path_join(name);
		if (dir->current_is_dir()) {
			if (tree_contains_eval(path)) {
				dir->list_dir_end();
				return true;
			}
			continue;
		}
		if (!name.ends_with(".js")) {
			continue;
		}
		Ref<FileAccess> file = FileAccess::open(path, FileAccess::READ);
		if (file.is_valid() && file->get_as_text().contains("eval(")) {
			dir->list_dir_end();
			return true;
		}
	}
	dir->list_dir_end();
	return false;
}

} // namespace

void EditorExportPlatformHost::configure(const String &p_name, const String &p_id, bool p_folder, bool p_index_html, bool p_reddit, bool p_boot) {
	platform_name = p_name;
	host_id = p_id;
	folder = p_folder;
	index_html = p_index_html;
	reddit_layout = p_reddit;
	boot_before_play = p_boot;
}

bool EditorExportPlatformHost::_export_extensions_enabled(const Ref<EditorExportPreset> &p_preset) const {
	(void)p_preset;
	return false;
}

bool EditorExportPlatformHost::_export_threads_enabled(const Ref<EditorExportPreset> &p_preset) const {
	(void)p_preset;
	return false;
}

bool EditorExportPlatformHost::_export_pwa_enabled(const Ref<EditorExportPreset> &p_preset) const {
	(void)p_preset;
	return false;
}

String EditorExportPlatformHost::_export_custom_html_shell(const Ref<EditorExportPreset> &p_preset) const {
	(void)p_preset;
	return String();
}

String EditorExportPlatformHost::_export_third_party_scripts(const Ref<EditorExportPreset> &p_preset, const String &p_name) const {
	(void)p_name;
	String html;
	if (host_id == "youtube") {
		html += "<script src=\"https://www.youtube.com/game_api/v1\"></script>\n";
	} else if (host_id == "telegram") {
		html += "<script src=\"https://telegram.org/js/telegram-web-app.js\"></script>\n";
	} else if (host_id == "poki") {
		html += "<script src=\"https://game-cdn.poki.com/scripts/v2/poki-sdk.js\"></script>\n";
	} else if (host_id == "meta") {
		html += "<script src=\"https://connect.facebook.net/en_US/fbinstant.6.3.js\"></script>\n";
	}
	html += "<script src=\"blazium-portals.js\"></script>\n";
	html += "<script src=\"blazium-portal-config.js\"></script>\n";
	if (host_id == "crazygames") {
		html += "<script src=\"https://sdk.crazygames.com/crazygames-sdk-v3.js\"></script>\n";
	}
	html += "<script src=\"blazium-" + host_id + ".js\"></script>\n";
	if (host_id == "gamedistribution") {
		html += "<script src=\"https://html5.api.gamedistribution.com/main.min.js\"></script>\n";
	}
	(void)p_preset;
	return html;
}

void EditorExportPlatformHost::get_preset_features(const Ref<EditorExportPreset> &p_preset, List<String> *r_features) const {
	EditorExportPlatformWeb::get_preset_features(p_preset, r_features);
	r_features->push_back(host_id);
	if (folder) {
		r_features->push_back("minigame");
	}
}

void EditorExportPlatformHost::get_platform_features(List<String> *r_features) const {
	EditorExportPlatformWeb::get_platform_features(r_features);
	if (folder) {
		r_features->push_back("minigame");
	}
}

void EditorExportPlatformHost::get_export_options(List<ExportOption> *r_options) const {
	EditorExportPlatformWeb::get_export_options(r_options);
	if (host_id == "discord") {
		r_options->push_back(ExportOption(PropertyInfo(Variant::STRING, "discord/client_id"), ""));
	} else if (host_id == "newgrounds") {
		r_options->push_back(ExportOption(PropertyInfo(Variant::STRING, "newgrounds/app_id"), ""));
		r_options->push_back(ExportOption(PropertyInfo(Variant::STRING, "newgrounds/encryption_key"), ""));
	} else if (host_id == "gamedistribution") {
		r_options->push_back(ExportOption(PropertyInfo(Variant::STRING, "gamedistribution/game_id"), ""));
	} else if (host_id == "wechat") {
		r_options->push_back(ExportOption(PropertyInfo(Variant::STRING, "wechat/app_id"), ""));
	} else if (host_id == "douyin") {
		r_options->push_back(ExportOption(PropertyInfo(Variant::STRING, "douyin/app_id"), ""));
	} else if (host_id == "tiktok") {
		r_options->push_back(ExportOption(PropertyInfo(Variant::STRING, "tiktok/client_key"), ""));
	}
	if (folder || host_id == "meta" || host_id == "crazygames") {
		r_options->push_back(ExportOption(PropertyInfo(Variant::STRING, "export/orientation", PROPERTY_HINT_ENUM, "landscape,portrait"), "landscape"));
	}
}

bool EditorExportPlatformHost::get_export_option_visibility(const EditorExportPreset *p_preset, const String &p_option) const {
	if (p_option == "variant/thread_support" || p_option == "variant/extensions_support" || p_option == "html/custom_html_shell") {
		return false;
	}
	if (p_option.begins_with("progressive_web_app/")) {
		return false;
	}
	if (p_option.begins_with("blazium/discord_embed") || p_option.begins_with("blazium/youtube_playable")) {
		return false;
	}
	return EditorExportPlatformWeb::get_export_option_visibility(p_preset, p_option);
}

String EditorExportPlatformHost::get_name() const {
	return platform_name;
}

String EditorExportPlatformHost::get_os_name() const {
	return platform_name;
}

bool EditorExportPlatformHost::has_valid_export_configuration(const Ref<EditorExportPreset> &p_preset, String &r_error, bool &r_missing_templates, bool p_debug) const {
#ifdef MODULE_MONO_ENABLED
	r_error += TTR("Exporting to Web is currently not supported in Blazium when using C#/.NET.") + "\n";
	r_error += TTR("If this project does not use C#, use a non-C# editor build to export the project.") + "\n";
	r_missing_templates = false;
	(void)p_preset;
	(void)p_debug;
	return false;
#else
	if (!folder) {
		return EditorExportPlatformWeb::has_valid_export_configuration(p_preset, r_error, r_missing_templates, p_debug);
	}
	String err;
	const String custom = p_debug ? String(p_preset->get("custom_template/debug")) : String(p_preset->get("custom_template/release"));
	bool valid = false;
	if (!custom.is_empty()) {
		valid = FileAccess::exists(custom);
		if (!valid) {
			err += TTR("Custom template not found.") + "\n";
		}
	} else {
		const bool debug_template = exists_export_template("blazium_host_debug.zip", &err);
		const bool release_template = exists_export_template("blazium_host_release.zip", &err);
		valid = p_debug ? debug_template : release_template;
	}
	r_missing_templates = !valid;
	if (!err.is_empty()) {
		r_error = err;
	}
	return valid;
#endif
}

bool EditorExportPlatformHost::has_valid_project_configuration(const Ref<EditorExportPreset> &p_preset, String &r_error) const {
	if (!EditorExportPlatformWeb::has_valid_project_configuration(p_preset, r_error)) {
		return false;
	}
	if (host_id == "discord" && String(p_preset->get("discord/client_id")).strip_edges().is_empty()) {
		r_error += TTR("Discord client id is required.") + "\n";
		return false;
	}
	if (host_id == "gamedistribution" && String(p_preset->get("gamedistribution/game_id")).strip_edges().is_empty()) {
		r_error += TTR("GameDistribution game id is required.") + "\n";
		return false;
	}
	if (host_id == "tiktok" && String(p_preset->get("tiktok/client_key")).strip_edges().is_empty()) {
		r_error += TTR("TikTok client key is required.") + "\n";
		return false;
	}
	return true;
}

List<String> EditorExportPlatformHost::get_binary_extensions(const Ref<EditorExportPreset> &p_preset) const {
	if (!folder) {
		return EditorExportPlatformWeb::get_binary_extensions(p_preset);
	}
	List<String> list;
	list.push_back("json");
	return list;
}

bool EditorExportPlatformHost::poll_export() {
	if (folder || reddit_layout) {
		return false;
	}
	return EditorExportPlatformWeb::poll_export();
}

int EditorExportPlatformHost::get_options_count() const {
	if (folder || reddit_layout) {
		return 0;
	}
	return EditorExportPlatformWeb::get_options_count();
}

Error EditorExportPlatformHost::run(const Ref<EditorExportPreset> &p_preset, int p_option, BitField<EditorExportPlatform::DebugFlags> p_debug_flags) {
	if (folder || reddit_layout) {
		return ERR_UNAVAILABLE;
	}
	return EditorExportPlatformWeb::run(p_preset, p_option, p_debug_flags);
}

Error EditorExportPlatformHost::export_project(const Ref<EditorExportPreset> &p_preset, bool p_debug, const String &p_path, BitField<EditorExportPlatform::DebugFlags> p_flags) {
	if (folder) {
		return export_folder(p_preset, p_debug, p_path, p_flags);
	}

	const bool preview = p_path.get_file().get_basename() == "tmp_js_export";
	String html_path = p_path;
	String output_root = p_path.get_base_dir();
	if (!preview && reddit_layout) {
		output_root = p_path.get_base_dir();
		Error dir_err = ensure_dir(output_root.path_join("public"));
		if (dir_err != OK) {
			add_message(EXPORT_MESSAGE_ERROR, TTR("Export"), TTR("Could not create the public directory."));
			return dir_err;
		}
		html_path = output_root.path_join("public/index.html");
	} else if (!preview && index_html) {
		html_path = output_root.path_join("index.html");
	}

	const String glue_dir = html_path.get_base_dir();
	Error glue_err = write_html_glue(p_preset, glue_dir);
	if (glue_err != OK) {
		return glue_err;
	}

	Error err = EditorExportPlatformWeb::export_project(p_preset, p_debug, html_path, p_flags);
	if (err != OK) {
		return err;
	}
	if (boot_before_play) {
		err = patch_starter(html_path);
		if (err != OK) {
			return err;
		}
	}
	if (host_id == "meta") {
		Dictionary instant;
		instant["platform_version"] = "RICH_GAMEPLAY";
		instant["orientation"] = orientation_of(p_preset).to_upper();
		instant["navigation_menu_version"] = "NAV_FLOATING";
		Dictionary config;
		config["instant_games"] = instant;
		err = write_text(glue_dir.path_join("fbapp-config.json"), JSON::stringify(config, "  "));
		if (err != OK) {
			add_message(EXPORT_MESSAGE_ERROR, TTR("Export"), TTR("Could not write fbapp-config.json."));
			return err;
		}
	}
	if (!preview && reddit_layout) {
		Dictionary devvit;
		devvit["name"] = project_name();
		devvit["version"] = "1.0.0";
		Dictionary webview;
		webview["entry"] = "public/index.html";
		devvit["webView"] = webview;
		err = write_text(output_root.path_join("devvit.json"), JSON::stringify(devvit, "  "));
		if (err != OK) {
			add_message(EXPORT_MESSAGE_ERROR, TTR("Export"), TTR("Could not write devvit.json."));
			return err;
		}
	}
	return OK;
}

Error EditorExportPlatformHost::write_html_glue(const Ref<EditorExportPreset> &p_preset, const String &p_dir) {
	const char *shared[] = { "blazium-portals.js", nullptr };
	for (int i = 0; shared[i] != nullptr; i++) {
		const char *body = script_body(shared[i]);
		if (body == nullptr) {
			add_message(EXPORT_MESSAGE_ERROR, TTR("Export"), TTR("Missing portal script."));
			return ERR_FILE_NOT_FOUND;
		}
		Error err = write_text(p_dir.path_join(String::utf8(shared[i])), String::utf8(body));
		if (err != OK) {
			return err;
		}
	}
	const String host_script = "blazium-" + host_id + ".js";
	const CharString host_utf8 = host_script.utf8();
	const char *host_body = script_body(host_utf8.get_data());
	if (host_body == nullptr) {
		add_message(EXPORT_MESSAGE_ERROR, TTR("Export"), vformat(TTR("Missing portal script %s."), host_script));
		return ERR_FILE_NOT_FOUND;
	}
	Error err = write_text(p_dir.path_join(host_script), String::utf8(host_body));
	if (err != OK) {
		return err;
	}

	Dictionary config;
	config["host"] = host_id;
	if (host_id == "discord") {
		config["clientId"] = String(p_preset->get("discord/client_id"));
	} else if (host_id == "newgrounds") {
		config["appId"] = String(p_preset->get("newgrounds/app_id"));
		config["encryptionKey"] = String(p_preset->get("newgrounds/encryption_key"));
	} else if (host_id == "gamedistribution") {
		config["gameId"] = String(p_preset->get("gamedistribution/game_id"));
	}
	return write_text(p_dir.path_join("blazium-portal-config.js"), "window.BlaziumPortalConfig = " + JSON::stringify(config) + ";\n");
}

Error EditorExportPlatformHost::patch_starter(const String &p_html_path) {
	const String starter_path = p_html_path.get_basename() + ".engine.starter.js";
	Ref<FileAccess> file = FileAccess::open(starter_path, FileAccess::READ);
	if (file.is_null()) {
		add_message(EXPORT_MESSAGE_ERROR, TTR("Export"), TTR("Could not read the engine starter."));
		return ERR_FILE_CANT_READ;
	}
	String text = file->get_as_text();
	file.unref();
	if (!text.contains("engine.startGame({")) {
		add_message(EXPORT_MESSAGE_ERROR, TTR("Export"), TTR("The engine starter is missing its start hook."));
		return ERR_FILE_CORRUPT;
	}
	text = text.replace_first("engine.startGame({", "BlaziumPortals.boot().then(function () {\n\t\treturn engine.startGame({");
	text = text.replace_first("'onProgress': function (current, total) {", "'onProgress': function (current, total) {\n\t\t\t\tif (window.BlaziumPortals) { BlaziumPortals.reportProgress(current, total); }");
	text = text.replace_first("}, displayFailureNotice);", "}, displayFailureNotice);\n\t\t});");
	return write_text(starter_path, text);
}

Error EditorExportPlatformHost::export_folder(const Ref<EditorExportPreset> &p_preset, bool p_debug, const String &p_path, BitField<EditorExportPlatform::DebugFlags> p_flags) {
	ExportNotifier notifier(*this, p_preset, p_debug, p_path, p_flags);
	const String root = p_path.get_base_dir();
	if (!DirAccess::exists(root)) {
		add_message(EXPORT_MESSAGE_ERROR, TTR("Export"), vformat(TTR("Target folder does not exist or is inaccessible: \"%s\""), root));
		return ERR_FILE_BAD_PATH;
	}
	Error err = ensure_dir(root.path_join("engine"));
	if (err != OK) {
		return err;
	}
	err = ensure_dir(root.path_join("js/libs"));
	if (err != OK) {
		return err;
	}

	String template_path = p_debug ? String(p_preset->get("custom_template/debug")) : String(p_preset->get("custom_template/release"));
	template_path = template_path.strip_edges();
	if (template_path.is_empty()) {
		template_path = find_export_template(p_debug ? "blazium_host_debug.zip" : "blazium_host_release.zip");
	}
	if (template_path.is_empty() || !FileAccess::exists(template_path)) {
		add_message(EXPORT_MESSAGE_ERROR, TTR("Prepare Templates"), TTR("The mini-game host export template was not found. Build it with blazium_host=yes."));
		return ERR_FILE_NOT_FOUND;
	}

	Vector<uint8_t> wasm;
	err = read_template_file(template_path, "blazium.wasm", wasm);
	if (err != OK || wasm.is_empty()) {
		add_message(EXPORT_MESSAGE_ERROR, TTR("Prepare Templates"), TTR("The host template is missing blazium.wasm."));
		return ERR_FILE_CORRUPT;
	}
	Vector<uint8_t> compressed;
	compressed.resize(Compression::get_max_compressed_buffer_size(wasm.size(), Compression::MODE_BROTLI));
	const int compressed_size = Compression::compress(compressed.ptrw(), wasm.ptr(), wasm.size(), Compression::MODE_BROTLI);
	if (compressed_size <= 0) {
		add_message(EXPORT_MESSAGE_ERROR, TTR("Export"), TTR("Could not compress the WebAssembly module."));
		return FAILED;
	}
	compressed.resize(compressed_size);
	err = write_bytes(root.path_join("engine/blazium.wasm.br"), compressed);
	if (err != OK) {
		return err;
	}

	Vector<uint8_t> javascript;
	err = read_template_file(template_path, "blazium.js", javascript);
	if (err != OK || javascript.is_empty()) {
		add_message(EXPORT_MESSAGE_ERROR, TTR("Prepare Templates"), TTR("The host template is missing blazium.js."));
		return ERR_FILE_CORRUPT;
	}
	err = write_bytes(root.path_join("engine/blazium.js"), javascript);
	if (err != OK) {
		return err;
	}
	const char *worklets[] = { "blazium.audio.worklet.js", "blazium.audio.position.worklet.js" };
	for (int i = 0; i < 2; i++) {
		Vector<uint8_t> worklet;
		if (read_template_file(template_path, worklets[i], worklet) == OK && !worklet.is_empty()) {
			write_bytes(root.path_join(String("engine/") + worklets[i]), worklet);
		}
	}

	Vector<SharedObject> shared_objects;
	err = save_pack(p_preset, p_debug, root.path_join("engine/blazium.zip"), &shared_objects);
	if (err != OK) {
		add_message(EXPORT_MESSAGE_ERROR, TTR("Export"), TTR("Could not write the game pack."));
		return err;
	}

	const char *runtime[][2] = {
		{ "blazium_adapter.js", "js/blazium_adapter.js" },
		{ "blazium_fetch.js", "js/blazium_fetch.js" },
		{ "blazium_runtime.js", "js/blazium_runtime.js" },
		{ "blazium_loader.js", "js/blazium_loader.js" },
		{ "blazium_host.js", "js/libs/blazium_host.js" },
	};
	for (int i = 0; i < 5; i++) {
		const char *body = script_body(runtime[i][0]);
		if (body == nullptr) {
			add_message(EXPORT_MESSAGE_ERROR, TTR("Export"), TTR("Missing host runtime script."));
			return ERR_FILE_NOT_FOUND;
		}
		err = write_text(root.path_join(runtime[i][1]), String::utf8(body));
		if (err != OK) {
			return err;
		}
	}

	const String game_js = String("require(\"./js/blazium_adapter.js\");\n") +
			"require(\"./js/blazium_fetch.js\");\n" +
			"require(\"./js/blazium_runtime.js\");\n" +
			"require(\"./js/libs/blazium_host.js\");\n" +
			"require(\"./js/blazium_loader.js\");\n" +
			"BlaziumHostLoader.start({ provider: \"" + host_id + "\" });\n";
	err = write_text(root.path_join("game.js"), game_js);
	if (err != OK) {
		return err;
	}

	if (host_id == "tiktok") {
		const char *entry = script_body("tiktok_entry.js");
		const String comment = entry ? String::utf8(entry) : String("/* Blazium host entry. */\n");
		err = ensure_dir(root.path_join("subpacks"));
		if (err != OK) {
			return err;
		}
		err = write_text(root.path_join("engine/game.js"), comment);
		if (err != OK) {
			return err;
		}
		err = write_text(root.path_join("subpacks/game.js"), comment);
		if (err != OK) {
			return err;
		}
	}

	const String orientation = orientation_of(p_preset);
	Dictionary game_json;
	game_json["deviceOrientation"] = orientation;
	Array subpackages;
	Dictionary engine_pack;
	engine_pack["name"] = "engine";
	engine_pack["root"] = "engine/";
	subpackages.push_back(engine_pack);
	if (host_id == "douyin") {
		game_json["subPackages"] = subpackages;
	} else {
		game_json["subpackages"] = subpackages;
	}
	if (host_id == "wechat") {
		game_json["iOSHighPerformance"] = true;
		game_json["workers"] = "workers";
		err = ensure_dir(root.path_join("workers"));
		if (err != OK) {
			return err;
		}
		err = write_text(root.path_join("workers/index.js"), "/* Blazium host worker slot. */\n");
		if (err != OK) {
			return err;
		}
	}
	err = write_text(p_path.get_extension() == "json" ? p_path : root.path_join("game.json"), JSON::stringify(game_json, "  "));
	if (err != OK) {
		return err;
	}

	String app_id;
	if (host_id == "wechat") {
		app_id = p_preset->get("wechat/app_id");
	} else if (host_id == "douyin") {
		app_id = p_preset->get("douyin/app_id");
	} else if (host_id == "tiktok") {
		app_id = p_preset->get("tiktok/client_key");
	}
	Dictionary project_config;
	project_config["description"] = project_name();
	project_config["appid"] = app_id;
	project_config["projectname"] = project_name();
	project_config["compileType"] = "game";
	Dictionary setting;
	setting["urlCheck"] = false;
	setting["es6"] = true;
	setting["minified"] = false;
	project_config["setting"] = setting;
	err = write_text(root.path_join("project.config.json"), JSON::stringify(project_config, "  "));
	if (err != OK) {
		return err;
	}
	if (host_id == "wechat") {
		Dictionary private_config;
		private_config["libVersion"] = "game";
		private_config["projectname"] = project_name();
		private_config["setting"] = setting;
		err = write_text(root.path_join("project.private.config.json"), JSON::stringify(private_config, "  "));
		if (err != OK) {
			return err;
		}
	}

	Dictionary manifest;
	manifest["format"] = 1;
	manifest["host"] = host_id;
	manifest["executable"] = "engine/blazium";
	manifest["script"] = "engine/blazium.js";
	manifest["wasm"] = "engine/blazium.wasm.br";
	manifest["pack"] = "engine/blazium.zip";
	manifest["bridge"] = "blazium-host-bridge";
	manifest["abi"] = 1;
	err = write_text(root.path_join("blazium-export.json"), JSON::stringify(manifest, "  "));
	if (err != OK) {
		return err;
	}

	if (tree_contains_eval(root)) {
		add_message(EXPORT_MESSAGE_ERROR, TTR("Export"), TTR("The host package contains the eval( token."));
		return FAILED;
	}
	add_message(EXPORT_MESSAGE_WARNING, TTR("Export"), TTR("Mini-game hosts draw fonts that are packed in the game. System fonts are not available."));
	if (host_id == "tiktok") {
		add_message(EXPORT_MESSAGE_WARNING, TTR("Export"), TTR("TikTok clients before 43.4.0 cannot run this package."));
		int64_t total = 0;
		int64_t main_size = 0;
		measure_tree(root, total, main_size);
		if (main_size > 4 * 1024 * 1024) {
			add_message(EXPORT_MESSAGE_WARNING, TTR("Export"), TTR("The main package is larger than 4 MB."));
		}
		if (total > 30 * 1024 * 1024) {
			add_message(EXPORT_MESSAGE_WARNING, TTR("Export"), TTR("The package is larger than 30 MB."));
		}
	}
	return OK;
}
