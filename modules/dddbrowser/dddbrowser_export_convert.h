/**************************************************************************/
/*  dddbrowser_export_convert.h                                           */
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

#include "core/templates/hash_map.h"
#include "core/templates/hash_set.h"
#include "core/variant/dictionary.h"
#include "core/variant/typed_array.h"

class Node;
class Node3D;
class MeshInstance3D;
class CollisionShape3D;
class DDDBrowserExporter;

struct DDDBrowserExportContext {
	String base_url;
	String export_dir;
	Array *assets = nullptr;
	Array *instances = nullptr;
	HashSet<String> *used_asset_ids = nullptr;
	HashMap<String, String> *script_path_to_asset_id = nullptr;
	HashSet<String> *used_script_filenames = nullptr;
	HashSet<ObjectID> *exported_mesh_ids = nullptr;
	bool has_spawn = false;
	PackedStringArray warnings;
};

class DDDBrowserExportConvert {
public:
	static Dictionary collider_from_shape_node(CollisionShape3D *p_shape, String *r_warning = nullptr);
	static Dictionary collider_from_mesh_node(MeshInstance3D *p_mi, String *r_warning = nullptr);
	static Error export_mesh_with_mtl(DDDBrowserExporter *p_exporter, MeshInstance3D *p_mi, const String &p_obj_path, const String &p_base_url, const String &p_export_dir, String *r_warning = nullptr);
	static String resolve_audio_stream_path(Node *p_node);
	static String resolve_luau_script_path(Node *p_node);
	static Dictionary collect_script_export_data(Node *p_node);
	static bool try_skybox_from_world_environment(Node *p_root, Dictionary &r_skybox);
};
