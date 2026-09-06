/**************************************************************************/
/*  test_inter_dvd_scene.h                                                */
/**************************************************************************/
/*                         This file is part of:                          */
/*                             BLAZIUM ENGINE                             */
/*                          https://blazium.app                           */
/**************************************************************************/

#pragma once

#include "modules/inter_dvd/author/inter_dvd_project.h"
#include "modules/inter_dvd/scene/inter_dvd_chapter.h"
#include "modules/inter_dvd/scene/inter_dvd_disc.h"
#include "modules/inter_dvd/scene/inter_dvd_hotspot.h"
#include "modules/inter_dvd/scene/inter_dvd_menu_page.h"
#include "modules/inter_dvd/scene/inter_dvd_title.h"
#include "modules/inter_dvd/scene/inter_dvd_title_set.h"
#include "scene/gui/control.h"
#include "scene/resources/packed_scene.h"
#include "tests/test_macros.h"

TEST_CASE("[SceneTree][Modules][InterDVD] disc nodes compile titles menus and first play") {
	InterDVDDisc *disc = InterDVDDisc::create_starter();
	REQUIRE(disc);
	CHECK(disc->get_name() == "Disc");

	InterDVDTitle *main = Object::cast_to<InterDVDTitle>(disc->get_node_or_null(NodePath("Main")));
	REQUIRE(main);
	InterDVDTitle *scene_two = disc->add_title("SceneTwo");
	disc->move_child(scene_two, main->get_index() + 1);
	InterDVDChapter *ch = scene_two->add_chapter("Clip");
	ch->set_source(InterDVDChapter::SOURCE_VIDEO);
	ch->set_source_path("D:/clips/two.mp4");
	InterDVDHotspot *next = main->add_hotspot("Next");
	next->set_destination(NodePath("../SceneTwo"));

	const Ref<InterDVDProject> project = disc->build_project();
	REQUIRE(project.is_valid());
	CHECK(project->get_ac3_bitrate_k() == 192);
	CHECK(project->get_ac3_channels() == 2);
	CHECK(project->get_menus().size() == 1);
	const Ref<InterDVDMenu> menu = project->get_menus()[0];
	REQUIRE(menu.is_valid());
	CHECK(menu->get_name() == "TitleMenu");
	CHECK(menu->get_menu_type() == InterDVDMenu::MENU_TITLE);
	CHECK(menu->get_still_time() == 0);
	REQUIRE(menu->get_buttons().size() == 1);
	const Ref<InterDVDButton> play = menu->get_buttons()[0];
	REQUIRE(play.is_valid());
	CHECK(play->get_action() == InterDVDButton::ACTION_JUMP_TITLE);
	CHECK(play->get_title_n() == 1);

	REQUIRE(project->get_titles().size() == 3);
	const Ref<InterDVDPGC> title1 = project->get_titles()[0];
	REQUIRE(title1.is_valid());
	CHECK(title1->get_name() == "Main");
	REQUIRE(title1->get_cells().size() == 1);
	REQUIRE(title1->get_buttons().size() == 1);
	const Ref<InterDVDButton> next_btn = title1->get_buttons()[0];
	REQUIRE(next_btn.is_valid());
	CHECK(next_btn->get_action() == InterDVDButton::ACTION_JUMP_TITLE);
	CHECK(next_btn->get_title_n() == 2);

	const Ref<InterDVDPGC> title2 = project->get_titles()[1];
	REQUIRE(title2.is_valid());
	CHECK(title2->get_name() == "SceneTwo");
	REQUIRE(title2->get_cells().size() == 1);
	const Ref<InterDVDCell> cell = title2->get_cells()[0];
	REQUIRE(cell.is_valid());
	CHECK(cell->get_source_path() == "D:/clips/two.mp4");

	const Ref<InterDVDPGC> menu_title = project->get_titles()[2];
	REQUIRE(menu_title.is_valid());
	CHECK(menu_title->get_name() == "Menu");
	CHECK(menu_title->get_still_time() == 255);
	REQUIRE(menu_title->get_buttons().size() == 1);
	CHECK(Ref<InterDVDButton>(menu_title->get_buttons()[0])->get_title_n() == 1);

	CHECK(project->get_first_play().is_null());

	disc->set_first_play(InterDVDDisc::FIRST_PLAY_MAIN_TITLE);
	const Ref<InterDVDProject> jumped = disc->build_project();
	REQUIRE(jumped->get_first_play().is_valid());
	CHECK(jumped->get_first_play()->get_pre_commands().size() == 1);

	disc->set_ac3_bitrate_k(256);
	disc->set_ac3_channels(6);
	const Ref<InterDVDProject> encoded = disc->build_project();
	CHECK(encoded->get_ac3_bitrate_k() == 256);
	CHECK(encoded->get_ac3_channels() == 6);

	memdelete(disc);
}

