/**************************************************************************/
/*  project_creator.cpp                                                   */
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

#include "project_creator.h"

#include "core/config/project_settings.h"
#include "core/io/dir_access.h"
#include "core/io/file_access.h"
#include "core/os/os.h"
#include "editor/editor_vcs_interface.h"
#include "editor/themes/editor_icons.h"
#include "servers/display_server.h"

String ProjectCreator::default_name_from_path(const String &p_path) {
	String base = p_path.get_file().replace("_", " ").replace("-", " ");
	PackedStringArray parts = base.split(" ", false);
	for (int i = 0; i < parts.size(); i++) {
		parts.write[i] = parts[i].capitalize();
	}
	return String(" ").join(parts);
}

String ProjectCreator::normalize_renderer(const String &p_renderer, bool p_rd_supported, String *r_error) {
	if (p_renderer == "forward_plus" || p_renderer == "mobile" || p_renderer == "gl_compatibility") {
		if ((p_renderer == "forward_plus" || p_renderer == "mobile") && !p_rd_supported) {
			return "gl_compatibility";
		}
		return p_renderer;
	}

	if (r_error) {
		*r_error = "Invalid renderer type. Expected forward_plus, mobile, or gl_compatibility.";
	}
	return String();
}

static bool _is_directory_nonempty(const Ref<DirAccess> &p_d) {
	p_d->list_dir_begin();
	String n = p_d->get_next();
	while (!n.is_empty()) {
		if (n[0] != '.') {
			p_d->list_dir_end();
			return true;
		}
		n = p_d->get_next();
	}
	p_d->list_dir_end();
	return false;
}

Error ProjectCreator::create_project(const ProjectCreateOptions &p_options, String *r_error) {
	String path = p_options.path.simplify_path();

	if (path.is_relative_path()) {
		if (r_error) {
			*r_error = "The path specified is invalid.";
		}
		return ERR_INVALID_PARAMETER;
	}

	if (path.get_file() != OS::get_singleton()->get_safe_dir_name(path.get_file())) {
		if (r_error) {
			*r_error = "The directory name specified contains invalid characters or trailing whitespace.";
		}
		return ERR_INVALID_PARAMETER;
	}

	String working_dir = DirAccess::create(DirAccess::ACCESS_FILESYSTEM)->get_current_dir();
	String executable_dir = OS::get_singleton()->get_executable_path().get_base_dir();
	if (path == working_dir || path == executable_dir) {
		if (r_error) {
			*r_error = "Creating a project at the engine's working directory or executable directory is not allowed.";
		}
		return ERR_INVALID_PARAMETER;
	}

#ifdef WINDOWS_ENABLED
	String home_dir = OS::get_singleton()->get_environment("USERPROFILE");
#else
	String home_dir = OS::get_singleton()->get_environment("HOME");
#endif
	String documents_dir = OS::get_singleton()->get_system_dir(OS::SYSTEM_DIR_DOCUMENTS);
	if (path == home_dir || path == documents_dir) {
		if (r_error) {
			*r_error = "You cannot save a project at the selected path. Please create a subfolder or choose a new path.";
		}
		return ERR_INVALID_PARAMETER;
	}

	Ref<DirAccess> d = DirAccess::create(DirAccess::ACCESS_FILESYSTEM);

	if (d->file_exists(path.path_join("project.godot"))) {
		if (r_error) {
			*r_error = "A project already exists at the specified path.";
		}
		return ERR_ALREADY_EXISTS;
	}

	if (d->dir_exists(path)) {
		if (d->change_dir(path) != OK) {
			if (r_error) {
				*r_error = "Couldn't access project directory, check permissions.";
			}
			return ERR_CANT_OPEN;
		}
		if (_is_directory_nonempty(d) && !p_options.allow_nonempty) {
			if (r_error) {
				*r_error = "The selected path is not empty. Use --force to create a project anyway.";
			}
			return ERR_ALREADY_EXISTS;
		}
	} else {
		if (!d->dir_exists(path.get_base_dir())) {
			if (r_error) {
				*r_error = "The parent directory of the path specified doesn't exist.";
			}
			return ERR_INVALID_PARAMETER;
		}
		if (d->make_dir(path) != OK) {
			if (r_error) {
				*r_error = "Couldn't create project directory, check permissions.";
			}
			return ERR_CANT_CREATE;
		}
	}

	String project_name = p_options.name.strip_edges();
	if (project_name.is_empty()) {
		project_name = default_name_from_path(path);
	}

	bool rd_supported = DisplayServer::is_rendering_device_supported();
	String renderer_type = normalize_renderer(p_options.renderer, rd_supported, r_error);
	if (renderer_type.is_empty()) {
		return ERR_INVALID_PARAMETER;
	}

	PackedStringArray project_features = ProjectSettings::get_required_features();
	ProjectSettings::CustomMap initial_settings;
	initial_settings["rendering/renderer/rendering_method"] = renderer_type;

	if (renderer_type == "forward_plus") {
		project_features.push_back("Forward Plus");
	} else if (renderer_type == "mobile") {
		project_features.push_back("Mobile");
	} else if (renderer_type == "gl_compatibility") {
		project_features.push_back("GL Compatibility");
		initial_settings["rendering/renderer/rendering_method.mobile"] = "gl_compatibility";
	} else {
		WARN_PRINT("Unknown renderer type. Please report this as a bug on GitHub.");
	}

	project_features.sort();
	initial_settings["application/config/features"] = project_features;
	initial_settings["application/config/name"] = project_name;
	initial_settings["application/config/icon"] = "res://icon.svg";

	Error err = ProjectSettings::get_singleton()->save_custom(path.path_join("project.godot"), initial_settings, Vector<String>(), false);
	if (err != OK) {
		if (r_error) {
			*r_error = "Couldn't create project.godot in project path.";
		}
		return err;
	}

	Ref<FileAccess> fa_icon = FileAccess::open(path.path_join("icon.svg"), FileAccess::WRITE, &err);
	if (err != OK) {
		if (r_error) {
			*r_error = "Couldn't create icon.svg in project path.";
		}
		return err;
	}
	fa_icon->store_string(get_default_project_icon());

	String vcs_path = path;
	EditorVCSInterface::create_vcs_metadata_files(p_options.vcs, vcs_path);

	const String editor_config_path = path.path_join(".editorconfig");
	Ref<FileAccess> f = FileAccess::open(editor_config_path, FileAccess::WRITE);
	if (f.is_null()) {
		ERR_PRINT("Couldn't create .editorconfig in project path.");
	} else {
		f->store_line("root = true");
		f->store_line("");
		f->store_line("[*]");
		f->store_line("charset = utf-8");
		f->close();
		FileAccess::set_hidden_attribute(editor_config_path, true);
	}

	return OK;
}
