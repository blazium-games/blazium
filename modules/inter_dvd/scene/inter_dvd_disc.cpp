/**************************************************************************/
/*  inter_dvd_disc.cpp                                                    */
/**************************************************************************/
/*                         This file is part of:                          */
/*                             BLAZIUM ENGINE                             */
/*                          https://blazium.app                           */
/**************************************************************************/

#include "inter_dvd_disc.h"

#include "inter_dvd_chapter.h"
#include "inter_dvd_hotspot.h"
#include "inter_dvd_menu_page.h"
#include "inter_dvd_title.h"
#include "inter_dvd_title_set.h"

#include "core/error/error_macros.h"
#include "core/object/class_db.h"

namespace {
void own_descendants(Node *p_node, Node *p_owner) {
	for (int i = 0; i < p_node->get_child_count(); i++) {
		Node *child = p_node->get_child(i);
		child->set_owner(p_owner);
		own_descendants(child, p_owner);
	}
}

template <typename F>
void visit_titles(const InterDVDDisc *p_disc, F p_fn) {
	for (int i = 0; i < p_disc->get_child_count(); i++) {
		Node *child = p_disc->get_child(i);
		if (InterDVDTitleSet *set = Object::cast_to<InterDVDTitleSet>(child)) {
			for (int t = 0; t < set->get_child_count(); t++) {
				if (InterDVDTitle *title = Object::cast_to<InterDVDTitle>(set->get_child(t))) {
					if (!p_fn(title)) {
						return;
					}
				}
			}
		} else if (InterDVDTitle *title = Object::cast_to<InterDVDTitle>(child)) {
			if (!p_fn(title)) {
				return;
			}
		}
	}
}

int title_index_of(const InterDVDDisc *p_disc, const InterDVDTitle *p_title) {
	int n = 0;
	int found = 0;
	visit_titles(p_disc, [&](const InterDVDTitle *title) {
		n++;
		if (title == p_title) {
			found = n;
			return false;
		}
		return true;
	});
	return found;
}

int menu_title_index(const InterDVDDisc *p_disc) {
	int n = 0;
	int found = 0;
	visit_titles(p_disc, [&](const InterDVDTitle *title) {
		n++;
		if (title->is_menu_title()) {
			found = n;
			return false;
		}
		return true;
	});
	return found;
}

Node *find_named_authoring(const InterDVDDisc *p_disc, const String &p_name) {
	if (!p_disc || p_name.is_empty()) {
		return nullptr;
	}
	Node *found = nullptr;
	visit_titles(p_disc, [&](InterDVDTitle *title) {
		if (String(title->get_name()) == p_name) {
			found = title;
			return false;
		}
		for (int i = 0; i < title->get_child_count(); i++) {
			Node *child = title->get_child(i);
			if (Object::cast_to<InterDVDChapter>(child) && String(child->get_name()) == p_name) {
				found = child;
				return false;
			}
		}
		return true;
	});
	if (found) {
		return found;
	}
	for (int i = 0; i < p_disc->get_child_count(); i++) {
		Node *child = p_disc->get_child(i);
		if (Object::cast_to<InterDVDMenuPage>(child) && String(child->get_name()) == p_name) {
			return child;
		}
		if (InterDVDTitleSet *set = Object::cast_to<InterDVDTitleSet>(child)) {
			for (int t = 0; t < set->get_child_count(); t++) {
				Node *nested = set->get_child(t);
				if (Object::cast_to<InterDVDMenuPage>(nested) && String(nested->get_name()) == p_name) {
					return nested;
				}
			}
		}
	}
	return nullptr;
}

Node *resolve_disc_path(const InterDVDDisc *p_disc, Node *p_from, const NodePath &p_path) {
	if (p_path.is_empty()) {
		return nullptr;
	}
	Node *dest = p_from ? p_from->get_node_or_null(p_path) : nullptr;
	if (!dest && p_from && p_from->get_parent()) {
		dest = p_from->get_parent()->get_node_or_null(p_path);
	}
	if (!dest && p_disc) {
		dest = p_disc->get_node_or_null(p_path);
	}
	if (!dest && p_disc) {
		const int parts = p_path.get_name_count();
		if (parts > 0) {
			dest = find_named_authoring(p_disc, String(p_path.get_name(parts - 1)));
		}
	}
	return dest;
}

int chapter_index_of(const InterDVDTitle *p_title, const InterDVDChapter *p_chapter) {
	if (!p_title) {
		return 0;
	}
	int n = 0;
	for (int i = 0; i < p_title->get_child_count(); i++) {
		if (const InterDVDChapter *chapter = Object::cast_to<InterDVDChapter>(p_title->get_child(i))) {
			n++;
			if (chapter == p_chapter) {
				return n;
			}
		}
	}
	return 0;
}

int main_title_index(const InterDVDDisc *p_disc) {
	int n = 0;
	int first = 0;
	int found = 0;
	visit_titles(p_disc, [&](const InterDVDTitle *title) {
		n++;
		if (first == 0) {
			first = n;
		}
		if (!title->is_menu_title() && found == 0) {
			found = n;
		}
		return true;
	});
	if (found > 0) {
		return found;
	}
	return first > 0 ? first : 1;
}

const InterDVDTitle *owning_title(const Node *p_node) {
	const Node *walk = p_node;
	while (walk) {
		if (const InterDVDTitle *title = Object::cast_to<InterDVDTitle>(walk)) {
			return title;
		}
		walk = walk->get_parent();
	}
	return nullptr;
}

void apply_hotspot_destination(const InterDVDDisc *p_disc, const InterDVDHotspot *p_spot, const Ref<InterDVDButton> &p_btn) {
	const bool from_title = owning_title(p_spot) != nullptr;
	if (p_spot->get_destination().is_empty()) {
		if (p_btn->get_action() == InterDVDButton::ACTION_JUMP_MENU && p_btn->get_target() < 2) {
			p_btn->set_target(int(InterDVDMenu::MENU_TITLE));
		}
		if (p_btn->get_action() == InterDVDButton::ACTION_JUMP_CHAPTER) {
			p_btn->set_title_n(MAX(title_index_of(p_disc, owning_title(p_spot)), 1));
		}
		if (p_btn->get_action() == InterDVDButton::ACTION_JUMP_TITLE && p_btn->get_title_n() < 1) {
			p_btn->set_title_n(1);
		}
		return;
	}
	Node *dest = resolve_disc_path(p_disc, const_cast<InterDVDHotspot *>(p_spot), p_spot->get_destination());
	if (const InterDVDTitle *title = Object::cast_to<InterDVDTitle>(dest)) {
		const int idx = MAX(title_index_of(p_disc, title), 1);
		if (p_btn->get_action() == InterDVDButton::ACTION_JUMP_PGC) {
			p_btn->set_target(idx);
			return;
		}
		p_btn->set_action(InterDVDButton::ACTION_JUMP_TITLE);
		p_btn->set_title_n(idx);
		return;
	}
	if (const InterDVDChapter *chapter = Object::cast_to<InterDVDChapter>(dest)) {
		const InterDVDTitle *title = Object::cast_to<InterDVDTitle>(chapter->get_parent());
		p_btn->set_action(InterDVDButton::ACTION_JUMP_CHAPTER);
		p_btn->set_title_n(MAX(title_index_of(p_disc, title), 1));
		p_btn->set_target(MAX(chapter_index_of(title, chapter), 1));
		return;
	}
	if (const InterDVDMenuPage *menu = Object::cast_to<InterDVDMenuPage>(dest)) {
		if (menu->get_menu_type() == InterDVDMenu::MENU_TITLE && from_title) {
			const int menu_idx = menu_title_index(p_disc);
			if (menu_idx > 0) {
				p_btn->set_action(InterDVDButton::ACTION_JUMP_TITLE);
				p_btn->set_title_n(menu_idx);
				return;
			}
		}
		p_btn->set_action(InterDVDButton::ACTION_JUMP_MENU);
		p_btn->set_target(int(menu->get_menu_type()));
	}
}

void resolve_parent_hotspots(const InterDVDDisc *p_disc, Node *p_parent, const TypedArray<InterDVDButton> &p_buttons) {
	int b = 0;
	for (int i = 0; i < p_parent->get_child_count() && b < p_buttons.size(); i++) {
		if (const InterDVDHotspot *spot = Object::cast_to<InterDVDHotspot>(p_parent->get_child(i))) {
			const Ref<InterDVDButton> btn = p_buttons[b];
			b++;
			if (btn.is_valid()) {
				apply_hotspot_destination(p_disc, spot, btn);
			}
		}
	}
}
} //namespace

