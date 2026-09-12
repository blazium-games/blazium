/**************************************************************************/
/*  gdk_accessibility.cpp                                                 */
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

#include "gdk_accessibility.h"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include "gdk_windows.h"

#include "gdk.h"
#include "gdk_result.h"
#include "gdk_runtime.h"

namespace {

#ifdef XBOX_MODULE_GDK_ENABLED
Color _xcolor_to_color(const XColor &p_color) {
	const uint8_t *channels = reinterpret_cast<const uint8_t *>(&p_color.Value);
	return Color(
			static_cast<float>(channels[1]) / 255.0f,
			static_cast<float>(channels[2]) / 255.0f,
			static_cast<float>(channels[3]) / 255.0f,
			static_cast<float>(channels[0]) / 255.0f);
}

GDKClosedCaptionProperties::FontEdgeAttribute _to_font_edge_attribute(XClosedCaptionFontEdgeAttribute p_value) {
	switch (p_value) {
		case XClosedCaptionFontEdgeAttribute::NoEdgeAttribute:
			return GDKClosedCaptionProperties::FONT_EDGE_ATTRIBUTE_NONE;
		case XClosedCaptionFontEdgeAttribute::RaisedEdges:
			return GDKClosedCaptionProperties::FONT_EDGE_ATTRIBUTE_RAISED;
		case XClosedCaptionFontEdgeAttribute::DepressedEdges:
			return GDKClosedCaptionProperties::FONT_EDGE_ATTRIBUTE_DEPRESSED;
		case XClosedCaptionFontEdgeAttribute::UniformedEdges:
			return GDKClosedCaptionProperties::FONT_EDGE_ATTRIBUTE_UNIFORM;
		case XClosedCaptionFontEdgeAttribute::DropShadowedEdges:
			return GDKClosedCaptionProperties::FONT_EDGE_ATTRIBUTE_DROP_SHADOW;
		case XClosedCaptionFontEdgeAttribute::Default:
		default:
			return GDKClosedCaptionProperties::FONT_EDGE_ATTRIBUTE_DEFAULT;
	}
}

GDKClosedCaptionProperties::FontStyle _to_font_style(XClosedCaptionFontStyle p_value) {
	switch (p_value) {
		case XClosedCaptionFontStyle::MonospacedWithSerifs:
			return GDKClosedCaptionProperties::FONT_STYLE_MONOSPACED_SERIF;
		case XClosedCaptionFontStyle::ProportionalWithSerifs:
			return GDKClosedCaptionProperties::FONT_STYLE_PROPORTIONAL_SERIF;
		case XClosedCaptionFontStyle::MonospacedWithoutSerifs:
			return GDKClosedCaptionProperties::FONT_STYLE_MONOSPACED_SANS_SERIF;
		case XClosedCaptionFontStyle::ProportionalWithoutSerifs:
			return GDKClosedCaptionProperties::FONT_STYLE_PROPORTIONAL_SANS_SERIF;
		case XClosedCaptionFontStyle::Casual:
			return GDKClosedCaptionProperties::FONT_STYLE_CASUAL;
		case XClosedCaptionFontStyle::Cursive:
			return GDKClosedCaptionProperties::FONT_STYLE_CURSIVE;
		case XClosedCaptionFontStyle::SmallCapitals:
			return GDKClosedCaptionProperties::FONT_STYLE_SMALL_CAPITALS;
		case XClosedCaptionFontStyle::Default:
		default:
			return GDKClosedCaptionProperties::FONT_STYLE_DEFAULT;
	}
}
#endif

String _font_edge_attribute_to_name(GDKClosedCaptionProperties::FontEdgeAttribute p_value) {
	switch (p_value) {
		case GDKClosedCaptionProperties::FONT_EDGE_ATTRIBUTE_NONE:
			return "none";
		case GDKClosedCaptionProperties::FONT_EDGE_ATTRIBUTE_RAISED:
			return "raised";
		case GDKClosedCaptionProperties::FONT_EDGE_ATTRIBUTE_DEPRESSED:
			return "depressed";
		case GDKClosedCaptionProperties::FONT_EDGE_ATTRIBUTE_UNIFORM:
			return "uniform";
		case GDKClosedCaptionProperties::FONT_EDGE_ATTRIBUTE_DROP_SHADOW:
			return "drop_shadow";
		case GDKClosedCaptionProperties::FONT_EDGE_ATTRIBUTE_DEFAULT:
		default:
			return "default";
	}
}

String _font_style_to_name(GDKClosedCaptionProperties::FontStyle p_value) {
	switch (p_value) {
		case GDKClosedCaptionProperties::FONT_STYLE_MONOSPACED_SERIF:
			return "monospaced_serif";
		case GDKClosedCaptionProperties::FONT_STYLE_PROPORTIONAL_SERIF:
			return "proportional_serif";
		case GDKClosedCaptionProperties::FONT_STYLE_MONOSPACED_SANS_SERIF:
			return "monospaced_sans_serif";
		case GDKClosedCaptionProperties::FONT_STYLE_PROPORTIONAL_SANS_SERIF:
			return "proportional_sans_serif";
		case GDKClosedCaptionProperties::FONT_STYLE_CASUAL:
			return "casual";
		case GDKClosedCaptionProperties::FONT_STYLE_CURSIVE:
			return "cursive";
		case GDKClosedCaptionProperties::FONT_STYLE_SMALL_CAPITALS:
			return "small_capitals";
		case GDKClosedCaptionProperties::FONT_STYLE_DEFAULT:
		default:
			return "default";
	}
}

} //namespace

