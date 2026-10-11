/**************************************************************************/
/*  obfuscation_editor_plugin.cpp                                         */
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

#include "editor/obfuscation_editor_plugin.h"

#include "core/config/project_settings.h"
#include "core/input/shortcut.h"
#include "core/object/callable_mp.h"
#include "core/string/ustring.h"
#include "editor/editor_node.h"
#include "editor/editor_string_names.h"
#include "editor/obfuscation_export_plugin.h"
#include "editor/settings/editor_settings.h"
#include "editor/themes/editor_scale.h"
#include "scene/gui/box_container.h"
#include "scene/gui/button.h"
#include "scene/gui/grid_container.h"
#include "scene/gui/label.h"
#include "scene/gui/line_edit.h"
#include "scene/gui/panel_container.h"
#include "scene/gui/scroll_container.h"
#include "scene/gui/text_edit.h"
#include "scene/gui/texture_rect.h"
#include "scene/resources/image_texture.h"
#include "scene/resources/style_box_flat.h"
#include "scene/scene_string_names.h"
#include "servers/text/text_server.h"

#include "modules/obfuscation/obfuscation.h"

void ObfuscationEditorPlugin::_refresh() {
	Obfuscation *ob = Obfuscation::get_singleton();
	if (!ob || !status) {
		return;
	}
	if (ob->has_identity()) {
		status->set_text(vformat(TTR("Identity ready. owner_id %s"), String::hex_encode_buffer(ob->get_owner_id().ptr(), ob->get_owner_id().size())));
	} else {
		status->set_text(TTR("No identity on this machine. Generate one for export marks. Full verify lives in BlazeSeal."));
	}
	if (publisher_edit && publisher_edit->get_text().is_empty()) {
		publisher_edit->set_text(GLOBAL_GET("obfuscation/publisher_id"));
	}
	if (project_edit && project_edit->get_text().is_empty()) {
		project_edit->set_text(GLOBAL_GET("obfuscation/project_id"));
	}
	if (author_edit) {
		author_edit->set_text(GLOBAL_GET("obfuscation/copyright/author"));
	}
	if (license_edit) {
		license_edit->set_text(GLOBAL_GET("obfuscation/copyright/license"));
	}
	if (notice_edit) {
		notice_edit->set_text(GLOBAL_GET("obfuscation/copyright/text"));
	}
}

void ObfuscationEditorPlugin::_update_badge() {
	Obfuscation *ob = Obfuscation::get_singleton();
	// Setting the overrides below emits theme_changed, which calls this again.
	if (!badge || !ob || updating_badge) {
		return;
	}
	updating_badge = true;
	const bool on = ob->is_enabled();
	const bool identity = ob->has_identity();
	const String publisher = GLOBAL_GET("obfuscation/publisher_id");
	if (on && identity) {
		// A plain C string is read as Latin-1, so build the separator from a wide literal.
		badge->set_text(publisher.is_empty() ? TTR("Protected") : vformat(String(U"%s · %s"), TTR("Protected"), publisher));
		badge->set_tooltip_text(TTR("Exports get obfuscation marks. Click to see what the last export did."));
		badge->set_button_icon(badge->get_editor_theme_icon(SNAME("Lock")));
		badge->add_theme_color_override(SceneStringName(font_color), badge->get_theme_color(SNAME("success_color"), EditorStringName(Editor)));
		badge->add_theme_color_override("icon_normal_color", badge->get_theme_color(SNAME("success_color"), EditorStringName(Editor)));
	} else {
		badge->set_text(on ? TTR("No identity") : TTR("Unprotected"));
		badge->set_tooltip_text(on ? TTR("Generate an identity in the Obfuscation dock so exports get claim marks.") : TTR("Obfuscation is off for this project (obfuscation/enabled)."));
		badge->set_button_icon(badge->get_editor_theme_icon(SNAME("Unlock")));
		badge->add_theme_color_override(SceneStringName(font_color), badge->get_theme_color(SNAME("warning_color"), EditorStringName(Editor)));
		badge->add_theme_color_override("icon_normal_color", badge->get_theme_color(SNAME("warning_color"), EditorStringName(Editor)));
	}
	updating_badge = false;
}

void ObfuscationEditorPlugin::_badge_pressed() {
	if (report_scroll) {
		make_bottom_panel_item_visible(report_scroll);
	}
}

