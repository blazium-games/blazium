/**************************************************************************/
/*  navimesh_export_collect.cpp                                           */
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

#include "navimesh_export_collect.h"

#include "navimesh_export_serialize.h"

#include "core/config/project_settings.h"
#include "core/io/dir_access.h"
#include "core/io/file_access.h"
#include "core/io/resource_loader.h"
#include "core/os/os.h"
#include "core/os/time.h"
#include "scene/2d/navigation_link_2d.h"
#include "scene/2d/navigation_obstacle_2d.h"
#include "scene/2d/navigation_region_2d.h"
#include "scene/3d/navigation_link_3d.h"
#include "scene/3d/navigation_obstacle_3d.h"
#include "scene/3d/navigation_region_3d.h"
#include "scene/main/node.h"
#include "scene/main/scene_tree.h"
#include "scene/main/viewport.h"
#include "scene/main/window.h"
#include "scene/resources/2d/navigation_polygon.h"
#include "scene/resources/3d/world_3d.h"
#include "scene/resources/navigation_mesh.h"
#include "scene/resources/packed_scene.h"

static const char *NAVIMESH_EXPORT_HOLDER = "__NavimeshExportHolder";
static const int NAVIMESH_SCAN_CHUNK = 8192;
static const int NAVIMESH_SCAN_OVERLAP = 24;

static String _node_path(Node *p_node) {
	ERR_FAIL_NULL_V(p_node, String());
	if (p_node->is_inside_tree()) {
		return String(p_node->get_path());
	}
	return p_node->get_name();
}

static Array _vec2_to_array(const Vector2 &p_value) {
	Array a;
	a.push_back(p_value.x);
	a.push_back(p_value.y);
	return a;
}

static Array _vec3_to_array(const Vector3 &p_value) {
	Array a;
	a.push_back(p_value.x);
	a.push_back(p_value.y);
	a.push_back(p_value.z);
	return a;
}

static Array _transform2d_to_array(const Transform2D &p_xf) {
	Array a;
	a.push_back(p_xf.columns[0].x);
	a.push_back(p_xf.columns[0].y);
	a.push_back(p_xf.columns[1].x);
	a.push_back(p_xf.columns[1].y);
	a.push_back(p_xf.columns[2].x);
	a.push_back(p_xf.columns[2].y);
	return a;
}

static Array _transform3d_to_array(const Transform3D &p_xf) {
	Array a;
	const Basis &b = p_xf.basis;
	a.push_back(b.rows[0][0]);
	a.push_back(b.rows[0][1]);
	a.push_back(b.rows[0][2]);
	a.push_back(b.rows[1][0]);
	a.push_back(b.rows[1][1]);
	a.push_back(b.rows[1][2]);
	a.push_back(b.rows[2][0]);
	a.push_back(b.rows[2][1]);
	a.push_back(b.rows[2][2]);
	a.push_back(p_xf.origin.x);
	a.push_back(p_xf.origin.y);
	a.push_back(p_xf.origin.z);
	return a;
}

static Array _rect2_to_array(const Rect2 &p_rect) {
	Array a;
	a.append_array(_vec2_to_array(p_rect.position));
	a.append_array(_vec2_to_array(p_rect.size));
	return a;
}

static Array _aabb_to_array(const AABB &p_aabb) {
	Array a;
	a.append_array(_vec3_to_array(p_aabb.position));
	a.append_array(_vec3_to_array(p_aabb.size));
	return a;
}

static Array _polygons_to_array(const Vector<Vector<int>> &p_polygons) {
	Array out;
	out.resize(p_polygons.size());
	for (int i = 0; i < p_polygons.size(); i++) {
		Array poly;
		const Vector<int> &src = p_polygons[i];
		poly.resize(src.size());
		for (int j = 0; j < src.size(); j++) {
			poly[j] = src[j];
		}
		out[i] = poly;
	}
	return out;
}

