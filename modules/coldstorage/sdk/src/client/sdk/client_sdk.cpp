/**************************************************************************/
/*  client_sdk.cpp                                                        */
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
#include "client/sdk/sdk_internal.h"
#include "client/sdk/workspace.h"
#include "client/sdk/workspace_filters.h"
#include "client/sdk/workspace_status.h"
#include "common/protocol/messages.h"
#include "common/util/net_trace.h"
#include "common/util/sanitize.h"
#include <atomic>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <mutex>
#include <thread>

namespace coldstorage {

ColdStorageClient::ColdStorageClient() :
		conn_(std::make_unique<ServerConnection>()) {}
ColdStorageClient::~ColdStorageClient() = default;

bool ColdStorageClient::connectTransport(const std::string &host, int port, const TlsOptions &tls) {
	std::string safeHost = sanitize::trim(host);
	if (!sanitize::isValidHost(safeHost)) {
		std::cerr << "Error: invalid server host\n";
		return false;
	}
	if (!sanitize::isValidPort(port)) {
		std::cerr << "Error: invalid port number\n";
		return false;
	}
	if (!conn_->connectTransport(safeHost, port, tls)) {
		return false;
	}
	lastHost_ = safeHost;
	lastPort_ = port;
	lastTls_ = tls;
	return true;
}

bool ColdStorageClient::negotiate() {
	return conn_->negotiate();
}

std::string ColdStorageClient::peerFingerprint() const {
	return conn_->peerFingerprint();
}

bool ColdStorageClient::connect(const std::string &host, int port, const TlsOptions &tls) {
	CS_NET_TRACE("ColdStorageClient", "connect begin host=" << host << " port=" << port);
	if (!connectTransport(host, port, tls)) {
		CS_NET_TRACE("ColdStorageClient", "connect transport failed");
		return false;
	}
	const bool ok = negotiate();
	CS_NET_TRACE("ColdStorageClient", "connect " << (ok ? "ok" : "negotiate failed"));
	return ok;
}

void ColdStorageClient::disconnect() {
	conn_->disconnect();
}
bool ColdStorageClient::isConnected() const {
	return conn_->isConnected();
}
void ColdStorageClient::setTicket(const std::string &ticket) {
	ticket_ = ticket;
}
void ColdStorageClient::setWorkspace(const std::string &name) {
	workspace_ = sanitize::trim(name);
	invalidateWorkspaceView();
}
std::string ColdStorageClient::getWorkspace() const {
	return workspace_;
}
void ColdStorageClient::setWorkspaceRoot(const std::string &root) {
	workspaceRoot_ = root;
	filters_.reset();
	invalidateWorkspaceView();
}

void ColdStorageClient::initWorkspaceFilters(const IgnoreLoadOptions &opts) {
	if (workspaceRoot_.empty()) {
		return;
	}
	filters_ = std::make_unique<WorkspaceFilters>(workspaceRoot_, opts);
}

void ColdStorageClient::setMaxDownloadBytes(size_t bytes) {
	maxDownloadBytes_ = bytes > 0 ? bytes : (32ULL * 1024 * 1024 * 1024);
}

void ColdStorageClient::setRepo(const std::string &repo) {
	repo_ = sanitize::trim(repo);
}
std::string ColdStorageClient::getRepo() const {
	return repo_;
}

StatusResult ColdStorageClient::status(const std::vector<std::string> &pathScope) {
	if (!filters_) {
		initWorkspaceFilters();
	}
	if (!filters_) {
		StatusResult r;
		r.error = "Workspace root not set";
		return r;
	}
	filters_->reloadIfStale();
	return computeWorkspaceStatus(*this, *filters_, pathScope);
}

FileInfoResult ColdStorageClient::fileInfo(const std::vector<std::string> &pathScope) {
	if (!filters_) {
		initWorkspaceFilters();
	}
	if (!filters_) {
		FileInfoResult r;
		r.error = "Workspace root not set";
		return r;
	}
	filters_->reloadIfStale();
	return computeWorkspaceFileInfo(*this, *filters_, pathScope);
}

ReconcileResult ColdStorageClient::reconcile(bool preview, ReconcileFlags flags,
		const std::vector<std::string> &pathScope) {
	if (!filters_) {
		initWorkspaceFilters();
	}
	if (!filters_) {
		ReconcileResult r;
		r.error = "Workspace root not set";
		return r;
	}
	filters_->reloadIfStale();
	return reconcileWorkspace(*this, *filters_, preview, flags, pathScope);
}

const Workspace *ColdStorageClient::ensureWorkspaceView() {
	if (cachedWorkspace_) {
		return cachedWorkspace_.get();
	}
	if (workspaceViewLoaded_) {
		return nullptr;
	}
	workspaceViewLoaded_ = true;
	if (workspace_.empty() || workspaceRoot_.empty()) {
		return nullptr;
	}

	auto page = exec("workspace_info", {});
	if (!page.success || !page.data.value("success", false)) {
		return nullptr;
	}

	nlohmann::json viewJson = page.data.value("view", nlohmann::json::array());
	auto mappings = Workspace::parseView(viewJson);
	cachedWorkspace_ = std::make_unique<Workspace>(workspace_, workspaceRoot_, mappings);
	return cachedWorkspace_.get();
}

void ColdStorageClient::invalidateWorkspaceView() {
	cachedWorkspace_.reset();
	workspaceViewLoaded_ = false;
}

Message ColdStorageClient::makeCommand(const std::string &func, const nlohmann::json &args) {
	Message msg;
	msg.func = func;
	msg.args = args;
	if (!ticket_.empty()) {
		msg.args["ticket"] = ticket_;
	}
	if (!user_.empty()) {
		msg.args["user"] = user_;
	}
	if (!workspace_.empty()) {
		msg.args["workspace"] = workspace_;
	}
	if (!repo_.empty()) {
		msg.args["repo"] = repo_;
	}
	return msg;
}

ServerConnection::CommandResult ColdStorageClient::exec(const std::string &func, const nlohmann::json &args) {
	return conn_->executeCommand(makeCommand(func, args));
}

std::string ColdStorageClient::login(const std::string &user, const std::string &password) {
	CS_NET_TRACE("ColdStorageClient", "login begin user=" << user);

	std::string safeUser = sanitize::trim(user);
	std::string safePw = sanitize::trim(password);
	auto result = exec("login", { { "user", safeUser }, { "password", safePw } });
	if (result.success) {
		ticket_ = result.data.value("ticket", "");
		user_ = safeUser;
		CS_NET_TRACE("ColdStorageClient", "login ok user=" << safeUser);
		return ticket_;
	}
	CS_NET_TRACE("ColdStorageClient", "login failed user=" << safeUser << " error=" << result.error);
	return "";
}

bool ColdStorageClient::authenticateWithJWT(const std::string &token) {
	std::string safeToken = sanitize::trim(token);
	if (!sanitize::isValidJWTFormat(safeToken)) {
		std::cerr << "Error: invalid JWT token format\n";
		return false;
	}
	auto result = exec("jwt_auth", { { "token", safeToken } });
	if (result.success) {
		user_ = result.data.value("user", "");
		repo_ = result.data.value("org", "") + "/" + result.data.value("repo", "");
		workspace_ = result.data.value("workspace", "");

		ticket_ = "";
		return true;
	}
	return false;
}

ServerInfo ColdStorageClient::info() {
	auto result = exec("info");
	ServerInfo si;
	si.name = result.data.value("name", "");
	si.version = result.data.value("version", "");
	si.uptime = result.data.value("uptime", 0);
	si.changeCount = result.data.value("change_count", 0);
	return si;
}

bool ColdStorageClient::add(const std::string &depotPath, const std::string &localPath) {
	auto result = exec("add", { { "depot_path", depotPath } });
	return result.success;
}

bool ColdStorageClient::edit(const std::string &depotPath) {
	auto result = exec("edit", { { "depot_path", depotPath } });
	return result.success;
}

bool ColdStorageClient::delete_(const std::string &depotPath) {
	auto result = exec("delete", { { "depot_path", depotPath } });
	return result.success;
}

bool ColdStorageClient::revert(const std::string &depotPath) {
	auto result = exec("revert", { { "depot_path", depotPath } });
	return result.success;
}

std::vector<OpenedFile> ColdStorageClient::opened() {
	lastError_.clear();
	auto result = exec("opened");
	std::vector<OpenedFile> files;
	if (!result.success) {
		lastError_ = result.error.empty() ? "opened failed" : result.error;
		return files;
	}
	if (result.data.contains("files")) {
		for (const auto &f : result.data["files"]) {
			OpenedFile of;
			of.depotPath = f.value("depot_path", "");
			of.action = f.value("action", "");
			of.baseRev = f.value("base_rev", 0);
			of.fromPath = f.value("from_path", "");
			of.fromRev = f.value("from_rev", 0);
			files.push_back(std::move(of));
		}
	}
	return files;
}

SubmitResult ColdStorageClient::submit(const std::string &description) {
	lastError_.clear();
	auto cmd = makeCommand("submit", { { "description", description } });
	if (!conn_->sendMessage(cmd)) {
		lastError_ = "Send failed";
		return { false, 0, "Send failed" };
	}

	SubmitResult sr{ false, 0, "" };
	while (true) {
		auto instr = conn_->readInstruction();
		if (!instr) {
			sr.error = "Connection lost";
			lastError_ = sr.error;
			return sr;
		}

		if (instr->op == InstructionOp::SendFile) {
			if (!respondSendFile(*instr)) {
				sr.error = "Failed to send file content";
				lastError_ = sr.error;
				return sr;
			}
		} else if (instr->op == InstructionOp::Release) {
			sr.success = instr->data.value("success", false);
			sr.changeNum = instr->data.value("change_num", 0);
			if (instr->data.contains("conflicts")) {
				sr.conflictFiles = instr->data["conflicts"].get<std::vector<std::string>>();
			}
			if (!sr.success && lastError_.empty() && !sr.error.empty()) {
				lastError_ = sr.error;
			} else if (!sr.success && lastError_.empty()) {
				lastError_ = "submit failed";
			}
			return sr;
		} else if (instr->op == InstructionOp::Error) {
			sr.error = instr->data.value("text", "");
			lastError_ = sr.error;
		}
	}
}

SyncResult ColdStorageClient::sync(const std::string &targetSpec, const std::string &cursor,
		const std::string &untilPath) {
	lastError_.clear();
	nlohmann::json args = { { "target", targetSpec } };
	if (!cursor.empty()) {
		args["cursor"] = cursor;
		args["after_path"] = cursor;
	}
	if (!untilPath.empty()) {
		args["until_path"] = untilPath;
	}
	auto cmd = makeCommand("sync", args);
	if (!conn_->sendMessage(cmd)) {
		lastError_ = "Send failed";
		return { false, 0, 0, 0, 0, "Send failed" };
	}

	SyncResult sr{};
	while (true) {
		auto instr = conn_->readInstruction();
		if (!instr) {
			sr.error = "Connection lost";
			lastError_ = sr.error;
			return sr;
		}

		if (instr->op == InstructionOp::WriteFile || instr->op == InstructionOp::ChunkBegin) {
			std::string err;
			if (!sdk_internal::writeInstructionToLocalFile(*conn_, *instr, workspaceRoot_, &err,
						maxDownloadBytes_, filters_.get())) {
				sr.error = err;
				lastError_ = err;
				return sr;
			}
		} else if (instr->op == InstructionOp::DeleteFile) {
			if (workspaceRoot_.empty()) {
				sr.error = "Workspace root required for local file deletes";
				lastError_ = sr.error;
				return sr;
			}
			std::string depotPath = instr->data.value("path", "");
			std::string localPath = sdk_internal::depotToLocalPath(depotPath, workspaceRoot_);
			if (localPath.empty()) {
				sr.error = "Invalid or escaped depot path on delete: " + depotPath;
				lastError_ = sr.error;
				return sr;
			}
			std::filesystem::remove(localPath);
		} else if (instr->op == InstructionOp::Release) {
			sr.success = instr->data.value("success", false);
			sr.filesUpdated = instr->data.value("updated", 0);
			sr.filesDeleted = instr->data.value("deleted", 0);
			sr.filesUnchanged = instr->data.value("unchanged", 0);
			sr.filesDenied = instr->data.value("denied", 0);
			sr.more = instr->data.value("more", false);
			sr.nextCursor = instr->data.value("next_cursor", "");
			sr.stoppedAtLimit = instr->data.value("stopped_at_limit", false);
			if (!sr.success && lastError_.empty()) {
				if (!sr.error.empty()) {
					lastError_ = sr.error;
				} else {
					lastError_ = "sync failed";
				}
			}
			return sr;
		} else if (instr->op == InstructionOp::Error) {
			sr.error = instr->data.value("text", "");
			lastError_ = sr.error;
		} else if (instr->op == InstructionOp::Info) {
		}
	}
}

SyncResult ColdStorageClient::syncAll(const std::string &targetSpec) {
	SyncResult total{};
	total.success = true;
	std::string cursor;
	constexpr int kMaxPages = 100000;
	for (int page = 0; page < kMaxPages; ++page) {
		auto sr = sync(targetSpec, cursor);
		if (!sr.success) {
			total.success = false;
			total.error = sr.error;
			return total;
		}
		total.filesUpdated += sr.filesUpdated;
		total.filesDeleted += sr.filesDeleted;
		total.filesUnchanged += sr.filesUnchanged;
		total.filesDenied += sr.filesDenied;
		if (!sr.more) {
			total.more = false;
			total.nextCursor.clear();
			total.stoppedAtLimit = false;
			return total;
		}
		cursor = sr.nextCursor;
		if (cursor.empty()) {
			total.success = false;
			total.error = "sync more=true but next_cursor empty";
			total.more = true;
			total.stoppedAtLimit = sr.stoppedAtLimit;
			return total;
		}
	}
	total.success = false;
	total.error = "syncAll exceeded max pages";
	total.more = true;
	return total;
}

SyncResult ColdStorageClient::syncAllParallel(int workers, const std::string &targetSpec) {
	if (workers < 1) {
		workers = 1;
	}
	if (workers > 8) {
		workers = 8;
	}
	if (workers == 1 || !conn_ || lastHost_.empty() || ticket_.empty()) {
		return syncAll(targetSpec);
	}

	static const char *kSplits[] = {
		"//0", "//4", "//8", "//A", "//E", "//a", "//e", "//~"
	};
	std::vector<std::string> bounds;
	bounds.push_back("");
	for (int i = 0; i < workers - 1 && i < 7; ++i) {
		bounds.push_back(kSplits[i]);
	}

	bounds.push_back("//~~~");

	SyncResult total{};
	total.success = true;
	std::mutex totalMu;
	std::atomic<bool> failed{ false };
	std::string failError;

	auto drainRange = [&](const std::string &after, const std::string &until) {
		ColdStorageClient worker;
		if (!worker.connect(lastHost_, lastPort_, lastTls_)) {
			std::lock_guard<std::mutex> lock(totalMu);
			failed = true;
			failError = "parallel sync connect failed";
			return;
		}
		worker.setTicket(ticket_);
		worker.user_ = user_;
		worker.setWorkspace(workspace_);
		worker.setWorkspaceRoot(workspaceRoot_);
		worker.setRepo(repo_);
		worker.setMaxDownloadBytes(maxDownloadBytes_);

		std::string cursor = after;
		constexpr int kMaxPages = 100000;
		for (int page = 0; page < kMaxPages && !failed.load(); ++page) {
			auto sr = worker.sync(targetSpec, cursor, until);
			if (!sr.success) {
				std::lock_guard<std::mutex> lock(totalMu);
				failed = true;
				failError = sr.error.empty() ? "parallel sync page failed" : sr.error;
				return;
			}
			{
				std::lock_guard<std::mutex> lock(totalMu);
				total.filesUpdated += sr.filesUpdated;
				total.filesDeleted += sr.filesDeleted;
				total.filesUnchanged += sr.filesUnchanged;
				total.filesDenied += sr.filesDenied;
			}
			if (!sr.more) {
				break;
			}
			cursor = sr.nextCursor;
			if (cursor.empty()) {
				break;
			}
			if (!until.empty() && cursor >= until) {
				break;
			}
		}
	};

	std::vector<std::thread> threads;
	threads.reserve(static_cast<size_t>(workers));
	for (size_t i = 0; i + 1 < bounds.size(); ++i) {
		threads.emplace_back(drainRange, bounds[i], bounds[i + 1]);
	}
	for (auto &t : threads) {
		t.join();
	}

	if (failed.load()) {
		total.success = false;
		total.error = failError;
		return total;
	}

	const std::string kOrphanPrefix = std::string("orphan") + '\x1f';
	std::string orphanCursor = kOrphanPrefix;
	constexpr int kMaxPages = 100000;
	for (int page = 0; page < kMaxPages; ++page) {
		auto sr = sync(targetSpec, orphanCursor);
		if (!sr.success) {
			total.success = false;
			total.error = sr.error;
			return total;
		}
		total.filesUpdated += sr.filesUpdated;
		total.filesDeleted += sr.filesDeleted;
		total.filesUnchanged += sr.filesUnchanged;
		total.filesDenied += sr.filesDenied;
		if (!sr.more) {
			break;
		}
		orphanCursor = sr.nextCursor;
		if (orphanCursor.empty()) {
			break;
		}
	}
	return total;
}

std::vector<LogEntry> ColdStorageClient::log(const std::string &depotPath, int maxEntries) {
	auto result = exec("log", { { "depot_path", depotPath }, { "max_entries", maxEntries } });
	std::vector<LogEntry> entries;
	if (result.data.contains("entries")) {
		for (const auto &e : result.data["entries"]) {
			entries.push_back({ e.value("depot_path", ""), e.value("rev_num", 0), e.value("change_num", 0),
					e.value("action", ""), e.value("user", ""), e.value("description", ""), e.value("timestamp", 0) });
		}
	}
	return entries;
}

BranchResult ColdStorageClient::branch(const std::string &src, const std::string &dst) {
	lastError_.clear();
	auto result = exec("branch", { { "source", src }, { "destination", dst } });
	BranchResult br;
	br.success = result.success;
	br.error = result.error;
	if (result.success) {
		br.changeNum = result.data.value("change_num", static_cast<int64_t>(0));
		br.files = result.data.value("files", 0);
		br.denied = result.data.value("denied", 0);
	} else {
		lastError_ = result.error.empty() ? "branch failed" : result.error;
	}
	return br;
}

IntegrateResult ColdStorageClient::integrate(const std::string &src, const std::string &dst) {
	lastError_.clear();
	auto result = exec("integrate", { { "source", src }, { "destination", dst } });
	IntegrateResult ir;
	ir.success = result.success;
	ir.filesScheduled = result.data.value("files_scheduled", 0);
	if (result.data.contains("resolve_needed")) {
		ir.resolveNeeded = result.data["resolve_needed"].get<std::vector<std::string>>();
	}
	ir.error = result.error;
	if (!ir.success) {
		lastError_ = result.error.empty() ? "integrate failed" : result.error;
	}
	return ir;
}

ResolveResult ColdStorageClient::resolve(const std::string &depotPath, const std::string &resolution) {
	lastError_.clear();
	auto cmd = makeCommand("resolve", { { "depot_path", depotPath }, { "resolution", resolution } });
	if (!conn_->sendMessage(cmd)) {
		lastError_ = "Send failed";
		return { false, "Send failed" };
	}

	ResolveResult rr;
	while (true) {
		auto instr = conn_->readInstruction();
		if (!instr) {
			rr.error = "Connection lost";
			lastError_ = rr.error;
			return rr;
		}

		if (instr->op == InstructionOp::WriteFile || instr->op == InstructionOp::ChunkBegin) {
			std::string err;
			if (!sdk_internal::writeInstructionToLocalFile(*conn_, *instr, workspaceRoot_, &err,
						maxDownloadBytes_, filters_.get())) {
				rr.error = err.empty() ? "Failed to write resolved file" : err;
				lastError_ = rr.error;
				return rr;
			}
		} else if (instr->op == InstructionOp::Release) {
			rr.success = instr->data.value("success", false);
			rr.hadConflicts = instr->data.value("had_conflicts", false);
			if (!rr.success && rr.error.empty()) {
				rr.error = instr->data.value("error", "Resolve failed");
			}
			if (!rr.success) {
				lastError_ = rr.error;
			}
			return rr;
		} else if (instr->op == InstructionOp::Error) {
			rr.error = instr->data.value("text", "");
			lastError_ = rr.error;
		} else if (instr->op == InstructionOp::Info) {
		}
	}
}

} //namespace coldstorage