void ObfuscationEditorPlugin::_add_report_tile(const String &p_label, const String &p_value, const Color &p_color) {
	PanelContainer *tile = memnew(PanelContainer);
	tile->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	Ref<StyleBoxFlat> tile_style;
	tile_style.instantiate();
	tile_style->set_bg_color(report_panel->get_theme_color(SNAME("dark_color_2"), EditorStringName(Editor)));
	tile_style->set_corner_radius_all(4 * EDSCALE);
	tile_style->set_content_margin_all(8 * EDSCALE);
	tile->add_theme_style_override(SceneStringName(panel), tile_style);
	VBoxContainer *vb = memnew(VBoxContainer);
	tile->add_child(vb);
	Label *label = memnew(Label);
	label->set_text(p_label);
	label->set_theme_type_variation("HeaderSmall");
	vb->add_child(label);
	Label *value = memnew(Label);
	value->set_text(p_value);
	value->add_theme_font_size_override(SceneStringName(font_size), report_panel->get_theme_font_size(SNAME("title_size"), EditorStringName(EditorFonts)));
	value->add_theme_color_override(SceneStringName(font_color), p_color);
	vb->add_child(value);
	report_grid->add_child(tile);
}

void ObfuscationEditorPlugin::_report_ready(const Dictionary &p_report) {
	if (!report_panel) {
		return;
	}
	for (int i = report_grid->get_child_count() - 1; i >= 0; i--) {
		Node *child = report_grid->get_child(i);
		report_grid->remove_child(child);
		memdelete(child);
	}
	if (p_report.is_empty()) {
		report_title->set_text(TTR("No export yet"));
		report_summary->set_text(TTR("Export the project to see what obfuscation did to it."));
		report_notes->set_text(String());
		return;
	}

	const Color normal = report_panel->get_theme_color(SceneStringName(font_color), EditorStringName(Editor));
	const Color good = report_panel->get_theme_color(SNAME("success_color"), EditorStringName(Editor));
	const Color warn = report_panel->get_theme_color(SNAME("warning_color"), EditorStringName(Editor));
	const Color off = report_panel->get_theme_color(SNAME("font_disabled_color"), EditorStringName(Editor));

	const bool on = p_report.get("protected", false);
	report_title->set_text(on ? TTR("Last export: protected") : TTR("Last export: not protected"));
	report_title->add_theme_color_override(SceneStringName(font_color), on ? good : warn);
	report_summary->set_text(vformat(String(U"%s · %s · %s · %s"), p_report.get("preset", String()), p_report.get("platform", String()), bool(p_report.get("debug", false)) ? TTR("debug") : TTR("release"), p_report.get("time", String())));

	if (on) {
		_add_report_tile(TTR("Scripts processed"), itos(p_report.get("scripts", 0)), normal);
		_add_report_tile(TTR("Images watermarked"), itos(p_report.get("images", 0)), normal);
		_add_report_tile(TTR("Scenes stamped"), itos(p_report.get("scenes_stamped", 0)), normal);
		if (bool(p_report.get("scramble_names", false))) {
			_add_report_tile(TTR("Files renamed"), itos(p_report.get("renamed", 0)), normal);
		} else {
			_add_report_tile(TTR("Files renamed"), TTR("Off"), off);
		}
		_add_report_tile(TTR("Pack seal"), bool(p_report.get("seal", false)) ? TTR("Added") : TTR("None"), bool(p_report.get("seal", false)) ? good : off);
		_add_report_tile(TTR("Comment lattice"), bool(p_report.get("lattice_skipped", false)) ? TTR("Skipped") : (bool(p_report.get("lattice_used", false)) ? TTR("Used") : TTR("Off")), bool(p_report.get("lattice_skipped", false)) ? warn : normal);
		const int problems = p_report.get("problems", 0);
		_add_report_tile(TTR("Problems"), itos(problems), problems > 0 ? report_panel->get_theme_color(SNAME("error_color"), EditorStringName(Editor)) : good);
	}

	String notes;
	if (bool(p_report.get("lattice_skipped", false))) {
		notes += TTR("Comment lattice was skipped because scripts are exported as binary tokens, which keep no comments.") + "\n";
	}
	if (int(p_report.get("problems", 0)) > 0) {
		notes += TTR("Some project settings point at files that were not packed under their scrambled names. See the export log.") + "\n";
	}
	if (!on) {
		notes += TTR("Obfuscation is off, the preset has the \"no_obfuscation\" feature, or this machine has no identity.") + "\n";
	}
	report_notes->set_text(notes.strip_edges());
}