void GDKClosedCaptionProperties::_bind_methods() {
	ClassDB::bind_method(D_METHOD("get_background_color"), &GDKClosedCaptionProperties::get_background_color);
	ClassDB::bind_method(D_METHOD("get_font_color"), &GDKClosedCaptionProperties::get_font_color);
	ClassDB::bind_method(D_METHOD("get_window_color"), &GDKClosedCaptionProperties::get_window_color);
	ClassDB::bind_method(D_METHOD("get_font_edge_attribute"), &GDKClosedCaptionProperties::get_font_edge_attribute);
	ClassDB::bind_method(D_METHOD("get_font_edge_attribute_name"), &GDKClosedCaptionProperties::get_font_edge_attribute_name);
	ClassDB::bind_method(D_METHOD("get_font_style"), &GDKClosedCaptionProperties::get_font_style);
	ClassDB::bind_method(D_METHOD("get_font_style_name"), &GDKClosedCaptionProperties::get_font_style_name);
	ClassDB::bind_method(D_METHOD("get_font_scale"), &GDKClosedCaptionProperties::get_font_scale);
	ClassDB::bind_method(D_METHOD("is_enabled"), &GDKClosedCaptionProperties::is_enabled);

	BIND_ENUM_CONSTANT(FONT_EDGE_ATTRIBUTE_DEFAULT);
	BIND_ENUM_CONSTANT(FONT_EDGE_ATTRIBUTE_NONE);
	BIND_ENUM_CONSTANT(FONT_EDGE_ATTRIBUTE_RAISED);
	BIND_ENUM_CONSTANT(FONT_EDGE_ATTRIBUTE_DEPRESSED);
	BIND_ENUM_CONSTANT(FONT_EDGE_ATTRIBUTE_UNIFORM);
	BIND_ENUM_CONSTANT(FONT_EDGE_ATTRIBUTE_DROP_SHADOW);

	BIND_ENUM_CONSTANT(FONT_STYLE_DEFAULT);
	BIND_ENUM_CONSTANT(FONT_STYLE_MONOSPACED_SERIF);
	BIND_ENUM_CONSTANT(FONT_STYLE_PROPORTIONAL_SERIF);
	BIND_ENUM_CONSTANT(FONT_STYLE_MONOSPACED_SANS_SERIF);
	BIND_ENUM_CONSTANT(FONT_STYLE_PROPORTIONAL_SANS_SERIF);
	BIND_ENUM_CONSTANT(FONT_STYLE_CASUAL);
	BIND_ENUM_CONSTANT(FONT_STYLE_CURSIVE);
	BIND_ENUM_CONSTANT(FONT_STYLE_SMALL_CAPITALS);

	ADD_PROPERTY(PropertyInfo(Variant::COLOR, "background_color"), "", "get_background_color");
	ADD_PROPERTY(PropertyInfo(Variant::COLOR, "font_color"), "", "get_font_color");
	ADD_PROPERTY(PropertyInfo(Variant::COLOR, "window_color"), "", "get_window_color");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "font_edge_attribute", PROPERTY_HINT_ENUM, "Default,None,Raised,Depressed,Uniform,Drop Shadow"), "", "get_font_edge_attribute");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "font_style", PROPERTY_HINT_ENUM, "Default,Monospaced Serif,Proportional Serif,Monospaced Sans Serif,Proportional Sans Serif,Casual,Cursive,Small Capitals"), "", "get_font_style");
	ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "font_scale"), "", "get_font_scale");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "enabled"), "", "is_enabled");
}