void InterDVDDisc::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_disc_title", "title"), &InterDVDDisc::set_disc_title);
	ClassDB::bind_method(D_METHOD("get_disc_title"), &InterDVDDisc::get_disc_title);
	ClassDB::bind_method(D_METHOD("set_volume_id", "id"), &InterDVDDisc::set_volume_id);
	ClassDB::bind_method(D_METHOD("get_volume_id"), &InterDVDDisc::get_volume_id);
	ClassDB::bind_method(D_METHOD("set_serial", "serial"), &InterDVDDisc::set_serial);
	ClassDB::bind_method(D_METHOD("get_serial"), &InterDVDDisc::get_serial);
	ClassDB::bind_method(D_METHOD("set_provider_id", "id"), &InterDVDDisc::set_provider_id);
	ClassDB::bind_method(D_METHOD("get_provider_id"), &InterDVDDisc::get_provider_id);
	ClassDB::bind_method(D_METHOD("set_region_mask", "mask"), &InterDVDDisc::set_region_mask);
	ClassDB::bind_method(D_METHOD("get_region_mask"), &InterDVDDisc::get_region_mask);
	ClassDB::bind_method(D_METHOD("set_parental_level", "level"), &InterDVDDisc::set_parental_level);
	ClassDB::bind_method(D_METHOD("get_parental_level"), &InterDVDDisc::get_parental_level);
	ClassDB::bind_method(D_METHOD("set_menu_language", "language"), &InterDVDDisc::set_menu_language);
	ClassDB::bind_method(D_METHOD("get_menu_language"), &InterDVDDisc::get_menu_language);
	ClassDB::bind_method(D_METHOD("set_audio_language", "language"), &InterDVDDisc::set_audio_language);
	ClassDB::bind_method(D_METHOD("get_audio_language"), &InterDVDDisc::get_audio_language);
	ClassDB::bind_method(D_METHOD("set_subtitle_language", "language"), &InterDVDDisc::set_subtitle_language);
	ClassDB::bind_method(D_METHOD("get_subtitle_language"), &InterDVDDisc::get_subtitle_language);
	ClassDB::bind_method(D_METHOD("set_ac3_bitrate_k", "kbps"), &InterDVDDisc::set_ac3_bitrate_k);
	ClassDB::bind_method(D_METHOD("get_ac3_bitrate_k"), &InterDVDDisc::get_ac3_bitrate_k);
	ClassDB::bind_method(D_METHOD("set_ac3_channels", "channels"), &InterDVDDisc::set_ac3_channels);
	ClassDB::bind_method(D_METHOD("get_ac3_channels"), &InterDVDDisc::get_ac3_channels);
	ClassDB::bind_method(D_METHOD("set_gop_size", "gop"), &InterDVDDisc::set_gop_size);
	ClassDB::bind_method(D_METHOD("get_gop_size"), &InterDVDDisc::get_gop_size);
	ClassDB::bind_method(D_METHOD("set_title_safe_bottom", "scanline"), &InterDVDDisc::set_title_safe_bottom);
	ClassDB::bind_method(D_METHOD("get_title_safe_bottom"), &InterDVDDisc::get_title_safe_bottom);
	ClassDB::bind_method(D_METHOD("set_bake_warmup_frames", "frames"), &InterDVDDisc::set_bake_warmup_frames);
	ClassDB::bind_method(D_METHOD("get_bake_warmup_frames"), &InterDVDDisc::get_bake_warmup_frames);
	ClassDB::bind_method(D_METHOD("set_pip_blackdetect_sec", "seconds"), &InterDVDDisc::set_pip_blackdetect_sec);
	ClassDB::bind_method(D_METHOD("get_pip_blackdetect_sec"), &InterDVDDisc::get_pip_blackdetect_sec);
	ClassDB::bind_method(D_METHOD("set_pip_blackdetect_pix_th", "threshold"), &InterDVDDisc::set_pip_blackdetect_pix_th);
	ClassDB::bind_method(D_METHOD("get_pip_blackdetect_pix_th"), &InterDVDDisc::get_pip_blackdetect_pix_th);
	ClassDB::bind_method(D_METHOD("set_extras", "extras"), &InterDVDDisc::set_extras);
	ClassDB::bind_method(D_METHOD("get_extras"), &InterDVDDisc::get_extras);
	ClassDB::bind_method(D_METHOD("set_extras_dir", "dir"), &InterDVDDisc::set_extras_dir);
	ClassDB::bind_method(D_METHOD("get_extras_dir"), &InterDVDDisc::get_extras_dir);
	ClassDB::bind_method(D_METHOD("set_copyright", "text"), &InterDVDDisc::set_copyright);
	ClassDB::bind_method(D_METHOD("get_copyright"), &InterDVDDisc::get_copyright);
	ClassDB::bind_method(D_METHOD("set_copyright_file", "path"), &InterDVDDisc::set_copyright_file);
	ClassDB::bind_method(D_METHOD("get_copyright_file"), &InterDVDDisc::get_copyright_file);
	ClassDB::bind_method(D_METHOD("set_license", "text"), &InterDVDDisc::set_license);
	ClassDB::bind_method(D_METHOD("get_license"), &InterDVDDisc::get_license);
	ClassDB::bind_method(D_METHOD("set_license_file", "path"), &InterDVDDisc::set_license_file);
	ClassDB::bind_method(D_METHOD("get_license_file"), &InterDVDDisc::get_license_file);
	ClassDB::bind_method(D_METHOD("set_readme", "text"), &InterDVDDisc::set_readme);
	ClassDB::bind_method(D_METHOD("get_readme"), &InterDVDDisc::get_readme);
	ClassDB::bind_method(D_METHOD("set_readme_file", "path"), &InterDVDDisc::set_readme_file);
	ClassDB::bind_method(D_METHOD("get_readme_file"), &InterDVDDisc::get_readme_file);
	ClassDB::bind_method(D_METHOD("set_credits", "text"), &InterDVDDisc::set_credits);
	ClassDB::bind_method(D_METHOD("get_credits"), &InterDVDDisc::get_credits);
	ClassDB::bind_method(D_METHOD("set_credits_file", "path"), &InterDVDDisc::set_credits_file);
	ClassDB::bind_method(D_METHOD("get_credits_file"), &InterDVDDisc::get_credits_file);
	ClassDB::bind_method(D_METHOD("set_autorun_label", "label"), &InterDVDDisc::set_autorun_label);
	ClassDB::bind_method(D_METHOD("get_autorun_label"), &InterDVDDisc::get_autorun_label);
	ClassDB::bind_method(D_METHOD("set_autorun_open", "open"), &InterDVDDisc::set_autorun_open);
	ClassDB::bind_method(D_METHOD("get_autorun_open"), &InterDVDDisc::get_autorun_open);
	ClassDB::bind_method(D_METHOD("set_autorun_icon", "icon"), &InterDVDDisc::set_autorun_icon);
	ClassDB::bind_method(D_METHOD("get_autorun_icon"), &InterDVDDisc::get_autorun_icon);
	ClassDB::bind_method(D_METHOD("set_publisher", "publisher"), &InterDVDDisc::set_publisher);
	ClassDB::bind_method(D_METHOD("get_publisher"), &InterDVDDisc::get_publisher);
	ClassDB::bind_method(D_METHOD("set_author", "author"), &InterDVDDisc::set_author);
	ClassDB::bind_method(D_METHOD("get_author"), &InterDVDDisc::get_author);
	ClassDB::bind_method(D_METHOD("set_studio", "studio"), &InterDVDDisc::set_studio);
	ClassDB::bind_method(D_METHOD("get_studio"), &InterDVDDisc::get_studio);
	ClassDB::bind_method(D_METHOD("set_website", "website"), &InterDVDDisc::set_website);
	ClassDB::bind_method(D_METHOD("get_website"), &InterDVDDisc::get_website);
	ClassDB::bind_method(D_METHOD("set_version", "version"), &InterDVDDisc::set_version);
	ClassDB::bind_method(D_METHOD("get_version"), &InterDVDDisc::get_version);
	ClassDB::bind_method(D_METHOD("set_first_play", "mode"), &InterDVDDisc::set_first_play);
	ClassDB::bind_method(D_METHOD("get_first_play"), &InterDVDDisc::get_first_play);
	ClassDB::bind_method(D_METHOD("set_first_play_path", "path"), &InterDVDDisc::set_first_play_path);
	ClassDB::bind_method(D_METHOD("get_first_play_path"), &InterDVDDisc::get_first_play_path);
	ClassDB::bind_method(D_METHOD("apply_settings_to", "project"), &InterDVDDisc::apply_settings_to);
	ClassDB::bind_method(D_METHOD("add_title", "name"), &InterDVDDisc::add_title, DEFVAL("Title"));
	ClassDB::bind_method(D_METHOD("add_chapter_to_last", "name"), &InterDVDDisc::add_chapter_to_last, DEFVAL("Chapter"));
	ClassDB::bind_method(D_METHOD("add_title_menu", "name"), &InterDVDDisc::add_title_menu, DEFVAL("TitleMenu"));
	ClassDB::bind_method(D_METHOD("add_root_menu", "name"), &InterDVDDisc::add_root_menu, DEFVAL("RootMenu"));
	ClassDB::bind_method(D_METHOD("add_title_set", "name"), &InterDVDDisc::add_title_set, DEFVAL("TitleSet"));
	ClassDB::bind_method(D_METHOD("add_menu_title", "name"), &InterDVDDisc::add_menu_title, DEFVAL("Menu"));
	ClassDB::bind_method(D_METHOD("apply_scene_owner", "owner"), &InterDVDDisc::apply_scene_owner);
	ClassDB::bind_method(D_METHOD("build_project"), &InterDVDDisc::build_project);
	ClassDB::bind_static_method("InterDVDDisc", D_METHOD("create_starter"), &InterDVDDisc::create_starter);
	ClassDB::bind_static_method("InterDVDDisc", D_METHOD("find_in_tree", "root"), &InterDVDDisc::find_in_tree);
	ADD_GROUP("Disc", "");
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "disc_title"), "set_disc_title", "get_disc_title");
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "volume_id"), "set_volume_id", "get_volume_id");
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "serial"), "set_serial", "get_serial");
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "provider_id"), "set_provider_id", "get_provider_id");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "region_mask"), "set_region_mask", "get_region_mask");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "parental_level"), "set_parental_level", "get_parental_level");
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "menu_language"), "set_menu_language", "get_menu_language");
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "audio_language"), "set_audio_language", "get_audio_language");
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "subtitle_language"), "set_subtitle_language", "get_subtitle_language");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "first_play", PROPERTY_HINT_ENUM, "Title Menu,Main Title,Custom Path"), "set_first_play", "get_first_play");
	ADD_PROPERTY(PropertyInfo(Variant::NODE_PATH, "first_play_path", PROPERTY_HINT_NODE_PATH_VALID_TYPES, "InterDVDTitle"), "set_first_play_path", "get_first_play_path");
	ADD_GROUP("Encode", "");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "ac3_bitrate_k", PROPERTY_HINT_RANGE, "64,448,32"), "set_ac3_bitrate_k", "get_ac3_bitrate_k");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "ac3_channels", PROPERTY_HINT_RANGE, "1,6,1"), "set_ac3_channels", "get_ac3_channels");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "gop_size", PROPERTY_HINT_RANGE, "1,30,1"), "set_gop_size", "get_gop_size");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "title_safe_bottom", PROPERTY_HINT_RANGE, "2,480,2"), "set_title_safe_bottom", "get_title_safe_bottom");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "bake_warmup_frames", PROPERTY_HINT_RANGE, "0,30,1"), "set_bake_warmup_frames", "get_bake_warmup_frames");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "pip_blackdetect_sec", PROPERTY_HINT_RANGE, "0,10,0.1"), "set_pip_blackdetect_sec", "get_pip_blackdetect_sec");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "pip_blackdetect_pix_th", PROPERTY_HINT_RANGE, "0,1,0.01"), "set_pip_blackdetect_pix_th", "get_pip_blackdetect_pix_th");
	ADD_GROUP("Extras", "");
	ADD_PROPERTY(PropertyInfo(Variant::PACKED_STRING_ARRAY, "extras"), "set_extras", "get_extras");
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "extras_dir", PROPERTY_HINT_DIR), "set_extras_dir", "get_extras_dir");
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "copyright", PROPERTY_HINT_MULTILINE_TEXT), "set_copyright", "get_copyright");
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "copyright_file", PROPERTY_HINT_FILE), "set_copyright_file", "get_copyright_file");
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "license", PROPERTY_HINT_MULTILINE_TEXT), "set_license", "get_license");
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "license_file", PROPERTY_HINT_FILE), "set_license_file", "get_license_file");
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "readme", PROPERTY_HINT_MULTILINE_TEXT), "set_readme", "get_readme");
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "readme_file", PROPERTY_HINT_FILE), "set_readme_file", "get_readme_file");
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "credits", PROPERTY_HINT_MULTILINE_TEXT), "set_credits", "get_credits");
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "credits_file", PROPERTY_HINT_FILE), "set_credits_file", "get_credits_file");
	ADD_GROUP("Autorun", "");
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "autorun_label"), "set_autorun_label", "get_autorun_label");
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "autorun_open"), "set_autorun_open", "get_autorun_open");
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "autorun_icon", PROPERTY_HINT_FILE), "set_autorun_icon", "get_autorun_icon");
	ADD_GROUP("Metadata", "");
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "publisher"), "set_publisher", "get_publisher");
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "author"), "set_author", "get_author");
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "studio"), "set_studio", "get_studio");
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "website"), "set_website", "get_website");
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "version"), "set_version", "get_version");
	BIND_ENUM_CONSTANT(FIRST_PLAY_TITLE_MENU);
	BIND_ENUM_CONSTANT(FIRST_PLAY_MAIN_TITLE);
	BIND_ENUM_CONSTANT(FIRST_PLAY_PATH);
}

