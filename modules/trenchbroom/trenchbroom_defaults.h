/**************************************************************************/
/*  trenchbroom_defaults.h                                                */
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

#include "core/object/ref_counted.h"

class BlaziumFGDFile;
class TrenchbroomGameConfig;
class TrenchbroomMapSettings;
class TrenchbroomTag;

class TrenchbroomDefaults : public RefCounted {
	GDCLASS(TrenchbroomDefaults, RefCounted);

protected:
	static void _bind_methods();

public:
	static String get_defaults_dir();
	static String get_module_defaults_dir();
	static String resolve_defaults_path(const String &p_path);
	static String get_default_texture_path();
	static String get_default_palette_path();
	static String get_quake2_palette_path();
	static void ensure_default_assets();
	static Ref<BlaziumFGDFile> create_default_fgd();
	static Ref<TrenchbroomMapSettings> create_default_map_settings();
	static Ref<TrenchbroomGameConfig> create_default_game_config();
	static Ref<TrenchbroomTag> make_face_tag(const String &p_name, const String &p_pattern);
	static Ref<TrenchbroomTag> make_brush_tag(const String &p_name, const String &p_pattern);
};
