/**************************************************************************/
/*  trenchbroom_local_config.h                                            */
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

class TrenchbroomLocalConfig : public Resource {
	GDCLASS(TrenchbroomLocalConfig, Resource);

public:
	enum Property {
		PROPERTY_FGD_OUTPUT_FOLDER,
		PROPERTY_TRENCHBROOM_GAME_CONFIG_FOLDER,
		PROPERTY_NETRADIANT_CUSTOM_GAMEPACKS_FOLDER,
		PROPERTY_MAP_EDITOR_GAME_PATH,
	};

protected:
	static void _bind_methods();

	Dictionary settings_dict;
	bool loaded = false;

	Variant _get_default_value(Variant::Type p_type) const;
	void _try_loading();

public:
	static Variant get_setting(Property p_name);

	void reload_trenchbroom_settings();
	void export_trenchbroom_settings();

	void set_fgd_output_folder(const String &p_folder);
	String get_fgd_output_folder() const;
	void set_trenchbroom_game_config_folder(const String &p_folder);
	String get_trenchbroom_game_config_folder() const;
	void set_netradiant_custom_gamepacks_folder(const String &p_folder);
	String get_netradiant_custom_gamepacks_folder() const;
	void set_map_editor_game_path(const String &p_folder);
	String get_map_editor_game_path() const;

	virtual bool _get(const StringName &p_name, Variant &r_ret) const;
	virtual void _get_property_list(List<PropertyInfo> *p_list) const;
	virtual bool _set(const StringName &p_name, const Variant &p_value);
};

VARIANT_ENUM_CAST(TrenchbroomLocalConfig::Property);
