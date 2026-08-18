/**************************************************************************/
/*  cold_storage_vcs.cpp                                                  */
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

#ifdef TOOLS_ENABLED

#include "cold_storage_vcs.h"

#include "core/io/file_access.h"
#include "core/os/os.h"
#include "core/templates/hash_set.h"

#include "client/sdk/client_sdk.h"
#include "client/sdk/tls_options.h"
#include "common/util/text_diff.h"
#ifdef CONNECT
#undef CONNECT
#endif
#ifdef IGNORE
#undef IGNORE
#endif

#include <exception>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <utility>
#include <vector>

void ColdStorageVCS::_bind_methods() {}

ColdStorageVCS::ColdStorageVCS() = default;

ColdStorageVCS::~ColdStorageVCS() {
	shut_down();
}

void ColdStorageVCS::apply_connection_config(const ColdStorageConnectionConfig &p_cfg) {
	config_ = p_cfg;
	remote_name_ = config_.host + ":" + itos(config_.port);
	if (!config_.workspace.is_empty()) {
		current_branch_ = config_.workspace;
	}
}

bool ColdStorageVCS::adopt_connected_client(std::unique_ptr<coldstorage::ColdStorageClient> p_client, const ColdStorageConnectionConfig &p_cfg, const String &p_project_path) {
	if (!p_client || !p_client->isConnected()) {
		last_error_ = "No connected ColdStorage client to adopt";
		return false;
	}
	shut_down();
	client_ = std::move(p_client);
	apply_connection_config(p_cfg);
	project_path_ = p_project_path;
	last_error_.clear();
	return true;
}

bool ColdStorageVCS::is_connected_to_server() const {
	return client_ && client_->isConnected();
}

String ColdStorageVCS::_to_depot_path(const String &p_file_path) const {
	String path = p_file_path.replace("\\", "/");
	if (path.begins_with("res://")) {
		path = path.substr(6);
	}
	while (path.begins_with("./")) {
		path = path.substr(2);
	}
	if (path.begins_with("//")) {
		return path;
	}
	return String("//depot/") + path;
}

String ColdStorageVCS::_to_local_rel(const String &p_depot_path) const {
	String d = p_depot_path;
	const String prefix = "//depot/";
	if (d.begins_with(prefix)) {
		d = d.substr(prefix.length());
	}
	return d;
}

bool ColdStorageVCS::_connect_and_auth() {
	last_error_.clear();
	try {
		if (!client_) {
			client_ = std::make_unique<coldstorage::ColdStorageClient>();
		}
		client_->disconnect();

		coldstorage::TlsOptions tls;
		tls.enabled = config_.use_tls;
		tls.verifyPeer = !config_.tls_insecure;
		tls.caFile = String(config_.ca_file).utf8().get_data();

		const std::string host = String(config_.host).utf8().get_data();
		if (!client_->connect(host, config_.port, tls)) {
			last_error_ = "Failed to connect to " + config_.host + ":" + itos(config_.port);
			return false;
		}

		client_->setWorkspace(String(config_.workspace).utf8().get_data());
		client_->setRepo(String(config_.repo).utf8().get_data());

		String root = config_.workspace_root;
		if (root.is_empty()) {
			root = project_path_;
		}
		client_->setWorkspaceRoot(String(root).utf8().get_data());

		if (!config_.jwt.is_empty()) {
			if (!client_->authenticateWithJWT(String(config_.jwt).utf8().get_data())) {
				last_error_ = "JWT authentication failed";
				client_->disconnect();
				return false;
			}
		} else if (!config_.ticket.is_empty()) {
			client_->setTicket(String(config_.ticket).utf8().get_data());
		} else {
			String user = config_.user.is_empty() ? username_ : config_.user;
			String pass = config_.password.is_empty() ? password_ : config_.password;
			if (!user.is_empty()) {
				std::string ticket = client_->login(String(user).utf8().get_data(), String(pass).utf8().get_data());
				if (ticket.empty()) {
					last_error_ = "Login failed";
					if (!client_->lastError().empty()) {
						last_error_ += ": " + String(client_->lastError().c_str());
					}
					client_->disconnect();
					return false;
				}
			}
		}

		client_->ensureWorkspaceView();
		remote_name_ = config_.host + ":" + itos(config_.port);
		return true;
	} catch (const std::exception &e) {
		last_error_ = String("ColdStorage exception: ") + e.what();
		popup_error(last_error_);
		if (client_) {
			client_->disconnect();
		}
		return false;
	} catch (...) {
		last_error_ = "ColdStorage unknown exception during connect";
		popup_error(last_error_);
		if (client_) {
			client_->disconnect();
		}
		return false;
	}
}

