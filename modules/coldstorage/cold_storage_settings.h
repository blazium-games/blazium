/**************************************************************************/
/*  cold_storage_settings.h                                               */
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

#include "core/string/ustring.h"
#include "core/variant/variant.h"

#ifdef TOOLS_ENABLED

void cold_storage_register_project_settings();
void cold_storage_register_editor_settings();

struct ColdStorageConnectionConfig {
	bool enabled = false;
	bool autoload_on_startup = false;
	bool validate_on_startup = true;
	bool auto_pull = false;
	String host = "127.0.0.1";
	int port = 1666;
	bool use_tls = false;
	bool tls_insecure = false;
	String user;
	String password;
	String ticket;
	String jwt;
	String workspace = "default";
	String repo = "default";
	String workspace_root;
	String ca_file;
};

ColdStorageConnectionConfig cold_storage_load_config();
void cold_storage_save_config(const ColdStorageConnectionConfig &p_cfg);

Variant cold_storage_get_setting(const String &p_key, const Variant &p_default);

#endif
