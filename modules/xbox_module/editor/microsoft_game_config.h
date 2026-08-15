/**************************************************************************/
/*  microsoft_game_config.h                                               */
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

class MicrosoftGameConfig : public RefCounted {
	GDCLASS(MicrosoftGameConfig, RefCounted);

public:
	static String get_project_config_path();
	static bool project_config_exists();

	static String read_executable_name(const String &p_config_path = String());
	static String read_executable_name_from_content(const String &p_content);
	static String inject_target_device_family(const String &p_content, const String &p_family = "PC");

	static Error copy_to_staging(const String &p_staging_dir, const String &p_family = "PC");
	static Error stage_logos(const String &p_staging_dir);

	static String get_template_xml(const String &p_executable_name = "blazium.windows.template_release.x86_64.exe");
	static Error write_template_to_project(const String &p_executable_name = String());

	static String validate_project_config(String *r_error = nullptr);
};
