/**************************************************************************/
/*  windows_inter_dvd_export_platform.cpp                                 */
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

#include "windows_inter_dvd_export_platform.h"

#include "core/config/project_settings.h"
#include "core/io/dir_access.h"
#include "core/io/file_access.h"
#include "core/io/image.h"
#include "core/io/resource_loader.h"
#include "core/io/zip_io.h"
#include "core/os/os.h"
#include "editor/editor_node.h"
#include "editor/editor_settings.h"
#include "editor/editor_string_names.h"
#include "modules/inter_dvd/author/inter_dvd_ifo_writer.h"
#include "modules/inter_dvd/author/inter_dvd_project.h"
#include "modules/inter_dvd/author/inter_dvd_vob_mux.h"
#include "scene/resources/image_texture.h"

namespace {
int count_export_steps(const Ref<InterDVDProject> &p_project, int p_kind) {
	int n = 3;
	if (p_project.is_valid()) {
		n += p_project->get_menus().size();
		const TypedArray<InterDVDPGC> titles = p_project->get_titles();
		for (int i = 0; i < titles.size(); i++) {
			const Ref<InterDVDPGC> title = titles[i];
			const int cells = title.is_valid() ? title->get_cells().size() : 0;
			n += cells > 0 ? cells : 1;
		}
	} else {
		n++;
	}
	if (p_kind == 1 || p_kind == 2) {
		n++;
	}
	return MAX(n, 1);
}
} //namespace

EditorExportPlatformWindowsInterDVD::EditorExportPlatformWindowsInterDVD() {
	if (EditorNode::get_singleton()) {
		Ref<Image> img = Image::create_empty(16, 16, false, Image::FORMAT_RGBA8);
		img->fill(Color(0.15, 0.2, 0.55));
		logo = ImageTexture::create_from_image(img);
	}
}

void EditorExportPlatformWindowsInterDVD::get_preset_features(const Ref<EditorExportPreset> &p_preset, List<String> *r_features) const {
	(void)p_preset;
	r_features->push_back("inter_dvd");
}

void EditorExportPlatformWindowsInterDVD::get_platform_features(List<String> *r_features) const {
	r_features->push_back("windows");
	r_features->push_back("inter_dvd");
}

void EditorExportPlatformWindowsInterDVD::get_export_options(List<ExportOption> *r_options) const {
	const int region = InterDVDSettings::setting_int("blazium/inter_dvd/default_region_mask", 1);
	const int parental = InterDVDSettings::setting_int("blazium/inter_dvd/default_parental_level", 1);
	r_options->push_back(ExportOption(PropertyInfo(Variant::INT, "inter_dvd/output_kind", PROPERTY_HINT_ENUM, "Folder,Zip,ISO"), 0));
	r_options->push_back(ExportOption(PropertyInfo(Variant::INT, "inter_dvd/region_mask", PROPERTY_HINT_RANGE, "1,255,1"), region));
	r_options->push_back(ExportOption(PropertyInfo(Variant::INT, "inter_dvd/parental_level", PROPERTY_HINT_RANGE, "1,8,1"), parental));
	r_options->push_back(ExportOption(PropertyInfo(Variant::BOOL, "inter_dvd/auto_find_ffmpeg"), true));
	r_options->push_back(ExportOption(PropertyInfo(Variant::STRING, "inter_dvd/ffmpeg", PROPERTY_HINT_GLOBAL_FILE), ""));
	r_options->push_back(ExportOption(PropertyInfo(Variant::STRING, "inter_dvd/iso_tool", PROPERTY_HINT_GLOBAL_FILE), ""));
	r_options->push_back(ExportOption(PropertyInfo(Variant::BOOL, "inter_dvd/allow_dummy_vob"), false));
	r_options->push_back(ExportOption(PropertyInfo(Variant::STRING, "inter_dvd/volume_id"), "BLAZIUM_DVD"));
	r_options->push_back(ExportOption(PropertyInfo(Variant::STRING, "inter_dvd/menu_language"), "en"));
	r_options->push_back(ExportOption(PropertyInfo(Variant::STRING, "inter_dvd/audio_language"), "en"));
	r_options->push_back(ExportOption(PropertyInfo(Variant::STRING, "inter_dvd/subtitle_language"), "en"));
	r_options->push_back(ExportOption(PropertyInfo(Variant::STRING, "inter_dvd/provider_id"), "BLAZIUM INTER-DVD"));
}

List<String> EditorExportPlatformWindowsInterDVD::get_binary_extensions(const Ref<EditorExportPreset> &p_preset) const {
	List<String> list;
	const int kind = p_preset.is_valid() ? int(p_preset->get("inter_dvd/output_kind")) : 0;
	if (kind == 1) {
		list.push_back("zip");
	} else if (kind == 2) {
		list.push_back("iso");
	} else {
		list.push_back("zip");
		list.push_back("iso");
	}
	return list;
}

bool EditorExportPlatformWindowsInterDVD::has_valid_export_configuration(const Ref<EditorExportPreset> &p_preset, String &r_error, bool &r_missing_templates, bool p_debug) const {
	(void)p_debug;
	r_missing_templates = false;
	String err;
	if (int(p_preset->get("inter_dvd/output_kind")) == 2) {
		String tool = p_preset->get("inter_dvd/iso_tool");
		if (tool.is_empty() && EditorSettings::get_singleton()) {
			tool = EDITOR_GET("export/inter_dvd/iso_tool");
		}
		if (tool.is_empty() || !FileAccess::exists(tool)) {
			err += TTR("ISO export needs oscdimg or mkisofs configured (export/inter_dvd/iso_tool).") + "\n";
		}
	}
	if (!err.is_empty()) {
		r_error = err;
		return false;
	}
	return true;
}

