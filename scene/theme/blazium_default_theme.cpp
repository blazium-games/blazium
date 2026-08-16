/**************************************************************************/
/*  blazium_default_theme.cpp                                             */
/**************************************************************************/
/*                         This file is part of:                          */
/*                            BLAZIUM ENGINE                              */
/*                          https://blazium.app                           */
/**************************************************************************/
/* Copyright (c) 2024-present Blazium Engine contributors.                */
/* Copyright (c) 2014-present Godot Engine contributors (see AUTHORS.md). */
/* Copyright (c) 2007-2014 Juan Linietsky, Ariel Manzur.                  */
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

#include "blazium_default_theme.h"

#include "default_font.gen.h"
#include "default_theme_icons.gen.h"

#include "scene/resources/font.h"
#include "scene/resources/gradient_texture.h"
#include "scene/resources/image_texture.h"
#include "scene/resources/style_box.h"
#include "scene/resources/style_box_flat.h"
#include "scene/resources/style_box_line.h"
#include "scene/resources/theme.h"
#include "scene/scene_string_names.h"
#include "scene/theme/theme_db.h"
#include "servers/text/text_server.h"

#include "modules/modules_enabled.gen.h" // For svg.

static Dictionary blazium_theme_icons;
static Dictionary user_icons_sources;
static Dictionary user_icons;

static Ref<StyleBoxFlat> button_normal_style;
static Ref<StyleBoxFlat> button_pressed_style;
static Ref<StyleBoxFlat> button_hover_style;
static Ref<StyleBoxFlat> button_disabled_style;
static Ref<StyleBoxFlat> button_focus_style;
static Ref<StyleBoxEmpty> button_empty_style;

static Ref<StyleBoxFlat> color_button_normal_style;
static Ref<StyleBoxFlat> color_button_hover_style;
static Ref<StyleBoxFlat> color_button_pressed_style;
static Ref<StyleBoxFlat> color_button_disabled_style;
static Ref<StyleBoxFlat> color_button_focus_style;

static Ref<StyleBoxFlat> focus_rect_style;
static Ref<StyleBoxFlat> focus_circle_style;

static Ref<StyleBoxFlat> popup_hover_style;
static Ref<StyleBoxFlat> popup_panel_style;

static Ref<StyleBoxFlat> panel_style;
static Ref<StyleBoxFlat> flat_panel_style;

static Ref<StyleBoxFlat> foldable_panel_style;
static Ref<StyleBoxFlat> flat_foldable_panel_style;
static Ref<StyleBoxFlat> foldable_title_style;
static Ref<StyleBoxFlat> foldable_title_collapsed_style;
static Ref<StyleBoxFlat> foldable_title_hover_style;
static Ref<StyleBoxFlat> foldable_title_collapsed_hover_style;

static Ref<StyleBoxFlat> tab_selected_style;
static Ref<StyleBoxFlat> flat_tab_selected_style;
static Ref<StyleBoxFlat> tab_unselected_style;
static Ref<StyleBoxFlat> tab_hover_style;
static Ref<StyleBoxFlat> tab_focus_style;
static Ref<StyleBoxEmpty> tab_empty_style;
static Ref<StyleBoxFlat> tab_panel_style;
static Ref<StyleBoxFlat> flat_tab_panel_style;

static Ref<StyleBoxFlat> slider_style;
static Ref<StyleBoxFlat> grabber_style;
static Ref<StyleBoxFlat> grabber_highlight_style;

static Ref<StyleBoxFlat> embedded_style;
static Ref<StyleBoxFlat> embedded_unfocused_style;

static Ref<StyleBoxFlat> progress_background_style;
static Ref<StyleBoxFlat> progress_fill_style;

static Ref<StyleBoxFlat> graph_title_style;
static Ref<StyleBoxFlat> graph_frame_title_style;
static Ref<StyleBoxFlat> graph_frame_title_selected_style;
static Ref<StyleBoxFlat> graph_title_selected_style;
static Ref<StyleBoxFlat> graph_panel_style;
static Ref<StyleBoxFlat> graph_panel_selected_style;

static Ref<StyleBoxFlat> h_split_bar_background;
static Ref<StyleBoxFlat> v_split_bar_background;

static Ref<StyleBoxFlat> code_edit_completion_style;

static Ref<StyleBoxFlat> h_scroll_style;
static Ref<StyleBoxFlat> v_scroll_style;

static Ref<StyleBoxLine> h_separator_style;
static Ref<StyleBoxLine> v_separator_style;

static Ref<FontVariation> font_variation;
static Ref<FontVariation> custom_font_variation;
static Ref<FontVariation> blazium_bold_font;
static Ref<FontVariation> blazium_bold_italics_font;
static Ref<FontVariation> blazium_italics_font;

static bool is_dark_theme = false;

#ifdef MODULE_SVG_ENABLED
static Ref<Image> generate_icon(const String &p_source, float p_scale, const Color &p_font_color, const Color &p_accent_color) {
	Ref<Image> img = memnew(Image);

	String svg = p_source;
	svg = svg.replace("\"red\"", vformat("\"#%s\"", p_font_color.to_html(false)));
	svg = svg.replace("\"#0f0\"", vformat("\"#%s\"", p_accent_color.to_html(false)));

	Error err = img->load_svg_from_string(svg, p_scale);
	if (err == OK) {
		img->fix_alpha_edges();
	}
	ERR_FAIL_COND_V_MSG(err != OK, Ref<Image>(), "Failed generating icon, unsupported or invalid SVG data in default theme.");

	return img;
}
#endif // MODULE_SVG_ENABLED

static void update_user_icon(const String &p_icon_name, Ref<Image> &p_image) {
	if (user_icons.has(p_icon_name) && ((Ref<ImageTexture>)user_icons[p_icon_name])->get_size() == (Vector2)p_image->get_size()) {
		((Ref<ImageTexture>)user_icons[p_icon_name])->update(p_image);
	} else {
		user_icons[p_icon_name] = ImageTexture::create_from_image(p_image);
	}
}

Error add_user_icon(const String &p_icon_name, const String &p_icon_source, float p_scale, const Color &p_font_color, const Color &p_accent_color) {
	ERR_FAIL_COND_V_MSG(p_icon_name.is_empty(), ERR_INVALID_PARAMETER, "Icon name cannot be empty.");
	ERR_FAIL_COND_V_MSG(p_icon_source.is_empty(), ERR_INVALID_PARAMETER, "Icon Source cannot be empty.");
	ERR_FAIL_COND_V_MSG(user_icons_sources.has(p_icon_name), ERR_ALREADY_EXISTS, vformat("An icon with same name: \"%s\" already exists.", p_icon_name));

#ifdef MODULE_SVG_ENABLED
	Ref<Image> img = generate_icon(p_icon_source, p_scale, p_font_color, p_accent_color);
	ERR_FAIL_COND_V_MSG(img.is_null(), ERR_INVALID_DATA, vformat("Icon: \"%s\" has an invalid svg source.", p_icon_name));
#else
	Ref<Image> img = Image::create_empty(Math::round(16 * p_scale), Math::round(16 * p_scale), false, Image::FORMAT_RGBA8);
#endif // MODULE_SVG_ENABLED

	user_icons_sources[p_icon_name] = p_icon_source;
	update_user_icon(p_icon_name, img);
	return OK;
}

Error remove_user_icon(const String &p_icon_name) {
	ERR_FAIL_COND_V_MSG(p_icon_name.is_empty(), ERR_INVALID_PARAMETER, "Icon name cannot be empty.");
	ERR_FAIL_COND_V_MSG(!user_icons.has(p_icon_name), ERR_DOES_NOT_EXIST, vformat("Icon: \"%s\" does not exist.", p_icon_name));

	user_icons_sources.erase(p_icon_name);
	user_icons.erase(p_icon_name);
	return OK;
}

bool has_user_icon(const String &p_icon_name) {
	ERR_FAIL_COND_V_MSG(p_icon_name.is_empty(), false, "Icon name cannot be empty.");

	return user_icons.has(p_icon_name);
}

Ref<ImageTexture> get_user_icon(const String &p_icon_name) {
	ERR_FAIL_COND_V_MSG(p_icon_name.is_empty(), Ref<ImageTexture>(), "Icon name cannot be empty.");

	if (user_icons.has(p_icon_name)) {
		return user_icons[p_icon_name];
	}
	return Ref<ImageTexture>();
}

PackedStringArray get_user_icons_list() {
	PackedStringArray icons_list;
	for (const String p_icon_name : user_icons.keys()) {
		icons_list.push_back(p_icon_name);
	}
	return icons_list;
}

bool has_icon(const String &p_icon_name) {
	ERR_FAIL_COND_V_MSG(p_icon_name.is_empty(), false, "Icon name cannot be empty.");

	return blazium_theme_icons.has(p_icon_name);
}

Ref<ImageTexture> get_icon(const String &p_icon_name) {
	ERR_FAIL_COND_V_MSG(p_icon_name.is_empty(), Ref<ImageTexture>(), "Icon name cannot be empty.");

	if (blazium_theme_icons.has(p_icon_name)) {
		return blazium_theme_icons[p_icon_name];
	}
	return Ref<ImageTexture>();
}

PackedStringArray get_icons_list() {
	PackedStringArray icons_list;
	for (const String p_icon_name : blazium_theme_icons.keys()) {
		icons_list.push_back(p_icon_name);
	}
	return icons_list;
}

