/**************************************************************************/
/*  microsoft_game_config.cpp                                             */
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

#include "microsoft_game_config.h"

#include "core/config/project_settings.h"
#include "core/io/dir_access.h"
#include "core/io/file_access.h"

namespace {

const char *CONFIG_FILENAME = "MicrosoftGame.config";

PackedStringArray _logo_attributes() {
	PackedStringArray attrs;
	attrs.push_back("StoreLogo");
	attrs.push_back("Square150x150Logo");
	attrs.push_back("Square44x44Logo");
	attrs.push_back("Square480x480Logo");
	attrs.push_back("SplashScreenImage");
	return attrs;
}

String _read_attr_value(const String &p_content, const String &p_attr) {
	const String pattern = vformat("%s=\"", p_attr);
	int pos = p_content.find(pattern);
	if (pos == -1) {
		return String();
	}
	pos += pattern.length();
	int end = p_content.find("\"", pos);
	if (end == -1) {
		return String();
	}
	return p_content.substr(pos, end - pos);
}

} //namespace

String MicrosoftGameConfig::get_project_config_path() {
	ProjectSettings *settings = ProjectSettings::get_singleton();
	ERR_FAIL_NULL_V(settings, String());
	return settings->globalize_path("res://").path_join(CONFIG_FILENAME);
}

bool MicrosoftGameConfig::project_config_exists() {
	return FileAccess::exists(get_project_config_path());
}

String MicrosoftGameConfig::read_executable_name_from_content(const String &p_content) {
	int pos = p_content.find("<Executable");
	if (pos == -1) {
		return String();
	}
	int name_pos = p_content.find("Name=\"", pos);
	if (name_pos == -1) {
		return String();
	}
	name_pos += 6;
	int end = p_content.find("\"", name_pos);
	if (end == -1) {
		return String();
	}
	return p_content.substr(name_pos, end - name_pos);
}

String MicrosoftGameConfig::read_executable_name(const String &p_config_path) {
	String path = p_config_path.is_empty() ? get_project_config_path() : p_config_path;
	if (!FileAccess::exists(path)) {
		return String();
	}
	Ref<FileAccess> file = FileAccess::open(path, FileAccess::READ);
	ERR_FAIL_COND_V(file.is_null(), String());
	return read_executable_name_from_content(file->get_as_text());
}

String MicrosoftGameConfig::inject_target_device_family(const String &p_content, const String &p_family) {
	if (p_content.find("TargetDeviceFamily=") != -1) {
		return p_content;
	}

	int pos = p_content.find("<Executable");
	if (pos == -1) {
		return p_content;
	}
	int end = p_content.find("/>", pos);
	bool self_closing = end != -1;
	if (!self_closing) {
		end = p_content.find(">", pos);
	}
	if (end == -1) {
		return p_content;
	}

	String tag = p_content.substr(pos, end - pos + (self_closing ? 2 : 1));
	String patched = tag;
	if (self_closing) {
		patched = tag.substr(0, tag.length() - 2) + vformat(" TargetDeviceFamily=\"%s\" />", p_family);
	} else {
		patched = tag.substr(0, tag.length() - 1) + vformat(" TargetDeviceFamily=\"%s\">", p_family);
	}

	return p_content.substr(0, pos) + patched + p_content.substr(end + (self_closing ? 2 : 1));
}

Error MicrosoftGameConfig::copy_to_staging(const String &p_staging_dir, const String &p_family) {
	const String src = get_project_config_path();
	if (!FileAccess::exists(src)) {
		return ERR_FILE_NOT_FOUND;
	}

	Ref<FileAccess> in = FileAccess::open(src, FileAccess::READ);
	ERR_FAIL_COND_V(in.is_null(), ERR_FILE_CANT_READ);

	String content = inject_target_device_family(in->get_as_text(), p_family);
	const String dst = p_staging_dir.path_join(CONFIG_FILENAME);
	Ref<FileAccess> out = FileAccess::open(dst, FileAccess::WRITE);
	ERR_FAIL_COND_V(out.is_null(), ERR_FILE_CANT_WRITE);
	out->store_string(content);
	return OK;
}