void InterDVDDisc::set_first_play(FirstPlay p_play) {
	if (first_play == p_play) {
		return;
	}
	first_play = p_play;
	notify_property_list_changed();
}

void InterDVDDisc::_validate_property(PropertyInfo &p_property) const {
	if (p_property.name == StringName("first_play_path") && first_play != FIRST_PLAY_PATH) {
		p_property.usage = PROPERTY_USAGE_NO_EDITOR;
	}
}

String InterDVDDisc::unique_child_name(Node *p_parent, const String &p_base) {
	const String base = p_base.is_empty() ? String("Node") : p_base;
	if (!p_parent || !p_parent->has_node(NodePath(base))) {
		return base;
	}
	int i = 2;
	while (p_parent->has_node(NodePath(base + itos(i)))) {
		i++;
	}
	return base + itos(i);
}

InterDVDTitle *InterDVDDisc::add_title(const String &p_name) {
	InterDVDTitle *title = memnew(InterDVDTitle);
	title->set_name(unique_child_name(this, p_name.is_empty() ? String("Title") : p_name));
	add_child(title);
	return title;
}

InterDVDChapter *InterDVDDisc::add_chapter_to_last(const String &p_name) {
	InterDVDTitle *last = nullptr;
	visit_titles(this, [&](InterDVDTitle *title) {
		last = title;
		return true;
	});
	if (!last) {
		last = add_title("Title");
	}
	return last->add_chapter(p_name);
}