TEST_CASE("[Modules][InterDVD] settings use compiled project then hardcoded defaults") {
	CHECK(InterDVDSettings::ac3_bitrate_k() == 192);
	CHECK(InterDVDSettings::gop_size() == 15);
	CHECK(InterDVDSettings::title_safe_bottom() == 432);
	CHECK(InterDVDSettings::ac3_channels() == 2);
	Ref<InterDVDProject> project;
	project.instantiate();
	project->set_ac3_bitrate_k(320);
	project->set_gop_size(12);
	project->set_ac3_channels(4);
	{
		InterDVDSettings::ActiveProjectGuard guard(project);
		CHECK(InterDVDSettings::ac3_bitrate_k() == 320);
		CHECK(InterDVDSettings::gop_size() == 12);
		CHECK(InterDVDSettings::ac3_channels() == 4);
	}
	CHECK(InterDVDSettings::ac3_bitrate_k() == 192);
}

TEST_CASE("[SceneTree][Modules][InterDVD] find_in_tree locates a nested disc") {
	Node *root = memnew(Node);
	root->set_name("Root");
	InterDVDDisc *disc = memnew(InterDVDDisc);
	disc->set_name("Disc");
	root->add_child(disc);
	CHECK(InterDVDDisc::find_in_tree(root) == disc);
	CHECK(InterDVDDisc::find_in_tree(disc) == disc);
	memdelete(root);
}

TEST_CASE("[SceneTree][Modules][InterDVD] title is a 720x480 Control") {
	InterDVDTitle *title = memnew(InterDVDTitle);
	CHECK(Object::cast_to<Control>(title) != nullptr);
	CHECK(title->get_size() == Size2(720, 480));
	memdelete(title);
}

TEST_CASE("[SceneTree][Modules][InterDVD] chapter streams and angle survive compile_cell") {
	InterDVDChapter *ch = memnew(InterDVDChapter);
	Ref<InterDVDStream> stream;
	stream.instantiate();
	stream->set_language("es");
	TypedArray<InterDVDStream> streams;
	streams.push_back(stream);
	ch->set_streams(streams);
	ch->set_angle(2);
	const Ref<InterDVDCell> cell = ch->compile_cell();
	REQUIRE(cell.is_valid());
	CHECK(cell->get_streams().size() == 1);
	CHECK(cell->get_angle() == 2);
	memdelete(ch);
}

