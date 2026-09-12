/**************************************************************************/
/*  steam_inventory_item.cpp                                              */
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

#include "steam_inventory_item.h"

#include "steam.h"
#include "steam_item_definition.h"

void SteamInventoryItem::_bind_methods() {
	ClassDB::bind_method(D_METHOD("get_item_instance_id"), &SteamInventoryItem::get_item_instance_id);
	ClassDB::bind_method(D_METHOD("get_def_id"), &SteamInventoryItem::get_def_id);
	ClassDB::bind_method(D_METHOD("get_quantity"), &SteamInventoryItem::get_quantity);
	ClassDB::bind_method(D_METHOD("get_flags"), &SteamInventoryItem::get_flags);
	ClassDB::bind_method(D_METHOD("get_properties"), &SteamInventoryItem::get_properties);
	ClassDB::bind_method(D_METHOD("get_property", "name"), &SteamInventoryItem::get_property);
	ClassDB::bind_method(D_METHOD("get_definition"), &SteamInventoryItem::get_definition);
}

String SteamInventoryItem::get_property(const String &p_name) const {
	if (!properties.has(p_name)) {
		return String();
	}
	const Variant value = properties[p_name];
	if (value.get_type() == Variant::STRING) {
		return value;
	}
	return value.stringify();
}

Ref<SteamItemDefinition> SteamInventoryItem::get_definition() const {
	Steam *steam = Steam::get_singleton();
	if (!steam || !steam->is_initialized()) {
		return Ref<SteamItemDefinition>();
	}
	return steam->get_item_definition(def_id);
}