void update_theme_icons(const Ref<Theme> &p_theme, const Color &p_font_color, const Color &p_accent_color) {
	if (blazium_theme_icons.is_empty()) {
		Ref<Texture2D> empty_icon = memnew(ImageTexture);
		p_theme->set_icon("increment", "HScrollBar", empty_icon);
		p_theme->set_icon("increment_highlight", "HScrollBar", empty_icon);
		p_theme->set_icon("increment_pressed", "HScrollBar", empty_icon);
		p_theme->set_icon("decrement", "HScrollBar", empty_icon);
		p_theme->set_icon("decrement_highlight", "HScrollBar", empty_icon);
		p_theme->set_icon("decrement_pressed", "HScrollBar", empty_icon);
		p_theme->set_icon("increment", "VScrollBar", empty_icon);
		p_theme->set_icon("increment_highlight", "VScrollBar", empty_icon);
		p_theme->set_icon("increment_pressed", "VScrollBar", empty_icon);
		p_theme->set_icon("decrement", "VScrollBar", empty_icon);
		p_theme->set_icon("decrement_highlight", "VScrollBar", empty_icon);
		p_theme->set_icon("decrement_pressed", "VScrollBar", empty_icon);
		p_theme->set_icon("updown", "SpinBox", empty_icon);
		{
			const int precision = 7;

			Ref<Gradient> hue_gradient;
			hue_gradient.instantiate();
			PackedFloat32Array offsets;
			offsets.resize(precision);
			PackedColorArray colors;
			colors.resize(precision);

			for (int i = 0; i < precision; i++) {
				float h = i / float(precision - 1);
				offsets.write[i] = h;
				colors.write[i] = Color::from_hsv(h, 1, 1);
			}
			hue_gradient->set_offsets(offsets);
			hue_gradient->set_colors(colors);

			Ref<GradientTexture2D> hue_texture;
			hue_texture.instantiate();
			hue_texture->set_width(512);
			hue_texture->set_height(1);
			hue_texture->set_gradient(hue_gradient);
			p_theme->set_icon("color_hue", "ColorPicker", hue_texture);
		}

		{
			const int precision = 7;

			Ref<Gradient> hue_gradient;
			hue_gradient.instantiate();
			PackedFloat32Array offsets;
			offsets.resize(precision);
			PackedColorArray colors;
			colors.resize(precision);

			for (int i = 0; i < precision; i++) {
				float h = i / float(precision - 1);
				offsets.write[i] = h;
				colors.write[i] = Color::from_ok_hsl(h, 1, 0.5);
			}
			hue_gradient->set_offsets(offsets);
			hue_gradient->set_colors(colors);

			Ref<GradientTexture2D> hue_texture;
			hue_texture.instantiate();
			hue_texture->set_width(512);
			hue_texture->set_height(1);
			hue_texture->set_gradient(hue_gradient);

			p_theme->set_icon("color_okhsl_hue", "ColorPicker", hue_texture);
		}
	}

	const float theme_scale = p_theme->get_default_base_scale();
#ifdef MODULE_SVG_ENABLED
	const Color font_color = p_font_color.clamp();
	const Color accent_color = p_accent_color.clamp();
#else
	Ref<Image> img = Image::create_empty(Math::round(16 * theme_scale), Math::round(16 * theme_scale), false, Image::FORMAT_RGBA8);
#endif // MODULE_SVG_ENABLED

	for (int i = 0; i < default_theme_icons_count; i++) {
#ifdef MODULE_SVG_ENABLED
		Ref<Image> img = generate_icon(default_theme_icons_sources[i], theme_scale, font_color, accent_color);
#endif // MODULE_SVG_ENABLED
		if (blazium_theme_icons.has(default_theme_icons_names[i]) && ((Ref<ImageTexture>)blazium_theme_icons[default_theme_icons_names[i]])->get_size() == (Vector2)img->get_size()) {
			((Ref<ImageTexture>)blazium_theme_icons[default_theme_icons_names[i]])->update(img);
		} else {
			blazium_theme_icons[default_theme_icons_names[i]] = ImageTexture::create_from_image(img);
		}
	}

	for (const String icon_name : user_icons_sources.keys()) {
#ifdef MODULE_SVG_ENABLED
		Ref<Image> img = generate_icon(user_icons_sources[icon_name], theme_scale, font_color, accent_color);
#endif // MODULE_SVG_ENABLED
		update_user_icon(icon_name, img);
	}

	p_theme->set_icon("checked", "CheckBox", blazium_theme_icons["checked"]);
	p_theme->set_icon("checked_disabled", "CheckBox", blazium_theme_icons["checked_disabled"]);
	p_theme->set_icon("unchecked", "CheckBox", blazium_theme_icons["unchecked"]);
	p_theme->set_icon("unchecked_disabled", "CheckBox", blazium_theme_icons["unchecked_disabled"]);
	p_theme->set_icon("radio_checked", "CheckBox", blazium_theme_icons["radio_checked"]);
	p_theme->set_icon("radio_checked_disabled", "CheckBox", blazium_theme_icons["radio_checked_disabled"]);
	p_theme->set_icon("radio_unchecked", "CheckBox", blazium_theme_icons["radio_unchecked"]);
	p_theme->set_icon("radio_unchecked_disabled", "CheckBox", blazium_theme_icons["radio_unchecked_disabled"]);

	p_theme->set_icon("checked", "CheckButton", blazium_theme_icons["toggle_on"]);
	p_theme->set_icon("checked_disabled", "CheckButton", blazium_theme_icons["toggle_on_disabled"]);
	p_theme->set_icon("unchecked", "CheckButton", blazium_theme_icons["toggle_off"]);
	p_theme->set_icon("unchecked_disabled", "CheckButton", blazium_theme_icons["toggle_off_disabled"]);
	p_theme->set_icon("checked_mirrored", "CheckButton", blazium_theme_icons["toggle_on_mirrored"]);
	p_theme->set_icon("checked_disabled_mirrored", "CheckButton", blazium_theme_icons["toggle_on_disabled_mirrored"]);
	p_theme->set_icon("unchecked_mirrored", "CheckButton", blazium_theme_icons["toggle_off_mirrored"]);
	p_theme->set_icon("unchecked_disabled_mirrored", "CheckButton", blazium_theme_icons["toggle_off_disabled_mirrored"]);

	p_theme->set_icon("up", "SpinBox", blazium_theme_icons["value_up"]);
	p_theme->set_icon("up_hover", "SpinBox", blazium_theme_icons["value_up"]);
	p_theme->set_icon("up_pressed", "SpinBox", blazium_theme_icons["value_up"]);
	p_theme->set_icon("up_disabled", "SpinBox", blazium_theme_icons["value_up"]);
	p_theme->set_icon("down", "SpinBox", blazium_theme_icons["value_down"]);
	p_theme->set_icon("down_hover", "SpinBox", blazium_theme_icons["value_down"]);
	p_theme->set_icon("down_pressed", "SpinBox", blazium_theme_icons["value_down"]);
	p_theme->set_icon("down_disabled", "SpinBox", blazium_theme_icons["value_down"]);

	p_theme->set_icon("arrow", "OptionButton", blazium_theme_icons["option_button_arrow"]);

	p_theme->set_icon("clear", "LineEdit", blazium_theme_icons["line_edit_clear"]);

	p_theme->set_icon("tab", "TextEdit", blazium_theme_icons["text_edit_tab"]);
	p_theme->set_icon("space", "TextEdit", blazium_theme_icons["text_edit_space"]);

	p_theme->set_icon("tab", "CodeEdit", blazium_theme_icons["text_edit_tab"]);
	p_theme->set_icon("space", "CodeEdit", blazium_theme_icons["text_edit_space"]);
	p_theme->set_icon("breakpoint", "CodeEdit", blazium_theme_icons["breakpoint"]);
	p_theme->set_icon("bookmark", "CodeEdit", blazium_theme_icons["bookmark"]);
	p_theme->set_icon("executing_line", "CodeEdit", blazium_theme_icons["arrow_right"]);
	p_theme->set_icon("can_fold", "CodeEdit", blazium_theme_icons["arrow_down"]);
	p_theme->set_icon("folded", "CodeEdit", blazium_theme_icons["arrow_right"]);
	p_theme->set_icon("can_fold_code_region", "CodeEdit", blazium_theme_icons["region_unfolded"]);
	p_theme->set_icon("folded_code_region", "CodeEdit", blazium_theme_icons["region_folded"]);
	p_theme->set_icon("folded_eol_icon", "CodeEdit", blazium_theme_icons["text_edit_ellipsis"]);
	p_theme->set_icon("completion_color_bg", "CodeEdit", blazium_theme_icons["mini_checkerboard"]);

	p_theme->set_icon("grabber", "HSlider", blazium_theme_icons["slider_grabber"]);
	p_theme->set_icon("grabber_highlight", "HSlider", blazium_theme_icons["slider_grabber_hl"]);
	p_theme->set_icon("grabber_disabled", "HSlider", blazium_theme_icons["slider_grabber_disabled"]);
	p_theme->set_icon("tick", "HSlider", blazium_theme_icons["hslider_tick"]);

	p_theme->set_icon("grabber", "VSlider", blazium_theme_icons["slider_grabber"]);
	p_theme->set_icon("grabber_highlight", "VSlider", blazium_theme_icons["slider_grabber_hl"]);
	p_theme->set_icon("grabber_disabled", "VSlider", blazium_theme_icons["slider_grabber_disabled"]);
	p_theme->set_icon("tick", "VSlider", blazium_theme_icons["vslider_tick"]);

	p_theme->set_icon("checked", "Tree", blazium_theme_icons["checked"]);
	p_theme->set_icon("checked_disabled", "Tree", blazium_theme_icons["checked_disabled"]);
	p_theme->set_icon("unchecked", "Tree", blazium_theme_icons["unchecked"]);
	p_theme->set_icon("unchecked_disabled", "Tree", blazium_theme_icons["unchecked_disabled"]);
	p_theme->set_icon("indeterminate", "Tree", blazium_theme_icons["indeterminate"]);
	p_theme->set_icon("indeterminate_disabled", "Tree", blazium_theme_icons["indeterminate_disabled"]);
	p_theme->set_icon("updown", "Tree", blazium_theme_icons["updown"]);
	p_theme->set_icon("select_arrow", "Tree", blazium_theme_icons["option_button_arrow"]);
	p_theme->set_icon("arrow", "Tree", blazium_theme_icons["arrow_down"]);
	p_theme->set_icon("arrow_collapsed", "Tree", blazium_theme_icons["arrow_right"]);
	p_theme->set_icon("arrow_collapsed_mirrored", "Tree", blazium_theme_icons["arrow_left"]);

	p_theme->set_icon("h_grabber", "SplitContainer", blazium_theme_icons["hsplitter"]);
	p_theme->set_icon("v_grabber", "SplitContainer", blazium_theme_icons["vsplitter"]);
	p_theme->set_icon("grabber", "VSplitContainer", blazium_theme_icons["vsplitter"]);
	p_theme->set_icon("grabber", "HSplitContainer", blazium_theme_icons["hsplitter"]);

	p_theme->set_icon("increment", "TabContainer", blazium_theme_icons["scroll_button_right"]);
	p_theme->set_icon("increment_highlight", "TabContainer", blazium_theme_icons["scroll_button_right_hl"]);
	p_theme->set_icon("decrement", "TabContainer", blazium_theme_icons["scroll_button_left"]);
	p_theme->set_icon("decrement_highlight", "TabContainer", blazium_theme_icons["scroll_button_left_hl"]);
	p_theme->set_icon("menu", "TabContainer", blazium_theme_icons["tabs_menu"]);
	p_theme->set_icon("menu_highlight", "TabContainer", blazium_theme_icons["tabs_menu_hl"]);
	p_theme->set_icon("drop_mark", "TabContainer", blazium_theme_icons["tabs_drop_mark"]);

	p_theme->set_icon("increment", "TabBar", blazium_theme_icons["scroll_button_right"]);
	p_theme->set_icon("increment_highlight", "TabBar", blazium_theme_icons["scroll_button_right_hl"]);
	p_theme->set_icon("decrement", "TabBar", blazium_theme_icons["scroll_button_left"]);
	p_theme->set_icon("decrement_highlight", "TabBar", blazium_theme_icons["scroll_button_left_hl"]);
	p_theme->set_icon("close", "TabBar", blazium_theme_icons["close"]);
	p_theme->set_icon("drop_mark", "TabBar", blazium_theme_icons["tabs_drop_mark"]);

	p_theme->set_icon("folded_arrow", "ColorPicker", blazium_theme_icons["arrow_right"]);
	p_theme->set_icon("folded_arrow_mirrored", "ColorPicker", blazium_theme_icons["arrow_left"]);
	p_theme->set_icon("expanded_arrow", "ColorPicker", blazium_theme_icons["arrow_down"]);
	p_theme->set_icon("menu_option", "ColorPicker", blazium_theme_icons["tabs_menu_hl"]);
	p_theme->set_icon("screen_picker", "ColorPicker", blazium_theme_icons["color_picker_pipette"]);
	p_theme->set_icon("shape_circle", "ColorPicker", blazium_theme_icons["picker_shape_circle"]);
	p_theme->set_icon("shape_rect", "ColorPicker", blazium_theme_icons["picker_shape_rectangle"]);
	p_theme->set_icon("shape_rect_wheel", "ColorPicker", blazium_theme_icons["picker_shape_rectangle_wheel"]);
	p_theme->set_icon("add_preset", "ColorPicker", blazium_theme_icons["add"]);
	p_theme->set_icon("sample_bg", "ColorPicker", blazium_theme_icons["mini_checkerboard"]);
	p_theme->set_icon("sample_revert", "ColorPicker", blazium_theme_icons["reload"]);
	p_theme->set_icon("overbright_indicator", "ColorPicker", blazium_theme_icons["color_picker_overbright"]);
	p_theme->set_icon("bar_arrow", "ColorPicker", blazium_theme_icons["color_picker_bar_arrow"]);
	p_theme->set_icon("picker_cursor", "ColorPicker", blazium_theme_icons["color_picker_cursor"]);
	p_theme->set_icon("picker_cursor_bg", "ColorPicker", blazium_theme_icons["color_picker_cursor_bg"]);
	p_theme->set_icon("hex_icon", "ColorPicker", blazium_theme_icons["color_picker_hex"]);
	p_theme->set_icon("code_icon", "ColorPicker", blazium_theme_icons["script"]);
	p_theme->set_icon("color_copy", "ColorPicker", blazium_theme_icons["action_copy"]);

	p_theme->set_icon("bg", "ColorPickerButton", blazium_theme_icons["mini_checkerboard"]);
	p_theme->set_icon("overbright_indicator", "ColorPickerButton", blazium_theme_icons["color_picker_overbright"]);

	p_theme->set_icon("bg", "ColorButton", blazium_theme_icons["mini_checkerboard"]);
	p_theme->set_icon("overbright_indicator", "ColorButton", blazium_theme_icons["color_picker_overbright"]);

	p_theme->set_icon("expanded_arrow", "FoldableContainer", blazium_theme_icons["expanded_arrow"]);
	p_theme->set_icon("expanded_arrow_mirrored", "FoldableContainer", blazium_theme_icons["expanded_arrow_mirrored"]);
	p_theme->set_icon("folded_arrow", "FoldableContainer", blazium_theme_icons["folded_arrow"]);
	p_theme->set_icon("folded_arrow_mirrored", "FoldableContainer", blazium_theme_icons["folded_arrow_mirrored"]);

	p_theme->set_icon("port", "GraphNode", blazium_theme_icons["graph_port"]);
	p_theme->set_icon("resizer", "GraphNode", blazium_theme_icons["resizer_se"]);
	p_theme->set_icon("resizer", "GraphFrame", blazium_theme_icons["resizer_se"]);
	p_theme->set_icon("zoom_out", "GraphEdit", blazium_theme_icons["zoom_less"]);
	p_theme->set_icon("zoom_in", "GraphEdit", blazium_theme_icons["zoom_more"]);
	p_theme->set_icon("zoom_reset", "GraphEdit", blazium_theme_icons["zoom_reset"]);
	p_theme->set_icon("grid_toggle", "GraphEdit", blazium_theme_icons["grid_toggle"]);
	p_theme->set_icon("minimap_toggle", "GraphEdit", blazium_theme_icons["grid_minimap"]);
	p_theme->set_icon("snapping_toggle", "GraphEdit", blazium_theme_icons["grid_snap"]);
	p_theme->set_icon("layout", "GraphEdit", blazium_theme_icons["grid_layout"]);
	p_theme->set_icon("resizer", "GraphEditMinimap", blazium_theme_icons["resizer_nw"]);

	p_theme->set_icon("parent_folder", "FileDialog", blazium_theme_icons["folder_up"]);
	p_theme->set_icon("back_folder", "FileDialog", blazium_theme_icons["arrow_left"]);
	p_theme->set_icon("forward_folder", "FileDialog", blazium_theme_icons["arrow_right"]);
	p_theme->set_icon("reload", "FileDialog", blazium_theme_icons["reload"]);
	p_theme->set_icon("toggle_hidden", "FileDialog", blazium_theme_icons["visibility_visible"]);
	p_theme->set_icon("folder", "FileDialog", blazium_theme_icons["folder"]);
	p_theme->set_icon("file", "FileDialog", blazium_theme_icons["file"]);
	p_theme->set_icon("create_folder", "FileDialog", blazium_theme_icons["folder_create"]);
	p_theme->set_icon("load", "FileDialog", blazium_theme_icons["load"]);
	p_theme->set_icon("save", "FileDialog", blazium_theme_icons["save"]);
	p_theme->set_icon("clear", "FileDialog", blazium_theme_icons["clear"]);

	p_theme->set_icon("checked", "PopupMenu", blazium_theme_icons["checked"]);
	p_theme->set_icon("checked_disabled", "PopupMenu", blazium_theme_icons["checked_disabled"]);
	p_theme->set_icon("unchecked", "PopupMenu", blazium_theme_icons["unchecked"]);
	p_theme->set_icon("unchecked_disabled", "PopupMenu", blazium_theme_icons["unchecked_disabled"]);
	p_theme->set_icon("radio_checked", "PopupMenu", blazium_theme_icons["radio_checked"]);
	p_theme->set_icon("radio_checked_disabled", "PopupMenu", blazium_theme_icons["radio_checked_disabled"]);
	p_theme->set_icon("radio_unchecked", "PopupMenu", blazium_theme_icons["radio_unchecked"]);
	p_theme->set_icon("radio_unchecked_disabled", "PopupMenu", blazium_theme_icons["radio_unchecked_disabled"]);
	p_theme->set_icon("submenu", "PopupMenu", blazium_theme_icons["popup_menu_arrow_right"]);
	p_theme->set_icon("submenu_mirrored", "PopupMenu", blazium_theme_icons["popup_menu_arrow_left"]);
	p_theme->set_icon("search", "PopupMenu", blazium_theme_icons["search"]);

	p_theme->set_icon("close", "Window", blazium_theme_icons["close"]);
	p_theme->set_icon("close_pressed", "Window", blazium_theme_icons["close_hl"]);

	p_theme->set_icon("zoom_less", "ZoomWidget", blazium_theme_icons["zoom_less"]);
	p_theme->set_icon("zoom_more", "ZoomWidget", blazium_theme_icons["zoom_more"]);

	p_theme->set_icon("close", "Icons", blazium_theme_icons["close"]);
	p_theme->set_icon("error_icon", "Icons", blazium_theme_icons["error_icon"]);

	ThemeDB::get_singleton()->set_fallback_icon(blazium_theme_icons["error_icon"]);
}

