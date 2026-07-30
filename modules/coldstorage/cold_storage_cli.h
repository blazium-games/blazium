/**************************************************************************/
/*  cold_storage_cli.h                                                    */
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

#ifdef TOOLS_ENABLED

#include "cold_storage_settings.h"

struct ColdStorageCliOverrides {
	bool enable = false;
	bool has_enable = false;
	bool auto_pull = false;
	bool has_auto_pull = false;
	bool no_validate = false;
	bool has_host = false;
	String host;
	bool has_port = false;
	int port = 1666;
	bool has_user = false;
	String user;
	bool has_password = false;
	String password;
	bool has_ticket = false;
	String ticket;
	bool has_jwt = false;
	String jwt;
	bool has_workspace = false;
	String workspace;
	bool has_repo = false;
	String repo;
	bool tls = false;
	bool has_tls = false;
	bool tls_insecure = false;
	bool has_tls_insecure = false;
};

ColdStorageCliOverrides cold_storage_parse_cli();
void cold_storage_apply_cli_to_config(ColdStorageConnectionConfig &r_cfg, const ColdStorageCliOverrides &p_cli);
bool cold_storage_should_autoconnect(const ColdStorageConnectionConfig &p_cfg, const ColdStorageCliOverrides &p_cli);

#endif
