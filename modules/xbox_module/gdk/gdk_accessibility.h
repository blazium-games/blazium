/**************************************************************************/
/*  gdk_accessibility.h                                                   */
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

#include "gdk_gdk_stubs.h"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifdef XBOX_MODULE_GDK_ENABLED
#include "gdk_windows.h"
#endif

#include "core/math/color.h"
#include "core/object/ref_counted.h"
#include "core/string/ustring.h"
#include "core/variant/type_info.h"

#ifdef XBOX_MODULE_GDK_ENABLED
#include <XAccessibility.h>
#endif

class GDK;
class GDKResult;
class GDKRuntime;

class GDKClosedCaptionProperties : public RefCounted {
	GDCLASS(GDKClosedCaptionProperties, RefCounted);

public:
	enum FontEdgeAttribute {
		FONT_EDGE_ATTRIBUTE_DEFAULT = 0,
		FONT_EDGE_ATTRIBUTE_NONE,
		FONT_EDGE_ATTRIBUTE_RAISED,
		FONT_EDGE_ATTRIBUTE_DEPRESSED,
		FONT_EDGE_ATTRIBUTE_UNIFORM,
		FONT_EDGE_ATTRIBUTE_DROP_SHADOW,
	};

	enum FontStyle {
		FONT_STYLE_DEFAULT = 0,
		FONT_STYLE_MONOSPACED_SERIF,
		FONT_STYLE_PROPORTIONAL_SERIF,
		FONT_STYLE_MONOSPACED_SANS_SERIF,
		FONT_STYLE_PROPORTIONAL_SANS_SERIF,
		FONT_STYLE_CASUAL,
		FONT_STYLE_CURSIVE,
		FONT_STYLE_SMALL_CAPITALS,
	};

private:
	Color m_background_color = Color(0, 0, 0, 1);
	Color m_font_color = Color(1, 1, 1, 1);
	Color m_window_color = Color(0, 0, 0, 1);
	FontEdgeAttribute m_font_edge_attribute = FONT_EDGE_ATTRIBUTE_DEFAULT;
	FontStyle m_font_style = FONT_STYLE_DEFAULT;
	double m_font_scale = 1.0;
	bool m_enabled = false;

protected:
	static void _bind_methods();

public:
	Color get_background_color() const;
	Color get_font_color() const;
	Color get_window_color() const;
	FontEdgeAttribute get_font_edge_attribute() const;
	String get_font_edge_attribute_name() const;
	FontStyle get_font_style() const;
	String get_font_style_name() const;
	double get_font_scale() const;
	bool is_enabled() const;

	void set_from_native(const XClosedCaptionProperties &p_properties);
};

class GDKAccessibility : public RefCounted {
	GDCLASS(GDKAccessibility, RefCounted);

public:
	enum HighContrastMode {
		HIGH_CONTRAST_MODE_OFF = 0,
		HIGH_CONTRAST_MODE_DARK,
		HIGH_CONTRAST_MODE_LIGHT,
		HIGH_CONTRAST_MODE_OTHER,
	};

private:
	GDK *m_owner = nullptr;

	GDKRuntime *_get_runtime() const;
	static HighContrastMode _to_high_contrast_mode(XHighContrastMode p_mode);
	static String _high_contrast_mode_to_name(HighContrastMode p_mode);

protected:
	static void _bind_methods();

public:
	void set_owner(GDK *p_owner);

	Ref<GDKResult> query_closed_caption_properties() const;
	Ref<GDKResult> set_closed_caption_enabled(bool p_enabled) const;
	Ref<GDKResult> query_high_contrast_mode() const;
	String get_high_contrast_mode_name(HighContrastMode p_mode) const;
};

VARIANT_ENUM_CAST(GDKClosedCaptionProperties::FontEdgeAttribute);
VARIANT_ENUM_CAST(GDKClosedCaptionProperties::FontStyle);
VARIANT_ENUM_CAST(GDKAccessibility::HighContrastMode);
