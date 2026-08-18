/**************************************************************************/
/*  client_sdk.h                                                          */
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

#include "client/sdk/connection.h"
#include "client/sdk/tls_options.h"
#include "common/ignore/ignore_loader.h"
#include <functional>
#include <memory>
#include <nlohmann/json.hpp>
#include <string>
#include <vector>

namespace coldstorage {

class Workspace;

struct ServerInfo {
	std::string name;
	std::string version;
	int64_t uptime;
	int64_t changeCount;
};
struct SubmitResult {
	bool success;
	int64_t changeNum;
	std::string error;
	std::vector<std::string> conflictFiles;
};
struct SyncResult {
	bool success = false;
	int filesUpdated = 0;
	int filesDeleted = 0;
	int filesUnchanged = 0;
	int filesDenied = 0;
	std::string error;
	bool more = false;
	std::string nextCursor;
	bool stoppedAtLimit = false;
};
struct LogEntry {
	std::string depotPath;
	int64_t revNum;
	int64_t changeNum;
	std::string action;
	std::string user;
	std::string description;
	int64_t timestamp;
};
struct OpenedFile {
	std::string depotPath;
	std::string action;
	int64_t baseRev = 0;
	std::string fromPath;
	int64_t fromRev = 0;
};
struct BranchResult {
	bool success = false;
	int64_t changeNum = 0;
	int files = 0;
	int denied = 0;
	std::string error;
};
struct IntegrateResult {
	bool success;
	int filesScheduled;
	std::vector<std::string> resolveNeeded;
	std::string error;
};
struct ResolveResult {
	bool success = false;
	std::string error;
	bool hadConflicts = false;
};
struct VerifyResult {
	bool success = false;
	int verified = 0;
	int errors = 0;
	bool more = false;
	std::string cursor;
	std::string nextCursor;
	std::string error;
};
struct GcResult {
	bool success = false;
	int orphansRemoved = 0;
	int blobsRemoved = 0;
	int64_t contentRefsPurged = 0;
	std::string error;
	bool more = false;
	std::string nextCursor;
};

class WorkspaceFilters;

struct StatusEntry {
	std::string depotPath;
	std::string localPath;
	std::string state;
};

struct StatusResult {
	bool success = false;
	std::vector<StatusEntry> entries;
	std::string error;
};

struct WorkspaceFileInfo {
	std::string depotPath;
	std::string localPath;
	std::string state;
	int64_t headRev = 0;
	std::string headDigest;
	int64_t headSize = 0;
	std::string headFtype;
	int64_t haveRev = 0;
	std::string haveDigest;
	int64_t haveSize = 0;
	std::string openedAction;
	int64_t openedBaseRev = 0;
	std::string localDigest;
	int64_t localSize = 0;
};

struct FileInfoResult {
	bool success = false;
	std::vector<WorkspaceFileInfo> entries;
	std::string error;
};

struct ReconcileFlags {
	bool allowAdd = true;
	bool allowEdit = true;
	bool allowDelete = true;
	bool detectMoves = false;
	bool keepWorkspace = false;
};

struct ReconcileResult {
	bool success = false;
	int added = 0;
	int edited = 0;
	int deleted = 0;
	int moved = 0;
	int skipped = 0;
	std::string error;
};

struct AdminResult {
	bool success = false;
	std::string error;
	nlohmann::json data = nlohmann::json::object();
};

class ColdStorageClient {
public:
	ColdStorageClient();
	~ColdStorageClient();

	bool connectTransport(const std::string &host, int port, const TlsOptions &tls = {});
	bool negotiate();
	std::string peerFingerprint() const;
	bool connect(const std::string &host, int port, const TlsOptions &tls = {});
	void disconnect();
	bool isConnected() const;

	std::string login(const std::string &user, const std::string &password);
	bool authenticateWithJWT(const std::string &token);
	void setTicket(const std::string &ticket);

	ServerInfo info();
	bool add(const std::string &depotPath, const std::string &localPath);
	bool edit(const std::string &depotPath);
	bool delete_(const std::string &depotPath);
	bool revert(const std::string &depotPath);
	std::vector<OpenedFile> opened();
	SubmitResult submit(const std::string &description);

	SyncResult sync(const std::string &targetSpec = "#head", const std::string &cursor = "",
			const std::string &untilPath = "");

	SyncResult syncAll(const std::string &targetSpec = "#head");

	SyncResult syncAllParallel(int workers = 4, const std::string &targetSpec = "#head");
	std::vector<LogEntry> log(const std::string &depotPath, int maxEntries = 100);

	std::string print(const std::string &pathSpec);

