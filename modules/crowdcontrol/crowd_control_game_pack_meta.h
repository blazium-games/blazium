/**************************************************************************/
/*  crowd_control_game_pack_meta.h                                        */
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

class CrowdControlGamePackMeta : public Resource {
	GDCLASS(CrowdControlGamePackMeta, Resource);

private:
	Variant game_name; // Can be String or Dictionary
	String release_date;
	String platform = "PC";
	PackedStringArray connector;
	int steam_id = 0;
	String description;
	String guide_url;

protected:
	static void _bind_methods();

public:
	void set_game_name(const Variant &p_name);
	Variant get_game_name() const;

	void set_release_date(const String &p_date);
	String get_release_date() const;

	void set_platform(const String &p_platform);
	String get_platform() const;

	void set_connector(const PackedStringArray &p_connector);
	PackedStringArray get_connector() const;

	void set_steam_id(int p_id);
	int get_steam_id() const;

	void set_description(const String &p_description);
	String get_description() const;

	void set_guide_url(const String &p_url);
	String get_guide_url() const;

	Dictionary to_json() const;

	CrowdControlGamePackMeta();
	~CrowdControlGamePackMeta();
};