InterDVDMenuPage *InterDVDDisc::add_title_menu(const String &p_name) {
	InterDVDMenuPage *menu = memnew(InterDVDMenuPage);
	menu->set_name(unique_child_name(this, p_name.is_empty() ? String("TitleMenu") : p_name));
	menu->set_menu_type(InterDVDMenu::MENU_TITLE);
	menu->set_still_time(0);
	add_child(menu);
	return menu;
}

InterDVDMenuPage *InterDVDDisc::add_root_menu(const String &p_name) {
	InterDVDTitleSet *set = nullptr;
	for (int i = get_child_count() - 1; i >= 0; i--) {
		set = Object::cast_to<InterDVDTitleSet>(get_child(i));
		if (set) {
			break;
		}
	}
	if (set) {
		return set->add_root_menu(p_name);
	}
	InterDVDMenuPage *menu = memnew(InterDVDMenuPage);
	menu->set_name(unique_child_name(this, p_name.is_empty() ? String("RootMenu") : p_name));
	menu->set_menu_type(InterDVDMenu::MENU_ROOT);
	menu->set_still_time(0);
	add_child(menu);
	return menu;
}

InterDVDTitleSet *InterDVDDisc::add_title_set(const String &p_name) {
	InterDVDTitleSet *set = memnew(InterDVDTitleSet);
	set->set_name(unique_child_name(this, p_name.is_empty() ? String("TitleSet") : p_name));
	add_child(set);
	return set;
}