bool ColdStorageVCS::initialize(const String &p_project_path) {
	project_path_ = p_project_path;
	if (config_.host.is_empty()) {
		config_ = cold_storage_load_config();
	}
	if (config_.workspace_root.is_empty()) {
		config_.workspace_root = p_project_path;
	}
	return _connect_and_auth();
}

void ColdStorageVCS::set_credentials(const String &p_username, const String &p_password, const String &, const String &, const String &) {
	username_ = p_username;
	password_ = p_password;
	if (!p_username.is_empty()) {
		config_.user = p_username;
	}
	if (!p_password.is_empty()) {
		config_.password = p_password;
	}
	if (client_ && client_->isConnected() && !config_.user.is_empty()) {
		try {
			client_->login(String(config_.user).utf8().get_data(), String(config_.password).utf8().get_data());
		} catch (const std::exception &e) {
			last_error_ = String("ColdStorage exception: ") + e.what();
			popup_error(last_error_);
		} catch (...) {
			last_error_ = "ColdStorage unknown exception during login";
			popup_error(last_error_);
		}
	}
}

List<EditorVCSInterface::StatusFile> ColdStorageVCS::get_modified_files_data() {
	List<StatusFile> status_files;
	TypedArray<Dictionary> out = _collect_modified_files();
	for (int i = 0; i < out.size(); i++) {
		status_files.push_back(_convert_status_file(out[i]));
	}
	return status_files;
}

TypedArray<Dictionary> ColdStorageVCS::_collect_modified_files() {
	TypedArray<Dictionary> out;
	if (!client_ || !client_->isConnected()) {
		return out;
	}

	try {
		HashSet<String> staged;
		for (const auto &f : client_->opened()) {
			ChangeType ct = CHANGE_TYPE_MODIFIED;
			if (f.action == "add") {
				ct = CHANGE_TYPE_NEW;
			} else if (f.action == "delete") {
				ct = CHANGE_TYPE_DELETED;
			} else if (f.action == "integrate" || f.action == "branch") {
				ct = CHANGE_TYPE_RENAMED;
			}
			const String local = _to_local_rel(String(f.depotPath.c_str()));
			staged.insert(local);
			out.push_back(create_status_file(local, ct, TREE_AREA_STAGED));
		}

		auto st = client_->status();
		if (st.success) {
			for (const auto &e : st.entries) {
				if (e.state == "opened" || e.state == "unchanged" || e.state.empty()) {
					continue;
				}
				const String local = _to_local_rel(String(e.depotPath.c_str()));
				if (staged.has(local)) {
					continue;
				}
				ChangeType ct = CHANGE_TYPE_MODIFIED;
				if (e.state == "add") {
					ct = CHANGE_TYPE_NEW;
				} else if (e.state == "delete") {
					ct = CHANGE_TYPE_DELETED;
				}
				out.push_back(create_status_file(local, ct, TREE_AREA_UNSTAGED));
			}
		}
	} catch (const std::exception &e) {
		last_error_ = String("ColdStorage exception: ") + e.what();
		popup_error(last_error_);
	} catch (...) {
		last_error_ = "ColdStorage unknown exception while collecting status";
		popup_error(last_error_);
	}
	return out;
}