Color contrast_color(const Color &p_color, float p_contrast) {
	if (Math::is_zero_approx(p_contrast)) {
		return p_color;
	}

	Color contrast_color = is_dark_theme ? Color(1, 1, 1) : Color(0, 0, 0);
	if (p_contrast < 0.f) {
		contrast_color.invert();
		p_contrast *= -1;
	}

	return p_color.lerp(contrast_color, p_contrast).clamp();
}

// `Panel` and `PanelContainer` uses the `base_color` by default, another variation `FlatPanel` and `FlatPanelContainer` uses `bg_color`.
// `TabContainer` and `FoldableContainer` panels uses the `bg_color` by default, another variation `FlatTabContainer` and `FlatFoldableContainer` uses `base_color`.
// Other panels that doesn't allow having children like `Tree` and `ItemList` uses `style_normal_color` by default.
void update_theme_colors(const Ref<Theme> &p_theme, const Color &p_base_color, const Color &p_accent_color, float p_contrast, float p_normal_contrast, float p_hover_contrast, float p_pressed_contrast, float p_bg_contrast) {
	const Color base_color = p_base_color.clamp();
	const Color accent_color = p_accent_color.clamp();

	const Color mono_color = contrast_color(base_color, p_contrast);
	const Color bg_color = base_color.lerp(mono_color, p_bg_contrast).clamp();
	const Color style_normal_color = base_color.lerp(mono_color, p_normal_contrast).clamp();
	const Color style_pressed_color = base_color.lerp(mono_color, p_pressed_contrast).clamp();
	const Color style_hover_color = base_color.lerp(mono_color, p_hover_contrast).clamp();
	const Color style_disabled_color = Color(style_normal_color.r, style_normal_color.g, style_normal_color.b, 0.4);
	const Color bg_color2 = Color(bg_color.r, bg_color.g, bg_color.b, 0.6);
	const Color accent_color2 = Color(accent_color.r, accent_color.g, accent_color.b, 0.6);

	panel_style->set_bg_color(base_color);
	graph_panel_style->set_bg_color(base_color);
	popup_panel_style->set_bg_color(base_color);
	flat_tab_panel_style->set_bg_color(base_color);
	flat_tab_selected_style->set_bg_color(base_color);
	flat_foldable_panel_style->set_bg_color(base_color);

	foldable_panel_style->set_bg_color(bg_color);
	tab_selected_style->set_bg_color(bg_color);
	tab_panel_style->set_bg_color(bg_color);
	flat_panel_style->set_bg_color(bg_color);
	p_theme->set_color("current_line_color", "TextEdit", bg_color);
	p_theme->set_color("current_line_color", "CodeEdit", bg_color);

	h_scroll_style->set_bg_color(bg_color2);
	v_scroll_style->set_bg_color(bg_color2);
	slider_style->set_bg_color(bg_color2);

	button_focus_style->set_bg_color(accent_color);
	color_button_focus_style->set_bg_color(accent_color);
	focus_rect_style->set_border_color(accent_color);
	focus_circle_style->set_border_color(accent_color);
	h_split_bar_background->set_bg_color(accent_color);
	v_split_bar_background->set_bg_color(accent_color);
	tab_focus_style->set_border_color(accent_color);
	graph_panel_selected_style->set_border_color(accent_color);
	graph_frame_title_selected_style->set_border_color(accent_color);
	graph_title_selected_style->set_border_color(accent_color);
	button_focus_style->set_border_color(accent_color);
	color_button_focus_style->set_border_color(accent_color);
	tab_selected_style->set_border_color(accent_color);
	flat_tab_selected_style->set_border_color(accent_color);
	p_theme->set_color("icon_pressed_color", "Button", accent_color);
	p_theme->set_color("icon_hover_pressed_color", "Button", accent_color);
	p_theme->set_color("font_pressed_color", "Button", accent_color);
	p_theme->set_color("font_hover_pressed_color", "Button", accent_color);
	p_theme->set_color("drop_mark_color", "TabContainer", accent_color);
	p_theme->set_color("drop_mark_color", "TabBar", accent_color);
	p_theme->set_color("font_pressed_color", "LinkButton", accent_color);
	p_theme->set_color("font_hover_pressed_color", "LinkButton", accent_color);
	p_theme->set_color("grabber_icon_pressed", "SplitContainer", accent_color);
	p_theme->set_color("grabber_icon_pressed", "HSplitContainer", accent_color);
	p_theme->set_color("grabber_icon_pressed", "VSplitContainer", accent_color);
	p_theme->set_color("clear_button_color_pressed", "LineEdit", accent_color);
	p_theme->set_color("font_pressed_color", "MenuBar", accent_color);
	p_theme->set_color("font_pressed_color", "OptionButton", accent_color);
	p_theme->set_color("font_pressed_color", "MenuButton", accent_color);
	p_theme->set_color("font_pressed_color", "CheckBox", accent_color);
	p_theme->set_color("font_hover_pressed_color", "CheckBox", accent_color);
	p_theme->set_color("checkbox_checked_color", "CheckBox", accent_color);
	p_theme->set_color("font_pressed_color", "CheckButton", accent_color);
	p_theme->set_color("font_hover_pressed_color", "CheckButton", accent_color);
	p_theme->set_color("button_checked_color", "CheckButton", accent_color);
	p_theme->set_color("down_pressed_icon_modulate", "SpinBox", accent_color);
	p_theme->set_color("up_pressed_icon_modulate", "SpinBox", accent_color);
	p_theme->set_color("selection_stroke", "GraphEdit", accent_color);
	p_theme->set_color("collapsed_font_color", "FoldableContainer", accent_color);
	p_theme->set_color("arrow_collapsed_color", "FoldableContainer", accent_color);

	p_theme->set_color("search_result_color", "TextEdit", accent_color2);
	p_theme->set_color("search_result_color", "CodeEdit", accent_color2);
	grabber_highlight_style->set_bg_color(accent_color2);
	progress_fill_style->set_bg_color(accent_color2);

	embedded_unfocused_style->set_bg_color(style_normal_color);
	tab_unselected_style->set_bg_color(style_normal_color);
	button_normal_style->set_bg_color(style_normal_color);
	color_button_normal_style->set_bg_color(style_normal_color);
	foldable_title_style->set_bg_color(style_normal_color);
	foldable_title_collapsed_style->set_bg_color(style_normal_color);
	graph_title_style->set_bg_color(style_normal_color);
	graph_frame_title_style->set_bg_color(style_normal_color);
	embedded_style->set_bg_color(style_normal_color);
	progress_background_style->set_bg_color(style_normal_color);
	code_edit_completion_style->set_bg_color(style_normal_color);

	button_hover_style->set_bg_color(style_hover_color);
	tab_hover_style->set_bg_color(style_hover_color);
	color_button_hover_style->set_bg_color(style_hover_color);
	foldable_title_hover_style->set_bg_color(style_hover_color);
	foldable_title_collapsed_hover_style->set_bg_color(style_hover_color);
	popup_hover_style->set_bg_color(style_hover_color);
	p_theme->set_color("word_highlighted_color", "TextEdit", style_hover_color);
	p_theme->set_color("word_highlighted_color", "CodeEdit", style_hover_color);

	color_button_pressed_style->set_bg_color(style_pressed_color);
	button_pressed_style->set_bg_color(style_pressed_color);
	graph_frame_title_selected_style->set_bg_color(style_pressed_color);
	graph_title_selected_style->set_bg_color(style_pressed_color);
	graph_panel_selected_style->set_bg_color(style_pressed_color);
	p_theme->set_color("selection_color", "LineEdit", style_pressed_color);
	p_theme->set_color("selection_color", "TextEdit", style_pressed_color);
	p_theme->set_color("selection_color", "CodeEdit", style_pressed_color);
	p_theme->set_color("selection_color", "RichTextLabel", style_pressed_color);

	button_disabled_style->set_bg_color(style_disabled_color);
	color_button_disabled_style->set_bg_color(style_disabled_color);
	h_separator_style->set_color(style_disabled_color);
	v_separator_style->set_color(style_disabled_color);

	float v = is_dark_theme ? 1.f : 0.f;
	p_theme->set_color("grid_minor", "GraphEdit", Color(v, v, v, 0.05));
	p_theme->set_color("grid_major", "GraphEdit", Color(v, v, v, 0.2));
	p_theme->set_color("activity", "GraphEdit", Color(v, v, v));
	p_theme->set_color("connection_hover_tint_color", "GraphEdit", Color(1.f - v, 1.f - v, 1.f - v, 0.3));
	p_theme->set_color("connection_valid_target_tint_color", "GraphEdit", Color(v, v, v, 0.4));

	p_theme->set_color("base_color", "Colors", base_color);
	p_theme->set_color("accent_color", "Colors", accent_color);
	p_theme->set_color("accent_color2", "Colors", accent_color2);
	p_theme->set_color("bg_color", "Colors", bg_color);
	p_theme->set_color("bg_color2", "Colors", bg_color2);
	p_theme->set_color("normal_color", "Colors", style_normal_color);
	p_theme->set_color("pressed_color", "Colors", style_pressed_color);
	p_theme->set_color("hover_color", "Colors", style_hover_color);
	p_theme->set_color("disabled_color", "Colors", style_disabled_color);
	p_theme->set_color("mono_color", "Colors", mono_color);
}

