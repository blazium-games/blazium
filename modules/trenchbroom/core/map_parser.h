/**************************************************************************/
/*  map_parser.h                                                          */
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

#include "core/templates/hash_set.h"
#include "modules/trenchbroom/core/data.h"

class TrenchbroomMapSettings;

class TrenchbroomMapParser : public RefCounted {
	GDCLASS(TrenchbroomMapParser, RefCounted);

protected:
	static void _bind_methods();

	int _find_unescaped_quote(const String &p_text, int p_start = 0) const;
	Dictionary _parse_quoted_key_value_line(const String &p_line) const;
	bool _parse_quake_map(const PackedStringArray &p_map_data, const TrenchbroomMapSettings *p_map_settings, ParseData &r_parse_data);
	bool _parse_vmf(const PackedStringArray &p_map_data, const TrenchbroomMapSettings *p_map_settings, ParseData &r_parse_data);
	void _resolve_vmf_instances(ParseData &r_parse_data, const String &p_map_file, const TrenchbroomMapSettings *p_map_settings);
	void _resolve_vmf_instances_impl(ParseData &r_parse_data, const String &p_map_file, const TrenchbroomMapSettings *p_map_settings, HashSet<String> &p_resolution_stack);
	static Vector3 _parse_vmf_vector(const String &p_value);
	static void _transform_brush(BrushData &p_brush, const Transform3D &p_xform);
	static void _transform_patch(PatchData &p_patch, const Transform3D &p_xform);
	static void _transform_entity_origin(EntityData &p_entity, const Transform3D &p_xform);
	void _convert_property_types(EntityData &r_entity, const BlaziumFGDEntityClass *p_def);
	void _apply_default_properties(EntityData &r_entity, const BlaziumFGDEntityClass *p_def, HashMap<String, Dictionary> &p_prop_defaults_cache, HashMap<String, Dictionary> &p_prop_descriptions_cache);

public:
	ParseData parse_map_data(const String &p_map_file, const TrenchbroomMapSettings *p_map_settings, bool p_show_profile = false, bool p_resolve_vmf_instances = true);
	Dictionary parse_map_data_dict(const String &p_map_file, const Ref<TrenchbroomMapSettings> &p_map_settings);
};
