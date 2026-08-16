/**************************************************************************/
/*  justamcp_asset_tools.cpp                                              */
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

#include "justamcp_asset_tools.h"

#include "../justamcp_editor_filesystem.h"

#include "core/config/project_settings.h"
#include "core/io/dir_access.h"
#include "core/io/image.h"

void JustAMCPAssetTools::_bind_methods() {}

Dictionary JustAMCPAssetTools::execute_tool(const String &p_tool_name, const Dictionary &p_args) {
	String tool_name = p_tool_name;
	if (tool_name.begins_with("asset_")) {
		tool_name = tool_name.substr(String("asset_").length());
	}
	if (tool_name == "generate_2d_asset") {
		return generate_2d_asset(p_args);
	}

	return Dictionary();
}

Dictionary JustAMCPAssetTools::generate_2d_asset(const Dictionary &p_args) {
	String svg_code = p_args.get("svg_code", "");
	String filename = p_args.get("filename", "");
	String save_path = p_args.get("save_path", "res://assets/generated/");
	float scale = p_args.get("scale", 1.0);

	if (svg_code.strip_edges().is_empty()) {
		Dictionary ret;
		ret["ok"] = false;
		ret["error"] = "Missing 'svg_code'";
		return ret;
	}

	if (filename.strip_edges().is_empty()) {
		Dictionary ret;
		ret["ok"] = false;
		ret["error"] = "Missing 'filename'";
		return ret;
	}

	if (!filename.ends_with(".png")) {
		filename += ".png";
	}

	if (!save_path.begins_with("res://")) {
		save_path = "res://" + save_path;
	}
	if (!save_path.ends_with("/")) {
		save_path += "/";
	}

	Ref<DirAccess> dir = DirAccess::open("res://");
	if (dir.is_valid()) {
		String relative_save_path = save_path.substr(6);
		if (!dir->dir_exists(relative_save_path)) {
			dir->make_dir_recursive(relative_save_path);
		}
	}

	Ref<Image> image;
	image.instantiate();
	PackedByteArray svg_bytes = svg_code.to_utf8_buffer();
	Error err = image->load_svg_from_buffer(svg_bytes, scale);

	if (err != OK) {
		Dictionary ret;
		ret["ok"] = false;
		ret["error"] = "Failed to load SVG from buffer. Error code: " + itos(err);
		return ret;
	}

	String full_path = save_path + filename;
	String global_path = ProjectSettings::get_singleton()->globalize_path(full_path);
	err = image->save_png(global_path);

	if (err != OK) {
		Dictionary ret;
		ret["ok"] = false;
		ret["error"] = "Failed to save PNG to " + global_path + ". Error code: " + itos(err);
		return ret;
	}

	JustAMCPEditorFilesystem::refresh_path(full_path);

	Dictionary ret;
	ret["ok"] = true;
	ret["resource_path"] = full_path;
	ret["absolute_path"] = global_path;
	ret["width"] = image->get_width();
	ret["height"] = image->get_height();
	ret["message"] = "Generated " + full_path + " (" + itos(image->get_width()) + "x" + itos(image->get_height()) + ")";
	return ret;
}

JustAMCPAssetTools::JustAMCPAssetTools() {}
JustAMCPAssetTools::~JustAMCPAssetTools() {}