Color GDKClosedCaptionProperties::get_background_color() const {
	return m_background_color;
}

Color GDKClosedCaptionProperties::get_font_color() const {
	return m_font_color;
}

Color GDKClosedCaptionProperties::get_window_color() const {
	return m_window_color;
}

GDKClosedCaptionProperties::FontEdgeAttribute GDKClosedCaptionProperties::get_font_edge_attribute() const {
	return m_font_edge_attribute;
}

String GDKClosedCaptionProperties::get_font_edge_attribute_name() const {
	return _font_edge_attribute_to_name(m_font_edge_attribute);
}

GDKClosedCaptionProperties::FontStyle GDKClosedCaptionProperties::get_font_style() const {
	return m_font_style;
}

String GDKClosedCaptionProperties::get_font_style_name() const {
	return _font_style_to_name(m_font_style);
}

double GDKClosedCaptionProperties::get_font_scale() const {
	return m_font_scale;
}

bool GDKClosedCaptionProperties::is_enabled() const {
	return m_enabled;
}

#ifdef XBOX_MODULE_GDK_ENABLED
void GDKClosedCaptionProperties::set_from_native(const XClosedCaptionProperties &p_properties) {
	m_background_color = _xcolor_to_color(p_properties.BackgroundColor);
	m_font_color = _xcolor_to_color(p_properties.FontColor);
	m_window_color = _xcolor_to_color(p_properties.WindowColor);
	m_font_edge_attribute = _to_font_edge_attribute(p_properties.FontEdgeAttribute);
	m_font_style = _to_font_style(p_properties.FontStyle);
	m_font_scale = static_cast<double>(p_properties.FontScale);
	m_enabled = p_properties.Enabled;
}
#else
void GDKClosedCaptionProperties::set_from_native(const XClosedCaptionProperties &p_properties) {
}
#endif

void GDKAccessibility::_bind_methods() {
	ClassDB::bind_method(D_METHOD("query_closed_caption_properties"), &GDKAccessibility::query_closed_caption_properties);
	ClassDB::bind_method(D_METHOD("set_closed_caption_enabled", "enabled"), &GDKAccessibility::set_closed_caption_enabled);
	ClassDB::bind_method(D_METHOD("query_high_contrast_mode"), &GDKAccessibility::query_high_contrast_mode);
	ClassDB::bind_method(D_METHOD("get_high_contrast_mode_name", "mode"), &GDKAccessibility::get_high_contrast_mode_name);

	BIND_ENUM_CONSTANT(HIGH_CONTRAST_MODE_OFF);
	BIND_ENUM_CONSTANT(HIGH_CONTRAST_MODE_DARK);
	BIND_ENUM_CONSTANT(HIGH_CONTRAST_MODE_LIGHT);
	BIND_ENUM_CONSTANT(HIGH_CONTRAST_MODE_OTHER);
}

#ifdef XBOX_MODULE_GDK_ENABLED
void GDKAccessibility::set_owner(GDK *p_owner) {
	m_owner = p_owner;
}

GDKRuntime *GDKAccessibility::_get_runtime() const {
	if (m_owner == nullptr) {
		return nullptr;
	}

	return m_owner->get_runtime();
}

GDKAccessibility::HighContrastMode GDKAccessibility::_to_high_contrast_mode(XHighContrastMode p_mode) {
	switch (p_mode) {
		case XHighContrastMode::Dark:
			return HIGH_CONTRAST_MODE_DARK;
		case XHighContrastMode::Light:
			return HIGH_CONTRAST_MODE_LIGHT;
		case XHighContrastMode::Other:
			return HIGH_CONTRAST_MODE_OTHER;
		case XHighContrastMode::Off:
		default:
			return HIGH_CONTRAST_MODE_OFF;
	}
}

