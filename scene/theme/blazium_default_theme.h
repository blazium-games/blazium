/**************************************************************************/
/*  blazium_default_theme.h                                               */
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

#pragma once

#include "scene/resources/theme.h"

struct ThemeTemplate {
	Color base_color = Color(0.188, 0.188, 0.188);
	Color accent_color = Color(0.226, 0.478, 0.921);
	Color font_color = Color(0.875, 0.875, 0.875);
	Color font_outline_color = Color(0, 0, 0, 1);

	float scale = 1.f;

	float contrast = -0.6f;
	float bg_contrast = 0.2f;
	float normal_contrast = 0.4f;
	float hover_contrast = -0.2f;
	float pressed_contrast = 0.6f;

	int margin = 4;
	int padding = 4;
	int border_width = 2;
	int corner_radius = 6;

	float font_embolden = 0.f;
	int font_size = 16;
	int font_outline_size = 0;
	int font_spacing_glyph = 0;
	int font_spacing_space = 0;
	int font_spacing_top = 0;
	int font_spacing_bottom = 0;

	TextServer::SubpixelPositioning font_subpixel = TextServer::SUBPIXEL_POSITIONING_AUTO;
	TextServer::Hinting font_hinting = TextServer::HINTING_LIGHT;
	TextServer::FontAntialiasing font_antialiasing = TextServer::FONT_ANTIALIASING_GRAY;
	TextServer::FontLCDSubpixelLayout font_lcd_subpixel_layout = TextServer::FontLCDSubpixelLayout::FONT_LCD_SUBPIXEL_LAYOUT_HRGB;

	bool font_msdf = false;
	bool font_generate_mipmaps = false;
};

class ImageTexture;

Error add_user_icon(const String &p_icon_name, const String &p_icon_source, float p_scale, const Color &p_font_color, const Color &p_accent_color);
Error remove_user_icon(const String &p_icon_name);
bool has_user_icon(const String &p_icon_name);
Ref<ImageTexture> get_user_icon(const String &p_icon_name);
PackedStringArray get_user_icons_list();
bool has_icon(const String &p_icon_name);
Ref<ImageTexture> get_icon(const String &p_icon_name);
PackedStringArray get_icons_list();
void update_theme_icons(const Ref<Theme> &p_theme, const Color &p_font_color, const Color &p_accent_color);
void update_font_color(const Ref<Theme> &p_theme, const Color &p_color);
void update_font_outline_color(const Ref<Theme> &p_theme, const Color &p_color);
void update_theme_margins(const Ref<Theme> &p_theme, int p_margin);
void update_theme_padding(const Ref<Theme> &p_theme, int p_padding);
void update_theme_corner_radius(const Ref<Theme> &p_theme, int p_corner_radius);
void update_theme_border_width(const Ref<Theme> &p_theme, int p_border_width);
void update_theme_border_padding(const Ref<Theme> &p_theme, int p_border_padding);
void update_font_outline_size(const Ref<Theme> &p_theme, int p_outline_size);
void update_font_size(const Ref<Theme> &p_theme, int p_font_size);
void update_theme_scale(const Ref<Theme> &p_theme);
void update_theme_colors(const Ref<Theme> &p_theme, const Color &p_base_color, const Color &p_accent_color, float p_contrast, float p_normal_contrast, float p_hover_contrast, float p_pressed_contrast, float p_bg_contrast);
void update_font_embolden(float p_embolden);
void update_font_spacing_glyph(int p_spacing);
void update_font_spacing_space(int p_spacing);
void update_font_spacing_top(int p_spacing);
void update_font_spacing_bottom(int p_spacing);
void update_theme_font(const Ref<Theme> &p_theme, Ref<Font> p_font);
void update_font_subpixel_positioning(TextServer::SubpixelPositioning p_font_subpixel_positioning);
void update_font_antialiasing(TextServer::FontAntialiasing p_font_antialiasing);
void update_font_lcd_subpixel_layout(TextServer::FontLCDSubpixelLayout p_font_lcd_subpixel_layout);
void update_font_hinting(TextServer::Hinting p_font_hinting);
void update_font_msdf(bool p_font_msdf);
void update_font_generate_mipmaps(bool p_font_generate_mipmaps);
void make_blazium_default_theme(Ref<Font> p_font, ThemeTemplate &p_template);
void finalize_blazium_default_theme();