bool NavimeshExportCollect::node_matches_dimension(Node *p_node, NavimeshExporter::Dimension p_dimension) {
	if (!p_node) {
		return false;
	}
	const bool is_2d = Object::cast_to<NavigationRegion2D>(p_node) || Object::cast_to<NavigationLink2D>(p_node) || Object::cast_to<NavigationObstacle2D>(p_node);
	const bool is_3d = Object::cast_to<NavigationRegion3D>(p_node) || Object::cast_to<NavigationLink3D>(p_node) || Object::cast_to<NavigationObstacle3D>(p_node);
	if (p_dimension == NavimeshExporter::DIMENSION_2D) {
		return is_2d;
	}
	if (p_dimension == NavimeshExporter::DIMENSION_3D) {
		return is_3d;
	}
	return is_2d || is_3d;
}

static SceneTree *_scene_tree() {
	if (SceneTree *tree = SceneTree::get_singleton()) {
		return tree;
	}
	if (OS::get_singleton()) {
		return Object::cast_to<SceneTree>(OS::get_singleton()->get_main_loop());
	}
	return nullptr;
}

static Node *_orphan_root(Node *p_node) {
	Node *top = p_node;
	while (top && top->get_parent()) {
		top = top->get_parent();
	}
	return top;
}

static void _prepare_instanced_nav(Node *p_node) {
	if (!p_node) {
		return;
	}
	if (NavigationRegion3D *region3d = Object::cast_to<NavigationRegion3D>(p_node)) {
		Ref<NavigationMesh> mesh = region3d->get_navigation_mesh();
		if (mesh.is_valid()) {
			region3d->set_navigation_mesh(mesh->duplicate());
		}
		region3d->set_enabled(false);
	} else if (NavigationRegion2D *region2d = Object::cast_to<NavigationRegion2D>(p_node)) {
		Ref<NavigationPolygon> poly = region2d->get_navigation_polygon();
		if (poly.is_valid()) {
			region2d->set_navigation_polygon(poly->duplicate());
		}
		region2d->set_enabled(false);
	}
	for (int i = 0; i < p_node->get_child_count(); i++) {
		_prepare_instanced_nav(p_node->get_child(i));
	}
}

static Node *_export_holder(SceneTree *p_tree) {
	if (!p_tree || !p_tree->get_root()) {
		return nullptr;
	}
	Node *root = p_tree->get_root();
	Node *holder = root->get_node_or_null(NodePath(NAVIMESH_EXPORT_HOLDER));
	if (SubViewport *existing = Object::cast_to<SubViewport>(holder)) {
		return existing;
	}
	if (holder) {
		root->remove_child(holder);
		holder->queue_free();
	}
	SubViewport *viewport = memnew(SubViewport);
	viewport->set_name(NAVIMESH_EXPORT_HOLDER);
	viewport->set_process_mode(Node::PROCESS_MODE_DISABLED);
	viewport->set_update_mode(SubViewport::UPDATE_DISABLED);
	viewport->set_disable_input(true);
	viewport->set_size(Vector2i(2, 2));
	Ref<World3D> world_3d;
	world_3d.instantiate();
	viewport->set_world_3d(world_3d);
	root->add_child(viewport);
	return viewport;
}

Node *NavimeshExportCollect::ensure_in_tree(Node *p_node) {
	ERR_FAIL_NULL_V(p_node, nullptr);
	if (p_node->is_inside_tree()) {
		return nullptr;
	}
	Node *top = _orphan_root(p_node);
	if (!top || top->is_inside_tree()) {
		return nullptr;
	}
	SceneTree *tree = _scene_tree();
	Node *holder = _export_holder(tree);
	if (!holder) {
		return nullptr;
	}
	holder->add_child(top);
	return top;
}

void NavimeshExportCollect::release_from_holder(Node *p_top) {
	if (!p_top) {
		return;
	}
	Node *parent = p_top->get_parent();
	if (!parent || parent->get_name() != StringName(NAVIMESH_EXPORT_HOLDER)) {
		return;
	}
	parent->remove_child(p_top);
}