String GDKAccessibility::_high_contrast_mode_to_name(HighContrastMode p_mode) {
	switch (p_mode) {
		case HIGH_CONTRAST_MODE_DARK:
			return "dark";
		case HIGH_CONTRAST_MODE_LIGHT:
			return "light";
		case HIGH_CONTRAST_MODE_OTHER:
			return "other";
		case HIGH_CONTRAST_MODE_OFF:
		default:
			return "off";
	}
}

Ref<GDKResult> GDKAccessibility::query_closed_caption_properties() const {
	GDKRuntime *runtime = _get_runtime();
	if (runtime == nullptr || !runtime->is_initialized()) {
		return GDKResult::error_result(E_FAIL, "runtime_unavailable", "GDK runtime is not initialized.");
	}

	XClosedCaptionProperties native_properties = {};
	HRESULT hr = XClosedCaptionGetProperties(&native_properties);
	if (FAILED(hr)) {
		Ref<GDKResult> result = GDKResult::hresult_error(hr, "Failed to query closed caption properties.", "closed_caption_get_failed");
		return result;
	}

	Ref<GDKClosedCaptionProperties> properties;
	properties.instantiate();
	properties->set_from_native(native_properties);

	return GDKResult::ok_result(properties);
}

Ref<GDKResult> GDKAccessibility::set_closed_caption_enabled(bool p_enabled) const {
	GDKRuntime *runtime = _get_runtime();
	if (runtime == nullptr || !runtime->is_initialized()) {
		return GDKResult::error_result(E_FAIL, "runtime_unavailable", "GDK runtime is not initialized.");
	}

	HRESULT hr = XClosedCaptionSetEnabled(p_enabled);
	if (FAILED(hr)) {
		Ref<GDKResult> result = GDKResult::hresult_error(hr, "Failed to update closed caption enabled state.", "closed_caption_set_failed");
		return result;
	}

	Dictionary data;
	data["enabled"] = p_enabled;
	return GDKResult::ok_result(data);
}

Ref<GDKResult> GDKAccessibility::query_high_contrast_mode() const {
	GDKRuntime *runtime = _get_runtime();
	if (runtime == nullptr || !runtime->is_initialized()) {
		return GDKResult::error_result(E_FAIL, "runtime_unavailable", "GDK runtime is not initialized.");
	}

	XHighContrastMode native_mode = XHighContrastMode::Off;
	HRESULT hr = XHighContrastGetMode(&native_mode);
	if (FAILED(hr)) {
		Ref<GDKResult> result = GDKResult::hresult_error(hr, "Failed to query high contrast mode.", "high_contrast_get_failed");
		return result;
	}

	const HighContrastMode mode = _to_high_contrast_mode(native_mode);
	Dictionary data;
	data["mode"] = static_cast<int64_t>(mode);
	data["mode_name"] = _high_contrast_mode_to_name(mode);

	return GDKResult::ok_result(data);
}

String GDKAccessibility::get_high_contrast_mode_name(HighContrastMode p_mode) const {
	return _high_contrast_mode_to_name(p_mode);
}

#else
void GDKAccessibility::set_owner(GDK *p_owner) {}

String GDKAccessibility::_high_contrast_mode_to_name(HighContrastMode p_mode) {
	return String();
}

Ref<GDKResult> GDKAccessibility::query_closed_caption_properties() const {
	return GDKResult::error_result(E_NOTIMPL, "gdk_not_enabled", "Xbox GDK is not enabled in this build.");
}

Ref<GDKResult> GDKAccessibility::set_closed_caption_enabled(bool p_enabled) const {
	return GDKResult::error_result(E_NOTIMPL, "gdk_not_enabled", "Xbox GDK is not enabled in this build.");
}

Ref<GDKResult> GDKAccessibility::query_high_contrast_mode() const {
	return GDKResult::error_result(E_NOTIMPL, "gdk_not_enabled", "Xbox GDK is not enabled in this build.");
}

String GDKAccessibility::get_high_contrast_mode_name(HighContrastMode p_mode) const {
	return String();
}
#endif
