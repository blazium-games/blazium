/**************************************************************************/
/*  sdk_admin.cpp                                                         */
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

#include "client/sdk/client_sdk.h"

namespace coldstorage {

bool ColdStorageClient::obliterate(const std::string &depotPath) {
	auto result = exec("obliterate", { { "depot_path", depotPath } });
	return result.success;
}

VerifyResult ColdStorageClient::verify(int64_t limit, const std::string &cursor) {
	if (limit <= 0) {
		limit = 500;
	}
	nlohmann::json args = { { "limit", limit } };
	if (!cursor.empty()) {
		args["cursor"] = cursor;
	}
	auto result = exec("verify", args);
	VerifyResult out;
	out.success = result.success;
	out.error = result.error;
	if (result.success) {
		out.verified = result.data.value("verified", 0);
		out.errors = result.data.value("errors", 0);
		out.more = result.data.value("more", false);
		out.cursor = result.data.value("cursor", "");
		out.nextCursor = result.data.value("next_cursor", "");
	}
	return out;
}

GcResult ColdStorageClient::gc(int64_t maxAgeSeconds, bool purgeZeroRefs,
		const std::string &cursor) {
	nlohmann::json args = {
		{ "max_age_seconds", maxAgeSeconds },
		{ "purge_zero_refs", purgeZeroRefs }
	};
	if (!cursor.empty()) {
		args["cursor"] = cursor;
	}
	auto result = exec("gc", args);
	GcResult out;
	out.success = result.success;
	out.error = result.error;
	if (result.success) {
		out.orphansRemoved = result.data.value("orphans_removed", 0);
		out.blobsRemoved = result.data.value("blobs_removed", 0);
		out.contentRefsPurged = result.data.value("content_refs_purged", static_cast<int64_t>(0));
		out.more = result.data.value("more", false);
		out.nextCursor = result.data.value("next_cursor", "");
	}
	return out;
}

GcResult ColdStorageClient::gcAll(int64_t maxAgeSeconds, bool purgeZeroRefs) {
	GcResult total{};
	total.success = true;
	std::string cursor;
	constexpr int kMaxPages = 100000;
	for (int page = 0; page < kMaxPages; ++page) {
		auto gr = gc(maxAgeSeconds, purgeZeroRefs, cursor);
		if (!gr.success) {
			total.success = false;
			total.error = gr.error;
			return total;
		}
		total.orphansRemoved += gr.orphansRemoved;
		total.blobsRemoved += gr.blobsRemoved;
		total.contentRefsPurged += gr.contentRefsPurged;
		if (!gr.more) {
			total.more = false;
			total.nextCursor.clear();
			return total;
		}
		cursor = gr.nextCursor;
		if (cursor.empty()) {
			total.success = false;
			total.error = "gc more=true but next_cursor empty";
			total.more = true;
			return total;
		}
	}
	total.success = false;
	total.error = "gcAll exceeded max pages";
	total.more = true;
	return total;
}

bool ColdStorageClient::lock(const std::string &depotPath) {
	auto result = exec("lock", { { "depot_path", depotPath } });
	return result.success;
}

bool ColdStorageClient::unlock(const std::string &depotPath) {
	auto result = exec("unlock", { { "depot_path", depotPath } });
	return result.success;
}

bool ColdStorageClient::labelCreate(const std::string &name, int64_t changeNum, const std::string &description) {
	nlohmann::json args = { { "action", "create" }, { "name", name }, { "description", description } };
	if (changeNum > 0) {
		args["change_num"] = changeNum;
	}
	auto result = exec("label", args);
	return result.success;
}

bool ColdStorageClient::labelDelete(const std::string &name) {
	auto result = exec("label", { { "action", "delete" }, { "name", name } });
	return result.success;
}

std::vector<nlohmann::json> ColdStorageClient::labelList() {
	auto result = exec("label", { { "action", "list" } });
	if (result.data.contains("labels")) {
		return result.data["labels"].get<std::vector<nlohmann::json>>();
	}
	return {};
}

bool ColdStorageClient::userCreate(const std::string &name, const std::string &password) {
	lastError_.clear();
	auto result = exec("user", { { "action", "create" }, { "name", name }, { "password", password } });
	if (!result.success) {
		lastError_ = result.error.empty() ? "user create failed" : result.error;
	}
	return result.success;
}

bool ColdStorageClient::userDelete(const std::string &name) {
	lastError_.clear();
	auto result = exec("user", { { "action", "delete" }, { "name", name } });
	if (!result.success) {
		lastError_ = result.error.empty() ? "user delete failed" : result.error;
	}
	return result.success;
}

bool ColdStorageClient::protectAdd(int64_t seq, const std::string &level, const std::string &subject,
		const std::string &path, bool isExclusion) {
	lastError_.clear();
	nlohmann::json args = {
		{ "action", "add" },
		{ "seq", seq },
		{ "level", level },
		{ "subject", subject },
		{ "path", path },
		{ "is_exclusion", isExclusion },
	};
	auto result = exec("protect", args);
	if (!result.success) {
		lastError_ = result.error.empty() ? "protect add failed" : result.error;
	}
	return result.success;
}

AdminResult ColdStorageClient::protectList() {
	lastError_.clear();
	auto result = exec("protect", { { "action", "list" } });
	AdminResult out{ result.success, result.error, result.data };
	if (!out.success) {
		lastError_ = result.error.empty() ? "protect list failed" : result.error;
	}
	return out;
}

AdminResult ColdStorageClient::backup(const std::string &path, const std::string &mode) {
	lastError_.clear();
	nlohmann::json args = { { "path", path } };
	if (!mode.empty()) {
		args["mode"] = mode;
	}
	auto result = exec("backup", args);
	AdminResult out{ result.success, result.error, result.data };
	if (!out.success) {
		lastError_ = result.error.empty() ? "backup failed" : result.error;
	}
	return out;
}

AdminResult ColdStorageClient::orgCreate(const std::string &name, const std::string &displayName,
		const std::string &description) {
	lastError_.clear();
	nlohmann::json args = { { "action", "create" }, { "name", name } };
	if (!displayName.empty()) {
		args["display_name"] = displayName;
	}
	if (!description.empty()) {
		args["description"] = description;
	}
	auto result = exec("org", args);
	AdminResult out{ result.success, result.error, result.data };
	if (!out.success) {
		lastError_ = result.error.empty() ? "org create failed" : result.error;
	}
	return out;
}

AdminResult ColdStorageClient::orgDelete(const std::string &name) {
	lastError_.clear();
	auto result = exec("org", { { "action", "delete" }, { "name", name } });
	AdminResult out{ result.success, result.error, result.data };
	if (!out.success) {
		lastError_ = result.error.empty() ? "org delete failed" : result.error;
	}
	return out;
}

AdminResult ColdStorageClient::orgList() {
	lastError_.clear();
	auto result = exec("org", { { "action", "list" } });
	AdminResult out{ result.success, result.error, result.data };
	if (!out.success) {
		lastError_ = result.error.empty() ? "org list failed" : result.error;
	}
	return out;
}

AdminResult ColdStorageClient::orgInfo(const std::string &name) {
	lastError_.clear();
	auto result = exec("org", { { "action", "info" }, { "name", name } });
	AdminResult out{ result.success, result.error, result.data };
	if (!out.success) {
		lastError_ = result.error.empty() ? "org info failed" : result.error;
	}
	return out;
}

AdminResult ColdStorageClient::orgMemberAdd(const std::string &org, const std::string &user,
		const std::string &role) {
	lastError_.clear();
	auto result = exec("org", { { "action", "member" }, { "member_action", "add" }, { "org", org }, { "user", user }, { "role", role } });
	AdminResult out{ result.success, result.error, result.data };
	if (!out.success) {
		lastError_ = result.error.empty() ? "org member add failed" : result.error;
	}
	return out;
}

AdminResult ColdStorageClient::orgMemberRemove(const std::string &org, const std::string &user) {
	lastError_.clear();
	auto result = exec("org", { { "action", "member" }, { "member_action", "remove" }, { "org", org }, { "user", user } });
	AdminResult out{ result.success, result.error, result.data };
	if (!out.success) {
		lastError_ = result.error.empty() ? "org member remove failed" : result.error;
	}
	return out;
}

AdminResult ColdStorageClient::orgMemberList(const std::string &org) {
	lastError_.clear();
	auto result = exec("org", { { "action", "member" }, { "member_action", "list" }, { "org", org } });
	AdminResult out{ result.success, result.error, result.data };
	if (!out.success) {
		lastError_ = result.error.empty() ? "org member list failed" : result.error;
	}
	return out;
}

AdminResult ColdStorageClient::repoCreate(const std::string &org, const std::string &name,
		const std::string &displayName,
		const std::string &description) {
	lastError_.clear();
	nlohmann::json args = { { "action", "create" }, { "org", org }, { "name", name } };
	if (!displayName.empty()) {
		args["display_name"] = displayName;
	}
	if (!description.empty()) {
		args["description"] = description;
	}
	auto result = exec("repo", args);
	AdminResult out{ result.success, result.error, result.data };
	if (!out.success) {
		lastError_ = result.error.empty() ? "repo create failed" : result.error;
	}
	return out;
}

AdminResult ColdStorageClient::repoDelete(const std::string &org, const std::string &name) {
	lastError_.clear();
	auto result = exec("repo", { { "action", "delete" }, { "org", org }, { "name", name } });
	AdminResult out{ result.success, result.error, result.data };
	if (!out.success) {
		lastError_ = result.error.empty() ? "repo delete failed" : result.error;
	}
	return out;
}

AdminResult ColdStorageClient::repoList(const std::string &org) {
	lastError_.clear();
	auto result = exec("repo", { { "action", "list" }, { "org", org } });
	AdminResult out{ result.success, result.error, result.data };
	if (!out.success) {
		lastError_ = result.error.empty() ? "repo list failed" : result.error;
	}
	return out;
}

AdminResult ColdStorageClient::repoInfo(const std::string &org, const std::string &name) {
	lastError_.clear();
	auto result = exec("repo", { { "action", "info" }, { "org", org }, { "name", name } });
	AdminResult out{ result.success, result.error, result.data };
	if (!out.success) {
		lastError_ = result.error.empty() ? "repo info failed" : result.error;
	}
	return out;
}

AdminResult ColdStorageClient::repoMemberAdd(const std::string &org, const std::string &name,
		const std::string &user, const std::string &role) {
	lastError_.clear();
	auto result = exec("repo", { { "action", "member" }, { "member_action", "add" }, { "org", org }, { "name", name }, { "user", user }, { "role", role } });
	AdminResult out{ result.success, result.error, result.data };
	if (!out.success) {
		lastError_ = result.error.empty() ? "repo member add failed" : result.error;
	}
	return out;
}

AdminResult ColdStorageClient::repoMemberRemove(const std::string &org, const std::string &name,
		const std::string &user) {
	lastError_.clear();
	auto result = exec("repo", { { "action", "member" }, { "member_action", "remove" }, { "org", org }, { "name", name }, { "user", user } });
	AdminResult out{ result.success, result.error, result.data };
	if (!out.success) {
		lastError_ = result.error.empty() ? "repo member remove failed" : result.error;
	}
	return out;
}

AdminResult ColdStorageClient::repoMemberList(const std::string &org, const std::string &name) {
	lastError_.clear();
	auto result = exec("repo", { { "action", "member" }, { "member_action", "list" }, { "org", org }, { "name", name } });
	AdminResult out{ result.success, result.error, result.data };
	if (!out.success) {
		lastError_ = result.error.empty() ? "repo member list failed" : result.error;
	}
	return out;
}

} //namespace coldstorage