	using PrintChunkFn = std::function<bool(const uint8_t *data, size_t len)>;
	bool printStreaming(const std::string &pathSpec, PrintChunkFn callback);

	bool printToFile(const std::string &pathSpec, const std::string &filePath);
	BranchResult branch(const std::string &src, const std::string &dst);
	IntegrateResult integrate(const std::string &src, const std::string &dst);
	ResolveResult resolve(const std::string &depotPath, const std::string &resolution);
	bool obliterate(const std::string &depotPath);
	VerifyResult verify(int64_t limit = 500, const std::string &cursor = "");

	GcResult gc(int64_t maxAgeSeconds = 0, bool purgeZeroRefs = true,
			const std::string &cursor = "");

	GcResult gcAll(int64_t maxAgeSeconds = 0, bool purgeZeroRefs = true);

	StatusResult status(const std::vector<std::string> &pathScope = {});
	FileInfoResult fileInfo(const std::vector<std::string> &pathScope = {});
	ReconcileResult reconcile(bool preview, ReconcileFlags flags,
			const std::vector<std::string> &pathScope = {});
	bool printAtRev(const std::string &depotPath, int64_t revNum, PrintChunkFn callback);
	const Workspace *ensureWorkspaceView();
	void invalidateWorkspaceView();

	bool lock(const std::string &depotPath);
	bool unlock(const std::string &depotPath);
	bool labelCreate(const std::string &name, int64_t changeNum = 0, const std::string &description = "");
	bool labelDelete(const std::string &name);
	std::vector<nlohmann::json> labelList();
	bool shelve(const std::string &name, const std::string &description = "");
	bool unshelve(const std::string &name, bool keep = false);

	bool userCreate(const std::string &name, const std::string &password);
	bool userDelete(const std::string &name);
	bool protectAdd(int64_t seq, const std::string &level, const std::string &subject,
			const std::string &path, bool isExclusion = false);
	AdminResult protectList();
	AdminResult backup(const std::string &path, const std::string &mode = "");

	AdminResult orgCreate(const std::string &name, const std::string &displayName = "",
			const std::string &description = "");
	AdminResult orgDelete(const std::string &name);
	AdminResult orgList();
	AdminResult orgInfo(const std::string &name);
	AdminResult orgMemberAdd(const std::string &org, const std::string &user,
			const std::string &role);
	AdminResult orgMemberRemove(const std::string &org, const std::string &user);
	AdminResult orgMemberList(const std::string &org);

	AdminResult repoCreate(const std::string &org, const std::string &name,
			const std::string &displayName = "",
			const std::string &description = "");
	AdminResult repoDelete(const std::string &org, const std::string &name);
	AdminResult repoList(const std::string &org);
	AdminResult repoInfo(const std::string &org, const std::string &name);
	AdminResult repoMemberAdd(const std::string &org, const std::string &name,
			const std::string &user, const std::string &role);
	AdminResult repoMemberRemove(const std::string &org, const std::string &name,
			const std::string &user);
	AdminResult repoMemberList(const std::string &org, const std::string &name);

	const std::string &lastError() const { return lastError_; }

	void setWorkspace(const std::string &name);
	std::string getWorkspace() const;
	void setWorkspaceRoot(const std::string &root);
	std::string workspaceRoot() const { return workspaceRoot_; }
	void setMaxDownloadBytes(size_t bytes);
	size_t maxDownloadBytes() const { return maxDownloadBytes_; }
	void initWorkspaceFilters(const IgnoreLoadOptions &opts = {});
	WorkspaceFilters *workspaceFilters() { return filters_.get(); }
	const WorkspaceFilters *workspaceFilters() const { return filters_.get(); }

	void setRepo(const std::string &repo);
	std::string getRepo() const;

	ServerConnection::CommandResult exec(const std::string &func, const nlohmann::json &args = {});

private:
	std::unique_ptr<ServerConnection> conn_;
	std::string ticket_;
	std::string user_;
	std::string workspace_;
	std::string workspaceRoot_;
	size_t maxDownloadBytes_ = 32ULL * 1024 * 1024 * 1024;
	std::string repo_;
	std::string lastError_;
	std::string lastHost_;
	int lastPort_ = 0;
	TlsOptions lastTls_;
	std::unique_ptr<WorkspaceFilters> filters_;
	mutable std::unique_ptr<Workspace> cachedWorkspace_;
	mutable bool workspaceViewLoaded_ = false;

	Message makeCommand(const std::string &func, const nlohmann::json &args = {});

	bool respondSendFile(const Instruction &instr);
};

} //namespace coldstorage