bool NavimeshExportCollect::region_has_baked_data(Node *p_node) {
	if (NavigationRegion3D *region3d = Object::cast_to<NavigationRegion3D>(p_node)) {
		Ref<NavigationMesh> mesh = region3d->get_navigation_mesh();
		return mesh.is_valid() && mesh->get_vertices().size() > 0 && mesh->get_polygon_count() > 0;
	}
	if (NavigationRegion2D *region2d = Object::cast_to<NavigationRegion2D>(p_node)) {
		Ref<NavigationPolygon> poly = region2d->get_navigation_polygon();
		return poly.is_valid() && poly->get_vertices().size() > 0 && poly->get_polygon_count() > 0;
	}
	return false;
}

Node *NavimeshExportCollect::instantiate_scene(const String &p_scene_path, Error &r_err) {
	Ref<PackedScene> packed = ResourceLoader::load(p_scene_path, "PackedScene", ResourceFormatLoader::CACHE_MODE_REUSE, &r_err);
	if (packed.is_null()) {
		r_err = ERR_CANT_OPEN;
		return nullptr;
	}
	Node *instance = packed->instantiate();
	if (!instance) {
		r_err = ERR_CANT_CREATE;
		return nullptr;
	}
	_prepare_instanced_nav(instance);
	ensure_in_tree(instance);
	r_err = OK;
	return instance;
}

void NavimeshExportCollect::free_instance(Node *p_instance) {
	if (!p_instance) {
		return;
	}
	if (p_instance->get_parent()) {
		p_instance->get_parent()->remove_child(p_instance);
	}
	if (p_instance->is_inside_tree()) {
		p_instance->queue_free();
	} else {
		memdelete(p_instance);
	}
}

Error NavimeshExportCollect::bake_node(Node *p_node) {
	ERR_FAIL_NULL_V(p_node, ERR_INVALID_PARAMETER);
	NavigationRegion3D *region3d = Object::cast_to<NavigationRegion3D>(p_node);
	NavigationRegion2D *region2d = Object::cast_to<NavigationRegion2D>(p_node);
	if (!region3d && !region2d) {
		return ERR_INVALID_PARAMETER;
	}

	if (region_has_baked_data(p_node)) {
		return OK;
	}

	Node *attached = nullptr;
	if (!p_node->is_inside_tree()) {
		attached = ensure_in_tree(p_node);
	}

	if (!p_node->is_inside_tree()) {
		return ERR_UNAVAILABLE;
	}

	if (region3d) {
		if (region3d->get_navigation_mesh().is_null()) {
			Ref<NavigationMesh> mesh;
			mesh.instantiate();
			region3d->set_navigation_mesh(mesh);
		}
		region3d->bake_navigation_mesh(false);
	} else {
		if (region2d->get_navigation_polygon().is_null()) {
			Ref<NavigationPolygon> poly;
			poly.instantiate();
			region2d->set_navigation_polygon(poly);
		}
		region2d->bake_navigation_polygon(false);
	}
	release_from_holder(attached);
	return OK;
}

static Dictionary _collect_region_3d(NavigationRegion3D *p_region) {
	Dictionary region;
	region["dimension"] = "3d";
	region["node_path"] = _node_path(p_region);
	region["enabled"] = p_region->is_enabled();
	region["use_edge_connections"] = p_region->get_use_edge_connections();
	region["navigation_layers"] = (int)p_region->get_navigation_layers();
	region["enter_cost"] = p_region->get_enter_cost();
	region["travel_cost"] = p_region->get_travel_cost();
	const Transform3D xf = p_region->get_global_transform();
	region["transform"] = _transform3d_to_array(xf);
	region["bounds"] = _aabb_to_array(p_region->get_bounds());

	Dictionary bake;
	Ref<NavigationMesh> mesh = p_region->get_navigation_mesh();
	if (mesh.is_valid()) {
		bake["cell_size"] = mesh->get_cell_size();
		bake["cell_height"] = mesh->get_cell_height();
		bake["border_size"] = mesh->get_border_size();
		bake["agent_height"] = mesh->get_agent_height();
		bake["agent_radius"] = mesh->get_agent_radius();
		bake["agent_max_climb"] = mesh->get_agent_max_climb();
		bake["agent_max_slope"] = mesh->get_agent_max_slope();
		bake["region_min_size"] = mesh->get_region_min_size();
		bake["region_merge_size"] = mesh->get_region_merge_size();
		bake["edge_max_length"] = mesh->get_edge_max_length();
		bake["edge_max_error"] = mesh->get_edge_max_error();
		bake["vertices_per_polygon"] = mesh->get_vertices_per_polygon();
		bake["detail_sample_distance"] = mesh->get_detail_sample_distance();
		bake["detail_sample_max_error"] = mesh->get_detail_sample_max_error();

		Vector<Vector3> verts;
		Vector<Vector<int>> polys;
		mesh->get_data(verts, polys);
		Array world_verts;
		world_verts.resize(verts.size());
		for (int i = 0; i < verts.size(); i++) {
			world_verts[i] = _vec3_to_array(xf.xform(verts[i]));
		}
		region["vertices"] = world_verts;
		region["polygons"] = _polygons_to_array(polys);
	} else {
		region["vertices"] = Array();
		region["polygons"] = Array();
	}
	region["bake"] = bake;
	return region;
}