void update_font_color(const Ref<Theme> &p_theme, const Color &p_color) {
	Color font_color = p_color.clamp();
	is_dark_theme = font_color.get_luminance() > 0.5;

	popup_panel_style->set_border_color(font_color);
	graph_title_style->set_border_color(font_color);
	graph_frame_title_style->set_border_color(font_color);
	graph_panel_style->set_border_color(font_color);
	color_button_pressed_style->set_border_color(font_color);
	p_theme->set_color("font_hover_color", "Button", font_color);
	p_theme->set_color("icon_hover_color", "Button", font_color);
	p_theme->set_color("font_hovered_color", "TabContainer", font_color);
	p_theme->set_color("font_hovered_color", "TabBar", font_color);
	p_theme->set_color("font_hover_color", "PopupMenu", font_color);
	p_theme->set_color("font_selected_color", "LineEdit", font_color);
	p_theme->set_color("font_selected_color", "TextEdit", font_color);
	p_theme->set_color("font_selected_color", "RichTextLabel", font_color);
	p_theme->set_color("font_selected_color", "CodeEdit", font_color);
	p_theme->set_color("font_selected_color", "ItemList", font_color);
	p_theme->set_color("font_hovered_selected_color", "ItemList", font_color);
	p_theme->set_color("font_selected_color", "TabContainer", font_color);
	p_theme->set_color("font_selected_color", "TabBar", font_color);
	p_theme->set_color("font_selected_color", "Tree", font_color);
	p_theme->set_color("font_hovered_color", "Tree", font_color);
	p_theme->set_color("caret_background_color", "TextEdit", font_color);
	p_theme->set_color("caret_background_color", "CodeEdit", font_color);
	p_theme->set_color("font_color", "ProgressBar", font_color);
	p_theme->set_color("font_hover_color", "LinkButton", font_color);
	p_theme->set_color("font_hover_color", "MenuBar", font_color);
	p_theme->set_color("font_hover_color", "LinkButton", font_color);
	p_theme->set_color("font_hover_pressed_color", "MenuBar", font_color);
	p_theme->set_color("font_hover_color", "OptionButton", font_color);
	p_theme->set_color("font_hover_pressed_color", "OptionButton", font_color);
	p_theme->set_color("font_hover_color", "MenuButton", font_color);
	p_theme->set_color("checkbox_unchecked_color", "CheckBox", font_color);
	p_theme->set_color("font_hover_color", "CheckBox", font_color);
	p_theme->set_color("button_unchecked_color", "CheckButton", font_color);
	p_theme->set_color("font_hover_color", "CheckButton", font_color);
	p_theme->set_color("drop_position_color", "Tree", font_color);
	p_theme->set_color("up_hover_icon_modulate", "SpinBox", font_color);
	p_theme->set_color("down_hover_icon_modulate", "SpinBox", font_color);
	p_theme->set_color("font_hovered_color", "ItemList", font_color);
	p_theme->set_color("connection_rim_color", "GraphEdit", font_color);
	p_theme->set_color("hover_font_color", "FoldableContainer", font_color);
	p_theme->set_color("arrow_hover_color", "FoldableContainer", font_color);

	p_theme->set_color("font_focus_color", "Button", font_color);
	p_theme->set_color("icon_focus_color", "Button", font_color);
	p_theme->set_color("parent_hl_line_color", "Tree", font_color);
	p_theme->set_color("font_focus_color", "LinkButton", font_color);
	p_theme->set_color("font_focus_color", "MenuBar", font_color);
	p_theme->set_color("title_color", "Window", font_color);
	p_theme->set_color("font_focus_color", "OptionButton", font_color);
	p_theme->set_color("font_focus_color", "MenuButton", font_color);
	p_theme->set_color("font_focus_color", "CheckBox", font_color);
	p_theme->set_color("font_focus_color", "CheckButton", font_color);
	p_theme->set_color("custom_button_font_highlight", "Tree", font_color);
	p_theme->set_color("resizer_color", "GraphEditMinimap", font_color);
	p_theme->set_color("resizer_color", "GraphNode", font_color);
	p_theme->set_color("resizer_color", "GraphFrame", font_color);

	p_theme->set_color(SceneStringName(font_color), "Button", font_color);
	p_theme->set_color(SceneStringName(font_color), "Tree", font_color);
	p_theme->set_color(SceneStringName(font_color), "LinkButton", font_color);
	p_theme->set_color(SceneStringName(font_color), "Label", font_color);
	p_theme->set_color(SceneStringName(font_color), "LineEdit", font_color);
	p_theme->set_color(SceneStringName(font_color), "TextEdit", font_color);
	p_theme->set_color(SceneStringName(font_color), "CodeEdit", font_color);
	p_theme->set_color(SceneStringName(font_color), "PopupMenu", font_color);
	p_theme->set_color(SceneStringName(font_color), "FoldableContainer", font_color);
	p_theme->set_color(SceneStringName(font_color), "MenuBar", font_color);
	p_theme->set_color(SceneStringName(font_color), "OptionButton", font_color);
	p_theme->set_color(SceneStringName(font_color), "MenuButton", font_color);
	p_theme->set_color(SceneStringName(font_color), "CheckBox", font_color);
	p_theme->set_color(SceneStringName(font_color), "CheckButton", font_color);
	p_theme->set_color(SceneStringName(font_color), "GraphNodeTitleLabel", font_color);
	p_theme->set_color(SceneStringName(font_color), "TooltipLabel", font_color);
	p_theme->set_color(SceneStringName(font_color), "GraphFrameTitleLabel", font_color);
	p_theme->set_color(SceneStringName(font_color), "ItemList", font_color);
	p_theme->set_color("icon_normal_color", "Button", font_color);
	p_theme->set_color("caret_color", "LineEdit", font_color);
	p_theme->set_color("caret_color", "TextEdit", font_color);
	p_theme->set_color("caret_color", "CodeEdit", font_color);
	p_theme->set_color("clear_button_color", "LineEdit", font_color);
	p_theme->set_color("font_unselected_color", "TabContainer", font_color);
	p_theme->set_color("font_unselected_color", "TabBar", font_color);
	p_theme->set_color("folder_icon_color", "FileDialog", font_color);
	p_theme->set_color("file_icon_color", "FileDialog", font_color);
	p_theme->set_color("title_button_color", "Tree", font_color);
	p_theme->set_color("children_hl_line_color", "Tree", font_color);
	p_theme->set_color("default_color", "RichTextLabel", font_color);
	p_theme->set_color("up_icon_modulate", "SpinBox", font_color);
	p_theme->set_color("down_icon_modulate", "SpinBox", font_color);
	p_theme->set_color("arrow_normal_color", "FoldableContainer", font_color);
	font_color.a = 0.6;
	grabber_style->set_bg_color(font_color);
	p_theme->set_color("font_separator_color", "PopupMenu", font_color);
	p_theme->set_color("font_accelerator_color", "PopupMenu", font_color);
	p_theme->set_color("font_disabled_color", "PopupMenu", font_color);
	p_theme->set_color("grabber_icon_normal", "SplitContainer", font_color);
	p_theme->set_color("grabber_icon_normal", "HSplitContainer", font_color);
	p_theme->set_color("grabber_icon_normal", "VSplitContainer", font_color);
	p_theme->set_color("font_uneditable_color", "LineEdit", font_color);
	p_theme->set_color("font_readonly_color", "TextEdit", font_color);
	p_theme->set_color("font_readonly_color", "CodeEdit", font_color);
	p_theme->set_color("font_placeholder_color", "LineEdit", font_color);
	p_theme->set_color("font_placeholder_color", "TextEdit", font_color);
	p_theme->set_color("font_placeholder_color", "CodeEdit", font_color);
	p_theme->set_color("relationship_line_color", "Tree", font_color);
	p_theme->set_color("font_hovered_dimmed_color", "Tree", font_color);
	font_color.a = 0.4;
	p_theme->set_color("icon_disabled_color", "Button", font_color);
	p_theme->set_color("font_disabled_color", "Button", font_color);
	p_theme->set_color("search_result_border_color", "TextEdit", font_color);
	p_theme->set_color("search_result_border_color", "CodeEdit", font_color);
	p_theme->set_color("guide_color", "Tree", font_color);
	p_theme->set_color("guide_color", "ItemList", font_color);
	p_theme->set_color("file_disabled_color", "FileDialog", font_color);
	p_theme->set_color("font_disabled_color", "TabContainer", font_color);
	p_theme->set_color("font_disabled_color", "TabBar", font_color);
	p_theme->set_color("font_disabled_color", "Tree", font_color);
	p_theme->set_color("icon_disabled_color", "LinkButton", font_color);
	p_theme->set_color("font_disabled_color", "MenuBar", font_color);
	p_theme->set_color("font_disabled_color", "LinkButton", font_color);
	p_theme->set_color("font_disabled_color", "OptionButton", font_color);
	p_theme->set_color("font_disabled_color", "MenuButton", font_color);
	p_theme->set_color("font_disabled_color", "CheckBox", font_color);
	p_theme->set_color("font_disabled_color", "CheckButton", font_color);
	p_theme->set_color("up_disabled_icon_modulate", "SpinBox", font_color);
	p_theme->set_color("down_disabled_icon_modulate", "SpinBox", font_color);
	p_theme->set_color("selection_fill", "GraphEdit", font_color);
	p_theme->set_color("completion_scroll_hovered_color", "CodeEdit", font_color);
	p_theme->set_color("completion_scroll_color", "CodeEdit", font_color);

	p_theme->set_color("font_color", "Colors", p_color);
}

void update_font_outline_color(const Ref<Theme> &p_theme, const Color &p_color) {
	Color outline_color = p_color.clamp();
	p_theme->set_color("font_outline_color", "Button", outline_color);
	p_theme->set_color("font_outline_color", "RichTextLabel", outline_color);
	p_theme->set_color("font_outline_color", "FoldableContainer", outline_color);
	p_theme->set_color("font_outline_color", "TooltipLabel", outline_color);
	p_theme->set_color("font_outline_color", "TabBar", outline_color);
	p_theme->set_color("font_outline_color", "TabContainer", outline_color);
	p_theme->set_color("font_outline_color", "ItemList", outline_color);
	p_theme->set_color("font_outline_color", "Tree", outline_color);
	p_theme->set_color("font_outline_color", "GraphFrameTitleLabel", outline_color);
	p_theme->set_color("font_outline_color", "GraphNodeTitleLabel", outline_color);
	p_theme->set_color("font_outline_color", "PopupMenu", outline_color);
	p_theme->set_color("font_outline_color", "CodeEdit", outline_color);
	p_theme->set_color("font_outline_color", "TextEdit", outline_color);
	p_theme->set_color("font_outline_color", "ProgressBar", outline_color);
	p_theme->set_color("font_outline_color", "LineEdit", outline_color);
	p_theme->set_color("font_outline_color", "Label", outline_color);
	p_theme->set_color("font_outline_color", "CheckButton", outline_color);
	p_theme->set_color("font_outline_color", "CheckBox", outline_color);
	p_theme->set_color("font_outline_color", "MenuButton", outline_color);
	p_theme->set_color("font_outline_color", "OptionButton", outline_color);
	p_theme->set_color("font_outline_color", "LinkButton", outline_color);
	p_theme->set_color("font_outline_color", "MenuBar", outline_color);
	p_theme->set_color("font_separator_outline_color", "PopupMenu", outline_color);
	p_theme->set_color("title_outline_modulate", "Window", outline_color);

	p_theme->set_color("font_outline_color", "Colors", outline_color);
}

void update_font_outline_size(const Ref<Theme> &p_theme, int p_outline_size) {
	int outline_size = p_outline_size * MAX(p_theme->get_default_base_scale(), 0.5);

	p_theme->set_constant("outline_size", "Button", outline_size);
	p_theme->set_constant("separator_outline_size", "PopupMenu", outline_size);
	p_theme->set_constant("outline_size", "RichTextLabel", outline_size);
	p_theme->set_constant("outline_size", "FoldableContainer", outline_size);
	p_theme->set_constant("outline_size", "TooltipLabel", outline_size);
	p_theme->set_constant("outline_size", "TabBar", outline_size);
	p_theme->set_constant("outline_size", "TabContainer", outline_size);
	p_theme->set_constant("outline_size", "ItemList", outline_size);
	p_theme->set_constant("outline_size", "Tree", outline_size);
	p_theme->set_constant("outline_size", "GraphFrameTitleLabel", outline_size);
	p_theme->set_constant("outline_size", "GraphNodeTitleLabel", outline_size);
	p_theme->set_constant("outline_size", "PopupMenu", outline_size);
	p_theme->set_constant("title_outline_size", "Window", outline_size);
	p_theme->set_constant("outline_size", "CodeEdit", outline_size);
	p_theme->set_constant("outline_size", "TextEdit", outline_size);
	p_theme->set_constant("outline_size", "ProgressBar", outline_size);
	p_theme->set_constant("outline_size", "LineEdit", outline_size);
	p_theme->set_constant("outline_size", "Label", outline_size);
	p_theme->set_constant("outline_size", "CheckButton", outline_size);
	p_theme->set_constant("outline_size", "CheckBox", outline_size);
	p_theme->set_constant("outline_size", "MenuButton", outline_size);
	p_theme->set_constant("outline_size", "OptionButton", outline_size);
	p_theme->set_constant("outline_size", "LinkButton", outline_size);
	p_theme->set_constant("outline_size", "MenuBar", outline_size);

	p_theme->set_constant("font_outline_size", "Constants", outline_size);
}

void update_font_size(const Ref<Theme> &p_theme, int p_font_size) {
	p_theme->set_default_font_size(p_font_size);

	p_theme->set_font_size(SceneStringName(font_size), "HeaderSmall", p_font_size + 4);
	p_theme->set_font_size(SceneStringName(font_size), "HeaderMedium", p_font_size + 8);
	p_theme->set_font_size(SceneStringName(font_size), "GraphFrameTitleLabel", p_font_size + 8);
	p_theme->set_font_size(SceneStringName(font_size), "HeaderLarge", p_font_size + 12);
}

void update_font_embolden(float p_embolden) {
	font_variation->set_variation_embolden(p_embolden);
	if (custom_font_variation.is_valid()) {
		return;
	}

	blazium_bold_font->set_variation_embolden(p_embolden + 0.2);
	blazium_bold_italics_font->set_variation_embolden(p_embolden + 0.2);
	blazium_italics_font->set_variation_embolden(p_embolden);
}

void update_font_spacing_glyph(int p_spacing) {
	font_variation->set_spacing(TextServer::SPACING_GLYPH, p_spacing);
	if (custom_font_variation.is_valid()) {
		return;
	}

	blazium_bold_font->set_spacing(TextServer::SPACING_GLYPH, p_spacing);
	blazium_bold_italics_font->set_spacing(TextServer::SPACING_GLYPH, p_spacing);
	blazium_italics_font->set_spacing(TextServer::SPACING_GLYPH, p_spacing);
}

void update_font_spacing_space(int p_spacing) {
	font_variation->set_spacing(TextServer::SPACING_SPACE, p_spacing);
	if (custom_font_variation.is_valid()) {
		return;
	}

	blazium_bold_font->set_spacing(TextServer::SPACING_SPACE, p_spacing);
	blazium_bold_italics_font->set_spacing(TextServer::SPACING_SPACE, p_spacing);
	blazium_italics_font->set_spacing(TextServer::SPACING_SPACE, p_spacing);
}

void update_font_spacing_top(int p_spacing) {
	font_variation->set_spacing(TextServer::SPACING_TOP, p_spacing);
	if (custom_font_variation.is_valid()) {
		return;
	}

	blazium_bold_italics_font->set_spacing(TextServer::SPACING_TOP, p_spacing);
	blazium_bold_font->set_spacing(TextServer::SPACING_TOP, p_spacing);
	blazium_italics_font->set_spacing(TextServer::SPACING_TOP, p_spacing);
}

void update_font_spacing_bottom(int p_spacing) {
	font_variation->set_spacing(TextServer::SPACING_BOTTOM, p_spacing);
	if (custom_font_variation.is_valid()) {
		return;
	}

	blazium_bold_font->set_spacing(TextServer::SPACING_BOTTOM, p_spacing);
	blazium_bold_italics_font->set_spacing(TextServer::SPACING_BOTTOM, p_spacing);
	blazium_italics_font->set_spacing(TextServer::SPACING_BOTTOM, p_spacing);
}

