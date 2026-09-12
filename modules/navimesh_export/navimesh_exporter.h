/**************************************************************************/
/*  navimesh_exporter.h                                                   */
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

#include "core/object/class_db.h"
#include "core/object/object.h"
#include "core/variant/type_info.h"

class Node;

class NavimeshExporter : public Object {
	GDCLASS(NavimeshExporter, Object);

	static NavimeshExporter *singleton;

protected:
	static void _bind_methods();

public:
	enum Dimension {
		DIMENSION_2D = 0,
		DIMENSION_3D = 1,
		DIMENSION_BOTH = 2,
	};

	enum Format {
		FORMAT_JSON = 0,
		FORMAT_BIN = 1,
		FORMAT_BOTH = 2,
	};

	enum Mode {
		MODE_INDIVIDUAL = 0,
		MODE_COMBINED = 1,
	};

	static NavimeshExporter *get_singleton();

	Error bake_node(Node *p_node);
	Dictionary bake_scene(const String &p_scene_path, Dimension p_dimension = DIMENSION_BOTH);
	Dictionary collect_from_node(Node *p_node, Dimension p_dimension = DIMENSION_BOTH);
	Error export_node(Node *p_node, const String &p_output_path, Format p_format = FORMAT_BOTH);
	Dictionary export_scene(const String &p_scene_path, const String &p_output_dir, Format p_format = FORMAT_BOTH, Dimension p_dimension = DIMENSION_BOTH);
	Dictionary export_project(const String &p_scenes = "res://", const String &p_output_dir = "res://.navimesh_export/", Mode p_mode = MODE_INDIVIDUAL, Format p_format = FORMAT_BOTH, Dimension p_dimension = DIMENSION_BOTH);
	PackedStringArray list_scenes(const String &p_root = "res://", Dimension p_dimension = DIMENSION_BOTH);

	static String dimension_to_string(Dimension p_dimension);
	static Dimension dimension_from_string(const String &p_value);
	static String format_to_string(Format p_format);
	static Format format_from_string(const String &p_value);
	static String mode_to_string(Mode p_mode);
	static Mode mode_from_string(const String &p_value);
	static Error write_payload(const Dictionary &p_payload, const String &p_base_path, Format p_format, PackedStringArray &r_written);

	NavimeshExporter();
	~NavimeshExporter();
};

VARIANT_ENUM_CAST(NavimeshExporter::Dimension);
VARIANT_ENUM_CAST(NavimeshExporter::Format);
VARIANT_ENUM_CAST(NavimeshExporter::Mode);

#endif
