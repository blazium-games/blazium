/**************************************************************************/
/*  workspace_view.cpp                                                    */
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

#include "client/sdk/workspace_view.h"
#include "client/sdk/client_sdk.h"
#include "client/sdk/sdk_internal.h"
#include <filesystem>

namespace fs = std::filesystem;
namespace coldstorage {
namespace {

std::string normalizeMappedLocal(const std::string &root, const std::string &mapped) {
	if (mapped.empty()) {
		return {};
	}
	fs::path p(mapped);
	if (p.is_absolute()) {
		return p.lexically_normal().generic_string();
	}
	if (mapped.rfind("./", 0) == 0) {
		return (fs::path(root) / mapped.substr(2)).lexically_normal().generic_string();
	}
	return (fs::path(root) / mapped).lexically_normal().generic_string();
}

std::string localPathForMapping(const Workspace &ws, const std::string &absPath) {
	std::error_code ec;
	fs::path rel = fs::relative(absPath, ws.root(), ec);
	if (ec) {
		return absPath;
	}
	const std::string relStr = rel.generic_string();
	if (relStr.empty() || relStr == ".") {
		return absPath;
	}
	return "./" + relStr;
}

} //namespace

const Workspace *ensureWorkspaceView(ColdStorageClient &client) {
	return client.ensureWorkspaceView();
}

void invalidateWorkspaceView(ColdStorageClient &client) {
	client.invalidateWorkspaceView();
}

std::string depotToLocalPathMapped(ColdStorageClient &client, const std::string &depotPath,
		const std::string &workspaceRoot) {
	if (const Workspace *ws = ensureWorkspaceView(client)) {
		std::string mapped = ws->depotToLocal(depotPath);
		if (mapped != depotPath && !mapped.empty()) {
			return normalizeMappedLocal(ws->root(), mapped);
		}
	}
	return sdk_internal::depotToLocalPath(depotPath, workspaceRoot);
}

std::optional<std::string> localToDepotPathMapped(ColdStorageClient &client,
		const std::string &localAbsPath,
		const std::string &workspaceRoot) {
	if (const Workspace *ws = ensureWorkspaceView(client)) {
		std::string asLocal = localPathForMapping(*ws, localAbsPath);
		std::string depot = ws->localToDepot(asLocal);
		if (depot != asLocal && !depot.empty() && depot.find("//") == 0) {
			return depot;
		}
		depot = ws->localToDepot(localAbsPath);
		if (depot != localAbsPath && !depot.empty() && depot.find("//") == 0) {
			return depot;
		}
	}
	fs::path root(workspaceRoot);
	std::error_code ec;
	fs::path rel = fs::relative(localAbsPath, root, ec);
	if (ec || rel.empty()) {
		return std::nullopt;
	}
	const std::string relStr = rel.generic_string();
	if (relStr.find("..") != std::string::npos) {
		return std::nullopt;
	}
	return std::string("//depot/") + relStr;
}

} //namespace coldstorage