void ColdStorageVCS::stage_file(const String &p_file_path) {
	if (!client_) {
		return;
	}
	try {
		const String depot = _to_depot_path(p_file_path);
		const std::string d = depot.utf8().get_data();
		std::filesystem::path local = std::filesystem::path(String(project_path_).utf8().get_data()) / String(_to_local_rel(depot)).utf8().get_data();
		std::error_code ec;
		const bool exists = std::filesystem::exists(local, ec);
		auto st = client_->fileInfo({ d });
		bool tracked = false;
		for (const auto &e : st.entries) {
			if (e.depotPath == d && e.haveRev > 0) {
				tracked = true;
				break;
			}
		}
		if (!exists && tracked) {
			client_->delete_(d);
		} else if (exists && !tracked) {
			client_->add(d, local.generic_string());
		} else if (exists) {
			client_->edit(d);
		}
	} catch (const std::exception &e) {
		last_error_ = String("ColdStorage exception: ") + e.what();
		popup_error(last_error_);
	} catch (...) {
		last_error_ = "ColdStorage unknown exception while staging";
		popup_error(last_error_);
	}
}

void ColdStorageVCS::unstage_file(const String &p_file_path) {
	if (!client_) {
		return;
	}
	try {
		client_->revert(_to_depot_path(p_file_path).utf8().get_data());
	} catch (const std::exception &e) {
		last_error_ = String("ColdStorage exception: ") + e.what();
		popup_error(last_error_);
	} catch (...) {
		last_error_ = "ColdStorage unknown exception while unstaging";
		popup_error(last_error_);
	}
}

void ColdStorageVCS::discard_file(const String &p_file_path) {
	unstage_file(p_file_path);
}

void ColdStorageVCS::commit(const String &p_msg) {
	if (!client_) {
		return;
	}
	try {
		auto r = client_->submit(String(p_msg).utf8().get_data());
		if (!r.success) {
			last_error_ = String(r.error.c_str());
			popup_error("ColdStorage submit failed: " + last_error_);
		}
	} catch (const std::exception &e) {
		last_error_ = String("ColdStorage exception: ") + e.what();
		popup_error(last_error_);
	} catch (...) {
		last_error_ = "ColdStorage unknown exception during submit";
		popup_error(last_error_);
	}
}

List<EditorVCSInterface::DiffFile> ColdStorageVCS::get_diff(const String &p_identifier, TreeArea p_area) {
	(void)p_area;
	List<DiffFile> diff_files;
	TypedArray<Dictionary> files = _build_diff(p_identifier);
	for (int i = 0; i < files.size(); i++) {
		diff_files.push_back(_convert_diff_file(files[i]));
	}
	return diff_files;
}