void update_theme_font(const Ref<Theme> &p_theme, Ref<Font> p_font) {
	if (p_font.is_valid() && p_font->is_class("FontVariation")) {
		custom_font_variation = p_font;

		blazium_bold_font = custom_font_variation->duplicate();
		blazium_bold_font->set_variation_embolden(custom_font_variation->get_variation_embolden() + 0.2);
		blazium_bold_italics_font = custom_font_variation->duplicate();
		blazium_bold_italics_font->set_variation_embolden(custom_font_variation->get_variation_embolden() + 0.2);
		blazium_bold_italics_font->set_variation_transform(Transform2D(1.0, 0.2, 0.0, 1.0, 0.0, 0.0));
		blazium_italics_font = custom_font_variation->duplicate();
		blazium_italics_font->set_variation_transform(Transform2D(1.0, 0.2, 0.0, 1.0, 0.0, 0.0));
		p_theme->set_font("bold_font", "RichTextLabel", blazium_bold_font);
		p_theme->set_font("italics_font", "RichTextLabel", blazium_italics_font);
		p_theme->set_font("bold_italics_font", "RichTextLabel", blazium_bold_italics_font);
		p_theme->set_default_font(custom_font_variation);

	} else {
		Ref<FontFile> base_font = p_font.is_valid() ? p_font : ThemeDB::get_singleton()->get_fallback_font();
		Ref<FontFile> cur_font = font_variation->get_base_font();
		if (cur_font.is_valid()) {
			if (base_font != cur_font) {
				base_font->set_subpixel_positioning(cur_font->get_subpixel_positioning());
				base_font->set_antialiasing(cur_font->get_antialiasing());
				base_font->set_hinting(cur_font->get_hinting());
				base_font->set_multichannel_signed_distance_field(cur_font->is_multichannel_signed_distance_field());
				base_font->set_generate_mipmaps(cur_font->get_generate_mipmaps());
			}
		}

		font_variation->set_base_font(base_font);

		if (custom_font_variation.is_valid()) {
			custom_font_variation = Ref<FontVariation>();

			blazium_bold_font = font_variation->duplicate();
			blazium_bold_font->set_variation_embolden(font_variation->get_variation_embolden() + 0.2);
			blazium_bold_italics_font = font_variation->duplicate();
			blazium_bold_italics_font->set_variation_embolden(font_variation->get_variation_embolden() + 0.2);
			blazium_bold_italics_font->set_variation_transform(Transform2D(1.0, 0.2, 0.0, 1.0, 0.0, 0.0));
			blazium_italics_font = font_variation->duplicate();
			blazium_italics_font->set_variation_transform(Transform2D(1.0, 0.2, 0.0, 1.0, 0.0, 0.0));
			p_theme->set_font("bold_font", "RichTextLabel", blazium_bold_font);
			p_theme->set_font("italics_font", "RichTextLabel", blazium_italics_font);
			p_theme->set_font("bold_italics_font", "RichTextLabel", blazium_bold_italics_font);
			p_theme->set_default_font(font_variation);
		}
	}
}

void update_font_subpixel_positioning(TextServer::SubpixelPositioning p_font_subpixel_positioning) {
	Ref<FontFile> base_font = font_variation->get_base_font();
	base_font->set_subpixel_positioning(p_font_subpixel_positioning);
}

void update_font_antialiasing(TextServer::FontAntialiasing p_font_antialiasing) {
	Ref<FontFile> base_font = font_variation->get_base_font();
	base_font->set_antialiasing(p_font_antialiasing);
}

void update_font_lcd_subpixel_layout(TextServer::FontLCDSubpixelLayout p_font_lcd_subpixel_layout) {
	// LCD subpixel layout is a global TextServer setting in 4.7, not a FontFile property.
}

void update_font_hinting(TextServer::Hinting p_font_hinting) {
	Ref<FontFile> base_font = font_variation->get_base_font();
	base_font->set_hinting(p_font_hinting);
}

void update_font_msdf(bool p_font_msdf) {
	Ref<FontFile> base_font = font_variation->get_base_font();
	base_font->set_multichannel_signed_distance_field(p_font_msdf);
}

void update_font_generate_mipmaps(bool p_font_generate_mipmaps) {
	Ref<FontFile> base_font = font_variation->get_base_font();
	base_font->set_generate_mipmaps(p_font_generate_mipmaps);
}

void update_theme_margins(const Ref<Theme> &p_theme, int p_margin) {
	int margin = p_margin * MAX(p_theme->get_default_base_scale(), 0.5);

	p_theme->set_constant("h_separation", "Button", margin);
	p_theme->set_constant("separation", "BoxContainer", margin);
	p_theme->set_constant("separation", "HBoxContainer", margin);
	p_theme->set_constant("separation", "VBoxContainer", margin);
	p_theme->set_constant("h_separation", "FoldableContainer", margin);
	p_theme->set_constant("h_separation", "Tree", margin);
	p_theme->set_constant("h_separation", "ScrollContainer", margin);
	p_theme->set_constant("h_separation", "TabBar", margin);
	p_theme->set_constant("h_separation", "OptionButton", margin);
	p_theme->set_constant("h_separation", "MenuButton", margin);
	p_theme->set_constant("h_separation", "CheckBox", margin);
	p_theme->set_constant("h_separation", "CheckButton", margin);
	p_theme->set_constant("h_separation", "FlowContainer", margin);
	p_theme->set_constant("v_separation", "FlowContainer", margin);
	p_theme->set_constant("h_separation", "HFlowContainer", margin);
	p_theme->set_constant("v_separation", "HFlowContainer", margin);
	p_theme->set_constant("h_separation", "VFlowContainer", margin);
	p_theme->set_constant("v_separation", "VFlowContainer", margin);
	p_theme->set_constant("v_separation", "ScrollContainer", margin);
	p_theme->set_constant("button_margin", "Tree", margin);
	p_theme->set_constant("inner_margin_left", "Tree", margin);
	p_theme->set_constant("inner_margin_right", "Tree", margin);
	p_theme->set_constant("separation", "HSeparator", margin);
	p_theme->set_constant("separation", "VSeparator", margin);
	p_theme->set_constant("scrollbar_h_separation", "Tree", margin);
	p_theme->set_constant("scrollbar_v_separation", "Tree", margin);
	p_theme->set_constant("h_separation", "ItemList", margin);
	p_theme->set_constant("v_separation", "ItemList", margin);
	p_theme->set_constant("icon_margin", "ItemList", margin);
	p_theme->set_constant("h_separation", "GridContainer", margin);
	p_theme->set_constant("v_separation", "GridContainer", margin);

	p_theme->set_constant("margin", "Constants", margin);
}

void update_theme_padding(const Ref<Theme> &p_theme, int p_padding) {
	float base_scale = MAX(p_theme->get_default_base_scale(), 0.5);
	int padding = p_padding * base_scale;

	panel_style->set_content_margin_all(padding);
	flat_panel_style->set_content_margin_all(padding);
	tab_panel_style->set_content_margin_all(padding);
	flat_tab_panel_style->set_content_margin_all(padding);
	progress_background_style->set_content_margin_all(padding);
	graph_frame_title_style->set_content_margin_all(padding);
	graph_frame_title_selected_style->set_content_margin_all(padding);
	foldable_panel_style->set_content_margin(SIDE_TOP, padding);
	flat_foldable_panel_style->set_content_margin(SIDE_TOP, padding);
	graph_title_style->set_content_margin_individual(12 * base_scale, padding, padding, padding);
	graph_title_selected_style->set_content_margin_individual(12 * base_scale, padding, padding, padding);
	p_theme->set_constant("margin_left", "PaddedMarginContainer", padding);
	p_theme->set_constant("margin_top", "PaddedMarginContainer", padding);
	p_theme->set_constant("margin_right", "PaddedMarginContainer", padding);
	p_theme->set_constant("margin_bottom", "PaddedMarginContainer", padding);
	p_theme->set_constant("item_start_padding", "PopupMenu", padding);
	p_theme->set_constant("item_end_padding", "PopupMenu", padding);

	p_theme->set_constant("padding", "Constants", padding);
}

void update_theme_corner_radius(const Ref<Theme> &p_theme, int p_corner_radius) {
	float base_scale = MAX(p_theme->get_default_base_scale(), 0.5);
	int corners = p_corner_radius * base_scale;

	panel_style->set_corner_radius_all(corners);
	flat_panel_style->set_corner_radius_all(corners);
	button_hover_style->set_corner_radius_all(corners);
	button_normal_style->set_corner_radius_all(corners);
	button_pressed_style->set_corner_radius_all(corners);
	button_disabled_style->set_corner_radius_all(corners);
	color_button_normal_style->set_corner_radius_all(MIN(corners, 2 * base_scale));
	color_button_hover_style->set_corner_radius_all(MIN(corners, 2 * base_scale));
	color_button_pressed_style->set_corner_radius_all(MIN(corners, 2 * base_scale));
	color_button_disabled_style->set_corner_radius_all(MIN(corners, 2 * base_scale));
	grabber_style->set_corner_radius_all(corners);
	grabber_highlight_style->set_corner_radius_all(corners);
	slider_style->set_corner_radius_all(corners);
	h_scroll_style->set_corner_radius_all(corners);
	v_scroll_style->set_corner_radius_all(corners);
	foldable_title_collapsed_style->set_corner_radius_all(corners);
	foldable_title_collapsed_hover_style->set_corner_radius_all(corners);
	foldable_panel_style->set_corner_radius_individual(0, 0, corners, corners);
	flat_foldable_panel_style->set_corner_radius_individual(0, 0, corners, corners);
	foldable_title_style->set_corner_radius_individual(corners, corners, 0, 0);
	foldable_title_hover_style->set_corner_radius_individual(corners, corners, 0, 0);
	graph_panel_style->set_corner_radius_individual(0, 0, corners, corners);
	graph_panel_selected_style->set_corner_radius_individual(0, 0, corners, corners);
	graph_title_style->set_corner_radius_individual(corners, corners, 0, 0);
	graph_frame_title_style->set_corner_radius_individual(corners, corners, 0, 0);
	graph_title_selected_style->set_corner_radius_individual(corners, corners, 0, 0);
	graph_frame_title_selected_style->set_corner_radius_individual(corners, corners, 0, 0);

	int focus_border = MAX(p_corner_radius - 2, 0) * base_scale;
	button_focus_style->set_corner_radius_all(focus_border);
	color_button_focus_style->set_corner_radius_all(MIN(focus_border, 2 * base_scale));

	p_theme->set_constant("corner_radius", "Constants", corners);
	p_theme->set_constant("focus_corners", "Constants", focus_border);
}

void update_theme_border_width(const Ref<Theme> &p_theme, int p_border_width) {
	int border_width = p_border_width * MAX(p_theme->get_default_base_scale(), 0.5);

	popup_panel_style->set_content_margin_all(MAX(border_width, 1));
	button_focus_style->set_border_width_all(MAX(border_width, 1));

	p_theme->set_constant("border_width", "Constants", border_width);
}

void update_theme_border_padding(const Ref<Theme> &p_theme, int p_border_padding) {
	int border_padding = p_border_padding * MAX(p_theme->get_default_base_scale(), 0.5);

	button_normal_style->set_content_margin_all(border_padding);
	button_empty_style->set_content_margin_all(border_padding);
	button_pressed_style->set_content_margin_all(border_padding);
	button_hover_style->set_content_margin_all(border_padding);
	button_disabled_style->set_content_margin_all(border_padding);
	p_theme->set_constant("inner_item_margin_left", "Tree", border_padding);
	p_theme->set_constant("inner_item_margin_right", "Tree", border_padding);
	p_theme->set_constant("arrow_margin", "OptionButton", border_padding);
	foldable_title_style->set_content_margin_all(border_padding);
	foldable_title_hover_style->set_content_margin_all(border_padding);
	foldable_title_collapsed_style->set_content_margin_all(border_padding);
	foldable_title_collapsed_hover_style->set_content_margin_all(border_padding);
	foldable_panel_style->set_content_margin(SIDE_LEFT, border_padding);
	foldable_panel_style->set_content_margin(SIDE_RIGHT, border_padding);
	foldable_panel_style->set_content_margin(SIDE_BOTTOM, border_padding);
	flat_foldable_panel_style->set_content_margin(SIDE_LEFT, border_padding);
	flat_foldable_panel_style->set_content_margin(SIDE_RIGHT, border_padding);
	flat_foldable_panel_style->set_content_margin(SIDE_BOTTOM, border_padding);

	p_theme->set_constant("border_padding", "Constants", border_padding);
}