static Dictionary _collect_region_2d(NavigationRegion2D *p_region) {
	Dictionary region;
	region["dimension"] = "2d";
	region["node_path"] = _node_path(p_region);
	region["enabled"] = p_region->is_enabled();
	region["use_edge_connections"] = p_region->get_use_edge_connections();
	region["navigation_layers"] = (int)p_region->get_navigation_layers();
	region["enter_cost"] = p_region->get_enter_cost();
	region["travel_cost"] = p_region->get_travel_cost();
	const Transform2D xf = p_region->get_global_transform();
	region["transform"] = _transform2d_to_array(xf);
	region["bounds"] = _rect2_to_array(p_region->get_bounds());

	Dictionary bake;
	Ref<NavigationPolygon> poly = p_region->get_navigation_polygon();
	if (poly.is_valid()) {
		bake["cell_size"] = poly->get_cell_size();
		bake["border_size"] = poly->get_border_size();
		bake["agent_radius"] = poly->get_agent_radius();
		bake["partition_type"] = (int)poly->get_sample_partition_type();
		const Rect2 baking_rect = poly->get_baking_rect();
		Array br;
		br.append_array(_vec2_to_array(baking_rect.position));
		br.append_array(_vec2_to_array(baking_rect.size));
		bake["baking_rect"] = br;
		bake["baking_rect_offset"] = _vec2_to_array(poly->get_baking_rect_offset());

		Vector<Vector2> verts;
		Vector<Vector<int>> polys;
		Vector<Vector<Vector2>> outlines;
		poly->get_data(verts, polys, outlines);
		Array world_verts;
		world_verts.resize(verts.size());
		for (int i = 0; i < verts.size(); i++) {
			world_verts[i] = _vec2_to_array(xf.xform(verts[i]));
		}
		region["vertices"] = world_verts;
		region["polygons"] = _polygons_to_array(polys);
		Array outline_arr;
		outline_arr.resize(outlines.size());
		for (int i = 0; i < outlines.size(); i++) {
			Array ring;
			ring.resize(outlines[i].size());
			for (int j = 0; j < outlines[i].size(); j++) {
				ring[j] = _vec2_to_array(xf.xform(outlines[i][j]));
			}
			outline_arr[i] = ring;
		}
		if (!outline_arr.is_empty()) {
			region["outlines"] = outline_arr;
		}
	} else {
		region["vertices"] = Array();
		region["polygons"] = Array();
	}
	region["bake"] = bake;
	return region;
}

static Dictionary _collect_link_3d(NavigationLink3D *p_link) {
	Dictionary link;
	link["dimension"] = "3d";
	link["node_path"] = _node_path(p_link);
	link["enabled"] = p_link->is_enabled();
	link["bidirectional"] = p_link->is_bidirectional();
	link["navigation_layers"] = (int)p_link->get_navigation_layers();
	link["enter_cost"] = p_link->get_enter_cost();
	link["travel_cost"] = p_link->get_travel_cost();
	link["start"] = _vec3_to_array(p_link->get_global_start_position());
	link["end"] = _vec3_to_array(p_link->get_global_end_position());
	return link;
}