TEST_CASE("[SceneTree][Modules][InterDVD] hotspot flags and all actions compile") {
	InterDVDHotspot *spot = memnew(InterDVDHotspot);
	spot->set_auto_action(true);
	spot->set_select_color(Color(0.1, 0.2, 0.3, 1));
	spot->set_action(InterDVDButton::ACTION_RESUME);
	Ref<InterDVDButton> btn = spot->compile_button();
	REQUIRE(btn.is_valid());
	CHECK(btn->get_auto_action());
	CHECK(btn->get_select_color().is_equal_approx(Color(0.1, 0.2, 0.3, 1)));
	CHECK(btn->get_action() == InterDVDButton::ACTION_RESUME);
	CHECK(btn->resolve_command().size() == 8);

	spot->set_action(InterDVDButton::ACTION_EXIT);
	btn = spot->compile_button();
	CHECK(btn->get_action() == InterDVDButton::ACTION_EXIT);
	CHECK(btn->resolve_command().size() == 8);

	spot->set_action(InterDVDButton::ACTION_SET_AUDIO);
	spot->set_stream(3);
	btn = spot->compile_button();
	CHECK(btn->get_action() == InterDVDButton::ACTION_SET_AUDIO);
	CHECK(btn->get_stream() == 3);
	CHECK(btn->resolve_command().size() == 8);

	spot->set_action(InterDVDButton::ACTION_JUMP_MENU);
	spot->set_target(1);
	btn = spot->compile_button();
	CHECK(btn->get_target() == int(InterDVDMenu::MENU_TITLE));
	memdelete(spot);
}

TEST_CASE("[SceneTree][Modules][InterDVD] identity copy matches disc encode extras and languages") {
	InterDVDDisc *disc = memnew(InterDVDDisc);
	disc->set_ac3_bitrate_k(256);
	disc->set_ac3_channels(6);
	disc->set_gop_size(12);
	disc->set_menu_language("fr");
	disc->set_audio_language("de");
	disc->set_subtitle_language("es");
	PackedStringArray extras;
	extras.push_back("note.txt");
	disc->set_extras(extras);
	disc->set_copyright("c");
	const Ref<InterDVDProject> project = disc->build_project();
	REQUIRE(project.is_valid());
	CHECK(project->get_ac3_bitrate_k() == 256);
	CHECK(project->get_ac3_channels() == 6);
	CHECK(project->get_gop_size() == 12);
	CHECK(project->get_menu_language() == "fr");
	CHECK(project->get_audio_language() == "de");
	CHECK(project->get_subtitle_language() == "es");
	CHECK(project->get_extras().size() == 1);
	CHECK(project->get_copyright() == "c");
	memdelete(disc);
}

TEST_CASE("[SceneTree][Modules][InterDVD] bare starter compiles two titles and one menu") {
	InterDVDDisc *disc = InterDVDDisc::create_starter();
	const Ref<InterDVDProject> project = disc->build_project();
	REQUIRE(project.is_valid());
	CHECK(project->get_titles().size() == 2);
	CHECK(project->get_menus().size() == 1);
	const Ref<InterDVDMenu> menu = project->get_menus()[0];
	REQUIRE(menu.is_valid());
	REQUIRE(menu->get_buttons().size() == 1);
	CHECK(Ref<InterDVDButton>(menu->get_buttons()[0])->get_action() == InterDVDButton::ACTION_JUMP_TITLE);
	CHECK(Ref<InterDVDButton>(menu->get_buttons()[0])->get_title_n() == 1);
	const Ref<InterDVDPGC> menu_title = project->get_titles()[1];
	REQUIRE(menu_title.is_valid());
	REQUIRE(menu_title->get_buttons().size() == 1);
	CHECK(Ref<InterDVDButton>(menu_title->get_buttons()[0])->get_title_n() == 1);
	memdelete(disc);
}