InterDVDTitle *InterDVDDisc::add_menu_title(const String &p_name) {
	InterDVDTitle *title = add_title(p_name.is_empty() ? String("Menu") : p_name);
	title->set_menu_title(true);
	title->set_still_time(255);
	return title;
}

void InterDVDDisc::apply_scene_owner(Node *p_owner) {
	if (!p_owner) {
		return;
	}
	if (this != p_owner) {
		set_owner(p_owner);
	}
	own_descendants(this, p_owner);
}

InterDVDDisc *InterDVDDisc::find_in_tree(Node *p_root) {
	if (!p_root) {
		return nullptr;
	}
	if (InterDVDDisc *disc = Object::cast_to<InterDVDDisc>(p_root)) {
		return disc;
	}
	for (int i = 0; i < p_root->get_child_count(); i++) {
		if (InterDVDDisc *disc = find_in_tree(p_root->get_child(i))) {
			return disc;
		}
	}
	return nullptr;
}

InterDVDDisc *InterDVDDisc::create_starter() {
	InterDVDDisc *disc = memnew(InterDVDDisc);
	disc->set_name("Disc");
	InterDVDMenuPage *menu = disc->add_title_menu("TitleMenu");
	InterDVDHotspot *play = menu->add_hotspot("Play");
	play->set_destination(NodePath("../Main"));
	InterDVDTitle *main = disc->add_title("Main");
	main->add_chapter("Chapter1");
	InterDVDTitle *menu_title = disc->add_menu_title("Menu");
	InterDVDHotspot *menu_play = menu_title->add_hotspot("Play");
	menu_play->set_destination(NodePath("../Main"));
	return disc;
}