TypedArray<Dictionary> ColdStorageVCS::_build_diff(const String &p_identifier) {
	TypedArray<Dictionary> files;
	if (!client_ || p_identifier.is_empty()) {
		return files;
	}

	try {
		const String depot = _to_depot_path(p_identifier);
		std::string remote_content;
		client_->printStreaming(depot.utf8().get_data(), [&](const uint8_t *data, size_t len) {
			remote_content.append(reinterpret_cast<const char *>(data), len);
			return true;
		});

		std::filesystem::path local_path = std::filesystem::path(String(project_path_).utf8().get_data()) / String(_to_local_rel(depot)).utf8().get_data();
		std::ifstream in(local_path, std::ios::binary);
		std::string local_content;
		if (in) {
			local_content.assign(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
		}

		Dictionary diff_file = create_diff_file(p_identifier, p_identifier);
		TypedArray<Dictionary> hunks;
		TypedArray<Dictionary> lines;

		if (coldstorage::looksBinary(remote_content) || coldstorage::looksBinary(local_content)) {
			Dictionary hunk = create_diff_hunk(1, 1, 1, 1);
			lines.push_back(create_diff_line(-1, -1, "Binary file (diff not shown)", " "));
			hunk = add_line_diffs_into_diff_hunk(hunk, lines);
			hunks.push_back(hunk);
			diff_file = add_diff_hunks_into_diff_file(diff_file, hunks);
			files.push_back(diff_file);
			return files;
		}

		const std::string rel = String(_to_local_rel(depot)).utf8().get_data();
		const std::string unified = coldstorage::unifiedDiffText(remote_content, local_content,
				"a/" + rel, "b/" + rel, rel);

		int old_line = 0;
		int new_line = 0;
		const size_t max_lines = 2000;
		size_t emitted = 0;
		bool truncated = false;
		std::istringstream ss(unified);
		std::string line;
		while (std::getline(ss, line)) {
			if (line.rfind("---", 0) == 0 || line.rfind("+++", 0) == 0 || line.rfind("@@", 0) == 0) {
				continue;
			}
			if (line.empty()) {
				continue;
			}
			const char tag = line[0];
			const String text = String(line.c_str() + 1);
			if (emitted >= max_lines) {
				truncated = true;
				break;
			}
			if (tag == ' ') {
				++old_line;
				++new_line;
				lines.push_back(create_diff_line(old_line, new_line, text, " "));
			} else if (tag == '-') {
				++old_line;
				lines.push_back(create_diff_line(old_line, -1, text, "-"));
			} else if (tag == '+') {
				++new_line;
				lines.push_back(create_diff_line(-1, new_line, text, "+"));
			} else {
				continue;
			}
			++emitted;
		}
		if (truncated) {
			lines.push_back(create_diff_line(-1, -1, String("… truncated after ") + itos((int64_t)max_lines) + " lines", " "));
		}

		Dictionary hunk = create_diff_hunk(1, 1, MAX(old_line, 1), MAX(new_line, 1));
		hunk = add_line_diffs_into_diff_hunk(hunk, lines);
		hunks.push_back(hunk);
		diff_file = add_diff_hunks_into_diff_file(diff_file, hunks);
		files.push_back(diff_file);
	} catch (const std::exception &e) {
		last_error_ = String("ColdStorage exception: ") + e.what();
		popup_error(last_error_);
	} catch (...) {
		last_error_ = "ColdStorage unknown exception while building diff";
		popup_error(last_error_);
	}
	return files;
}

bool ColdStorageVCS::shut_down() {
	try {
		if (client_) {
			client_->disconnect();
			client_.reset();
		}
	} catch (...) {
		client_.reset();
	}
	return true;
}

String ColdStorageVCS::get_vcs_name() {
	return "ColdStorage";
}

List<EditorVCSInterface::Commit> ColdStorageVCS::get_previous_commits(int p_max_commits) {
	List<Commit> commits;
	if (!client_) {
		return commits;
	}
	try {
		int max_n = p_max_commits > 0 ? p_max_commits : 32;
		auto entries = client_->log("//depot/...", max_n);
		for (const auto &e : entries) {
			commits.push_back(_convert_commit(create_commit(String(e.description.c_str()), String(e.user.c_str()),
					itos(e.changeNum), e.timestamp, 0)));
		}
	} catch (const std::exception &e) {
		last_error_ = String("ColdStorage exception: ") + e.what();
		popup_error(last_error_);
	} catch (...) {
		last_error_ = "ColdStorage unknown exception while fetching log";
		popup_error(last_error_);
	}
	return commits;
}

List<String> ColdStorageVCS::get_branch_list() {
	List<String> out;
	out.push_back(current_branch_);
	return out;
}

List<String> ColdStorageVCS::get_remotes() {
	List<String> out;
	out.push_back(remote_name_);
	return out;
}

void ColdStorageVCS::create_branch(const String &p_branch_name) {
	if (!client_) {
		return;
	}
	try {
		const String src = "//depot/" + current_branch_ + "/...";
		const String dst = "//depot/" + p_branch_name + "/...";
		auto r = client_->branch(src.utf8().get_data(), dst.utf8().get_data());
		if (r.success) {
			current_branch_ = p_branch_name;
		} else {
			last_error_ = String(r.error.c_str());
		}
	} catch (const std::exception &e) {
		last_error_ = String("ColdStorage exception: ") + e.what();
		popup_error(last_error_);
	} catch (...) {
		last_error_ = "ColdStorage unknown exception while creating branch";
		popup_error(last_error_);
	}
}

void ColdStorageVCS::remove_branch(const String &p_branch_name) {
	last_error_ = "ColdStorage does not support deleting branch '" + p_branch_name + "' from the editor client.";
	popup_error(last_error_);
}

void ColdStorageVCS::create_remote(const String &p_remote_name, const String &p_remote_url) {
	remote_name_ = p_remote_name;
	if (p_remote_url.contains(":")) {
		PackedStringArray parts = p_remote_url.split(":");
		if (parts.size() >= 2) {
			config_.host = parts[0];
			config_.port = parts[1].to_int();
		}
	}
}

void ColdStorageVCS::remove_remote(const String &p_remote_name) {
	if (p_remote_name == remote_name_) {
		remote_name_ = config_.host + ":" + itos(config_.port);
		return;
	}
	last_error_ = "ColdStorage remote '" + p_remote_name + "' was not found.";
	popup_error(last_error_);
}

String ColdStorageVCS::get_current_branch_name() {
	return current_branch_;
}

bool ColdStorageVCS::checkout_branch(const String &p_branch_name) {
	current_branch_ = p_branch_name;
	pull(remote_name_);
	return true;
}

void ColdStorageVCS::pull(const String &) {
	auto_pull_sync();
}

void ColdStorageVCS::push(const String &, bool) {
	last_error_ = "ColdStorage publishes changes on Commit (submit). Push / force-push are not used.";
	popup_error(last_error_);
}

void ColdStorageVCS::fetch(const String &p_remote) {
	pull(p_remote);
}

List<EditorVCSInterface::DiffHunk> ColdStorageVCS::get_line_diff(const String &p_file_path, const String &) {
	List<DiffHunk> hunks;
	TypedArray<Dictionary> files = _build_diff(p_file_path);
	if (files.is_empty()) {
		return hunks;
	}
	DiffFile df = _convert_diff_file(files[0]);
	for (const DiffHunk &h : df.diff_hunks) {
		hunks.push_back(h);
	}
	return hunks;
}

bool ColdStorageVCS::validate_connection() {
	if (!client_ || !client_->isConnected()) {
		last_error_ = "Not connected";
		return false;
	}
	try {
		auto info = client_->info();
		if (info.name.empty() && info.version.empty()) {
			last_error_ = "Server info() failed";
			if (!client_->lastError().empty()) {
				last_error_ += ": " + String(client_->lastError().c_str());
			}
			return false;
		}
		return true;
	} catch (const std::exception &e) {
		last_error_ = String("ColdStorage exception: ") + e.what();
		popup_error(last_error_);
		return false;
	} catch (...) {
		last_error_ = "ColdStorage unknown exception during validate";
		popup_error(last_error_);
		return false;
	}
}

bool ColdStorageVCS::auto_pull_sync() {
	if (!client_ || !client_->isConnected()) {
		return false;
	}
	try {
		auto r = client_->syncAll("#head");
		if (!r.success) {
			last_error_ = String(r.error.c_str());
			return false;
		}
		return true;
	} catch (const std::exception &e) {
		last_error_ = String("ColdStorage exception: ") + e.what();
		popup_error(last_error_);
		return false;
	} catch (...) {
		last_error_ = "ColdStorage unknown exception during sync";
		popup_error(last_error_);
		return false;
	}
}

#endif
