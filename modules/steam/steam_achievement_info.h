/**************************************************************************/
/*  steam_achievement_info.h                                              */
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

#include "core/io/image.h"
#include "core/object/ref_counted.h"

class SteamAchievementInfo : public RefCounted {
	GDCLASS(SteamAchievementInfo, RefCounted);

	String api_name;
	String display_name;
	String description;
	bool hidden = false;
	bool achieved = false;
	int unlock_time = 0;
	Ref<Image> icon;

protected:
	static void _bind_methods();

public:
	void set_api_name(const String &p_name) { api_name = p_name; }
	String get_api_name() const { return api_name; }

	void set_display_name(const String &p_name) { display_name = p_name; }
	String get_display_name() const { return display_name; }

	void set_description(const String &p_description) { description = p_description; }
	String get_description() const { return description; }

	void set_hidden(bool p_hidden) { hidden = p_hidden; }
	bool is_hidden() const { return hidden; }

	void set_achieved(bool p_achieved) { achieved = p_achieved; }
	bool is_achieved() const { return achieved; }

	void set_unlock_time(int p_time) { unlock_time = p_time; }
	int get_unlock_time() const { return unlock_time; }

	void set_icon(const Ref<Image> &p_icon) { icon = p_icon; }
	Ref<Image> get_icon() const { return icon; }
};
