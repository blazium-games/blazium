/**************************************************************************/
/*  entity_assembler.h                                                    */
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
#include "modules/trenchbroom/core/data.h"

class TrenchbroomMap;
class TrenchbroomMapSettings;
class BlaziumFGDPointClass;
class BlaziumFGDSolidClass;

class TrenchbroomEntityAssembler : public RefCounted {
	GDCLASS(TrenchbroomEntityAssembler, RefCounted);

protected:
	static void _bind_methods();

	const TrenchbroomMapSettings *map_settings = nullptr;
	int build_flags = 0;

public:
	TrenchbroomEntityAssembler() = default;
	explicit TrenchbroomEntityAssembler(const TrenchbroomMapSettings *p_settings);

	static Ref<Script> get_script_by_class_name(const String &p_class_name);

	Node3D *generate_group_node(GroupData &p_group_data);
	Node *generate_solid_entity_node(Node *p_node, const String &p_node_name, EntityData &p_data, const BlaziumFGDSolidClass *p_definition);
	Node *generate_point_entity_node(Node *p_node, const String &p_node_name, Dictionary &p_properties, const BlaziumFGDPointClass *p_definition);
	void apply_entity_properties(Node *p_node, EntityData &p_data);
	Node *generate_entity_node(EntityData &p_entity_data, int p_entity_index);
	void build(TrenchbroomMap *p_map_node, LocalVector<EntityData> &p_entities, LocalVector<GroupData> &p_groups);
};