void InterDVDDisc::apply_settings_to(const Ref<InterDVDProject> &p_project) const {
	ERR_FAIL_COND(p_project.is_null());
	p_project->set_disc_title(disc_title);
	p_project->set_volume_id(volume_id);
	p_project->set_serial(serial);
	p_project->set_provider_id(provider_id);
	p_project->set_region_mask(region_mask);
	p_project->set_parental_level(parental_level);
	p_project->set_menu_language(menu_language);
	p_project->set_audio_language(audio_language);
	p_project->set_subtitle_language(subtitle_language);
	p_project->set_ac3_bitrate_k(ac3_bitrate_k);
	p_project->set_ac3_channels(ac3_channels);
	p_project->set_gop_size(gop_size);
	p_project->set_title_safe_bottom(title_safe_bottom);
	p_project->set_bake_warmup_frames(bake_warmup_frames);
	p_project->set_pip_blackdetect_sec(pip_blackdetect_sec);
	p_project->set_pip_blackdetect_pix_th(pip_blackdetect_pix_th);
	p_project->set_extras(extras);
	p_project->set_extras_dir(extras_dir);
	p_project->set_copyright(copyright);
	p_project->set_copyright_file(copyright_file);
	p_project->set_license(license);
	p_project->set_license_file(license_file);
	p_project->set_readme(readme);
	p_project->set_readme_file(readme_file);
	p_project->set_credits(credits);
	p_project->set_credits_file(credits_file);
	p_project->set_autorun_label(autorun_label);
	p_project->set_autorun_open(autorun_open);
	p_project->set_autorun_icon(autorun_icon);
	p_project->set_publisher(publisher);
	p_project->set_author(author);
	p_project->set_studio(studio);
	p_project->set_website(website);
	p_project->set_version(version);
}

