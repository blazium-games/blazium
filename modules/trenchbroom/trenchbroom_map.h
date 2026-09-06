/**************************************************************************/
/*  trenchbroom_map.h                                                     */
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

#include "trenchbroom_map_settings.h"

#include "scene/3d/node_3d.h"

class TrenchbroomMap : public Node3D {
	GDCLASS(TrenchbroomMap, Node3D);

public:
	enum BuildFlags {
		UNWRAP_UV2 = 1 << 0,
		SHOW_PROFILE_INFO = 1 << 1,
		DISABLE_SMOOTHING = 1 << 2,
		INCLUDE_CORDON_VOLUMES = 1 << 3,
	};

protected:
	static void _bind_methods();

	String local_map_file;
	String global_map_file;
	String map_file_internal;
	Ref<TrenchbroomMapSettings> map_settings;
	int build_flags = 0;
	real_t hyperplane_size = 512.0;

	void fail_build(const String &p_reason, bool p_notify = false);
	Error verify();

	Callable _get_build_func() const;
	Callable _get_clear_func() const;

public:
	void set_local_map_file(const String &p_file) { local_map_file = p_file; }
	String get_local_map_file() const { return local_map_file; }

	void set_global_map_file(const String &p_file) { global_map_file = p_file; }
	String get_global_map_file() const { return global_map_file; }

	void set_map_settings(const Ref<TrenchbroomMapSettings> &p_settings) { map_settings = p_settings; }
	Ref<TrenchbroomMapSettings> get_map_settings() const { return map_settings; }

	void set_build_flags(int p_flags) { build_flags = p_flags; }
	int get_build_flags() const { return build_flags; }

	void set_hyperplane_size(real_t p_size) { hyperplane_size = p_size; }
	real_t get_hyperplane_size() const { return hyperplane_size; }

	void clear_children();
	void build();
	Dictionary parse_map_data(const String &p_map_file);
};

VARIANT_BITFIELD_CAST(TrenchbroomMap::BuildFlags);