void ObfuscationEditorPlugin::_notification(int p_what) {
	switch (p_what) {
		case NOTIFICATION_ENTER_TREE: {
			Obfuscation *ob = Obfuscation::get_singleton();
			if (ob && !ob->has_identity()) {
				ob->load_identity();
				_refresh();
			}
			_update_badge();
			_report_ready(ObfuscationExportPlugin::get_last_report());
			if (!ProjectSettings::get_singleton()->is_connected("settings_changed", callable_mp(this, &ObfuscationEditorPlugin::_update_badge))) {
				ProjectSettings::get_singleton()->connect("settings_changed", callable_mp(this, &ObfuscationEditorPlugin::_update_badge));
			}
		} break;
	}
}

void ObfuscationEditorPlugin::_generate_pressed() {
	Obfuscation *ob = Obfuscation::get_singleton();
	ERR_FAIL_NULL(ob);
	const String pub = publisher_edit ? publisher_edit->get_text() : String();
	const String proj = project_edit ? project_edit->get_text() : String();
	const Error err = ob->generate_identity(pub, proj);
	if (err != OK) {
		if (status) {
			status->set_text(TTR("Could not generate identity."));
		}
		return;
	}
	ProjectSettings::get_singleton()->set("obfuscation/publisher_id", pub);
	ProjectSettings::get_singleton()->set("obfuscation/project_id", proj);
	_preview_seal();
	_refresh();
	_update_badge();
}

void ObfuscationEditorPlugin::_load_pressed() {
	Obfuscation *ob = Obfuscation::get_singleton();
	ERR_FAIL_NULL(ob);
	ob->load_identity();
	_preview_seal();
	_refresh();
	_update_badge();
}

void ObfuscationEditorPlugin::_clear_pressed() {
	Obfuscation *ob = Obfuscation::get_singleton();
	ERR_FAIL_NULL(ob);
	ob->clear_identity();
	if (preview) {
		preview->set_texture(Ref<Texture2D>());
	}
	_refresh();
	_update_badge();
}

void ObfuscationEditorPlugin::_save_settings_pressed() {
	if (!ProjectSettings::get_singleton()) {
		return;
	}
	if (publisher_edit) {
		ProjectSettings::get_singleton()->set("obfuscation/publisher_id", publisher_edit->get_text());
	}
	if (project_edit) {
		ProjectSettings::get_singleton()->set("obfuscation/project_id", project_edit->get_text());
	}
	if (author_edit) {
		ProjectSettings::get_singleton()->set("obfuscation/copyright/author", author_edit->get_text());
	}
	if (license_edit) {
		ProjectSettings::get_singleton()->set("obfuscation/copyright/license", license_edit->get_text());
	}
	if (notice_edit) {
		ProjectSettings::get_singleton()->set("obfuscation/copyright/text", notice_edit->get_text());
	}
	if (status) {
		status->set_text(TTR("Copyright and identity fields stored in Project Settings. Plaintext notice is stripped on export."));
	}
	_update_badge();
}

void ObfuscationEditorPlugin::_preview_seal() {
	Obfuscation *ob = Obfuscation::get_singleton();
	if (!ob || !ob->has_identity() || !preview) {
		return;
	}
	Ref<Image> img = ob->generate_seal_image();
	if (img.is_valid()) {
		preview->set_texture(ImageTexture::create_from_image(img));
	}
}