Ref<InterDVDProject> InterDVDDisc::build_project() const {
	Ref<InterDVDProject> project;
	project.instantiate();
	apply_settings_to(project);

	TypedArray<InterDVDPGC> titles;
	TypedArray<InterDVDMenu> menus;
	bool has_title_menu_buttons = false;
	int menu_title_pgc = 0;

	auto compile_title = [&](InterDVDTitle *title, int p_set) {
		const Ref<InterDVDPGC> pgc = title->compile_pgc();
		pgc->set_title_set_nr(p_set);
		if (pgc->get_goup_pgc() == 0 && menu_title_pgc > 0 && !title->is_menu_title()) {
			pgc->set_goup_pgc(menu_title_pgc);
		}
		resolve_parent_hotspots(this, title, pgc->get_buttons());
		titles.push_back(pgc);
		if (title->is_menu_title() && menu_title_pgc == 0) {
			menu_title_pgc = titles.size();
		}
	};
	auto compile_menu = [&](InterDVDMenuPage *page, int p_set) {
		const Ref<InterDVDMenu> menu = page->compile_menu();
		if (page->get_menu_type() != InterDVDMenu::MENU_TITLE) {
			menu->set_title_set_nr(p_set);
		} else {
			menu->set_title_set_nr(0);
		}
		if (menu->get_goup_pgc() == 0) {
			menu->set_goup_pgc(1);
		}
		resolve_parent_hotspots(this, page, menu->get_buttons());
		if (page->get_menu_type() == InterDVDMenu::MENU_TITLE && menu->get_buttons().size() > 0) {
			has_title_menu_buttons = true;
		}
		menus.push_back(menu);
	};

	int set_n = 0;
	for (int i = 0; i < get_child_count(); i++) {
		Node *child = get_child(i);
		if (InterDVDTitleSet *set = Object::cast_to<InterDVDTitleSet>(child)) {
			set_n++;
			for (int t = 0; t < set->get_child_count(); t++) {
				if (InterDVDTitle *title = Object::cast_to<InterDVDTitle>(set->get_child(t))) {
					compile_title(title, set_n);
				} else if (InterDVDMenuPage *page = Object::cast_to<InterDVDMenuPage>(set->get_child(t))) {
					compile_menu(page, set_n);
				}
			}
		} else if (InterDVDTitle *title = Object::cast_to<InterDVDTitle>(child)) {
			compile_title(title, 1);
		} else if (InterDVDMenuPage *page = Object::cast_to<InterDVDMenuPage>(child)) {
			compile_menu(page, 1);
		}
	}
	if (menu_title_pgc > 0) {
		for (int i = 0; i < titles.size(); i++) {
			Ref<InterDVDPGC> pgc = titles[i];
			if (pgc.is_valid() && pgc->get_goup_pgc() == 0 && i + 1 != menu_title_pgc) {
				pgc->set_goup_pgc(menu_title_pgc);
			}
		}
	}
	project->set_titles(titles);
	project->set_menus(menus);

	if (first_play == FIRST_PLAY_MAIN_TITLE) {
		project->ensure_first_play(main_title_index(this));
	} else if (first_play == FIRST_PLAY_PATH && !first_play_path.is_empty()) {
		if (const InterDVDTitle *title = Object::cast_to<InterDVDTitle>(resolve_disc_path(this, const_cast<InterDVDDisc *>(this), first_play_path))) {
			project->ensure_first_play(MAX(title_index_of(this, title), 1));
		}
	} else if (first_play == FIRST_PLAY_TITLE_MENU && !has_title_menu_buttons) {
		project->ensure_first_play(main_title_index(this));
	}

	return project;
}
