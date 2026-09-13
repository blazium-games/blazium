/**************************************************************************/
/*  navimesh_export_serialize.cpp                                         */
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

#include "navimesh_export_serialize.h"

#include "core/io/file_access.h"
#include "core/io/json.h"
#include "core/variant/array.h"

namespace {
constexpr uint8_t BIN_DIM_2D = 2;
constexpr uint8_t BIN_DIM_3D = 3;
} //namespace

Error NavimeshExportSerialize::write_json(const Dictionary &p_payload, const String &p_path) {
	Ref<FileAccess> file = FileAccess::open(p_path, FileAccess::WRITE);
	ERR_FAIL_COND_V_MSG(file.is_null(), ERR_CANT_CREATE, vformat("NavimeshExporter: cannot write JSON '%s'.", p_path));
	file->store_string(JSON::stringify(p_payload, "\t", false));
	return OK;
}

Error NavimeshExportSerialize::read_json(const String &p_path, Dictionary &r_payload) {
	Ref<FileAccess> file = FileAccess::open(p_path, FileAccess::READ);
	ERR_FAIL_COND_V_MSG(file.is_null(), ERR_CANT_OPEN, vformat("NavimeshExporter: cannot read JSON '%s'.", p_path));
	return parse_json_text(file->get_as_text(), r_payload);
}

Error NavimeshExportSerialize::parse_json_text(const String &p_text, Dictionary &r_payload) {
	const Variant parsed = JSON::parse_string(p_text);
	ERR_FAIL_COND_V_MSG(parsed.get_type() != Variant::DICTIONARY, ERR_INVALID_DATA, "NavimeshExporter: JSON root must be an object.");
	r_payload = parsed;
	return OK;
}

static void _store_string_f(Ref<FileAccess> p_file, const String &p_value) {
	const CharString utf8 = p_value.utf8();
	p_file->store_32(utf8.length());
	if (utf8.length() > 0) {
		p_file->store_buffer((const uint8_t *)utf8.get_data(), utf8.length());
	}
}

static String _load_string_f(Ref<FileAccess> p_file) {
	const uint32_t len = p_file->get_32();
	if (len == 0) {
		return String();
	}
	Vector<uint8_t> buf;
	buf.resize(len);
	p_file->get_buffer(buf.ptrw(), len);
	return String::utf8((const char *)buf.ptr(), len);
}

static void _store_floats(Ref<FileAccess> p_file, const Array &p_values) {
	p_file->store_32(p_values.size());
	for (int i = 0; i < p_values.size(); i++) {
		p_file->store_float(p_values[i]);
	}
}

static Array _load_floats(Ref<FileAccess> p_file) {
	Array out;
	const uint32_t count = p_file->get_32();
	out.resize(count);
	for (uint32_t i = 0; i < count; i++) {
		out[i] = p_file->get_float();
	}
	return out;
}

static void _store_vertices(Ref<FileAccess> p_file, const Array &p_vertices, uint8_t p_stride) {
	p_file->store_8(p_stride);
	p_file->store_32(p_vertices.size());
	for (int i = 0; i < p_vertices.size(); i++) {
		const Array v = p_vertices[i];
		for (uint8_t c = 0; c < p_stride; c++) {
			p_file->store_float(c < (uint8_t)v.size() ? float(v[c]) : 0.0f);
		}
	}
}

static Array _load_vertices(Ref<FileAccess> p_file) {
	const uint8_t stride = p_file->get_8();
	const uint32_t count = p_file->get_32();
	Array out;
	out.resize(count);
	for (uint32_t i = 0; i < count; i++) {
		Array v;
		v.resize(stride);
		for (uint8_t c = 0; c < stride; c++) {
			v[c] = p_file->get_float();
		}
		out[i] = v;
	}
	return out;
}

static void _store_polys(Ref<FileAccess> p_file, const Array &p_polygons) {
	p_file->store_32(p_polygons.size());
	for (int i = 0; i < p_polygons.size(); i++) {
		const Array poly = p_polygons[i];
		p_file->store_32(poly.size());
		for (int j = 0; j < poly.size(); j++) {
			p_file->store_32((uint32_t)(int)poly[j]);
		}
	}
}

