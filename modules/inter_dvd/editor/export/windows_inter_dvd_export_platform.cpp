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
#include "modules/inter_dvd/editor/inter_dvd_toolchain.h"
#include "modules/inter_dvd/scene/inter_dvd_disc.h"
#include "scene/main/node.h"
#include "scene/resources/image_texture.h"
#include "scene/resources/packed_scene.h"

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

Ref<InterDVDProject> project_from_node_tree(Node *p_root) {
	InterDVDDisc *disc = InterDVDDisc::find_in_tree(p_root);
	if (!disc) {
		return Ref<InterDVDProject>();
	}
	return disc->build_project();
}

Ref<InterDVDProject> project_from_scene_file(const String &p_path) {
	if (p_path.is_empty()) {
		return Ref<InterDVDProject>();
	}
	Ref<PackedScene> packed = ResourceLoader::load(p_path, "PackedScene");
	if (packed.is_null()) {
		return Ref<InterDVDProject>();
	}
	Node *root = packed->instantiate();
	if (!root) {
		return Ref<InterDVDProject>();
	}
	const Ref<InterDVDProject> project = project_from_node_tree(root);
	memdelete(root);
	return project;
}

Ref<InterDVDProject> resolve_authoring_project() {
	if (EditorNode::get_singleton() && EditorNode::get_singleton()->get_edited_scene()) {
		const Ref<InterDVDProject> from_edited = project_from_node_tree(EditorNode::get_singleton()->get_edited_scene());
		if (from_edited.is_valid() && from_edited->get_titles().size() + from_edited->get_menus().size() > 0) {
			return from_edited;
		}
	}
	const String main_scene = GLOBAL_GET("application/run/main_scene");
	const Ref<InterDVDProject> from_main = project_from_scene_file(main_scene);
	if (from_main.is_valid()) {
		return from_main;
	}
	const String project_path = GLOBAL_GET("blazium/inter_dvd/project");
	if (!project_path.is_empty()) {
		return ResourceLoader::load(project_path);
	}
	return Ref<InterDVDProject>();
}

bool has_authoring_source() {
	if (EditorNode::get_singleton() && InterDVDDisc::find_in_tree(EditorNode::get_singleton()->get_edited_scene())) {
		return true;
	}
	const String main_scene = GLOBAL_GET("application/run/main_scene");
	if (!main_scene.is_empty()) {
		Ref<PackedScene> packed = ResourceLoader::load(main_scene, "PackedScene");
		if (packed.is_valid()) {
			Node *root = packed->instantiate();
			const bool found = InterDVDDisc::find_in_tree(root) != nullptr;
			if (root) {
				memdelete(root);
			}
			if (found) {
				return true;
			}
		}
	}
	const String project_path = GLOBAL_GET("blazium/inter_dvd/project");
	if (project_path.is_empty()) {
		return false;
	}
	return ResourceLoader::load(project_path, "InterDVDProject").is_valid();
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
	r_options->push_back(ExportOption(PropertyInfo(Variant::INT, "inter_dvd/output_kind", PROPERTY_HINT_ENUM, "Folder,Zip,ISO"), 0));
	r_options->push_back(ExportOption(PropertyInfo(Variant::STRING, "inter_dvd/toolchain", PROPERTY_HINT_GLOBAL_FILE, "*.exe"), ""));
	r_options->push_back(ExportOption(PropertyInfo(Variant::BOOL, "inter_dvd/allow_dummy_vob"), false));
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
		String tool = p_preset->get("inter_dvd/toolchain");
		if (tool.is_empty()) {
			tool = InterDVDToolchain::discover_binary();
		}
		if (tool.is_empty() || !FileAccess::exists(tool)) {
			err += TTR("ISO export needs blazium-toolchain (export/inter_dvd/toolchain, BLAZIUM_TOOLCHAIN, or PATH).") + "\n";
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
	if (has_authoring_source()) {
		return true;
	}
	r_error = TTR("Add an InterDVDDisc to the scene, or set blazium/inter_dvd/project to an InterDVDProject resource.");
	return false;
}

Error EditorExportPlatformWindowsInterDVD::export_project(const Ref<EditorExportPreset> &p_preset, bool p_debug, const String &p_path, BitField<EditorExportPlatform::DebugFlags> p_flags) {
	ExportNotifier notifier(*this, p_preset, p_debug, p_path, p_flags);

	Ref<InterDVDProject> project = resolve_authoring_project();
	if (project.is_null()) {
		add_message(EXPORT_MESSAGE_ERROR, TTR("Interactive DVD"), TTR("No InterDVDDisc graph and no valid InterDVDProject resource."));
		return ERR_INVALID_DATA;
	}

	if (p_preset.is_valid()) {
		const String toolchain = p_preset->get("inter_dvd/toolchain");
		if (!toolchain.is_empty() && EditorSettings::get_singleton()) {
			EditorSettings::get_singleton()->set("export/inter_dvd/toolchain", toolchain);
		}
	}
	const String ffmpeg;
	const bool auto_find = true;

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
		zip_folder_recursive(zip, work_dir, "", "");
		zipClose(zip, nullptr);
		if (da->change_dir(work_dir) == OK) {
			da->erase_contents_recursive();
		}
	} else if (kind == 2) {
		if (InterDVDToolchain::discover_binary().is_empty()) {
			add_message(EXPORT_MESSAGE_ERROR, TTR("Interactive DVD"), TTR("blazium-toolchain is not configured."));
			return ERR_UNCONFIGURED;
		}
		progress->report("Creating ISO");
		const String meta_path = work_dir.path_join("disc.interdvd.json");
		String pipe;
		const Error iso_err = InterDVDToolchain::write_iso(work_dir, p_path, meta_path, &pipe);
		if (iso_err != OK) {
			add_message(EXPORT_MESSAGE_ERROR, TTR("Interactive DVD"), pipe.is_empty() ? TTR("Toolchain ISO export failed.") : pipe);
			return iso_err;
		}
	}

	return OK;
}

#endif