void update_theme_scale(const Ref<Theme> &p_theme) {
	float base_scale = MAX(p_theme->get_default_base_scale(), 0.5);
	int int_scale = MAX(Math::floor(base_scale), 1);
	int x2_scale = 2 * base_scale;
	int x4_scale = 4 * base_scale;
	int x6_scale = 6 * base_scale;

	focus_circle_style->set_corner_radius_all(Math::round(256 * base_scale));
	focus_circle_style->set_corner_detail(Math::round(32 * base_scale));

	popup_panel_style->set_border_width_all(int_scale);
	p_theme->set_constant("children_hl_line_width", "Tree", int_scale);
	p_theme->set_constant("shadow_offset_x", "Label", int_scale);
	p_theme->set_constant("shadow_offset_y", "Label", int_scale);
	p_theme->set_constant("shadow_outline_size", "Label", int_scale);
	p_theme->set_constant("shadow_offset_x", "GraphNodeTitleLabel", int_scale);
	p_theme->set_constant("shadow_offset_y", "GraphNodeTitleLabel", int_scale);
	p_theme->set_constant("shadow_outline_size", "GraphNodeTitleLabel", int_scale);
	p_theme->set_constant("shadow_offset_x", "GraphFrameTitleLabel", int_scale);
	p_theme->set_constant("shadow_offset_y", "GraphFrameTitleLabel", int_scale);
	p_theme->set_constant("shadow_outline_size", "GraphFrameTitleLabel", int_scale);
	p_theme->set_constant("shadow_offset_x", "TooltipLabel", int_scale);
	p_theme->set_constant("shadow_offset_y", "TooltipLabel", int_scale);
	p_theme->set_constant("shadow_offset_x", "RichTextLabel", int_scale);
	p_theme->set_constant("shadow_offset_y", "RichTextLabel", int_scale);
	p_theme->set_constant("shadow_outline_size", "RichTextLabel", int_scale);
	p_theme->set_constant("caret_width", "LineEdit", int_scale);
	p_theme->set_constant("caret_width", "TextEdit", int_scale);
	p_theme->set_constant("relationship_line_width", "Tree", int_scale);
	p_theme->set_constant("parent_hl_line_width", "Tree", 2 * int_scale);
	p_theme->set_constant("indent", "PopupMenu", 10 * base_scale);
	p_theme->set_constant("buttons_separation", "AcceptDialog", 10 * base_scale);
	p_theme->set_constant("scroll_speed", "Tree", 12 * base_scale);
	p_theme->set_constant("item_margin", "Tree", 12 * base_scale);
	p_theme->set_constant("buttons_width", "SpinBox", 16 * base_scale);
	p_theme->set_constant("close_h_offset", "Window", 18 * base_scale);
	p_theme->set_constant("port_hotzone_inner_extent", "GraphEdit", 22 * base_scale);
	p_theme->set_constant("close_v_offset", "Window", 24 * base_scale);
	p_theme->set_constant("port_hotzone_outer_extent", "GraphEdit", 26 * base_scale);
	p_theme->set_constant("title_height", "Window", 36 * base_scale);
	p_theme->set_constant("completion_max_width", "CodeEdit", 50 * base_scale);
	p_theme->set_constant("h_width", "ColorPicker", 30 * base_scale);
	p_theme->set_constant("sample_height", "ColorPicker", 30 * base_scale);
	p_theme->set_constant("preset_size", "ColorPicker", 30 * base_scale);
	p_theme->set_constant("sv_width", "ColorPicker", 256 * base_scale);
	p_theme->set_constant("sv_height", "ColorPicker", 256 * base_scale);
	p_theme->set_constant("label_width", "ColorPicker", 14 * base_scale);

	color_button_focus_style->set_border_width_all(x2_scale);
	focus_rect_style->set_border_width_all(x2_scale);
	focus_rect_style->set_expand_margin_all(x2_scale);
	focus_circle_style->set_border_width_all(x2_scale);
	focus_circle_style->set_expand_margin_all(x2_scale);
	tab_focus_style->set_border_width_all(x2_scale);
	h_separator_style->set_thickness(x2_scale);
	v_separator_style->set_thickness(x2_scale);
	slider_style->set_content_margin_all(x2_scale);
	p_theme->set_constant("line_spacing", "Label", x2_scale);
	p_theme->set_constant("line_spacing", "GraphNodeTitleLabel", x2_scale);
	p_theme->set_constant("line_spacing", "GraphFrameTitleLabel", x2_scale);
	p_theme->set_constant("underline_spacing", "LinkButton", x2_scale);
	p_theme->set_constant("field_and_buttons_separation", "SpinBox", x2_scale);
	p_theme->set_constant("icon_separation", "TabContainer", x2_scale);
	p_theme->set_constant("text_highlight_h_padding", "RichTextLabel", x2_scale);
	p_theme->set_constant("text_highlight_v_padding", "RichTextLabel", x2_scale);
	p_theme->set_constant("separation", "GraphNode", x2_scale);
	p_theme->set_constant(SceneStringName(line_separation), "ItemList", x2_scale);
	p_theme->set_constant("table_h_separation", "RichTextLabel", x2_scale);
	p_theme->set_constant("table_v_separation", "RichTextLabel", x2_scale);
	graph_panel_style->set_border_width(SIDE_LEFT, x2_scale);
	graph_panel_style->set_border_width(SIDE_RIGHT, x2_scale);
	graph_panel_style->set_border_width(SIDE_BOTTOM, x2_scale);
	graph_panel_selected_style->set_border_width(SIDE_LEFT, x2_scale);
	graph_panel_selected_style->set_border_width(SIDE_RIGHT, x2_scale);
	graph_panel_selected_style->set_border_width(SIDE_BOTTOM, x2_scale);
	graph_title_style->set_border_width(SIDE_LEFT, x2_scale);
	graph_title_style->set_border_width(SIDE_RIGHT, x2_scale);
	graph_title_style->set_border_width(SIDE_TOP, x2_scale);
	graph_frame_title_style->set_border_width(SIDE_LEFT, x2_scale);
	graph_frame_title_style->set_border_width(SIDE_RIGHT, x2_scale);
	graph_frame_title_style->set_border_width(SIDE_TOP, x2_scale);
	graph_title_selected_style->set_border_width(SIDE_LEFT, x2_scale);
	graph_title_selected_style->set_border_width(SIDE_RIGHT, x2_scale);
	graph_title_selected_style->set_border_width(SIDE_TOP, x2_scale);
	graph_frame_title_selected_style->set_border_width(SIDE_LEFT, x2_scale);
	graph_frame_title_selected_style->set_border_width(SIDE_RIGHT, x2_scale);
	graph_frame_title_selected_style->set_border_width(SIDE_TOP, x2_scale);
	tab_selected_style->set_border_width(SIDE_TOP, x2_scale);
	flat_tab_selected_style->set_border_width(SIDE_TOP, x2_scale);
	h_split_bar_background->set_expand_margin_individual(x2_scale, 0, x2_scale, 0);
	v_split_bar_background->set_expand_margin_individual(0, x2_scale, 0, x2_scale);

	progress_fill_style->set_content_margin_all(x4_scale);
	grabber_style->set_content_margin_all(x4_scale);
	h_scroll_style->set_content_margin_individual(0, x4_scale, 0, x4_scale);
	v_scroll_style->set_content_margin_individual(x4_scale, 0, x4_scale, 0);
	color_button_pressed_style->set_border_width_all(x4_scale);
	p_theme->set_constant("line_spacing", "TextEdit", x4_scale);
	p_theme->set_constant("line_spacing", "CodeEdit", x4_scale);
	p_theme->set_constant("h_separation", "PopupMenu", x4_scale);
	p_theme->set_constant("v_separation", "PopupMenu", x4_scale);
	p_theme->set_constant("v_separation", "Tree", x4_scale);
	p_theme->set_constant("h_separation", "MenuBar", x4_scale);
	p_theme->set_constant("scroll_border", "Tree", x4_scale);
	p_theme->set_constant("resize_margin", "Window", x4_scale);
	p_theme->set_constant("drop_mark_width", "TabContainer", x4_scale);
	p_theme->set_constant("drop_mark_width", "TabBar", x4_scale);
	p_theme->set_constant("check_h_separation", "Tree", x4_scale);
	p_theme->set_constant("icon_h_separation", "Tree", x4_scale);
	p_theme->set_constant("search_bar_separation", "PopupMenu", x4_scale);

	color_button_normal_style->set_content_margin_all(x6_scale);
	color_button_hover_style->set_content_margin_all(x6_scale);
	color_button_pressed_style->set_content_margin_all(x6_scale);
	color_button_disabled_style->set_content_margin_all(x6_scale);
	p_theme->set_constant("separation", "SplitContainer", x6_scale);
	p_theme->set_constant("separation", "HSplitContainer", x6_scale);
	p_theme->set_constant("separation", "VSplitContainer", x6_scale);
	p_theme->set_constant("minimum_grab_thickness", "SplitContainer", x6_scale);
	p_theme->set_constant("minimum_grab_thickness", "HSplitContainer", x6_scale);
	p_theme->set_constant("minimum_grab_thickness", "VSplitContainer", x6_scale);
	p_theme->set_constant("separation", "ColorPicker", x6_scale);
	p_theme->set_constant("margin", "ColorPicker", x6_scale);

	tab_unselected_style->set_content_margin_individual(x6_scale, x4_scale, x6_scale, x2_scale);
	tab_hover_style->set_content_margin_individual(x6_scale, x4_scale, x6_scale, x2_scale);
	tab_empty_style->set_content_margin_individual(x6_scale, x4_scale, x6_scale, x2_scale);
	tab_selected_style->set_content_margin_individual(x6_scale, x4_scale, x6_scale, x2_scale);
	flat_tab_selected_style->set_content_margin_individual(x6_scale, x4_scale, x6_scale, x2_scale);
}