static Array _load_polys(Ref<FileAccess> p_file) {
	Array out;
	const uint32_t count = p_file->get_32();
	out.resize(count);
	for (uint32_t i = 0; i < count; i++) {
		const uint32_t n = p_file->get_32();
		Array poly;
		poly.resize(n);
		for (uint32_t j = 0; j < n; j++) {
			poly[j] = (int)p_file->get_32();
		}
		out[i] = poly;
	}
	return out;
}

static void _store_variant_dict(Ref<FileAccess> p_file, const Dictionary &p_dict) {
	const Array keys = p_dict.keys();
	p_file->store_32(keys.size());
	for (int i = 0; i < keys.size(); i++) {
		const String key = keys[i];
		_store_string_f(p_file, key);
		const Variant value = p_dict[key];
		if (value.get_type() == Variant::FLOAT || value.get_type() == Variant::INT) {
			p_file->store_8(1);
			p_file->store_float(value);
		} else if (value.get_type() == Variant::STRING) {
			p_file->store_8(2);
			_store_string_f(p_file, value);
		} else if (value.get_type() == Variant::BOOL) {
			p_file->store_8(3);
			p_file->store_8(bool(value) ? 1 : 0);
		} else if (value.get_type() == Variant::ARRAY) {
			p_file->store_8(4);
			_store_floats(p_file, value);
		} else {
			p_file->store_8(2);
			_store_string_f(p_file, String(value));
		}
	}
}

static Dictionary _load_variant_dict(Ref<FileAccess> p_file) {
	Dictionary out;
	const uint32_t count = p_file->get_32();
	for (uint32_t i = 0; i < count; i++) {
		const String key = _load_string_f(p_file);
		const uint8_t type = p_file->get_8();
		if (type == 1) {
			out[key] = p_file->get_float();
		} else if (type == 2) {
			out[key] = _load_string_f(p_file);
		} else if (type == 3) {
			out[key] = p_file->get_8() != 0;
		} else if (type == 4) {
			out[key] = _load_floats(p_file);
		}
	}
	return out;
}

static void _store_region_bin(Ref<FileAccess> p_file, const Dictionary &p_region) {
	const String dimension = p_region.get("dimension", "3d");
	const uint8_t stride = dimension == "2d" ? BIN_DIM_2D : BIN_DIM_3D;
	p_file->store_8(stride);
	_store_string_f(p_file, dimension);
	_store_string_f(p_file, p_region.get("node_path", ""));
	p_file->store_8(bool(p_region.get("enabled", true)) ? 1 : 0);
	p_file->store_8(bool(p_region.get("use_edge_connections", true)) ? 1 : 0);
	p_file->store_32((uint32_t)(int)p_region.get("navigation_layers", 1));
	p_file->store_float(p_region.get("enter_cost", 0.0));
	p_file->store_float(p_region.get("travel_cost", 1.0));
	_store_floats(p_file, p_region.get("transform", Array()));
	_store_floats(p_file, p_region.get("bounds", Array()));
	_store_variant_dict(p_file, p_region.get("bake", Dictionary()));
	_store_vertices(p_file, p_region.get("vertices", Array()), stride);
	_store_polys(p_file, p_region.get("polygons", Array()));
	const Array outlines = p_region.get("outlines", Array());
	p_file->store_32(outlines.size());
	for (int i = 0; i < outlines.size(); i++) {
		_store_vertices(p_file, outlines[i], stride);
	}
}

