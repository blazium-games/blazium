/**************************************************************************/
/*  steam_achievement_info.cpp                                            */
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

#include "core/object/class_db.h"
#include "steam_achievement_info.h"

#include "core/io/image.h"

void SteamAchievementInfo::_bind_methods() {
	ClassDB::bind_method(D_METHOD("get_api_name"), &SteamAchievementInfo::get_api_name);
	ClassDB::bind_method(D_METHOD("get_display_name"), &SteamAchievementInfo::get_display_name);
	ClassDB::bind_method(D_METHOD("get_description"), &SteamAchievementInfo::get_description);
	ClassDB::bind_method(D_METHOD("is_hidden"), &SteamAchievementInfo::is_hidden);
	ClassDB::bind_method(D_METHOD("is_achieved"), &SteamAchievementInfo::is_achieved);
	ClassDB::bind_method(D_METHOD("get_unlock_time"), &SteamAchievementInfo::get_unlock_time);
	ClassDB::bind_method(D_METHOD("get_icon"), &SteamAchievementInfo::get_icon);
}
