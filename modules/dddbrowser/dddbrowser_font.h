/**************************************************************************/
/*  dddbrowser_font.h                                                     */
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

#include "scene/3d/node_3d.h"

class DDDBrowserFont : public Node3D {
	GDCLASS(DDDBrowserFont, Node3D);

public:
	enum Style {
		STYLE_NORMAL,
		STYLE_BOLD,
		STYLE_ITALIC,
	};

private:
	String asset_id;
	String source_path;
	float size = 16.0f;
	Style style = STYLE_NORMAL;

protected:
	static void _bind_methods();

public:
	void set_asset_id(const String &p_id);
	String get_asset_id() const;
	void set_source_path(const String &p_path);
	String get_source_path() const;
	void set_size(float p_size);
	float get_size() const;
	void set_style(Style p_style);
	Style get_style() const;
	String style_string() const;
};

VARIANT_ENUM_CAST(DDDBrowserFont::Style);