static Dictionary _collect_link_2d(NavigationLink2D *p_link) {
	Dictionary link;
	link["dimension"] = "2d";
	link["node_path"] = _node_path(p_link);
	link["enabled"] = p_link->is_enabled();
	link["bidirectional"] = p_link->is_bidirectional();
	link["navigation_layers"] = (int)p_link->get_navigation_layers();
	link["enter_cost"] = p_link->get_enter_cost();
	link["travel_cost"] = p_link->get_travel_cost();
	link["start"] = _vec2_to_array(p_link->get_global_start_position());
	link["end"] = _vec2_to_array(p_link->get_global_end_position());
	return link;
}

static Dictionary _collect_obstacle_3d(NavigationObstacle3D *p_obstacle) {
	Dictionary obstacle;
	obstacle["dimension"] = "3d";
	obstacle["node_path"] = _node_path(p_obstacle);
	obstacle["radius"] = p_obstacle->get_radius();
	obstacle["height"] = p_obstacle->get_height();
	obstacle["affect_navigation_mesh"] = p_obstacle->get_affect_navigation_mesh();
	obstacle["carve_navigation_mesh"] = p_obstacle->get_carve_navigation_mesh();
	const Transform3D xf = p_obstacle->get_global_transform();
	Array verts;
	const Vector<Vector3> &src = p_obstacle->get_vertices();
	verts.resize(src.size());
	for (int i = 0; i < src.size(); i++) {
		verts[i] = _vec3_to_array(xf.xform(src[i]));
	}
	obstacle["vertices"] = verts;
	return obstacle;
}

static Dictionary _collect_obstacle_2d(NavigationObstacle2D *p_obstacle) {
	Dictionary obstacle;
	obstacle["dimension"] = "2d";
	obstacle["node_path"] = _node_path(p_obstacle);
	obstacle["radius"] = p_obstacle->get_radius();
	obstacle["height"] = 0.0;
	obstacle["affect_navigation_mesh"] = p_obstacle->get_affect_navigation_mesh();
	obstacle["carve_navigation_mesh"] = p_obstacle->get_carve_navigation_mesh();
	const Transform2D xf = p_obstacle->get_global_transform();
	Array verts;
	const Vector<Vector2> &src = p_obstacle->get_vertices();
	verts.resize(src.size());
	for (int i = 0; i < src.size(); i++) {
		verts[i] = _vec2_to_array(xf.xform(src[i]));
	}
	obstacle["vertices"] = verts;
	return obstacle;
}

static void _walk(Node *p_node, NavimeshExporter::Dimension p_dimension, bool p_include_2d_links, bool p_include_3d_links, Array &r_regions, Array &r_links, Array &r_obstacles, NavigationRegion3D *p_only_3d, NavigationRegion2D *p_only_2d) {
	if (!p_node) {
		return;
	}

	if (NavigationRegion3D *region3d = Object::cast_to<NavigationRegion3D>(p_node)) {
		if (p_dimension != NavimeshExporter::DIMENSION_2D && (p_only_3d == nullptr || p_only_3d == region3d)) {
			r_regions.push_back(_collect_region_3d(region3d));
		}
	} else if (NavigationRegion2D *region2d = Object::cast_to<NavigationRegion2D>(p_node)) {
		if (p_dimension != NavimeshExporter::DIMENSION_3D && (p_only_2d == nullptr || p_only_2d == region2d)) {
			r_regions.push_back(_collect_region_2d(region2d));
		}
	} else if (NavigationLink3D *link3d = Object::cast_to<NavigationLink3D>(p_node)) {
		if (p_include_3d_links && p_dimension != NavimeshExporter::DIMENSION_2D) {
			r_links.push_back(_collect_link_3d(link3d));
		}
	} else if (NavigationLink2D *link2d = Object::cast_to<NavigationLink2D>(p_node)) {
		if (p_include_2d_links && p_dimension != NavimeshExporter::DIMENSION_3D) {
			r_links.push_back(_collect_link_2d(link2d));
		}
	} else if (NavigationObstacle3D *obs3d = Object::cast_to<NavigationObstacle3D>(p_node)) {
		if (p_dimension != NavimeshExporter::DIMENSION_2D && p_only_3d == nullptr) {
			r_obstacles.push_back(_collect_obstacle_3d(obs3d));
		}
	} else if (NavigationObstacle2D *obs2d = Object::cast_to<NavigationObstacle2D>(p_node)) {
		if (p_dimension != NavimeshExporter::DIMENSION_3D && p_only_2d == nullptr) {
			r_obstacles.push_back(_collect_obstacle_2d(obs2d));
		}
	}

	for (int i = 0; i < p_node->get_child_count(); i++) {
		_walk(p_node->get_child(i), p_dimension, p_include_2d_links, p_include_3d_links, r_regions, r_links, r_obstacles, p_only_3d, p_only_2d);
	}
}

