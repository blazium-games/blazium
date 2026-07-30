/**************************************************************************/
/*  cold_storage_vcs.h                                                    */
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
#include "editor/editor_vcs_interface.h"

#include <memory>
#include <string>

namespace coldstorage {
class ColdStorageClient;
}

class ColdStorageVCS : public EditorVCSInterface {
	GDCLASS(ColdStorageVCS, EditorVCSInterface);

	std::unique_ptr<coldstorage::ColdStorageClient> client_;
	ColdStorageConnectionConfig config_;
	String project_path_;
	String last_error_;
	String username_;
	String password_;
	String current_branch_ = "main";
	String remote_name_ = "origin";

	bool _connect_and_auth();
	String _to_depot_path(const String &p_file_path) const;
	String _to_local_rel(const String &p_depot_path) const;

	TypedArray<Dictionary> _collect_modified_files();
	TypedArray<Dictionary> _build_diff(const String &p_identifier);

protected:
	static void _bind_methods();

public:
	virtual bool initialize(const String &p_project_path) override;
	virtual void set_credentials(const String &p_username, const String &p_password, const String &p_ssh_public_key_path, const String &p_ssh_private_key_path, const String &p_ssh_passphrase) override;
	virtual List<StatusFile> get_modified_files_data() override;
	virtual void stage_file(const String &p_file_path) override;
	virtual void unstage_file(const String &p_file_path) override;
	virtual void discard_file(const String &p_file_path) override;
	virtual void commit(const String &p_msg) override;
	virtual List<DiffFile> get_diff(const String &p_identifier, TreeArea p_area) override;
	virtual bool shut_down() override;
	virtual String get_vcs_name() override;
	virtual List<Commit> get_previous_commits(int p_max_commits) override;
	virtual List<String> get_branch_list() override;
	virtual List<String> get_remotes() override;
	virtual void create_branch(const String &p_branch_name) override;
	virtual void remove_branch(const String &p_branch_name) override;
	virtual void create_remote(const String &p_remote_name, const String &p_remote_url) override;
	virtual void remove_remote(const String &p_remote_name) override;
	virtual String get_current_branch_name() override;
	virtual bool checkout_branch(const String &p_branch_name) override;
	virtual void pull(const String &p_remote) override;
	virtual void push(const String &p_remote, bool p_force) override;
	virtual void fetch(const String &p_remote) override;
	virtual List<DiffHunk> get_line_diff(const String &p_file_path, const String &p_text) override;

	void apply_connection_config(const ColdStorageConnectionConfig &p_cfg);
	bool is_connected_to_server() const;
	String get_last_error() const { return last_error_; }
	bool validate_connection();
	bool auto_pull_sync();

	// Adopt a client already connected on a worker thread (main thread only).
	bool adopt_connected_client(std::unique_ptr<coldstorage::ColdStorageClient> p_client, const ColdStorageConnectionConfig &p_cfg, const String &p_project_path);

	ColdStorageVCS();
	~ColdStorageVCS() override;
};

#endif