static Dictionary _load_region_bin(Ref<FileAccess> p_file) {
	Dictionary region;
	const uint8_t stride = p_file->get_8();
	(void)stride;
	region["dimension"] = _load_string_f(p_file);
	region["node_path"] = _load_string_f(p_file);
	region["enabled"] = p_file->get_8() != 0;
	region["use_edge_connections"] = p_file->get_8() != 0;
	region["navigation_layers"] = (int)p_file->get_32();
	region["enter_cost"] = p_file->get_float();
	region["travel_cost"] = p_file->get_float();
	region["transform"] = _load_floats(p_file);
	region["bounds"] = _load_floats(p_file);
	region["bake"] = _load_variant_dict(p_file);
	region["vertices"] = _load_vertices(p_file);
	region["polygons"] = _load_polys(p_file);
	const uint32_t outline_count = p_file->get_32();
	Array outlines;
	outlines.resize(outline_count);
	for (uint32_t i = 0; i < outline_count; i++) {
		outlines[i] = _load_vertices(p_file);
	}
	if (outline_count > 0) {
		region["outlines"] = outlines;
	}
	return region;
}

static void _store_link_bin(Ref<FileAccess> p_file, const Dictionary &p_link) {
	_store_string_f(p_file, p_link.get("dimension", "3d"));
	_store_string_f(p_file, p_link.get("node_path", ""));
	p_file->store_8(bool(p_link.get("enabled", true)) ? 1 : 0);
	p_file->store_8(bool(p_link.get("bidirectional", true)) ? 1 : 0);
	p_file->store_32((uint32_t)(int)p_link.get("navigation_layers", 1));
	p_file->store_float(p_link.get("enter_cost", 0.0));
	p_file->store_float(p_link.get("travel_cost", 1.0));
	_store_floats(p_file, p_link.get("start", Array()));
	_store_floats(p_file, p_link.get("end", Array()));
}

static Dictionary _load_link_bin(Ref<FileAccess> p_file) {
	Dictionary link;
	link["dimension"] = _load_string_f(p_file);
	link["node_path"] = _load_string_f(p_file);
	link["enabled"] = p_file->get_8() != 0;
	link["bidirectional"] = p_file->get_8() != 0;
	link["navigation_layers"] = (int)p_file->get_32();
	link["enter_cost"] = p_file->get_float();
	link["travel_cost"] = p_file->get_float();
	link["start"] = _load_floats(p_file);
	link["end"] = _load_floats(p_file);
	return link;
}

static void _store_obstacle_bin(Ref<FileAccess> p_file, const Dictionary &p_obstacle) {
	const String dimension = p_obstacle.get("dimension", "3d");
	const uint8_t stride = dimension == "2d" ? BIN_DIM_2D : BIN_DIM_3D;
	_store_string_f(p_file, dimension);
	_store_string_f(p_file, p_obstacle.get("node_path", ""));
	p_file->store_float(p_obstacle.get("radius", 0.0));
	p_file->store_float(p_obstacle.get("height", 0.0));
	p_file->store_8(bool(p_obstacle.get("affect_navigation_mesh", false)) ? 1 : 0);
	p_file->store_8(bool(p_obstacle.get("carve_navigation_mesh", false)) ? 1 : 0);
	_store_vertices(p_file, p_obstacle.get("vertices", Array()), stride);
}

static Dictionary _load_obstacle_bin(Ref<FileAccess> p_file) {
	Dictionary obstacle;
	obstacle["dimension"] = _load_string_f(p_file);
	obstacle["node_path"] = _load_string_f(p_file);
	obstacle["radius"] = p_file->get_float();
	obstacle["height"] = p_file->get_float();
	obstacle["affect_navigation_mesh"] = p_file->get_8() != 0;
	obstacle["carve_navigation_mesh"] = p_file->get_8() != 0;
	obstacle["vertices"] = _load_vertices(p_file);
	return obstacle;
}