void NavimeshExportCollect::collect_maps(Dictionary &r_payload) {
	Dictionary maps;
	Dictionary map3d;
	map3d["cell_size"] = GLOBAL_GET("navigation/3d/default_cell_size");
	map3d["cell_height"] = GLOBAL_GET("navigation/3d/default_cell_height");
	map3d["edge_connection_margin"] = GLOBAL_GET("navigation/3d/default_edge_connection_margin");
	map3d["link_connection_radius"] = GLOBAL_GET("navigation/3d/default_link_connection_radius");
	map3d["use_edge_connections"] = GLOBAL_GET("navigation/3d/use_edge_connections");
	const Vector3 up = GLOBAL_GET("navigation/3d/default_up");
	map3d["up"] = _vec3_to_array(up);
	map3d["coordinate_system"] = "godot_y_up";

	Dictionary map2d;
	map2d["cell_size"] = GLOBAL_GET("navigation/2d/default_cell_size");
	map2d["edge_connection_margin"] = GLOBAL_GET("navigation/2d/default_edge_connection_margin");
	map2d["link_connection_radius"] = GLOBAL_GET("navigation/2d/default_link_connection_radius");
	map2d["use_edge_connections"] = GLOBAL_GET("navigation/2d/use_edge_connections");
	map2d["coordinate_system"] = "godot_y_down";

	maps["3d"] = map3d;
	maps["2d"] = map2d;
	r_payload["maps"] = maps;
}

Dictionary NavimeshExportCollect::make_empty_scene_payload(const String &p_scene_path) {
	Dictionary payload;
	payload["format"] = NavimeshExportSerialize::FORMAT_SCENE;
	payload["version"] = (int)NavimeshExportSerialize::VERSION;
	Dictionary source;
	source["engine"] = "blazium";
	source["scene_path"] = p_scene_path;
	source["exported_at"] = Time::get_singleton()->get_datetime_string_from_system(true, true);
	payload["source"] = source;
	collect_maps(payload);
	payload["regions"] = Array();
	payload["links"] = Array();
	payload["obstacles"] = Array();
	return payload;
}

Dictionary NavimeshExportCollect::collect_from_node(Node *p_node, NavimeshExporter::Dimension p_dimension, bool p_single_region) {
	Dictionary payload = make_empty_scene_payload(p_node && p_node->get_owner() ? String(p_node->get_owner()->get_scene_file_path()) : (p_node ? p_node->get_scene_file_path() : String()));

	Node *root = p_node;
	if (p_node && p_node->is_inside_tree()) {
		while (root->get_owner()) {
			root = root->get_owner();
		}
	} else if (p_node) {
		while (root->get_parent()) {
			root = root->get_parent();
		}
	}

	NavigationRegion3D *only_3d = nullptr;
	NavigationRegion2D *only_2d = nullptr;
	bool include_2d_links = true;
	bool include_3d_links = true;
	if (p_single_region) {
		only_3d = Object::cast_to<NavigationRegion3D>(p_node);
		only_2d = Object::cast_to<NavigationRegion2D>(p_node);
		include_2d_links = only_2d != nullptr || Object::cast_to<NavigationLink2D>(p_node);
		include_3d_links = only_3d != nullptr || Object::cast_to<NavigationLink3D>(p_node);
		if (Object::cast_to<NavigationLink3D>(p_node) || Object::cast_to<NavigationLink2D>(p_node)) {
			only_3d = nullptr;
			only_2d = nullptr;
			include_2d_links = p_dimension != NavimeshExporter::DIMENSION_3D;
			include_3d_links = p_dimension != NavimeshExporter::DIMENSION_2D;
		}
	}

	Array regions;
	Array links;
	Array obstacles;
	_walk(root, p_dimension, include_2d_links, include_3d_links, regions, links, obstacles, only_3d, only_2d);
	payload["regions"] = regions;
	payload["links"] = links;
	payload["obstacles"] = obstacles;
	return payload;
}

