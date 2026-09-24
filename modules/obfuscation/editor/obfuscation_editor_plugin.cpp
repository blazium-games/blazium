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
#include "editor/themes/editor_scale.h"
#include "scene/gui/box_container.h"
#include "scene/gui/button.h"
#include "scene/gui/label.h"
#include "scene/gui/line_edit.h"
#include "scene/gui/text_edit.h"
#include "scene/gui/texture_rect.h"
#include "scene/resources/image_texture.h"
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
}

void ObfuscationEditorPlugin::_load_pressed() {
	Obfuscation *ob = Obfuscation::get_singleton();
	ERR_FAIL_NULL(ob);
	ob->load_identity();
	_preview_seal();
	_refresh();
}

void ObfuscationEditorPlugin::_clear_pressed() {
	Obfuscation *ob = Obfuscation::get_singleton();
	ERR_FAIL_NULL(ob);
	ob->clear_identity();
	if (preview) {
		preview->set_texture(Ref<Texture2D>());
	}
	_refresh();
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

	add_control_to_dock(DOCK_SLOT_LEFT_UL, dock);
	_refresh();
}

ObfuscationEditorPlugin::~ObfuscationEditorPlugin() {
	if (dock) {
		remove_control_from_docks(dock);
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
