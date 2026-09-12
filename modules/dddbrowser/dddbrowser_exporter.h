/**************************************************************************/
/*  dddbrowser_exporter.h                                                 */
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

#pragma once

#include "core/object/ref_counted.h"
#include "core/templates/hash_map.h"
#include "core/templates/hash_set.h"
#include "core/variant/dictionary.h"

class Node;
class Node3D;
class MeshInstance3D;
class Light3D;
class DDDBrowserModel;
class DDDBrowserFont;
class DDDBrowserScript;
class DDDBrowserExportConvert;

class DDDBrowserExporter : public RefCounted {
	GDCLASS(DDDBrowserExporter, RefCounted);
	friend class DDDBrowserExportConvert;

	mutable String last_error;
	mutable String last_warning;
	mutable PackedStringArray export_warnings;

	static String _sanitize_id(const String &p_name);
	static String _unique_id(const String &p_base, HashSet<String> &r_used);
	static Dictionary _vec3(const Vector3 &p_v);
	static Dictionary _safe_scale(const Vector3 &p_s);
	static Dictionary _color_vec3(const Color &p_c);
	static Dictionary _transform_fields(Node3D *p_node);
	static String _build_uri(const String &p_base_url, const String &p_export_dir, const String &p_filepath);
	static bool _is_remote_uri(const String &p_path);
	static String _resolve_source_path(const String &p_path);
	static String _media_type_for_extension(const String &p_ext);
	static String _script_filename_from_path(const String &p_path);
	Error _export_mesh_obj(MeshInstance3D *p_mi, const String &p_path) const;
	Dictionary _light_instance(Light3D *p_light) const;
	Error _write_text(const String &p_path, const String &p_text) const;
	Error _copy_file(const String &p_from, const String &p_to) const;
	String _ensure_script_asset(const String &p_source_path, bool p_pin_sha256, const String &p_preferred_id, const String &p_base_url, const String &p_export_dir, Array &r_assets, HashSet<String> &r_used_ids, HashMap<String, String> &r_path_to_asset_id, HashSet<String> &r_used_filenames, String *r_exported_filename = nullptr) const;
	String _ensure_font_asset(DDDBrowserFont *p_font, const String &p_base_url, const String &p_export_dir, Array &r_assets, HashSet<String> &r_used_ids) const;

protected:
	static void _bind_methods();

public:
	Error export_scene(Node *p_root, const String &p_export_dir, bool p_generate_html = true);
	Dictionary build_scene_dictionary(Node *p_root, const String &p_export_dir);
	bool validate_scene_dictionary(const Dictionary &p_scene) const;
	PackedStringArray get_validation_errors(const Dictionary &p_scene) const;
	String get_last_error() const;
	String get_last_warning() const;
};
