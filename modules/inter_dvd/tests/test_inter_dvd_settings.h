/**************************************************************************/
/*  test_inter_dvd_settings.h                                             */
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

#include "core/config/project_settings.h"
#include "core/io/dir_access.h"
#include "core/io/file_access.h"
#include "core/io/resource_loader.h"
#include "core/os/os.h"
#include "modules/inter_dvd/author/inter_dvd_ifo_writer.h"
#include "modules/inter_dvd/author/inter_dvd_project.h"
#include "scene/2d/sprite_2d.h"
#include "scene/gui/control.h"
#include "scene/resources/packed_scene.h"
#include "scene/resources/placeholder_textures.h"
#include "tests/test_macros.h"
#include "tests/test_utils.h"

#ifdef TOOLS_ENABLED
#include "modules/inter_dvd/editor/export/windows_inter_dvd_export_platform.h"
#include "modules/inter_dvd/editor/inter_dvd_scene_baker.h"
#endif

TEST_CASE("[Modules][InterDVD] ProjectSettings defaults keep IFO region byte") {
	CHECK(InterDVDSettings::setting_int("blazium/inter_dvd/default_region_mask", 1) == 1);
	CHECK(InterDVDSettings::title_safe_bottom() == 432);
	CHECK(InterDVDSettings::gop_size() == 15);
	CHECK(InterDVDSettings::ac3_bitrate_k() == 192);
	Ref<InterDVDProject> project;
	project.instantiate();
	CHECK(project->get_region_mask() == 1);
	CHECK(project->get_parental_level() == 1);
	CHECK(project->get_title_safe_bottom() == 432);
}

TEST_CASE("[Modules][InterDVD] progress reports total greater than zero") {
	const String root = TestUtils::get_temp_path("inter_dvd_progress");
	Ref<DirAccess> da = DirAccess::create(DirAccess::ACCESS_FILESYSTEM);
	da->make_dir_recursive(root);
	Ref<InterDVDProject> project;
	project.instantiate();
	Ref<InterDVDPGC> title;
	title.instantiate();
	TypedArray<InterDVDPGC> titles;
	titles.push_back(title);
	project->set_titles(titles);
	Ref<InterDVDExportProgress> progress;
	progress.instantiate();
	String err;
	CHECK(InterDVDIfoWriter::write_video_ts(root, project, true, "", &err, true, progress) == OK);
	CHECK(progress->get_total() > 0);
	CHECK(progress->get_step() >= 1);
}

TEST_CASE("[SceneTree][Modules][InterDVD] sync_highlight_from_scene uses Control and Sprite2D rects") {
	Control *root = memnew(Control);
	Control *play = memnew(Control);
	play->set_name("Play");
	play->set_position(Vector2(80, 200));
	play->set_size(Vector2(240, 60));
	root->add_child(play);

	Ref<InterDVDButton> btn;
	btn.instantiate();
	btn->set_control_path(NodePath("Play"));
	btn->sync_highlight_from_scene(root, Rect2(), 432);
	CHECK(btn->get_highlight().position.x == 80);
	CHECK(btn->get_highlight().position.y == 200);
	CHECK(btn->get_highlight().size.x == 240);
	CHECK(btn->get_highlight().size.y == 60);

	Sprite2D *sprite = memnew(Sprite2D);
	sprite->set_name("Icon");
	sprite->set_centered(false);
	sprite->set_offset(Vector2());
	sprite->set_position(Vector2(10, 20));
	Ref<PlaceholderTexture2D> tex;
	tex.instantiate();
	tex->set_size(Size2(32, 16));
	sprite->set_texture(tex);
	root->add_child(sprite);

	Ref<InterDVDButton> spr_btn;
	spr_btn.instantiate();
	spr_btn->set_control_path(NodePath("Icon"));
	spr_btn->sync_highlight_from_scene(root, Rect2(), 432);
	CHECK(spr_btn->get_highlight().position.x == 10);
	CHECK(spr_btn->get_highlight().position.y == 20);
	CHECK(spr_btn->get_highlight().size.x == 32);
	CHECK(spr_btn->get_highlight().size.y == 16);

	Ref<InterDVDButton> missing;
	missing.instantiate();
	missing->set_control_path(NodePath("Gone"));
	missing->sync_highlight_from_scene(root, Rect2(40, 50, 60, 20), 432);
	CHECK(missing->get_highlight().position.x == 40);
	CHECK(missing->get_highlight().size.x == 60);

	Ref<InterDVDButton> unset;
	unset.instantiate();
	unset->set_control_path(NodePath("Gone"));
	unset->sync_highlight_from_scene(root);
	CHECK(unset->get_highlight().size.x == 0);
	CHECK(unset->get_highlight().size.y == 0);

	memdelete(root);
}