static void _store_payload_bin(Ref<FileAccess> p_file, const Dictionary &p_payload) {
	_store_string_f(p_file, p_payload.get("format", NavimeshExportSerialize::FORMAT_SCENE));
	p_file->store_32((uint32_t)(int)p_payload.get("version", (int)NavimeshExportSerialize::VERSION));
	_store_variant_dict(p_file, p_payload.get("source", Dictionary()));
	const Dictionary maps = p_payload.get("maps", Dictionary());
	_store_variant_dict(p_file, maps.get("3d", Dictionary()));
	_store_variant_dict(p_file, maps.get("2d", Dictionary()));

	const Array regions = p_payload.get("regions", Array());
	p_file->store_32(regions.size());
	for (int i = 0; i < regions.size(); i++) {
		_store_region_bin(p_file, regions[i]);
	}
	const Array links = p_payload.get("links", Array());
	p_file->store_32(links.size());
	for (int i = 0; i < links.size(); i++) {
		_store_link_bin(p_file, links[i]);
	}
	const Array obstacles = p_payload.get("obstacles", Array());
	p_file->store_32(obstacles.size());
	for (int i = 0; i < obstacles.size(); i++) {
		_store_obstacle_bin(p_file, obstacles[i]);
	}

	const Array levels = p_payload.get("levels", Array());
	p_file->store_32(levels.size());
	for (int i = 0; i < levels.size(); i++) {
		_store_payload_bin(p_file, levels[i]);
	}
}

static Dictionary _load_payload_bin(Ref<FileAccess> p_file) {
	Dictionary payload;
	payload["format"] = _load_string_f(p_file);
	payload["version"] = (int)p_file->get_32();
	payload["source"] = _load_variant_dict(p_file);
	Dictionary maps;
	maps["3d"] = _load_variant_dict(p_file);
	maps["2d"] = _load_variant_dict(p_file);
	payload["maps"] = maps;

	Array regions;
	const uint32_t region_count = p_file->get_32();
	regions.resize(region_count);
	for (uint32_t i = 0; i < region_count; i++) {
		regions[i] = _load_region_bin(p_file);
	}
	payload["regions"] = regions;

	Array links;
	const uint32_t link_count = p_file->get_32();
	links.resize(link_count);
	for (uint32_t i = 0; i < link_count; i++) {
		links[i] = _load_link_bin(p_file);
	}
	payload["links"] = links;

	Array obstacles;
	const uint32_t obstacle_count = p_file->get_32();
	obstacles.resize(obstacle_count);
	for (uint32_t i = 0; i < obstacle_count; i++) {
		obstacles[i] = _load_obstacle_bin(p_file);
	}
	payload["obstacles"] = obstacles;

	Array levels;
	const uint32_t level_count = p_file->get_32();
	levels.resize(level_count);
	for (uint32_t i = 0; i < level_count; i++) {
		levels[i] = _load_payload_bin(p_file);
	}
	if (level_count > 0) {
		payload["levels"] = levels;
	}
	return payload;
}

Error NavimeshExportSerialize::write_binary(const Dictionary &p_payload, const String &p_path) {
	Ref<FileAccess> file = FileAccess::open(p_path, FileAccess::WRITE);
	ERR_FAIL_COND_V_MSG(file.is_null(), ERR_CANT_CREATE, vformat("NavimeshExporter: cannot write binary '%s'.", p_path));
	file->store_8('B');
	file->store_8('N');
	file->store_8('A');
	file->store_8('V');
	file->store_32(VERSION);
	_store_payload_bin(file, p_payload);
	return OK;
}

Error NavimeshExportSerialize::read_binary(const String &p_path, Dictionary &r_payload) {
	Ref<FileAccess> file = FileAccess::open(p_path, FileAccess::READ);
	ERR_FAIL_COND_V_MSG(file.is_null(), ERR_CANT_OPEN, vformat("NavimeshExporter: cannot read binary '%s'.", p_path));
	ERR_FAIL_COND_V_MSG(file->get_8() != 'B' || file->get_8() != 'N' || file->get_8() != 'A' || file->get_8() != 'V', ERR_FILE_UNRECOGNIZED, "NavimeshExporter: invalid BNAV magic.");
	const uint32_t version = file->get_32();
	ERR_FAIL_COND_V_MSG(version != VERSION, ERR_FILE_UNRECOGNIZED, vformat("NavimeshExporter: unsupported BNAV version %d.", version));
	r_payload = _load_payload_bin(file);
	return OK;
}

#endif
