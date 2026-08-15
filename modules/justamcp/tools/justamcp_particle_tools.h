/**************************************************************************/
/*  justamcp_particle_tools.h                                             */
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

#include "core/object/object.h"
#include "editor/plugins/editor_plugin.h"
#include "scene/main/node.h"

class JustAMCPParticleTools : public Object {
	GDCLASS(JustAMCPParticleTools, Object);

private:
	EditorPlugin *editor_plugin = nullptr;

	Node *_find_node_by_path(const String &p_path);
	Node *_get_particles_node_any(const String &p_path);
	Color _parse_color(const String &p_color_str);

	Dictionary _create_particles(const Dictionary &p_params);
	Dictionary _set_particle_material(const Dictionary &p_params);
	Dictionary _set_particle_color_gradient(const Dictionary &p_params);
	Dictionary _apply_particle_preset(const Dictionary &p_params);
	Dictionary _get_particle_info(const Dictionary &p_params);

	void _apply_gradient(Object *p_mat, const Array &p_stops);

protected:
	static void _bind_methods();

public:
	void set_editor_plugin(EditorPlugin *p_plugin) { editor_plugin = p_plugin; }
	Dictionary execute_tool(const String &p_tool_name, const Dictionary &p_args);

	JustAMCPParticleTools();
	~JustAMCPParticleTools();
};
