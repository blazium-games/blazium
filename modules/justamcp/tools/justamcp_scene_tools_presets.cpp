/**************************************************************************/
/*  justamcp_scene_tools_presets.cpp                                      */
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

#include "../justamcp_editor_plugin.h"
#include "justamcp_scene_tools.h"

#include "core/config/project_settings.h"
#include "core/io/dir_access.h"
#include "core/io/file_access.h"
#include "core/io/json.h"
#include "core/io/resource_loader.h"
#include "core/io/resource_saver.h"
#include "core/object/message_queue.h"
#include "core/object/script_language.h"
#include "core/os/mutex.h"
#include "core/os/os.h"
#include "core/os/thread.h"
#include "core/templates/hash_map.h"
#include "editor/editor_file_system.h"
#include "editor/editor_interface.h"
#include "editor/editor_node.h"
#include "editor/editor_undo_redo_manager.h"
#include "scene/2d/sprite_2d.h"
#include "scene/3d/sprite_3d.h"
#include "scene/resources/packed_scene.h"

Dictionary JustAMCPSceneTools::create_area_2d(const Dictionary &p_args) {
	Dictionary mutable_args = p_args;
	mutable_args["nodeType"] = "Area2D";
	return add_node(mutable_args);
}

Dictionary JustAMCPSceneTools::create_line_2d(const Dictionary &p_args) {
	Dictionary mutable_args = p_args;
	mutable_args["nodeType"] = "Line2D";

	Array points = mutable_args.get("points", Array());
	Dictionary proxy_props;
	if (mutable_args.has("properties")) {
		proxy_props = mutable_args["properties"];
	}
	if (!points.is_empty()) {
		PackedVector2Array varray;
		for (int i = 0; i < points.size(); i++) {
			Dictionary pt = points[i];
			varray.push_back(Vector2((float)pt.get("x", 0.0), (float)pt.get("y", 0.0)));
		}
		proxy_props["points"] = varray;
	}
	mutable_args["properties"] = proxy_props;

	return add_node(mutable_args);
}

Dictionary JustAMCPSceneTools::create_polygon_2d(const Dictionary &p_args) {
	Dictionary mutable_args = p_args;
	mutable_args["nodeType"] = "Polygon2D";

	Array points = mutable_args.get("points", Array());
	Dictionary proxy_props;
	if (mutable_args.has("properties")) {
		proxy_props = mutable_args["properties"];
	}
	if (!points.is_empty()) {
		PackedVector2Array varray;
		for (int i = 0; i < points.size(); i++) {
			Dictionary pt = points[i];
			varray.push_back(Vector2((float)pt.get("x", 0.0), (float)pt.get("y", 0.0)));
		}
		proxy_props["polygon"] = varray;
	}
	mutable_args["properties"] = proxy_props;

	return add_node(mutable_args);
}

Dictionary JustAMCPSceneTools::create_csg_shape(const Dictionary &p_args) {
	Dictionary mutable_args = p_args;
	String shape_type = p_args.get("shapeType", "CSGBox3D");
	mutable_args["nodeType"] = shape_type;

	Dictionary proxy_props;
	if (mutable_args.has("properties")) {
		proxy_props = mutable_args["properties"];
	}

	if (shape_type == "CSGBox3D") {
		proxy_props["size"] = Vector3(
				p_args.get("width", 1.0),
				p_args.get("height", 1.0),
				p_args.get("depth", 1.0));
	} else if (shape_type == "CSGSphere3D") {
		proxy_props["radius"] = p_args.get("radius", 0.5);
	} else if (shape_type == "CSGCylinder3D") {
		proxy_props["radius"] = p_args.get("radius", 0.5);
		proxy_props["height"] = p_args.get("height", 1.0);
	}

	mutable_args["properties"] = proxy_props;
	return add_node(mutable_args);
}

Dictionary JustAMCPSceneTools::setup_camera_2d(const Dictionary &p_args) {
	Dictionary mutable_args = p_args;
	mutable_args["nodeType"] = "Camera2D";

	Dictionary proxy_props;
	if (mutable_args.has("properties")) {
		proxy_props = mutable_args["properties"];
	}

	if (p_args.has("zoom")) {
		float z = p_args.get("zoom", 1.0);
		proxy_props["zoom"] = Vector2(z, z);
	}
	proxy_props["position_smoothing_enabled"] = p_args.get("smoothing", true);

	mutable_args["properties"] = proxy_props;
	return add_node(mutable_args);
}

Dictionary JustAMCPSceneTools::setup_parallax_2d(const Dictionary &p_args) {
	Dictionary mutable_args = p_args;
	mutable_args["nodeType"] = "ParallaxBackground";

	Dictionary proxy_props;
	if (mutable_args.has("properties")) {
		proxy_props = mutable_args["properties"];
	}
	mutable_args["properties"] = proxy_props;

	return add_node(mutable_args);
}

Dictionary JustAMCPSceneTools::create_multimesh(const Dictionary &p_args) {
	Dictionary mutable_args = p_args;
	mutable_args["nodeType"] = "MultiMeshInstance3D";

	Dictionary proxy_props;
	if (mutable_args.has("properties")) {
		proxy_props = mutable_args["properties"];
	}
	mutable_args["properties"] = proxy_props;

	return add_node(mutable_args);
}

Dictionary JustAMCPSceneTools::setup_skeleton(const Dictionary &p_args) {
	Dictionary mutable_args = p_args;
	mutable_args["nodeType"] = "Skeleton3D";
	return add_node(mutable_args);
}

Dictionary JustAMCPSceneTools::setup_occlusion(const Dictionary &p_args) {
	Dictionary mutable_args = p_args;
	mutable_args["nodeType"] = "OccluderInstance3D";
	return add_node(mutable_args);
}

#endif
