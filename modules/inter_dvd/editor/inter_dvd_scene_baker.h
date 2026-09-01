/**************************************************************************/
/*  inter_dvd_scene_baker.h                                               */
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

#ifdef TOOLS_ENABLED

#include "core/error/error_list.h"
#include "core/io/image.h"
#include "core/io/resource.h"
#include "core/object/object.h"
#include "core/string/ustring.h"

class InterDVDCell;
class Node;

class InterDVDSceneBaker : public Object {
	GDCLASS(InterDVDSceneBaker, Object);

protected:
	static void _bind_methods();

public:
	static Ref<Image> raster_root(Node *p_root);
	static Error bake_cell(const Ref<InterDVDCell> &p_cell, const String &p_ffmpeg, bool p_auto_find_ffmpeg, String *r_error = nullptr);
	static Error bake_cell_bind(const Ref<InterDVDCell> &p_cell, const String &p_ffmpeg = String(), bool p_auto_find_ffmpeg = true);
};

#endif