TEST_CASE("[SceneTree][Modules][InterDVD] hotspot dest chapter menu and first play") {
	InterDVDDisc *disc = InterDVDDisc::create_starter();
	InterDVDTitle *main = Object::cast_to<InterDVDTitle>(disc->get_node_or_null(NodePath("Main")));
	REQUIRE(main);
	InterDVDChapter *ch = Object::cast_to<InterDVDChapter>(main->get_node_or_null(NodePath("Chapter1")));
	REQUIRE(ch);
	InterDVDHotspot *to_ch = main->add_hotspot("ToChapter");
	to_ch->set_destination(to_ch->get_path_to(ch));
	InterDVDMenuPage *title_menu = Object::cast_to<InterDVDMenuPage>(disc->get_node_or_null(NodePath("TitleMenu")));
	REQUIRE(title_menu);
	InterDVDHotspot *to_title_menu = main->add_hotspot("ToTitleMenu");
	to_title_menu->set_destination(to_title_menu->get_path_to(title_menu));
	InterDVDMenuPage *root = disc->add_root_menu("Root");
	InterDVDHotspot *to_root = main->add_hotspot("ToRoot");
	to_root->set_destination(to_root->get_path_to(root));

	const Ref<InterDVDProject> project = disc->build_project();
	REQUIRE(project->get_titles().size() >= 1);
	const Ref<InterDVDPGC> title1 = project->get_titles()[0];
	REQUIRE(title1->get_buttons().size() >= 3);
	const Ref<InterDVDButton> ch_btn = title1->get_buttons()[0];
	CHECK(ch_btn->get_action() == InterDVDButton::ACTION_JUMP_CHAPTER);
	CHECK(ch_btn->get_target() == 1);
	const Ref<InterDVDButton> menu_btn = title1->get_buttons()[1];
	CHECK(menu_btn->get_action() == InterDVDButton::ACTION_JUMP_TITLE);
	CHECK(menu_btn->get_title_n() == 2);
	const Ref<InterDVDButton> root_btn = title1->get_buttons()[2];
	CHECK(root_btn->get_action() == InterDVDButton::ACTION_JUMP_MENU);
	CHECK(root_btn->get_target() == int(InterDVDMenu::MENU_ROOT));

	CHECK(project->get_first_play().is_null());
	disc->set_first_play(InterDVDDisc::FIRST_PLAY_PATH);
	disc->set_first_play_path(NodePath("Main"));
	const Ref<InterDVDProject> path_play = disc->build_project();
	REQUIRE(path_play->get_first_play().is_valid());
	CHECK(path_play->get_first_play()->get_pre_commands().size() == 1);
	memdelete(disc);
}

TEST_CASE("[SceneTree][Modules][InterDVD] menu title still_time 0 survives pack") {
	InterDVDDisc *disc = memnew(InterDVDDisc);
	disc->set_name("Disc");
	InterDVDTitle *menu = disc->add_menu_title("Menu");
	CHECK(menu->is_menu_title());
	CHECK(menu->get_still_time() == 255);
	menu->set_still_time(0);
	CHECK(menu->get_still_time() == 0);
	disc->apply_scene_owner(disc);
	Ref<PackedScene> packed;
	packed.instantiate();
	REQUIRE(packed->pack(disc) == OK);
	Node *loaded = packed->instantiate();
	REQUIRE(loaded);
	InterDVDTitle *reloaded = Object::cast_to<InterDVDTitle>(loaded->get_node_or_null(NodePath("Menu")));
	REQUIRE(reloaded);
	CHECK(reloaded->is_menu_title());
	CHECK(reloaded->get_still_time() == 0);
	memdelete(loaded);
	memdelete(disc);
}

TEST_CASE("[SceneTree][Modules][InterDVD] two title sets compile distinct VTS numbers") {
	InterDVDDisc *disc = memnew(InterDVDDisc);
	InterDVDTitleSet *a = disc->add_title_set("SetA");
	a->add_title("One");
	InterDVDTitleSet *b = disc->add_title_set("SetB");
	b->add_title("Two");
	const Ref<InterDVDProject> project = disc->build_project();
	REQUIRE(project->get_titles().size() == 2);
	CHECK(Ref<InterDVDPGC>(project->get_titles()[0])->get_title_set_nr() == 1);
	CHECK(Ref<InterDVDPGC>(project->get_titles()[1])->get_title_set_nr() == 2);
	memdelete(disc);
}