Error MicrosoftGameConfig::stage_logos(const String &p_staging_dir) {
	const String config_path = p_staging_dir.path_join(CONFIG_FILENAME);
	if (!FileAccess::exists(config_path)) {
		return ERR_FILE_NOT_FOUND;
	}

	Ref<FileAccess> file = FileAccess::open(config_path, FileAccess::READ);
	ERR_FAIL_COND_V(file.is_null(), ERR_FILE_CANT_READ);
	const String content = file->get_as_text();

	ProjectSettings *settings = ProjectSettings::get_singleton();
	ERR_FAIL_NULL_V(settings, ERR_UNAVAILABLE);
	const String project_dir = settings->globalize_path("res://");

	for (const String &attr : _logo_attributes()) {
		const String rel = _read_attr_value(content, attr);
		if (rel.is_empty()) {
			continue;
		}

		const String filename = rel.get_file();
		Vector<String> candidates;
		candidates.push_back(project_dir.path_join(rel));
		candidates.push_back(project_dir.path_join("storelogos").path_join(filename));
		candidates.push_back(project_dir.path_join(filename));

		String src;
		for (const String &candidate : candidates) {
			if (FileAccess::exists(candidate)) {
				src = candidate;
				break;
			}
		}
		if (src.is_empty()) {
			WARN_PRINT(vformat("xbox_module: %s logo not found for %s", attr, rel));
			continue;
		}

		const String dst = p_staging_dir.path_join(rel);
		Error mk_err = DirAccess::make_dir_recursive_absolute(dst.get_base_dir());
		if (mk_err != OK) {
			return mk_err;
		}
		Error copy_err = DirAccess::copy_absolute(src, dst);
		if (copy_err != OK) {
			WARN_PRINT(vformat("xbox_module: failed to copy logo %s", src));
		}
	}

	return OK;
}

String MicrosoftGameConfig::get_template_xml(const String &p_executable_name) {
	return vformat(
			"<?xml version=\"1.0\" encoding=\"utf-8\"?>\n"
			"<Game configVersion=\"1\">\n"
			"  <Identity Name=\"PublisherToken.YourGameTitle\"\n"
			"            Publisher=\"CN=XXXXXXXX-XXXX-XXXX-XXXX-XXXXXXXXXXXX\"\n"
			"            Version=\"1.0.0.0\" />\n"
			"  <TitleId>FFFFFFFF</TitleId>\n"
			"  <MSAAppId>00000000-0000-0000-0000-000000000000</MSAAppId>\n"
			"  <StoreId>9NXXXXXXXXXX</StoreId>\n"
			"  <ExecutableList>\n"
			"    <Executable Name=\"%s\"\n"
			"                TargetDeviceFamily=\"PC\"\n"
			"                Id=\"Game\"\n"
			"                IsDevOnly=\"false\" />\n"
			"  </ExecutableList>\n"
			"  <ShellVisuals DefaultDisplayName=\"Your Game Title\"\n"
			"                PublisherDisplayName=\"Your Publisher\"\n"
			"                StoreLogo=\"StoreLogo.png\"\n"
			"                Square150x150Logo=\"Logo150.png\"\n"
			"                Square44x44Logo=\"Logo44.png\"\n"
			"                Square480x480Logo=\"Logo480.png\"\n"
			"                SplashScreenImage=\"SplashScreen.png\"\n"
			"                ForegroundText=\"light\"\n"
			"                BackgroundColor=\"#1a1a2e\" />\n"
			"  <DesktopRegistration>\n"
			"    <DependencyList>\n"
			"      <KnownDependency Name=\"VC14\" />\n"
			"    </DependencyList>\n"
			"  </DesktopRegistration>\n"
			"</Game>\n",
			p_executable_name);
}

Error MicrosoftGameConfig::write_template_to_project(const String &p_executable_name) {
	String exe_name = p_executable_name;
	if (exe_name.is_empty()) {
		exe_name = "blazium.windows.template_release.x86_64.exe";
	}

	const String path = get_project_config_path();
	if (FileAccess::exists(path)) {
		return ERR_ALREADY_EXISTS;
	}

	Ref<FileAccess> file = FileAccess::open(path, FileAccess::WRITE);
	ERR_FAIL_COND_V(file.is_null(), ERR_FILE_CANT_WRITE);
	file->store_string(get_template_xml(exe_name));
	return OK;
}

String MicrosoftGameConfig::validate_project_config(String *r_error) {
	if (!project_config_exists()) {
		if (r_error) {
			*r_error = "MicrosoftGame.config not found at project root.";
		}
		return String();
	}

	const String exe_name = read_executable_name();
	if (exe_name.is_empty()) {
		if (r_error) {
			*r_error = "MicrosoftGame.config is missing <Executable Name=\"...\">.";
		}
		return String();
	}

	if (r_error) {
		r_error->clear();
	}
	return exe_name;
}