bool EditorExportPlatformWindowsInterDVD::has_valid_project_configuration(const Ref<EditorExportPreset> &p_preset, String &r_error) const {
	(void)p_preset;
	const String path = GLOBAL_GET("blazium/inter_dvd/project");
	if (path.is_empty()) {
		r_error = TTR("Set blazium/inter_dvd/project to an InterDVDProject resource.");
		return false;
	}
	return true;
}

Error EditorExportPlatformWindowsInterDVD::export_project(const Ref<EditorExportPreset> &p_preset, bool p_debug, const String &p_path, BitField<EditorExportPlatform::DebugFlags> p_flags) {
	ExportNotifier notifier(*this, p_preset, p_debug, p_path, p_flags);

	const String project_path = GLOBAL_GET("blazium/inter_dvd/project");
	Ref<InterDVDProject> project;
	if (!project_path.is_empty()) {
		project = ResourceLoader::load(project_path);
	}
	if (project.is_null()) {
		project.instantiate();
	}
	if (p_preset.is_valid()) {
		project->set_region_mask(p_preset->get("inter_dvd/region_mask"));
		project->set_parental_level(p_preset->get("inter_dvd/parental_level"));
		const String volume = p_preset->get("inter_dvd/volume_id");
		if (!volume.is_empty()) {
			project->set_volume_id(volume);
		}
		const String menu_lang = p_preset->get("inter_dvd/menu_language");
		if (!menu_lang.is_empty()) {
			project->set_menu_language(menu_lang);
		}
		const String audio_lang = p_preset->get("inter_dvd/audio_language");
		if (!audio_lang.is_empty()) {
			project->set_audio_language(audio_lang);
		}
		const String sub_lang = p_preset->get("inter_dvd/subtitle_language");
		if (!sub_lang.is_empty()) {
			project->set_subtitle_language(sub_lang);
		}
		const String provider = p_preset->get("inter_dvd/provider_id");
		if (!provider.is_empty()) {
			project->set_provider_id(provider);
		}
	}

	String ffmpeg = p_preset->get("inter_dvd/ffmpeg");
	if (ffmpeg.is_empty() && EditorSettings::get_singleton()) {
		ffmpeg = EDITOR_GET("export/inter_dvd/ffmpeg");
	}
	const bool auto_find = p_preset.is_valid() && bool(p_preset->get("inter_dvd/auto_find_ffmpeg"));
	String iso_tool = p_preset->get("inter_dvd/iso_tool");
	if (iso_tool.is_empty() && EditorSettings::get_singleton()) {
		iso_tool = EDITOR_GET("export/inter_dvd/iso_tool");
	}

	const int kind = p_preset->get("inter_dvd/output_kind");
	String work_dir = p_path;
	if (kind != 0) {
		work_dir = p_path.get_base_dir().path_join(p_path.get_file().get_basename() + "_disc");
	} else if (p_path.get_extension() != "") {
		work_dir = p_path.get_base_dir();
	}

	Ref<DirAccess> da = DirAccess::create(DirAccess::ACCESS_FILESYSTEM);
	da->make_dir_recursive(work_dir);

	const bool allow_dummy = p_preset.is_valid() && bool(p_preset->get("inter_dvd/allow_dummy_vob"));
	Ref<InterDVDExportProgress> progress;
	progress.instantiate();
	const int steps = count_export_steps(project, kind);
	progress->begin(steps);
	EditorProgress ep("inter_dvd", TTR("Interactive DVD"), steps);
	String write_error;
	Error err = InterDVDIfoWriter::write_video_ts(work_dir, project, allow_dummy, ffmpeg, &write_error, auto_find, progress);
	if (err != OK) {
		add_message(EXPORT_MESSAGE_ERROR, TTR("Interactive DVD"), write_error.is_empty() ? TTR("Failed to write VIDEO_TS.") : write_error);
		return err;
	}

	if (kind == 1) {
		progress->report("Creating ZIP");
		if (FileAccess::exists(p_path)) {
			OS::get_singleton()->move_to_trash(p_path);
		}
		Ref<FileAccess> io_fa;
		zlib_filefunc_def io = zipio_create_io(&io_fa);
		zipFile zip = zipOpen2(p_path.utf8().get_data(), APPEND_STATUS_CREATE, nullptr, &io);
		zip_folder_recursive(zip, work_dir, "", "VIDEO_TS");
		zipClose(zip, nullptr);
		if (da->change_dir(work_dir) == OK) {
			da->erase_contents_recursive();
		}
	} else if (kind == 2) {
		if (iso_tool.is_empty()) {
			add_message(EXPORT_MESSAGE_ERROR, TTR("Interactive DVD"), TTR("ISO tool (oscdimg or mkisofs) is not configured."));
			return ERR_UNCONFIGURED;
		}
		progress->report("Creating ISO");
		const Vector<String> iso_args = InterDVDIfoWriter::iso_tool_args(iso_tool, project->get_volume_id(), work_dir, p_path);
		List<String> args;
		for (int i = 0; i < iso_args.size(); i++) {
			args.push_back(iso_args[i]);
		}
		String pipe;
		const int code = OS::get_singleton()->execute(iso_tool, args, &pipe);
		if (code != 0) {
			add_message(EXPORT_MESSAGE_ERROR, TTR("Interactive DVD"), vformat(TTR("ISO tool failed (%d): %s"), code, pipe));
			return FAILED;
		}
	}

	return OK;
}

#endif