static uint32_t inter_dvd_property_usage(const Ref<InterDVDButton> &p_btn, const StringName &p_name) {
	List<PropertyInfo> plist;
	p_btn->get_property_list(&plist);
	for (const PropertyInfo &pi : plist) {
		if (pi.name == p_name) {
			return pi.usage;
		}
	}
	return 0;
}

TEST_CASE("[Modules][InterDVD] title settings are editor-visible only for title actions") {
	Ref<InterDVDButton> btn;
	btn.instantiate();
	btn->set_action(InterDVDButton::ACTION_JUMP_TITLE);
	btn->set_target(3);
	CHECK(btn->get_title_n() == 3);
	CHECK((inter_dvd_property_usage(btn, "title_n") & PROPERTY_USAGE_EDITOR) != 0);
	CHECK((inter_dvd_property_usage(btn, "target") & PROPERTY_USAGE_EDITOR) == 0);
	btn->set_title_n(5);
	CHECK(btn->get_target() == 5);
	CHECK(btn->get_title_n() == 5);

	btn->set_action(InterDVDButton::ACTION_JUMP_CHAPTER);
	btn->set_title_n(2);
	btn->set_target(4);
	CHECK(btn->get_title_n() == 2);
	CHECK(btn->get_target() == 4);
	CHECK((inter_dvd_property_usage(btn, "title_n") & PROPERTY_USAGE_EDITOR) != 0);
	CHECK((inter_dvd_property_usage(btn, "target") & PROPERTY_USAGE_EDITOR) != 0);

	btn->set_action(InterDVDButton::ACTION_JUMP_TITLE);
	CHECK(btn->get_title_n() == 2);
	CHECK(btn->get_target() == 2);

	btn->set_action(InterDVDButton::ACTION_SET_AUDIO);
	CHECK((inter_dvd_property_usage(btn, "title_n") & PROPERTY_USAGE_EDITOR) == 0);
	CHECK((inter_dvd_property_usage(btn, "target") & PROPERTY_USAGE_EDITOR) == 0);
	CHECK((inter_dvd_property_usage(btn, "stream") & PROPERTY_USAGE_EDITOR) != 0);
}

#ifdef TOOLS_ENABLED
TEST_CASE("[Modules][InterDVD] export preset lists output options only") {
	EditorExportPlatformWindowsInterDVD plat;
	List<EditorExportPlatform::ExportOption> opts;
	plat.get_export_options(&opts);
	bool kind = false;
	bool toolchain = false;
	bool dummy = false;
	bool region = false;
	for (const EditorExportPlatform::ExportOption &opt : opts) {
		if (opt.option.name == "inter_dvd/output_kind") {
			kind = true;
		} else if (opt.option.name == "inter_dvd/toolchain") {
			toolchain = true;
		} else if (opt.option.name == "inter_dvd/allow_dummy_vob") {
			dummy = true;
		} else if (opt.option.name == "inter_dvd/region_mask") {
			region = true;
		}
	}
	CHECK(kind);
	CHECK(toolchain);
	CHECK(dummy);
	CHECK_FALSE(region);
}

#ifndef _3D_DISABLED
TEST_CASE("[Modules][InterDVD] dummy bake rejects packed Node3D without ffmpeg") {
	const String path = TestUtils::get_temp_path("inter_dvd_node3d.tscn");
	Ref<FileAccess> f = FileAccess::open(path, FileAccess::WRITE);
	REQUIRE(f.is_valid());
	f->store_string("[gd_scene format=3]\n\n[node name=\"Root\" type=\"Node\"]\n\n[node name=\"Mesh\" type=\"MeshInstance3D\" parent=\".\"]\n");
	f->close();

	Ref<PackedScene> packed = ResourceLoader::load(path, "PackedScene");
	REQUIRE(packed.is_valid());

	Ref<InterDVDCell> cell;
	cell.instantiate();
	cell->set_packed_scene(packed);
	cell->set_duration_sec(1.0);
	String err;
	const Error code = InterDVDSceneBaker::bake_cell(cell, String(), false, &err);
	if (OS::get_singleton()->get_current_rendering_method() == "dummy") {
		CHECK(code == ERR_UNAVAILABLE);
		CHECK(err.contains("3D"));
	} else {
		CHECK(code != OK);
	}
}
#endif
#endif
