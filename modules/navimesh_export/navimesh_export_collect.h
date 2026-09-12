/**************************************************************************/
/*  navimesh_export_collect.h                                             */
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

#ifdef TOOLS_ENABLED

#include "core/error/error_list.h"
#include "core/string/ustring.h"
#include "core/variant/dictionary.h"
#include "navimesh_exporter.h"

class Node;

class NavimeshExportCollect {
public:
	struct SceneNavScan {
		bool has_2d = false;
		bool has_3d = false;
		bool maybe = false;

		bool matches(NavimeshExporter::Dimension p_dimension) const {
			if (maybe) {
				return true;
			}
			if (p_dimension == NavimeshExporter::DIMENSION_2D) {
				return has_2d;
			}
			if (p_dimension == NavimeshExporter::DIMENSION_3D) {
				return has_3d;
			}
			return has_2d || has_3d;
		}

		String dimensions_string() const {
			if (maybe) {
				return "2d,3d";
			}
			if (has_2d && has_3d) {
				return "2d,3d";
			}
			if (has_2d) {
				return "2d";
			}
			if (has_3d) {
				return "3d";
			}
			return String();
		}
	};

	static Error bake_node(Node *p_node);
	static Dictionary collect_from_node(Node *p_node, NavimeshExporter::Dimension p_dimension, bool p_single_region);
	static Dictionary make_empty_scene_payload(const String &p_scene_path);
	static void collect_maps(Dictionary &r_payload);
	static bool node_matches_dimension(Node *p_node, NavimeshExporter::Dimension p_dimension);
	static bool scene_has_nav(Node *p_root, NavimeshExporter::Dimension p_dimension);
	static void collect_scene_paths(const String &p_root, PackedStringArray &r_paths, bool p_include_addons = false);
	static bool path_looks_like_scene(const String &p_path);
	static SceneNavScan scan_scene_file(const String &p_path);
	static bool scene_file_has_nav(const String &p_path, NavimeshExporter::Dimension p_dimension);
	static Node *ensure_in_tree(Node *p_node);
	static void release_from_holder(Node *p_top);
	static bool region_has_baked_data(Node *p_node);
	static Node *instantiate_scene(const String &p_scene_path, Error &r_err);
	static void free_instance(Node *p_instance);
};

#endif
