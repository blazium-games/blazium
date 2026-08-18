/**************************************************************************/
/*  workspace_status.cpp                                                  */
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

#include "client/sdk/workspace_status.h"
#include "client/sdk/workspace_filters.h"
#include "client/sdk/workspace_view.h"
#include "common/util/crypto.h"
#include <filesystem>
#include <iostream>
#include <set>
#include <unordered_map>

namespace fs = std::filesystem;
namespace coldstorage {
namespace {

bool inScope(const std::string &depotPath, const std::vector<std::string> &scope) {
	if (scope.empty()) {
		return true;
	}
	for (const auto &s : scope) {
		auto depot = s.find("//") == 0 ? s : std::string("//depot/") + s;
		if (depotPath == depot || depotPath.rfind(depot + "/", 0) == 0) {
			return true;
		}
		fs::path p(s);
		if (p.is_relative()) {
			std::string prefix = "//depot/" + p.generic_string();
			if (depotPath == prefix || depotPath.rfind(prefix + "/", 0) == 0) {
				return true;
			}
		}
	}
	return false;
}

bool inScopeLocal(ColdStorageClient &client, WorkspaceFilters &filters,
		const std::string &localAbs, const std::vector<std::string> &scope) {
	if (scope.empty()) {
		return true;
	}
	for (const auto &s : scope) {
		if (s.find("//") == 0) {
			auto local = depotToLocalPathMapped(client, s, filters.workspaceRoot());
			if (!local.empty()) {
				std::error_code ec;
				if (fs::equivalent(localAbs, local, ec)) {
					return true;
				}
				const std::string prefix = local + "/";
				if (localAbs.rfind(prefix, 0) == 0) {
					return true;
				}
			}
			continue;
		}
		fs::path root(filters.workspaceRoot());
		fs::path target = fs::path(s).is_absolute() ? fs::path(s) : root / s;
		std::error_code ec;
		target = fs::weakly_canonical(target, ec);
		if (ec) {
			target = (root / s).lexically_normal();
		}
		const std::string targetStr = target.generic_string();
		if (localAbs == targetStr) {
			return true;
		}
		if (localAbs.rfind(targetStr + "/", 0) == 0) {
			return true;
		}
	}
	return false;
}

std::string computeState(const WorkspaceFileInfo &info, bool isOpened) {
	if (isOpened) {
		return "opened";
	}
	const int64_t haveRev = info.haveRev;
	const int64_t headRev = info.headRev;
	const std::string &haveDigest = info.haveDigest;
	std::error_code ec;
	const bool localExists = !info.localPath.empty() && fs::exists(info.localPath, ec);

	if (haveRev > 0 && !localExists) {
		return "delete";
	}
	if (localExists && haveRev > 0 && !haveDigest.empty()) {
		if (info.localDigest != haveDigest) {
			return "edit";
		}
		return (headRev > haveRev) ? "sync" : "unchanged";
	}
	if (localExists && haveRev == 0) {
		return "sync";
	}
	if (!localExists && haveRev == 0) {
		return "";
	}
	return headRev > haveRev ? "sync" : "unchanged";
}

void fillLocalDigest(WorkspaceFileInfo &info) {
	std::error_code ec;
	if (info.localPath.empty() || !fs::exists(info.localPath, ec)) {
		return;
	}
	info.localDigest = sha256File(info.localPath, &info.localSize);
}

} //namespace

FileInfoResult computeWorkspaceFileInfo(ColdStorageClient &client, WorkspaceFilters &filters,
		const std::vector<std::string> &pathScope) {
	FileInfoResult result;
	std::unordered_map<std::string, OpenedFile> opened;
	for (const auto &f : client.opened()) {
		opened[f.depotPath] = f;
	}

	std::set<std::string> depotKnown;
	std::string cursor;
	do {
		auto page = client.exec("status", { { "cursor", cursor } });
		if (!page.success) {
			result.error = page.error.empty() ? client.lastError() : page.error;
			return result;
		}
		if (!page.data.contains("entries")) {
			break;
		}
		for (const auto &e : page.data["entries"]) {
			const std::string depotPath = e.value("depot_path", "");
			if (!inScope(depotPath, pathScope)) {
				continue;
			}
			depotKnown.insert(depotPath);

			WorkspaceFileInfo info;
			info.depotPath = depotPath;
			info.localPath =
					depotToLocalPathMapped(client, depotPath, filters.workspaceRoot());
			info.headRev = e.value("head_rev", static_cast<int64_t>(0));
			info.headDigest = e.value("head_digest", "");
			info.headSize = e.value("head_size", static_cast<int64_t>(0));
			info.headFtype = e.value("head_ftype", "");
			info.haveRev = e.value("have_rev", static_cast<int64_t>(0));
			info.haveDigest = e.value("have_digest", "");
			info.haveSize = e.value("have_size", static_cast<int64_t>(0));
			info.openedAction = e.value("opened_action", "");
			info.openedBaseRev = e.value("opened_base_rev", static_cast<int64_t>(0));

			const bool isOpened = opened.count(depotPath) > 0;
			fillLocalDigest(info);
			info.state = computeState(info, isOpened);
			if (info.state.empty()) {
				continue;
			}
			result.entries.push_back(std::move(info));
		}
		if (!page.data.value("more", false)) {
			break;
		}
		cursor = page.data.value("next_cursor", "");
	} while (!cursor.empty());

	fs::path root(filters.workspaceRoot());
	std::error_code ec;
	if (fs::exists(root, ec)) {
		for (auto it = fs::recursive_directory_iterator(
					 root, fs::directory_options::skip_permission_denied, ec);
				!ec && it != fs::recursive_directory_iterator(); it.increment(ec)) {
			if (!it->is_regular_file(ec)) {
				continue;
			}
			const std::string localAbs = it->path().lexically_normal().string();
			std::error_code ec2;
			auto rel = fs::relative(it->path(), root, ec2).generic_string();
			if (ec2 || rel.empty()) {
				continue;
			}
			if (rel.rfind(".coldstorage", 0) == 0) {
				continue;
			}
			if (filters.matchIgnore(rel, false).ignored) {
				continue;
			}
			if (!inScopeLocal(client, filters, localAbs, pathScope)) {
				continue;
			}

			auto depotOpt = localToDepotPathMapped(client, localAbs, filters.workspaceRoot());
			if (!depotOpt) {
				continue;
			}
			if (depotKnown.count(*depotOpt)) {
				continue;
			}

			WorkspaceFileInfo info;
			info.depotPath = *depotOpt;
			info.localPath = localAbs;
			fillLocalDigest(info);
			info.state = "add";
			result.entries.push_back(std::move(info));
		}
	}

	result.success = true;
	return result;
}

StatusResult computeWorkspaceStatus(ColdStorageClient &client, WorkspaceFilters &filters,
		const std::vector<std::string> &pathScope) {
	StatusResult result;
	auto fileInfo = computeWorkspaceFileInfo(client, filters, pathScope);
	if (!fileInfo.success) {
		result.error = fileInfo.error;
		return result;
	}
	result.entries.reserve(fileInfo.entries.size());
	for (const auto &fi : fileInfo.entries) {
		StatusEntry se;
		se.depotPath = fi.depotPath;
		se.localPath = fi.localPath;
		se.state = fi.state;
		result.entries.push_back(std::move(se));
	}
	result.success = true;
	return result;
}

ReconcileResult reconcileWorkspace(ColdStorageClient &client, WorkspaceFilters &filters,
		bool preview, ReconcileFlags flags,
		const std::vector<std::string> &pathScope) {
	ReconcileResult rr;
	auto fileInfo = computeWorkspaceFileInfo(client, filters, pathScope);
	if (!fileInfo.success) {
		rr.error = fileInfo.error;
		return rr;
	}

	std::unordered_map<std::string, OpenedFile> opened;
	for (const auto &f : client.opened()) {
		opened[f.depotPath] = f;
	}

	struct PendingAction {
		enum Kind { Add,
			Edit,
			Delete,
			Move } kind;
		WorkspaceFileInfo info;
		std::string moveFromDepot;
	};
	std::vector<PendingAction> pending;

	for (const auto &e : fileInfo.entries) {
		if (e.state == "opened" || e.state == "unchanged" || e.state == "sync") {
			++rr.skipped;
			continue;
		}
		if (opened.count(e.depotPath)) {
			++rr.skipped;
			continue;
		}
		if (e.state == "add" && flags.allowAdd) {
			pending.push_back({ PendingAction::Add, e, {} });
		} else if (e.state == "edit" && flags.allowEdit) {
			pending.push_back({ PendingAction::Edit, e, {} });
		} else if (e.state == "delete" && flags.allowDelete) {
			if (flags.keepWorkspace) {
				++rr.skipped;
				if (preview) {
					std::cout << "skip delete (keep workspace) " << e.depotPath << "\n";
				}
				continue;
			}
			pending.push_back({ PendingAction::Delete, e, {} });
		} else {
			++rr.skipped;
		}
	}

	if (flags.detectMoves && flags.allowAdd && flags.allowDelete) {
		std::vector<size_t> addIdx;
		std::vector<size_t> delIdx;
		for (size_t i = 0; i < pending.size(); ++i) {
			if (pending[i].kind == PendingAction::Add && !pending[i].info.localDigest.empty()) {
				addIdx.push_back(i);
			} else if (pending[i].kind == PendingAction::Delete &&
					!pending[i].info.haveDigest.empty()) {
				delIdx.push_back(i);
			}
		}
		std::vector<bool> usedAdd(addIdx.size(), false);
		std::vector<bool> usedDel(delIdx.size(), false);
		for (size_t di = 0; di < delIdx.size(); ++di) {
			const auto &delInfo = pending[delIdx[di]].info;
			for (size_t ai = 0; ai < addIdx.size(); ++ai) {
				if (usedAdd[ai] || usedDel[di]) {
					continue;
				}
				const auto &addInfo = pending[addIdx[ai]].info;
				if (delInfo.haveDigest == addInfo.localDigest) {
					usedAdd[ai] = true;
					usedDel[di] = true;
					pending[delIdx[di]].kind = PendingAction::Move;
					pending[delIdx[di]].moveFromDepot = delInfo.depotPath;
					pending[delIdx[di]].info = addInfo;
					break;
				}
			}
		}
		for (size_t ai = 0; ai < addIdx.size(); ++ai) {
			if (usedAdd[ai]) {
				pending[addIdx[ai]].kind = PendingAction::Move;
			}
		}
	}

	for (const auto &p : pending) {
		if (p.kind == PendingAction::Move && p.moveFromDepot.empty()) {
			continue;
		}
		if (p.kind == PendingAction::Move) {
			if (preview) {
				std::cout << "move " << p.moveFromDepot << " -> " << p.info.depotPath << "\n";
				++rr.moved;
				continue;
			}
			if (!client.delete_(p.moveFromDepot)) {
				rr.error = client.lastError();
				return rr;
			}
			if (!client.add(p.info.depotPath, p.info.localPath)) {
				rr.error = client.lastError();
				return rr;
			}
			++rr.moved;
		} else if (p.kind == PendingAction::Add) {
			if (preview) {
				std::cout << "add " << p.info.depotPath << "\n";
			} else if (!client.add(p.info.depotPath, p.info.localPath)) {
				rr.error = client.lastError();
				return rr;
			}
			++rr.added;
		} else if (p.kind == PendingAction::Edit) {
			if (preview) {
				std::cout << "edit " << p.info.depotPath << "\n";
			} else if (!client.edit(p.info.depotPath)) {
				rr.error = client.lastError();
				return rr;
			}
			++rr.edited;
		} else if (p.kind == PendingAction::Delete) {
			if (preview) {
				std::cout << "delete " << p.info.depotPath << "\n";
			} else if (!client.delete_(p.info.depotPath)) {
				rr.error = client.lastError();
				return rr;
			}
			++rr.deleted;
		}
	}

	rr.success = rr.error.empty();
	return rr;
}

} //namespace coldstorage
