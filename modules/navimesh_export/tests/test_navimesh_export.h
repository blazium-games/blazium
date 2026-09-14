/**************************************************************************/
/*  test_navimesh_export.h                                                */
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

#include "tests/test_macros.h"

#ifdef TOOLS_ENABLED

#include "../navimesh_export_cmdline.h"
#include "../navimesh_export_collect.h"
#include "../navimesh_export_serialize.h"
#include "../navimesh_exporter.h"

#include "core/io/dir_access.h"
#include "core/io/file_access.h"
#include "core/object/class_db.h"
#include "core/os/os.h"
#include "core/string/ustring.h"
#include "scene/2d/navigation/navigation_region_2d.h"
#include "scene/3d/navigation/navigation_region_3d.h"
#include "scene/main/scene_tree.h"
#include "scene/main/viewport.h"
#include "scene/resources/2d/navigation_polygon.h"
#include "scene/resources/navigation_mesh.h"

namespace TestNavimeshExport {

static Dictionary _make_region_payload(const String &p_dimension) {
	Dictionary region;
	region["dimension"] = p_dimension;
	region["node_path"] = "/root/Level/Nav";
	region["enabled"] = true;
	region["use_edge_connections"] = true;
	region["navigation_layers"] = 1;
	region["enter_cost"] = 0.0;
	region["travel_cost"] = 1.0;
	Array verts;
	Array polygons;
	if (p_dimension == "2d") {
		Array xf;
		xf.push_back(1.0);
		xf.push_back(0.0);
		xf.push_back(0.0);
		xf.push_back(1.0);
		xf.push_back(0.0);
		xf.push_back(0.0);
		region["transform"] = xf;
		Array bounds;
		bounds.push_back(0.0);
		bounds.push_back(0.0);
		bounds.push_back(10.0);
		bounds.push_back(10.0);
		region["bounds"] = bounds;
		Array a;
		a.push_back(0.0);
		a.push_back(0.0);
		Array b;
		b.push_back(10.0);
		b.push_back(0.0);
		Array c;
		c.push_back(0.0);
		c.push_back(10.0);
		verts.push_back(a);
		verts.push_back(b);
		verts.push_back(c);
	} else {
		Array xf;
		xf.push_back(1.0);
		xf.push_back(0.0);
		xf.push_back(0.0);
		xf.push_back(0.0);
		xf.push_back(1.0);
		xf.push_back(0.0);
		xf.push_back(0.0);
		xf.push_back(0.0);
		xf.push_back(1.0);
		xf.push_back(0.0);
		xf.push_back(0.0);
		xf.push_back(0.0);
		region["transform"] = xf;
		Array bounds;
		bounds.push_back(0.0);
		bounds.push_back(0.0);
		bounds.push_back(0.0);
		bounds.push_back(10.0);
		bounds.push_back(1.0);
		bounds.push_back(10.0);
		region["bounds"] = bounds;
		Array a;
		a.push_back(0.0);
		a.push_back(0.0);
		a.push_back(0.0);
		Array b;
		b.push_back(10.0);
		b.push_back(0.0);
		b.push_back(0.0);
		Array c;
		c.push_back(0.0);
		c.push_back(0.0);
		c.push_back(10.0);
		verts.push_back(a);
		verts.push_back(b);
		verts.push_back(c);
	}
	Array poly;
	poly.push_back(0);
	poly.push_back(1);
	poly.push_back(2);
	polygons.push_back(poly);
	region["vertices"] = verts;
	region["polygons"] = polygons;
	Dictionary bake;
	bake["cell_size"] = p_dimension == "2d" ? 1.0 : 0.25;
	bake["agent_radius"] = p_dimension == "2d" ? 10.0 : 0.5;
	region["bake"] = bake;
	return region;
}

static Dictionary _make_scene_payload(const String &p_dimension) {
	Dictionary payload;
	payload["format"] = NavimeshExportSerialize::FORMAT_SCENE;
	payload["version"] = (int)NavimeshExportSerialize::VERSION;
	Dictionary source;
	source["engine"] = "blazium";
	source["scene_path"] = "res://scenes/level.tscn";
	payload["source"] = source;
	Dictionary maps;
	Dictionary map3d;
	map3d["cell_size"] = 0.25;
	map3d["cell_height"] = 0.25;
	map3d["coordinate_system"] = "godot_y_up";
	map3d["use_edge_connections"] = true;
	Dictionary map2d;
	map2d["cell_size"] = 1.0;
	map2d["coordinate_system"] = "godot_y_down";
	map2d["use_edge_connections"] = true;
	maps["3d"] = map3d;
	maps["2d"] = map2d;
	payload["maps"] = maps;
	Array regions;
	regions.push_back(_make_region_payload(p_dimension));
	payload["regions"] = regions;
	payload["links"] = Array();
	payload["obstacles"] = Array();
	return payload;
}

static String _temp_base(const String &p_name) {
	return OS::get_singleton()->get_cache_path().path_join(p_name);
}

TEST_SUITE("[Modules][NavimeshExport]") {
	TEST_CASE("[NavimeshExport] JSON 3D payload round-trips") {
		const Dictionary original = _make_scene_payload("3d");
		const String path = _temp_base("navimesh_export_3d.nav.json");
		CHECK(NavimeshExportSerialize::write_json(original, path) == OK);
		Dictionary loaded;
		CHECK(NavimeshExportSerialize::read_json(path, loaded) == OK);
		CHECK(String(loaded["format"]) == NavimeshExportSerialize::FORMAT_SCENE);
		CHECK(int(loaded["version"]) == (int)NavimeshExportSerialize::VERSION);
		const Array regions = loaded["regions"];
		REQUIRE(regions.size() == 1);
		const Dictionary region = regions[0];
		CHECK(String(region["dimension"]) == "3d");
		const Array verts = region["vertices"];
		REQUIRE(verts.size() == 3);
		const Array first = verts[0];
		CHECK(first.size() == 3);
		const Dictionary maps = loaded["maps"];
		const Dictionary map3d = maps["3d"];
		CHECK(String(map3d["coordinate_system"]) == "godot_y_up");
		DirAccess::remove_absolute(path);
	}

	TEST_CASE("[NavimeshExport] JSON 2D payload uses stride 2") {
		const Dictionary original = _make_scene_payload("2d");
		const String path = _temp_base("navimesh_export_2d.nav.json");
		CHECK(NavimeshExportSerialize::write_json(original, path) == OK);
		Dictionary loaded;
		CHECK(NavimeshExportSerialize::read_json(path, loaded) == OK);
		const Array regions = loaded["regions"];
		const Dictionary region = regions[0];
		CHECK(String(region["dimension"]) == "2d");
		const Array verts = region["vertices"];
		const Array first = verts[0];
		CHECK(first.size() == 2);
		const Dictionary maps = loaded["maps"];
		const Dictionary map2d = maps["2d"];
		CHECK(String(map2d["coordinate_system"]) == "godot_y_down");
		DirAccess::remove_absolute(path);
	}

	TEST_CASE("[NavimeshExport] Binary 3D and 2D payloads round-trip") {
		for (const String &dimension : { String("3d"), String("2d") }) {
			const Dictionary original = _make_scene_payload(dimension);
			const String path = _temp_base("navimesh_export_" + dimension + ".nav.bin");
			CHECK(NavimeshExportSerialize::write_binary(original, path) == OK);
			Dictionary loaded;
			CHECK(NavimeshExportSerialize::read_binary(path, loaded) == OK);
			CHECK(String(loaded["format"]) == NavimeshExportSerialize::FORMAT_SCENE);
			const Array regions = loaded["regions"];
			REQUIRE(regions.size() == 1);
			const Dictionary region = regions[0];
			CHECK(String(region["dimension"]) == dimension);
			const Array verts = region["vertices"];
			REQUIRE(verts.size() == 3);
			const Array first = verts[0];
			CHECK(first.size() == (dimension == "2d" ? 2 : 3));
			const Array polygons = region["polygons"];
			REQUIRE(polygons.size() == 1);
			DirAccess::remove_absolute(path);
		}
	}

	TEST_CASE("[NavimeshExport] Mixed-dimension payload keeps both regions") {
		Dictionary payload = _make_scene_payload("3d");
		Array regions = payload["regions"];
		regions.push_back(_make_region_payload("2d"));
		payload["regions"] = regions;
		const String json_path = _temp_base("navimesh_export_mixed.nav.json");
		const String bin_path = _temp_base("navimesh_export_mixed.nav.bin");
		CHECK(NavimeshExportSerialize::write_json(payload, json_path) == OK);
		CHECK(NavimeshExportSerialize::write_binary(payload, bin_path) == OK);
		Dictionary json_loaded;
		Dictionary bin_loaded;
		CHECK(NavimeshExportSerialize::read_json(json_path, json_loaded) == OK);
		CHECK(NavimeshExportSerialize::read_binary(bin_path, bin_loaded) == OK);
		CHECK(Array(json_loaded["regions"]).size() == 2);
		CHECK(Array(bin_loaded["regions"]).size() == 2);
		DirAccess::remove_absolute(json_path);
		DirAccess::remove_absolute(bin_path);
	}

	TEST_CASE("[NavimeshExport] Collector skips nodes with no matching nav") {
		Node *root = memnew(Node);
		root->set_name("Empty");
		CHECK_FALSE(NavimeshExportCollect::scene_has_nav(root, NavimeshExporter::DIMENSION_BOTH));
		const Dictionary payload = NavimeshExportCollect::collect_from_node(root, NavimeshExporter::DIMENSION_BOTH, false);
		CHECK(Array(payload["regions"]).is_empty());
		memdelete(root);
	}

	TEST_CASE("[NavimeshExport][SceneTree] Collector reads baked 3D and 2D region geometry") {
		NavigationRegion3D *region3d = memnew(NavigationRegion3D);
		Ref<NavigationMesh> mesh;
		mesh.instantiate();
		Vector<Vector3> verts3;
		verts3.push_back(Vector3(0, 0, 0));
		verts3.push_back(Vector3(2, 0, 0));
		verts3.push_back(Vector3(0, 0, 2));
		Vector<Vector<int>> polys3;
		Vector<int> tri;
		tri.push_back(0);
		tri.push_back(1);
		tri.push_back(2);
		polys3.push_back(tri);
		mesh->set_data(verts3, polys3);
		region3d->set_navigation_mesh(mesh);

		const Dictionary collected3 = NavimeshExportCollect::collect_from_node(region3d, NavimeshExporter::DIMENSION_3D, true);
		const Array regions3 = collected3["regions"];
		REQUIRE(regions3.size() == 1);
		CHECK(Array(Dictionary(regions3[0])["vertices"]).size() == 3);
		CHECK(Array(Dictionary(regions3[0])["polygons"]).size() == 1);

		NavigationRegion2D *region2d = memnew(NavigationRegion2D);
		Ref<NavigationPolygon> poly;
		poly.instantiate();
		Vector<Vector2> verts2;
		verts2.push_back(Vector2(0, 0));
		verts2.push_back(Vector2(4, 0));
		verts2.push_back(Vector2(0, 4));
		Vector<Vector<int>> polys2;
		polys2.push_back(tri);
		poly->set_data(verts2, polys2);
		region2d->set_navigation_polygon(poly);
		const Dictionary collected2 = NavimeshExportCollect::collect_from_node(region2d, NavimeshExporter::DIMENSION_2D, true);
		const Array regions2 = collected2["regions"];
		REQUIRE(regions2.size() == 1);
		CHECK(String(Dictionary(regions2[0])["dimension"]) == "2d");
		CHECK(Array(Dictionary(regions2[0])["vertices"]).size() == 3);

		memdelete(region3d);
		memdelete(region2d);
	}

	TEST_CASE("[NavimeshExport][SceneTree] bake_node accepts orphan region that already has mesh data") {
		NavigationRegion2D *region2d = memnew(NavigationRegion2D);
		Ref<NavigationPolygon> poly;
		poly.instantiate();
		Vector<Vector2> verts2;
		verts2.push_back(Vector2(0, 0));
		verts2.push_back(Vector2(4, 0));
		verts2.push_back(Vector2(0, 4));
		Vector<Vector<int>> polys2;
		Vector<int> tri;
		tri.push_back(0);
		tri.push_back(1);
		tri.push_back(2);
		polys2.push_back(tri);
		poly->set_data(verts2, polys2);
		region2d->set_navigation_polygon(poly);

		CHECK(NavimeshExportCollect::region_has_baked_data(region2d));
		CHECK(NavimeshExportCollect::bake_node(region2d) == OK);
		CHECK_FALSE(region2d->is_inside_tree());
		memdelete(region2d);
	}

	TEST_CASE("[NavimeshExport][SceneTree] export holder uses an isolated viewport") {
		NavigationRegion3D *region3d = memnew(NavigationRegion3D);
		Node *attached = NavimeshExportCollect::ensure_in_tree(region3d);
		REQUIRE(attached != nullptr);
		Node *holder = attached->get_parent();
		REQUIRE(holder != nullptr);
		CHECK(String(holder->get_name()) == "__NavimeshExportHolder");
		CHECK(Object::cast_to<Viewport>(holder) != nullptr);
		NavimeshExportCollect::release_from_holder(attached);
		memdelete(region3d);
	}

	TEST_CASE("[NavimeshExport] scene file scan finds region types without loading the scene") {
		const String path = _temp_base("navimesh_scan_sample.tscn");
		Ref<FileAccess> f = FileAccess::open(path, FileAccess::WRITE);
		REQUIRE(f.is_valid());
		f->store_string("[gd_scene format=3]\n[node name=\"N\" type=\"NavigationRegion2D\"]\n");
		f->close();

		const NavimeshExportCollect::SceneNavScan scan = NavimeshExportCollect::scan_scene_file(path);
		CHECK(scan.has_2d);
		CHECK_FALSE(scan.has_3d);
		CHECK(NavimeshExportCollect::scene_file_has_nav(path, NavimeshExporter::DIMENSION_2D));
		CHECK_FALSE(NavimeshExportCollect::scene_file_has_nav(path, NavimeshExporter::DIMENSION_3D));
		DirAccess::remove_absolute(path);

		const NavimeshExportCollect::SceneNavScan binary = NavimeshExportCollect::scan_scene_file("res://missing.scn");
		CHECK(binary.maybe);
		CHECK(binary.matches(NavimeshExporter::DIMENSION_BOTH));
	}

	TEST_CASE("[NavimeshExport] CLI parser reads dimension, mode, and format") {
		PackedStringArray args;
		args.push_back("--export-navmesh");
		args.push_back("--export-navmesh-scenes");
		args.push_back("res://levels/");
		args.push_back("--export-navmesh-output=user://nav/");
		args.push_back("--export-navmesh-mode");
		args.push_back("combined");
		args.push_back("--export-navmesh-format");
		args.push_back("json");
		args.push_back("--export-navmesh-dimension");
		args.push_back("2d");
		const NavimeshExportCliOptions options = NavimeshExportCmdline::parse(args);
		CHECK(options.enabled);
		CHECK(options.scenes == "res://levels/");
		CHECK(options.output == "user://nav/");
		CHECK(options.mode == NavimeshExporter::MODE_COMBINED);
		CHECK(options.format == NavimeshExporter::FORMAT_JSON);
		CHECK(options.dimension == NavimeshExporter::DIMENSION_2D);
	}

	TEST_CASE("[NavimeshExport] CLI parser defaults when only --export-navmesh is present") {
		PackedStringArray args;
		args.push_back("--export-navmesh");
		const NavimeshExportCliOptions options = NavimeshExportCmdline::parse(args);
		CHECK(options.enabled);
		CHECK(options.mode == NavimeshExporter::MODE_INDIVIDUAL);
		CHECK(options.format == NavimeshExporter::FORMAT_BOTH);
		CHECK(options.dimension == NavimeshExporter::DIMENSION_BOTH);
	}

	TEST_CASE("[NavimeshExport] ClassDB exposes bake and export methods") {
		CHECK(ClassDB::class_exists("NavimeshExporter"));
		CHECK(ClassDB::has_method("NavimeshExporter", "bake_node", true));
		CHECK(ClassDB::has_method("NavimeshExporter", "export_scene", true));
		CHECK(ClassDB::has_method("NavimeshExporter", "list_scenes", true));
		CHECK(ClassDB::has_method("NavimeshExporter", "export_project", true));
	}

	TEST_CASE("[NavimeshExport] Dimension helpers parse CLI tokens") {
		CHECK(NavimeshExporter::dimension_from_string("2d") == NavimeshExporter::DIMENSION_2D);
		CHECK(NavimeshExporter::dimension_from_string("3d") == NavimeshExporter::DIMENSION_3D);
		CHECK(NavimeshExporter::dimension_from_string("both") == NavimeshExporter::DIMENSION_BOTH);
		CHECK(NavimeshExporter::dimension_to_string(NavimeshExporter::DIMENSION_2D) == "2d");
	}
}

} //namespace TestNavimeshExport

#endif