void make_blazium_default_theme(Ref<Font> p_font, ThemeTemplate &p_template) {
	float theme_scale = CLAMP(p_template.scale, 0.5, 8.0);

	Ref<Theme> t;
	t.instantiate();

	Ref<FontFile> base_font;
	base_font.instantiate();
	base_font->set_data_ptr(_font_OpenSans_SemiBold, _font_OpenSans_SemiBold_size);

	font_variation.instantiate();
	font_variation->set_base_font(base_font);

	ThemeDB::get_singleton()->set_fallback_font(base_font);
	t->set_default_font(font_variation);

	blazium_bold_font.instantiate();
	blazium_bold_italics_font.instantiate();
	blazium_italics_font.instantiate();

	t->set_default_base_scale(theme_scale);

	t->set_type_variation("FlatButton", "Button");
	t->set_type_variation("FlatMenuButton", "MenuButton");
	t->set_type_variation("TooltipPanel", "PopupPanel");
	t->set_type_variation("FlatPanelContainer", "PanelContainer");
	t->set_type_variation("FlatPanel", "Panel");
	t->set_type_variation("ButtonsTabBar", "TabBar");
	t->set_type_variation("FlatTabContainer", "TabContainer");
	t->set_type_variation("FlatFoldableContainer", "TabContainer");
	t->set_type_variation("GraphNodeTitleLabel", "Label");
	t->set_type_variation("GraphFrameTitleLabel", "Label");
	t->set_type_variation("TooltipLabel", "Label");
	t->set_type_variation("HeaderSmall", "Label");
	t->set_type_variation("HeaderMedium", "Label");
	t->set_type_variation("HeaderLarge", "Label");
	t->set_type_variation("FlatSplitContainer", "SplitContainer");
	t->set_type_variation("FlatHSplitContainer", "HSplitContainer");
	t->set_type_variation("FlatVSplitContainer", "VSplitContainer");
	t->set_type_variation("PaddedMarginContainer", "MarginContainer");

	Ref<StyleBoxEmpty> empty_style(memnew(StyleBoxEmpty));

	panel_style.instantiate();
	flat_panel_style.instantiate();
	popup_panel_style.instantiate();
	tab_selected_style.instantiate();
	flat_tab_selected_style.instantiate();
	tab_unselected_style.instantiate();
	tab_hover_style.instantiate();
	tab_focus_style.instantiate();
	tab_empty_style.instantiate();
	tab_panel_style.instantiate();
	flat_tab_panel_style.instantiate();
	button_hover_style.instantiate();
	button_normal_style.instantiate();
	button_pressed_style.instantiate();
	button_disabled_style.instantiate();
	button_focus_style.instantiate();
	color_button_normal_style.instantiate();
	color_button_hover_style.instantiate();
	color_button_pressed_style.instantiate();
	color_button_disabled_style.instantiate();
	color_button_focus_style.instantiate();
	focus_rect_style.instantiate();
	focus_circle_style.instantiate();
	popup_hover_style.instantiate();
	progress_background_style.instantiate();
	progress_fill_style.instantiate();
	grabber_style.instantiate();
	grabber_highlight_style.instantiate();
	slider_style.instantiate();
	h_scroll_style.instantiate();
	v_scroll_style.instantiate();
	foldable_panel_style.instantiate();
	flat_foldable_panel_style.instantiate();
	foldable_title_style.instantiate();
	foldable_title_collapsed_style.instantiate();
	foldable_title_hover_style.instantiate();
	foldable_title_collapsed_hover_style.instantiate();
	h_separator_style.instantiate();
	v_separator_style.instantiate();
	button_empty_style.instantiate();
	embedded_style.instantiate();
	embedded_unfocused_style.instantiate();
	graph_title_style.instantiate();
	graph_frame_title_style.instantiate();
	graph_frame_title_selected_style.instantiate();
	graph_title_selected_style.instantiate();
	graph_panel_style.instantiate();
	graph_panel_selected_style.instantiate();
	code_edit_completion_style.instantiate();
	h_split_bar_background.instantiate();
	v_split_bar_background.instantiate();

	update_theme_font(t, p_font);
	update_theme_margins(t, p_template.margin);
	update_theme_padding(t, p_template.padding);
	update_theme_corner_radius(t, p_template.corner_radius);
	update_theme_border_width(t, p_template.border_width);
	update_theme_border_padding(t, p_template.border_width + p_template.padding);
	update_theme_scale(t);
	update_font_color(t, p_template.font_color); // Update font color before blazium_theme_icons and theme colors.
	update_font_outline_color(t, p_template.font_outline_color);
	update_theme_icons(t, p_template.font_color, p_template.accent_color);
	update_theme_colors(t, p_template.base_color, p_template.accent_color, p_template.contrast, p_template.normal_contrast, p_template.hover_contrast, p_template.pressed_contrast, p_template.bg_contrast);
	update_font_outline_size(t, p_template.font_outline_size);
	update_font_size(t, p_template.font_size);
	update_font_embolden(p_template.font_embolden);
	update_font_spacing_glyph(p_template.font_spacing_glyph);
	update_font_spacing_space(p_template.font_spacing_space);
	update_font_spacing_top(p_template.font_spacing_top);
	update_font_spacing_bottom(p_template.font_spacing_bottom);
	update_font_subpixel_positioning(p_template.font_subpixel);
	update_font_lcd_subpixel_layout(p_template.font_lcd_subpixel_layout);
	update_font_antialiasing(p_template.font_antialiasing);
	update_font_hinting(p_template.font_hinting);
	update_font_msdf(p_template.font_msdf);
	update_font_generate_mipmaps(p_template.font_generate_mipmaps);

	t->set_stylebox(CoreStringName(normal), "Button", button_normal_style);
	t->set_stylebox(SceneStringName(pressed), "Button", button_pressed_style);
	t->set_stylebox("hover", "Button", button_hover_style);
	t->set_stylebox("disabled", "Button", button_disabled_style);
	t->set_stylebox("focus", "Button", button_focus_style);

	t->set_stylebox(CoreStringName(normal), "ColorButton", color_button_normal_style);
	t->set_stylebox(SceneStringName(pressed), "ColorButton", color_button_pressed_style);
	t->set_stylebox("hover", "ColorButton", color_button_hover_style);
	t->set_stylebox("disabled", "ColorButton", color_button_disabled_style);
	t->set_stylebox("focus", "ColorButton", color_button_focus_style);

	t->set_stylebox("sample_focus", "ColorPicker", focus_rect_style);
	t->set_stylebox("picker_focus_rectangle", "ColorPicker", focus_rect_style);
	t->set_stylebox("picker_focus_circle", "ColorPicker", focus_circle_style);

	t->set_stylebox(CoreStringName(normal), "MenuButton", button_normal_style);
	t->set_stylebox(SceneStringName(pressed), "MenuButton", button_pressed_style);
	t->set_stylebox("hover", "MenuButton", button_hover_style);
	t->set_stylebox("disabled", "MenuButton", button_disabled_style);
	t->set_stylebox("focus", "MenuButton", button_focus_style);

	t->set_stylebox(CoreStringName(normal), "OptionButton", button_normal_style);
	t->set_stylebox(SceneStringName(pressed), "OptionButton", button_pressed_style);
	t->set_stylebox("hover", "OptionButton", button_hover_style);
	t->set_stylebox("disabled", "OptionButton", button_disabled_style);
	t->set_stylebox("focus", "OptionButton", button_focus_style);
	t->set_stylebox("normal_mirrored", "OptionButton", button_normal_style);
	t->set_stylebox("pressed_mirrored", "OptionButton", button_pressed_style);
	t->set_stylebox("hover_mirrored", "OptionButton", button_hover_style);
	t->set_stylebox("disabled_mirrored", "OptionButton", button_disabled_style);

	t->set_stylebox(CoreStringName(normal), "CheckBox", button_empty_style);
	t->set_stylebox(SceneStringName(pressed), "CheckBox", button_empty_style);
	t->set_stylebox("hover", "CheckBox", button_empty_style);
	t->set_stylebox("hover_pressed", "CheckBox", button_empty_style);
	t->set_stylebox("disabled", "CheckBox", button_empty_style);
	t->set_stylebox("focus", "CheckBox", button_focus_style);

	t->set_stylebox(CoreStringName(normal), "CheckButton", button_empty_style);
	t->set_stylebox(SceneStringName(pressed), "CheckButton", button_empty_style);
	t->set_stylebox("hover", "CheckButton", button_empty_style);
	t->set_stylebox("hover_pressed", "CheckButton", button_empty_style);
	t->set_stylebox("disabled", "CheckButton", button_empty_style);
	t->set_stylebox("focus", "CheckButton", button_focus_style);

	t->set_stylebox(CoreStringName(normal), "FlatButton", button_empty_style);
	t->set_stylebox(SceneStringName(pressed), "FlatButton", button_pressed_style);
	t->set_stylebox("hover", "FlatButton", button_hover_style);
	t->set_stylebox("disabled", "FlatButton", button_empty_style);

	t->set_stylebox(CoreStringName(normal), "FlatMenuButton", button_empty_style);
	t->set_stylebox(SceneStringName(pressed), "FlatMenuButton", button_pressed_style);
	t->set_stylebox("hover", "FlatMenuButton", button_empty_style);
	t->set_stylebox("disabled", "FlatMenuButton", button_empty_style);

	t->set_stylebox("focus", "LinkButton", button_empty_style);

	t->set_stylebox("tab_selected", "TabBar", tab_selected_style);
	t->set_stylebox("tab_unselected", "TabBar", tab_unselected_style);
	t->set_stylebox("tab_hovered", "TabBar", tab_hover_style);
	t->set_stylebox("tab_disabled", "TabBar", tab_empty_style);
	t->set_stylebox("tab_focus", "TabBar", tab_focus_style);
	t->set_stylebox("button_pressed", "TabBar", button_pressed_style);
	t->set_stylebox("button_highlight", "TabBar", button_hover_style);

	t->set_stylebox("tab_selected", "ButtonsTabBar", button_pressed_style);
	t->set_stylebox("tab_unselected", "ButtonsTabBar", button_empty_style);
	t->set_stylebox("tab_hovered", "ButtonsTabBar", button_hover_style);
	t->set_stylebox("tab_disabled", "ButtonsTabBar", button_empty_style);
	t->set_stylebox("tab_focus", "ButtonsTabBar", button_focus_style);

	t->set_stylebox(SceneStringName(panel), "TabContainer", tab_panel_style);
	t->set_stylebox("tab_selected", "TabContainer", tab_selected_style);
	t->set_stylebox("tab_unselected", "TabContainer", tab_unselected_style);
	t->set_stylebox("tab_hovered", "TabContainer", tab_hover_style);
	t->set_stylebox("tab_disabled", "TabContainer", tab_empty_style);
	t->set_stylebox("tab_focus", "TabContainer", tab_focus_style);
	t->set_stylebox("tabbar_background", "TabContainer", empty_style);

	t->set_stylebox(SceneStringName(panel), "FlatTabContainer", flat_tab_panel_style);
	t->set_stylebox("tab_selected", "FlatTabContainer", flat_tab_selected_style);

	t->set_stylebox("focus", "FoldableContainer", button_focus_style);
	t->set_stylebox("title_panel", "FoldableContainer", foldable_title_style);
	t->set_stylebox("title_collapsed_panel", "FoldableContainer", foldable_title_collapsed_style);
	t->set_stylebox("title_hover_panel", "FoldableContainer", foldable_title_hover_style);
	t->set_stylebox("title_collapsed_hover_panel", "FoldableContainer", foldable_title_collapsed_hover_style);
	t->set_stylebox(SceneStringName(panel), "FoldableContainer", foldable_panel_style);

	t->set_stylebox(SceneStringName(panel), "FlatFoldableContainer", flat_foldable_panel_style);

	t->set_stylebox("up_background_hovered", "SpinBox", button_hover_style);
	t->set_stylebox("down_background_hovered", "SpinBox", button_hover_style);
	t->set_stylebox("up_background_pressed", "SpinBox", button_pressed_style);
	t->set_stylebox("down_background_pressed", "SpinBox", button_pressed_style);
	t->set_stylebox("field_and_buttons_separator", "SpinBox", empty_style);
	t->set_stylebox("up_down_buttons_separator", "SpinBox", empty_style);
	t->set_stylebox("up_background", "SpinBox", button_empty_style);
	t->set_stylebox("down_background", "SpinBox", button_empty_style);
	t->set_stylebox("up_background_disabled", "SpinBox", button_empty_style);
	t->set_stylebox("down_background_disabled", "SpinBox", button_empty_style);

	t->set_stylebox("custom_button", "Tree", button_empty_style);
	t->set_stylebox("custom_button_hover", "Tree", button_hover_style);
	t->set_stylebox("custom_button_pressed", "Tree", button_pressed_style);
	t->set_stylebox("focus", "Tree", button_focus_style);
	t->set_stylebox("selected", "Tree", popup_hover_style);
	t->set_stylebox("selected_focus", "Tree", popup_hover_style);
	t->set_stylebox("title_button_hover", "Tree", button_hover_style);
	t->set_stylebox("title_button_normal", "Tree", button_empty_style);
	t->set_stylebox("title_button_pressed", "Tree", button_pressed_style);
	t->set_stylebox("cursor", "Tree", button_focus_style);
	t->set_stylebox("cursor_unfocused", "Tree", button_focus_style);
	t->set_stylebox("button_pressed", "Tree", button_pressed_style);
	t->set_stylebox(SceneStringName(panel), "Tree", button_normal_style);

	t->set_stylebox("hover", "PopupMenu", popup_hover_style);
	t->set_stylebox("labeled_separator_left", "PopupMenu", h_separator_style);
	t->set_stylebox("labeled_separator_right", "PopupMenu", h_separator_style);
	t->set_stylebox("separator", "PopupMenu", h_separator_style);
	t->set_stylebox(SceneStringName(panel), "PopupMenu", popup_panel_style);

	t->set_stylebox(CoreStringName(normal), "LineEdit", button_normal_style);
	t->set_stylebox("read_only", "LineEdit", button_disabled_style);
	t->set_stylebox("focus", "LineEdit", button_focus_style);

	t->set_stylebox(CoreStringName(normal), "TextEdit", button_normal_style);
	t->set_stylebox("read_only", "TextEdit", button_disabled_style);
	t->set_stylebox("focus", "TextEdit", button_focus_style);

	t->set_stylebox(CoreStringName(normal), "CodeEdit", button_normal_style);
	t->set_stylebox("focus", "CodeEdit", button_focus_style);
	t->set_stylebox("read_only", "CodeEdit", button_disabled_style);
	t->set_stylebox("completion", "CodeEdit", code_edit_completion_style);

	t->set_stylebox("grabber_area", "HSlider", grabber_style);
	t->set_stylebox("slider", "HSlider", slider_style);
	t->set_stylebox("grabber_area_highlight", "HSlider", grabber_highlight_style);

	t->set_stylebox("grabber_area", "VSlider", grabber_style);
	t->set_stylebox("grabber_area_highlight", "VSlider", grabber_highlight_style);
	t->set_stylebox("slider", "VSlider", slider_style);

	t->set_stylebox("grabber", "HScrollBar", grabber_style);
	t->set_stylebox("grabber_highlight", "HScrollBar", grabber_highlight_style);
	t->set_stylebox("grabber_pressed", "HScrollBar", grabber_highlight_style);
	t->set_stylebox("scroll", "HScrollBar", h_scroll_style);
	t->set_stylebox("scroll_focus", "HScrollBar", empty_style);

	t->set_stylebox("grabber", "VScrollBar", grabber_style);
	t->set_stylebox("grabber_highlight", "VScrollBar", grabber_highlight_style);
	t->set_stylebox("grabber_pressed", "VScrollBar", grabber_highlight_style);
	t->set_stylebox("scroll", "VScrollBar", v_scroll_style);
	t->set_stylebox("scroll_focus", "VScrollBar", empty_style);

	t->set_stylebox("focus", "ItemList", button_focus_style);
	t->set_stylebox("cursor", "ItemList", button_focus_style);
	t->set_stylebox("cursor_unfocused", "ItemList", button_focus_style);
	t->set_stylebox("hovered", "ItemList", button_hover_style);
	t->set_stylebox("selected", "ItemList", button_pressed_style);
	t->set_stylebox("hovered_selected", "ItemList", button_pressed_style);
	t->set_stylebox("selected_focus", "ItemList", button_pressed_style);
	t->set_stylebox("hovered_selected_focus", "ItemList", button_pressed_style);
	t->set_stylebox(SceneStringName(panel), "ItemList", panel_style);

	t->set_stylebox(CoreStringName(normal), "MenuBar", button_empty_style);
	t->set_stylebox("hover", "MenuBar", button_hover_style);
	t->set_stylebox(SceneStringName(pressed), "MenuBar", button_pressed_style);
	t->set_stylebox("disabled", "MenuBar", button_disabled_style);

	t->set_stylebox("menu_panel", "GraphEdit", button_disabled_style);
	t->set_stylebox(SceneStringName(panel), "GraphEdit", tab_panel_style);

	t->set_stylebox("camera", "GraphEditMinimap", button_focus_style);
	t->set_stylebox("node", "GraphEditMinimap", empty_style);
	t->set_stylebox(SceneStringName(panel), "GraphEditMinimap", empty_style);

	t->set_stylebox("titlebar", "GraphFrame", graph_frame_title_style);
	t->set_stylebox("titlebar_selected", "GraphFrame", graph_frame_title_selected_style);
	t->set_stylebox("panel_selected", "GraphFrame", graph_panel_selected_style);
	t->set_stylebox(SceneStringName(panel), "GraphFrame", graph_panel_style);

	t->set_stylebox("panel_selected", "GraphNode", graph_panel_selected_style);
	t->set_stylebox("titlebar", "GraphNode", graph_title_style);
	t->set_stylebox("titlebar_selected", "GraphNode", graph_title_selected_style);
	t->set_stylebox("slot", "GraphNode", empty_style);
	t->set_stylebox(SceneStringName(panel), "GraphNode", graph_panel_style);

	t->set_stylebox(CoreStringName(normal), "GraphFrameTitleLabel", empty_style);
	t->set_stylebox(CoreStringName(normal), "GraphNodeTitleLabel", empty_style);

	t->set_stylebox(SceneStringName(panel), "Panel", panel_style);
	t->set_stylebox(SceneStringName(panel), "PanelContainer", panel_style);
	t->set_stylebox(SceneStringName(panel), "FlatPanel", flat_panel_style);
	t->set_stylebox(SceneStringName(panel), "FlatPanelContainer", flat_panel_style);
	t->set_stylebox(SceneStringName(panel), "PopupPanel", popup_panel_style);
	t->set_stylebox(SceneStringName(panel), "AcceptDialog", popup_panel_style);
	t->set_stylebox(SceneStringName(panel), "TooltipPanel", popup_panel_style);
	t->set_stylebox(SceneStringName(panel), "PopupDialog", popup_panel_style);
	t->set_stylebox(SceneStringName(panel), "ScrollContainer", empty_style);

	t->set_stylebox("background", "ProgressBar", progress_background_style);
	t->set_stylebox("fill", "ProgressBar", progress_fill_style);

	t->set_stylebox("separator", "HSeparator", h_separator_style);
	t->set_stylebox("separator", "VSeparator", v_separator_style);

	t->set_stylebox(CoreStringName(normal), "Label", empty_style);

	t->set_stylebox("focus", "RichTextLabel", button_focus_style);
	t->set_stylebox(CoreStringName(normal), "RichTextLabel", button_empty_style);

	t->set_stylebox("embedded_border", "Window", embedded_style);
	t->set_stylebox("embedded_unfocused_border", "Window", embedded_unfocused_style);

	t->set_stylebox("h_split_bar_background", "SplitContainer", empty_style);
	t->set_stylebox("v_split_bar_background", "SplitContainer", empty_style);
	t->set_stylebox("split_bar_background", "HSplitContainer", empty_style);
	t->set_stylebox("split_bar_background", "VSplitContainer", empty_style);

	t->set_stylebox("h_split_bar_background", "FlatSplitContainer", h_split_bar_background);
	t->set_stylebox("v_split_bar_background", "FlatSplitContainer", v_split_bar_background);
	t->set_stylebox("split_bar_background", "FlatHSplitContainer", h_split_bar_background);
	t->set_stylebox("split_bar_background", "FlatVSplitContainer", v_split_bar_background);

	v_separator_style->set_vertical(true);
	tab_focus_style->set_draw_center(false);
	button_focus_style->set_draw_center(false);
	color_button_focus_style->set_draw_center(false);
	focus_rect_style->set_draw_center(false);
	focus_circle_style->set_draw_center(false);

	t->set_font_size(SceneStringName(font_size), "Button", -1);
	t->set_font_size(SceneStringName(font_size), "MenuBar", -1);
	t->set_font_size(SceneStringName(font_size), "LinkButton", -1);
	t->set_font_size(SceneStringName(font_size), "OptionButton", -1);
	t->set_font_size(SceneStringName(font_size), "MenuButton", -1);
	t->set_font_size(SceneStringName(font_size), "CheckBox", -1);
	t->set_font_size(SceneStringName(font_size), "CheckButton", -1);
	t->set_font_size(SceneStringName(font_size), "Label", -1);
	t->set_font_size(SceneStringName(font_size), "LineEdit", -1);
	t->set_font_size(SceneStringName(font_size), "ProgressBar", -1);
	t->set_font_size(SceneStringName(font_size), "TextEdit", -1);
	t->set_font_size(SceneStringName(font_size), "CodeEdit", -1);
	t->set_font_size(SceneStringName(font_size), "PopupMenu", -1);
	t->set_font_size(SceneStringName(font_size), "GraphNodeTitleLabel", -1);
	t->set_font_size(SceneStringName(font_size), "Tree", -1);
	t->set_font_size(SceneStringName(font_size), "ItemList", -1);
	t->set_font_size(SceneStringName(font_size), "TabContainer", -1);
	t->set_font_size(SceneStringName(font_size), "TabBar", -1);
	t->set_font_size(SceneStringName(font_size), "TooltipLabel", -1);
	t->set_font_size(SceneStringName(font_size), "FoldableContainer", -1);
	t->set_font_size("title_font_size", "Window", -1);
	t->set_font_size("font_separator_size", "PopupMenu", -1);
	t->set_font_size("title_button_font_size", "Tree", -1);
	t->set_font_size("normal_font_size", "RichTextLabel", -1);
	t->set_font_size("bold_font_size", "RichTextLabel", -1);
	t->set_font_size("italics_font_size", "RichTextLabel", -1);
	t->set_font_size("bold_italics_font_size", "RichTextLabel", -1);
	t->set_font_size("mono_font_size", "RichTextLabel", -1);
	t->set_constant("scrollbar_margin_left", "Tree", -1);
	t->set_constant("scrollbar_margin_top", "Tree", -1);
	t->set_constant("scrollbar_margin_right", "Tree", -1);
	t->set_constant("scrollbar_margin_bottom", "Tree", -1);

	t->set_constant("icon_max_width", "Button", 0);
	t->set_constant("align_to_largest_stylebox", "Button", 0);
	t->set_constant("icon_max_width", "Tree", 0);
	t->set_constant("check_v_offset", "CheckBox", 0);
	t->set_constant("check_v_offset", "CheckButton", 0);
	t->set_constant("center_grabber", "HSlider", 0);
	t->set_constant("grabber_offset", "HSlider", 0);
	t->set_constant("center_grabber", "VSlider", 0);
	t->set_constant("grabber_offset", "VSlider", 0);
	t->set_constant("inner_item_margin_bottom", "Tree", 0);
	t->set_constant("inner_item_margin_top", "Tree", 0);
	t->set_constant("buttons_vertical_separation", "SpinBox", 0);
	t->set_constant("icon_max_width", "PopupMenu", 0);
	t->set_constant("parent_hl_line_margin", "Tree", 0);
	t->set_constant("icon_max_width", "TabContainer", 0);
	t->set_constant("icon_max_width", "TabBar", 0);
	t->set_constant("draw_guides", "Tree", 0);
	t->set_constant("side_margin", "TabContainer", 0);
	t->set_constant("modulate_arrow", "OptionButton", 0);
	t->set_constant("port_h_offset", "GraphNode", 0);
	t->set_constant(SceneStringName(line_separation), "RichTextLabel", 0);
	t->set_constant("separation", "FlatSplitContainer", 0);
	t->set_constant("separation", "FlatHSplitContainer", 0);
	t->set_constant("separation", "FlatVSplitContainer", 0);
	t->set_constant("draw_grabber_icon", "FlatSplitContainer", 0);
	t->set_constant("draw_grabber_icon", "FlatHSplitContainer", 0);
	t->set_constant("draw_grabber_icon", "FlatVSplitContainer", 0);
	t->set_constant("connection_hover_thickness", "GraphEdit", 0);

	t->set_constant("draw_split_bar", "FlatSplitContainer", 1);
	t->set_constant("draw_split_bar", "FlatHSplitContainer", 1);
	t->set_constant("draw_split_bar", "FlatVSplitContainer", 1);
	t->set_constant("autohide", "SplitContainer", 1);
	t->set_constant("autohide", "HSplitContainer", 1);
	t->set_constant("autohide", "VSplitContainer", 1);
	t->set_constant("autohide_split_bar", "SplitContainer", 1);
	t->set_constant("autohide_split_bar", "HSplitContainer", 1);
	t->set_constant("autohide_split_bar", "VSplitContainer", 1);
	t->set_constant("draw_grabber_icon", "SplitContainer", 1);
	t->set_constant("draw_grabber_icon", "HSplitContainer", 1);
	t->set_constant("draw_grabber_icon", "VSplitContainer", 1);
	t->set_constant("draw_relationship_lines", "Tree", 1);
	t->set_constant("center_slider_grabbers", "ColorPicker", 1);
	t->set_constant("colorize_sliders", "ColorPicker", 1);
	t->set_constant("gutter_compact", "PopupMenu", 1);

	t->set_constant("minimum_character_width", "LineEdit", 4);
	t->set_constant("completion_scroll_width", "CodeEdit", 6);
	t->set_constant("completion_lines", "CodeEdit", 7);
	t->set_constant("wrap_offset", "TextEdit", 10);
	t->set_constant("dragging_unfold_wait_msec", "Tree", 500);

	t->set_color("font_shadow_color", "Label", Color(0, 0, 0, 0));
	t->set_color("font_shadow_color", "GraphNodeTitleLabel", Color(0, 0, 0, 0));
	t->set_color("font_shadow_color", "GraphFrameTitleLabel", Color(0, 0, 0, 0));
	t->set_color("font_shadow_color", "TooltipLabel", Color(0, 0, 0, 0));
	t->set_color("font_shadow_color", "RichTextLabel", Color(0, 0, 0, 0));
	t->set_color("background_color", "TextEdit", Color(0, 0, 0, 0));
	t->set_color("background_color", "CodeEdit", Color(0, 0, 0, 0));
	t->set_color("table_odd_row_bg", "RichTextLabel", Color(0, 0, 0, 0));
	t->set_color("table_even_row_bg", "RichTextLabel", Color(0, 0, 0, 0));
	t->set_color("table_border", "RichTextLabel", Color(0, 0, 0, 0));

	t->set_color("completion_background_color", "CodeEdit", Color(0.17, 0.16, 0.2));
	t->set_color("completion_selected_color", "CodeEdit", Color(0.26, 0.26, 0.27));
	t->set_color("completion_existing_color", "CodeEdit", Color(0.87, 0.87, 0.87, 0.13));
	t->set_color("bookmark_color", "CodeEdit", Color(0.5, 0.64, 1, 0.8));
	t->set_color("breakpoint_color", "CodeEdit", Color(0.9, 0.29, 0.3));
	t->set_color("executing_line_color", "CodeEdit", Color(0.98, 0.89, 0.27));
	t->set_color("code_folding_color", "CodeEdit", Color(0.8, 0.8, 0.8, 0.8));
	t->set_color("folded_code_region_color", "CodeEdit", Color(0.68, 0.46, 0.77, 0.2));
	t->set_color("brace_mismatch_color", "CodeEdit", Color(1, 0.2, 0.2));
	t->set_color("line_number_color", "CodeEdit", Color(0.67, 0.67, 0.67, 0.4));
	t->set_color("line_length_guideline_color", "CodeEdit", Color(0.3, 0.5, 0.8, 0.1));
	t->set_color("focused_not_editing_cursor_color", "ColorPicker", Color(1, 1, 1, 0.2));

	embedded_style->set_content_margin_individual(10, 28, 10, 8);
	embedded_unfocused_style->set_content_margin_individual(10, 28, 10, 8);
	embedded_style->set_expand_margin_individual(8, 32, 8, 6);
	embedded_unfocused_style->set_expand_margin_individual(8, 32, 8, 6);

	t->set_font(SceneStringName(font), "Button", Ref<Font>());
	t->set_font(SceneStringName(font), "TabBar", Ref<Font>());
	t->set_font(SceneStringName(font), "ItemList", Ref<Font>());
	t->set_font(SceneStringName(font), "Tree", Ref<Font>());
	t->set_font(SceneStringName(font), "GraphNodeTitleLabel", Ref<Font>());
	t->set_font(SceneStringName(font), "PopupMenu", Ref<Font>());
	t->set_font(SceneStringName(font), "CodeEdit", Ref<Font>());
	t->set_font(SceneStringName(font), "TextEdit", Ref<Font>());
	t->set_font(SceneStringName(font), "ProgressBar", Ref<Font>());
	t->set_font(SceneStringName(font), "LineEdit", Ref<Font>());
	t->set_font(SceneStringName(font), "Label", Ref<Font>());
	t->set_font(SceneStringName(font), "CheckBox", Ref<Font>());
	t->set_font(SceneStringName(font), "CheckButton", Ref<Font>());
	t->set_font(SceneStringName(font), "FoldableContainer", Ref<Font>());
	t->set_font(SceneStringName(font), "LinkButton", Ref<Font>());
	t->set_font(SceneStringName(font), "MenuBar", Ref<Font>());
	t->set_font(SceneStringName(font), "MenuButton", Ref<Font>());
	t->set_font(SceneStringName(font), "OptionButton", Ref<Font>());
	t->set_font(SceneStringName(font), "TabContainer", Ref<Font>());
	t->set_font(SceneStringName(font), "TooltipLabel", Ref<Font>());
	t->set_font("font_separator", "PopupMenu", Ref<Font>());
	t->set_font("title_button_font", "Tree", Ref<Font>());
	t->set_font("title_font", "Window", Ref<Font>());
	t->set_font("normal_font", "RichTextLabel", Ref<Font>());
	t->set_font("mono_font", "RichTextLabel", Ref<Font>());

	ThemeDB::get_singleton()->set_default_theme(t);
	ThemeDB::get_singleton()->set_fallback_base_scale(theme_scale);
	ThemeDB::get_singleton()->set_fallback_stylebox(empty_style);
	ThemeDB::get_singleton()->set_fallback_font_size(p_template.font_size * theme_scale);
}

