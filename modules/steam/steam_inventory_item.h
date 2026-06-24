/**************************************************************************/
/*  steam_inventory_item.h                                                */
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

class SteamItemDefinition;

class SteamInventoryItem : public RefCounted {
	GDCLASS(SteamInventoryItem, RefCounted);

	uint64_t instance_id = 0;
	int def_id = 0;
	int quantity = 0;
	int flags = 0;
	Dictionary properties;

protected:
	static void _bind_methods();

public:
	void set_instance_id(uint64_t p_id) { instance_id = p_id; }
	uint64_t get_item_instance_id() const { return instance_id; }

	void set_def_id(int p_def_id) { def_id = p_def_id; }
	int get_def_id() const { return def_id; }

	void set_quantity(int p_quantity) { quantity = p_quantity; }
	int get_quantity() const { return quantity; }

	void set_flags(int p_flags) { flags = p_flags; }
	int get_flags() const { return flags; }

	void set_properties(const Dictionary &p_properties) { properties = p_properties; }
	Dictionary get_properties() const { return properties; }

	String get_property(const String &p_name) const;
	Ref<SteamItemDefinition> get_definition() const;
};
