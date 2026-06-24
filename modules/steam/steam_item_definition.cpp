/**************************************************************************/
/*  steam_item_definition.cpp                                             */
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

#include "steam_item_definition.h"

void SteamItemDefinition::_bind_methods() {
	ClassDB::bind_method(D_METHOD("get_def_id"), &SteamItemDefinition::get_def_id);
	ClassDB::bind_method(D_METHOD("get_name"), &SteamItemDefinition::get_name);
	ClassDB::bind_method(D_METHOD("get_description"), &SteamItemDefinition::get_description);
	ClassDB::bind_method(D_METHOD("get_item_type"), &SteamItemDefinition::get_item_type);
	ClassDB::bind_method(D_METHOD("get_icon_url"), &SteamItemDefinition::get_icon_url);
	ClassDB::bind_method(D_METHOD("get_properties"), &SteamItemDefinition::get_properties);
	ClassDB::bind_method(D_METHOD("get_property", "name"), &SteamItemDefinition::get_property);
}

String SteamItemDefinition::get_property(const String &p_name) const {
	if (!properties.has(p_name)) {
		return String();
	}
	const Variant value = properties[p_name];
	if (value.get_type() == Variant::STRING) {
		return value;
	}
	return value.stringify();
}