void finalize_blazium_default_theme() {
	blazium_theme_icons.clear();
	user_icons.clear();
	user_icons_sources.clear();

	panel_style.unref();
	flat_panel_style.unref();
	popup_panel_style.unref();
	tab_selected_style.unref();
	flat_tab_selected_style.unref();
	tab_unselected_style.unref();
	tab_hover_style.unref();
	tab_focus_style.unref();
	tab_empty_style.unref();
	tab_panel_style.unref();
	flat_tab_panel_style.unref();
	button_normal_style.unref();
	button_empty_style.unref();
	button_pressed_style.unref();
	button_hover_style.unref();
	button_disabled_style.unref();
	button_focus_style.unref();
	color_button_normal_style.unref();
	color_button_hover_style.unref();
	color_button_pressed_style.unref();
	color_button_disabled_style.unref();
	color_button_focus_style.unref();
	focus_rect_style.unref();
	focus_circle_style.unref();
	popup_hover_style.unref();
	progress_background_style.unref();
	progress_fill_style.unref();
	grabber_style.unref();
	grabber_highlight_style.unref();
	slider_style.unref();
	h_scroll_style.unref();
	v_scroll_style.unref();
	foldable_panel_style.unref();
	flat_foldable_panel_style.unref();
	foldable_title_style.unref();
	foldable_title_collapsed_style.unref();
	foldable_title_hover_style.unref();
	foldable_title_collapsed_hover_style.unref();
	h_separator_style.unref();
	v_separator_style.unref();
	embedded_style.unref();
	embedded_unfocused_style.unref();
	graph_title_style.unref();
	graph_frame_title_style.unref();
	graph_frame_title_selected_style.unref();
	graph_title_selected_style.unref();
	graph_panel_style.unref();
	graph_panel_selected_style.unref();
	code_edit_completion_style.unref();
	h_split_bar_background.unref();
	v_split_bar_background.unref();

	if (custom_font_variation.is_valid()) {
		custom_font_variation.unref();
	}

	font_variation.unref();
	blazium_bold_font.unref();
	blazium_bold_italics_font.unref();
	blazium_italics_font.unref();
}