ObfuscationEditorPlugin::ObfuscationEditorPlugin() {
	dock = memnew(VBoxContainer);
	dock->set_name(TTR("Obfuscation"));

	Label *title = memnew(Label);
	title->set_text(TTR("Obfuscation"));
	dock->add_child(title);

	status = memnew(Label);
	status->set_autowrap_mode(TextServer::AUTOWRAP_WORD_SMART);
	dock->add_child(status);

	publisher_edit = memnew(LineEdit);
	publisher_edit->set_placeholder(TTR("Publisher id"));
	dock->add_child(publisher_edit);

	project_edit = memnew(LineEdit);
	project_edit->set_placeholder(TTR("Project id"));
	dock->add_child(project_edit);

	HBoxContainer *id_row = memnew(HBoxContainer);
	dock->add_child(id_row);
	Button *gen = memnew(Button);
	gen->set_text(TTR("Generate identity"));
	gen->connect(SceneStringName(pressed), callable_mp(this, &ObfuscationEditorPlugin::_generate_pressed));
	id_row->add_child(gen);
	Button *load = memnew(Button);
	load->set_text(TTR("Load"));
	load->connect(SceneStringName(pressed), callable_mp(this, &ObfuscationEditorPlugin::_load_pressed));
	id_row->add_child(load);
	Button *clear = memnew(Button);
	clear->set_text(TTR("Clear"));
	clear->connect(SceneStringName(pressed), callable_mp(this, &ObfuscationEditorPlugin::_clear_pressed));
	id_row->add_child(clear);

	Label *copy_lbl = memnew(Label);
	copy_lbl->set_text(TTR("Copyright notice"));
	dock->add_child(copy_lbl);

	author_edit = memnew(LineEdit);
	author_edit->set_placeholder(TTR("Author"));
	dock->add_child(author_edit);

	license_edit = memnew(LineEdit);
	license_edit->set_placeholder(TTR("License"));
	dock->add_child(license_edit);

	notice_edit = memnew(TextEdit);
	notice_edit->set_custom_minimum_size(Size2(0, 80) * EDSCALE);
	notice_edit->set_placeholder(TTR("Notice text"));
	dock->add_child(notice_edit);

	Button *save_set = memnew(Button);
	save_set->set_text(TTR("Save settings"));
	save_set->connect(SceneStringName(pressed), callable_mp(this, &ObfuscationEditorPlugin::_save_settings_pressed));
	dock->add_child(save_set);

	preview = memnew(TextureRect);
	preview->set_custom_minimum_size(Size2(0, 128) * EDSCALE);
	preview->set_expand_mode(TextureRect::EXPAND_IGNORE_SIZE);
	preview->set_stretch_mode(TextureRect::STRETCH_KEEP_ASPECT_CENTERED);
	dock->add_child(preview);

	Label *hint = memnew(Label);
	hint->set_autowrap_mode(TextServer::AUTOWRAP_WORD_SMART);
	hint->set_text(TTR("Export injects marks. Generate and verify claim images with the BlazeSeal app."));
	dock->add_child(hint);

	add_blazium_window("Obfuscation", "Obfuscation", dock);

	badge = memnew(Button);
	badge->set_flat(true);
	badge->set_theme_type_variation("FlatMenuButton");
	badge->set_focus_mode(Control::FOCUS_ACCESSIBILITY);
	badge->set_accessibility_name(TTRC("Obfuscation Status"));
	badge->connect(SceneStringName(pressed), callable_mp(this, &ObfuscationEditorPlugin::_badge_pressed));
	badge->connect(SceneStringName(theme_changed), callable_mp(this, &ObfuscationEditorPlugin::_update_badge));
	add_control_to_container(CONTAINER_TOOLBAR, badge);

	report_scroll = memnew(ScrollContainer);
	// Not "Obfuscation": that is the settings window's name, and editor
	// layouts match panels by name.
	report_scroll->set_name(TTRC("Obfuscation Report"));
	report_scroll->set_custom_minimum_size(Size2(0, 180) * EDSCALE);
	report_scroll->set_horizontal_scroll_mode(ScrollContainer::SCROLL_MODE_DISABLED);
	report_panel = memnew(VBoxContainer);
	report_panel->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	report_scroll->add_child(report_panel);
	report_title = memnew(Label);
	report_title->set_theme_type_variation("HeaderMedium");
	report_panel->add_child(report_title);
	report_summary = memnew(Label);
	report_summary->set_text_overrun_behavior(TextServer::OVERRUN_TRIM_ELLIPSIS);
	report_panel->add_child(report_summary);
	report_grid = memnew(GridContainer);
	report_grid->set_columns(4);
	report_grid->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	report_panel->add_child(report_grid);
	report_notes = memnew(Label);
	report_notes->set_autowrap_mode(TextServer::AUTOWRAP_WORD_SMART);
	report_panel->add_child(report_notes);
	add_control_to_bottom_panel(report_scroll, TTRC("Obfuscation Report"));

	ObfuscationExportPlugin::set_report_callback(callable_mp(this, &ObfuscationEditorPlugin::_report_ready));
	_refresh();
}

ObfuscationEditorPlugin::~ObfuscationEditorPlugin() {
	ObfuscationExportPlugin::set_report_callback(Callable());
	if (badge) {
		remove_control_from_container(CONTAINER_TOOLBAR, badge);
		memdelete(badge);
		badge = nullptr;
	}
	if (report_scroll) {
		remove_control_from_bottom_panel(report_scroll);
		memdelete(report_scroll);
		report_scroll = nullptr;
		report_panel = nullptr;
	}
	if (dock) {
		remove_blazium_item("Obfuscation", "Obfuscation");
		memdelete(dock);
		dock = nullptr;
		status = nullptr;
		publisher_edit = nullptr;
		project_edit = nullptr;
		author_edit = nullptr;
		license_edit = nullptr;
		notice_edit = nullptr;
		preview = nullptr;
	}
}

#endif
