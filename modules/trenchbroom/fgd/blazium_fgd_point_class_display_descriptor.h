/**************************************************************************/
/*  blazium_fgd_point_class_display_descriptor.h                          */
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

#include "core/io/resource.h"

class BlaziumFGDPointClassDisplayDescriptor : public Resource {
	GDCLASS(BlaziumFGDPointClassDisplayDescriptor, Resource);

protected:
	static void _bind_methods();

	String display_asset_path;
	String scale;
	String skin;
	String frame;
	String conditional;

public:
	void set_display_asset_path(const String &p_path) { display_asset_path = p_path; }
	String get_display_asset_path() const { return display_asset_path; }

	void set_scale(const String &p_scale) { scale = p_scale; }
	String get_scale() const { return scale; }

	void set_skin(const String &p_skin) { skin = p_skin; }
	String get_skin() const { return skin; }

	void set_frame(const String &p_frame) { frame = p_frame; }
	String get_frame() const { return frame; }

	void set_conditional(const String &p_conditional) { conditional = p_conditional; }
	String get_conditional() const { return conditional; }
};