bool NavimeshExportCollect::scene_has_nav(Node *p_root, NavimeshExporter::Dimension p_dimension) {
	if (!p_root) {
		return false;
	}
	if (Object::cast_to<NavigationRegion3D>(p_root) && p_dimension != NavimeshExporter::DIMENSION_2D) {
		return true;
	}
	if (Object::cast_to<NavigationRegion2D>(p_root) && p_dimension != NavimeshExporter::DIMENSION_3D) {
		return true;
	}
	for (int i = 0; i < p_root->get_child_count(); i++) {
		if (scene_has_nav(p_root->get_child(i), p_dimension)) {
			return true;
		}
	}
	return false;
}

bool NavimeshExportCollect::path_looks_like_scene(const String &p_path) {
	return p_path.ends_with(".tscn") || p_path.ends_with(".scn");
}

void NavimeshExportCollect::collect_scene_paths(const String &p_root, PackedStringArray &r_paths, bool p_include_addons) {
	const String root = p_root.is_empty() ? String("res://") : p_root;
	if (FileAccess::exists(root) && path_looks_like_scene(root)) {
		r_paths.push_back(root);
		return;
	}

	const PackedStringArray parts = root.split(",", false);
	if (parts.size() > 1) {
		for (int i = 0; i < parts.size(); i++) {
			collect_scene_paths(parts[i].strip_edges(), r_paths, p_include_addons);
		}
		return;
	}

	Ref<DirAccess> dir = DirAccess::open(root);
	if (dir.is_null()) {
		return;
	}
	dir->list_dir_begin();
	String name = dir->get_next();
	while (!name.is_empty()) {
		if (name.begins_with(".")) {
			name = dir->get_next();
			continue;
		}
		const String full_path = root.path_join(name);
		if (dir->current_is_dir()) {
			if (name == "addons" && !p_include_addons) {
				name = dir->get_next();
				continue;
			}
			collect_scene_paths(full_path, r_paths, p_include_addons);
		} else if (path_looks_like_scene(name)) {
			r_paths.push_back(full_path);
		}
		name = dir->get_next();
	}
	dir->list_dir_end();
}

NavimeshExportCollect::SceneNavScan NavimeshExportCollect::scan_scene_file(const String &p_path) {
	SceneNavScan scan;
	if (p_path.ends_with(".scn")) {
		scan.maybe = true;
		return scan;
	}

	Error err = OK;
	Ref<FileAccess> f = FileAccess::open(p_path, FileAccess::READ, &err);
	if (f.is_null()) {
		return scan;
	}

	String carry;
	while (!f->eof_reached() && !(scan.has_2d && scan.has_3d)) {
		const Vector<uint8_t> bytes = f->get_buffer(NAVIMESH_SCAN_CHUNK);
		if (bytes.is_empty()) {
			break;
		}
		const String chunk = carry + String::utf8((const char *)bytes.ptr(), bytes.size());
		if (!scan.has_2d && chunk.contains("NavigationRegion2D")) {
			scan.has_2d = true;
		}
		if (!scan.has_3d && chunk.contains("NavigationRegion3D")) {
			scan.has_3d = true;
		}
		if (chunk.length() > NAVIMESH_SCAN_OVERLAP) {
			carry = chunk.substr(chunk.length() - NAVIMESH_SCAN_OVERLAP);
		} else {
			carry = chunk;
		}
	}
	return scan;
}

bool NavimeshExportCollect::scene_file_has_nav(const String &p_path, NavimeshExporter::Dimension p_dimension) {
	return scan_scene_file(p_path).matches(p_dimension);
}

#endif
