/**************************************************************************/
/*  sdk_paths.cpp                                                         */
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

#include "client/sdk/sdk_internal.h"
#include "client/sdk/workspace_filters.h"
#include "common/util/sanitize.h"
#include <filesystem>
#include <fstream>
#include <nlohmann/json.hpp>

namespace coldstorage {
namespace sdk_internal {
namespace {

bool pathUnderRoot(const std::filesystem::path &candidate,
		const std::filesystem::path &root) {
	std::error_code ec;
	auto normalCand = std::filesystem::weakly_canonical(candidate, ec);
	if (ec) {
		normalCand = candidate.lexically_normal();
	}
	auto normalRoot = std::filesystem::weakly_canonical(root, ec);
	if (ec) {
		normalRoot = root.lexically_normal();
	}
	auto rel = normalCand.lexically_relative(normalRoot);
	auto s = rel.generic_string();
	if (s.empty() || s == ".") {
		return false;
	}
	if (s == ".." || s.rfind("../", 0) == 0) {
		return false;
	}
	if (s.find("/../") != std::string::npos) {
		return false;
	}
	return true;
}

bool isValidDownloadId(const std::string &id) {
	return sanitize::isHexDigest64(id);
}

bool saveDownloadState(const std::filesystem::path &statePath, int64_t offset,
		int64_t totalSize, const std::string &downloadId) {
	nlohmann::json j = {
		{ "offset", offset },
		{ "total_size", totalSize },
		{ "download_id", downloadId },
	};
	std::ofstream out(statePath, std::ios::trunc);
	if (!out.is_open()) {
		return false;
	}
	out << j.dump();
	return static_cast<bool>(out);
}

bool loadDownloadState(const std::filesystem::path &statePath, const std::string &downloadId,
		int64_t &offsetOut, int64_t &totalSizeOut) {
	std::ifstream in(statePath);
	if (!in.is_open()) {
		return false;
	}
	try {
		nlohmann::json j;
		in >> j;
		if (j.value("download_id", "") != downloadId) {
			return false;
		}
		offsetOut = j.value("offset", static_cast<int64_t>(0));
		totalSizeOut = j.value("total_size", static_cast<int64_t>(-1));
		if (offsetOut < 0) {
			offsetOut = 0;
		}
		return true;
	} catch (...) {
		return false;
	}
}

} //namespace

std::string depotToLocalPath(const std::string &depotPath, const std::string &workspaceRoot) {
	if (!sanitize::isValidDepotPath(depotPath)) {
		return {};
	}
	std::string relPath = depotPath;
	if (relPath.find("//") == 0) {
		auto thirdSlash = relPath.find('/', 2);
		if (thirdSlash != std::string::npos) {
			relPath = relPath.substr(thirdSlash + 1);
		} else {
			return {};
		}
	}
	if (relPath.empty() || relPath.find("..") != std::string::npos) {
		return {};
	}
	if (workspaceRoot.empty()) {
		return relPath;
	}

	std::filesystem::path root(workspaceRoot);
	std::filesystem::path local = root / relPath;
	if (!pathUnderRoot(local, root)) {
		return {};
	}
	return local.generic_string();
}

bool writeInstructionToLocalFile(ServerConnection &conn, const Instruction &instr,
		const std::string &workspaceRoot,
		std::string *errorOut, size_t maxDownloadBytes,
		WorkspaceFilters *filters) {
	if (workspaceRoot.empty()) {
		if (errorOut) {
			*errorOut = "Workspace root required for local file writes";
		}
		return false;
	}
	if (maxDownloadBytes == 0) {
		maxDownloadBytes = 32ULL * 1024 * 1024 * 1024;
	}
	std::string depotPath = instr.data.value("path", "");
	std::string localPath = depotToLocalPath(depotPath, workspaceRoot);
	if (localPath.empty()) {
		if (errorOut) {
			*errorOut = "Invalid or escaped depot path: " + depotPath;
		}
		return false;
	}
	std::filesystem::path p(localPath);
	std::filesystem::create_directories(p.parent_path());

	if (instr.op == InstructionOp::WriteFile) {
		std::vector<uint8_t> payload = instr.payload;
		if (filters) {
			payload = filters->normalizeForSyncWrite(depotPath, payload);
		}
		std::ofstream out(localPath, std::ios::binary | std::ios::trunc);
		if (!out.is_open()) {
			if (errorOut) {
				*errorOut = "Failed to open local file: " + localPath;
			}
			return false;
		}
		out.write(reinterpret_cast<const char *>(payload.data()),
				static_cast<std::streamsize>(payload.size()));
		return true;
	}

	if (instr.op == InstructionOp::DeleteFile) {
		std::error_code ec;
		std::filesystem::remove(localPath, ec);
		return true;
	}

	if (instr.op != InstructionOp::ChunkBegin) {
		if (errorOut) {
			*errorOut = "Unexpected instruction for file write";
		}
		return false;
	}

	const bool resumeSupported = instr.data.value("resume_supported", false);
	const std::string downloadId = instr.data.value("download_id", "");
	const int64_t totalSize = instr.data.value("total_size", static_cast<int64_t>(-1));
	int64_t segmentBytes = instr.data.value("segment_bytes", static_cast<int64_t>(0));
	if (segmentBytes <= 0) {
		segmentBytes = 256LL * 1024 * 1024;
	}

	std::filesystem::path partialPath = p;
	std::filesystem::path statePath;
	int64_t resumeFrom = 0;

	if (resumeSupported && !downloadId.empty()) {
		if (!isValidDownloadId(downloadId)) {
			if (errorOut) {
				*errorOut = "Invalid download_id (expected 64 hex chars)";
			}
			return false;
		}
		auto dlDir = std::filesystem::path(workspaceRoot) / ".coldstorage" / "downloads" / downloadId;
		if (!pathUnderRoot(dlDir, std::filesystem::path(workspaceRoot))) {
			if (errorOut) {
				*errorOut = "download_id path escapes workspace";
			}
			return false;
		}
		std::filesystem::create_directories(dlDir);
		partialPath = dlDir / "partial";
		statePath = dlDir / "STATE.json";
		if (!pathUnderRoot(partialPath, std::filesystem::path(workspaceRoot)) ||
				!pathUnderRoot(statePath, std::filesystem::path(workspaceRoot))) {
			if (errorOut) {
				*errorOut = "download session path escapes workspace";
			}
			return false;
		}
		std::error_code ec;
		int64_t partialSize = 0;
		if (std::filesystem::exists(partialPath, ec) && !ec) {
			partialSize = static_cast<int64_t>(std::filesystem::file_size(partialPath, ec));
			if (ec) {
				partialSize = 0;
			}
		}
		int64_t stateOffset = 0;
		int64_t stateTotal = -1;
		if (std::filesystem::exists(statePath, ec) && !ec &&
				loadDownloadState(statePath, downloadId, stateOffset, stateTotal)) {
			if (stateTotal >= 0 && totalSize >= 0 && stateTotal != totalSize) {
				resumeFrom = partialSize;
			} else {
				resumeFrom = std::min(stateOffset, partialSize);
			}
		} else {
			resumeFrom = partialSize;
		}
		if (totalSize >= 0 && resumeFrom > totalSize) {
			resumeFrom = 0;
		}

		if (totalSize >= 0) {
			auto downloadsRoot =
					std::filesystem::path(workspaceRoot) / ".coldstorage" / "downloads";
			uint64_t used = 0;
			std::error_code walkEc;
			if (std::filesystem::exists(downloadsRoot, walkEc) && !walkEc) {
				for (auto it = std::filesystem::recursive_directory_iterator(downloadsRoot, walkEc);
						!walkEc && it != std::filesystem::recursive_directory_iterator();
						it.increment(walkEc)) {
					if (!it->is_regular_file(walkEc)) {
						continue;
					}
					used += static_cast<uint64_t>(it->file_size(walkEc));
				}
			}
			int64_t need = totalSize - resumeFrom;
			if (need < 0) {
				need = 0;
			}

			if (used + static_cast<uint64_t>(need) > maxDownloadBytes) {
				if (errorOut) {
					*errorOut = "download disk quota exceeded (limits.max_download_bytes)";
				}
				return false;
			}
		}

		Instruction reply;
		reply.op = InstructionOp::ResumeDownload;
		reply.data = { { "download_id", downloadId }, { "resume_from", resumeFrom } };
		if (!conn.sendInstruction(reply)) {
			if (errorOut) {
				*errorOut = "Failed to send resume_download";
			}
			return false;
		}
	} else if (resumeSupported) {
		Instruction reply;
		reply.op = InstructionOp::ResumeDownload;
		reply.data = { { "download_id", downloadId }, { "resume_from", 0 } };
		if (!conn.sendInstruction(reply)) {
			if (errorOut) {
				*errorOut = "Failed to send resume_download";
			}
			return false;
		}
	}

	std::ios::openmode mode = std::ios::binary;
	if (resumeFrom > 0) {
		mode |= std::ios::app;
	} else {
		mode |= std::ios::trunc;
	}
	std::ofstream out(partialPath, mode);
	if (!out.is_open()) {
		if (errorOut) {
			*errorOut = "Failed to open local file for chunked write: " +
					partialPath.generic_string();
		}
		return false;
	}

	int64_t written = resumeFrom;
	int64_t lastStateAt = resumeFrom;
	while (true) {
		auto chunkInstr = conn.readInstruction();
		if (!chunkInstr) {
			out.close();
			if (resumeSupported && !statePath.empty()) {
				saveDownloadState(statePath, written, totalSize, downloadId);
			}
			if (errorOut) {
				*errorOut = "Connection lost during chunked transfer";
			}
			return false;
		}
		if (chunkInstr->op == InstructionOp::ChunkEnd) {
			break;
		}
		if (chunkInstr->op == InstructionOp::Error) {
			out.close();
			if (errorOut) {
				*errorOut = chunkInstr->data.value("text", "Server error during download");
			}
			return false;
		}
		if (chunkInstr->op != InstructionOp::ChunkData) {
			out.close();
			if (errorOut) {
				*errorOut = "Unexpected instruction during chunked transfer";
			}
			return false;
		}
		int64_t chunkOffset = chunkInstr->data.value("offset", static_cast<int64_t>(-1));
		if (chunkOffset >= 0 && chunkOffset != written) {
			out.close();
			if (errorOut) {
				*errorOut = "ChunkData offset mismatch: expected " + std::to_string(written) +
						" got " + std::to_string(chunkOffset);
			}
			return false;
		}
		out.write(reinterpret_cast<const char *>(chunkInstr->payload.data()),
				static_cast<std::streamsize>(chunkInstr->payload.size()));
		if (!out) {
			if (errorOut) {
				*errorOut = "Write failed during chunked transfer";
			}
			return false;
		}
		written += static_cast<int64_t>(chunkInstr->payload.size());

		if (resumeSupported && !statePath.empty() &&
				written - lastStateAt >= segmentBytes) {
			saveDownloadState(statePath, written, totalSize, downloadId);
			lastStateAt = written;
		}
	}
	out.close();
	if (resumeSupported && !statePath.empty() && written != lastStateAt) {
		saveDownloadState(statePath, written, totalSize, downloadId);
	}

	if (resumeSupported && !downloadId.empty() && partialPath != p) {
		if (!isValidDownloadId(downloadId) ||
				!pathUnderRoot(partialPath, std::filesystem::path(workspaceRoot))) {
			if (errorOut) {
				*errorOut = "Refusing to promote unsafe download partial";
			}
			return false;
		}
		std::error_code ec;
		std::filesystem::create_directories(p.parent_path(), ec);
		std::filesystem::rename(partialPath, p, ec);
		if (ec) {
			std::filesystem::copy_file(partialPath, p,
					std::filesystem::copy_options::overwrite_existing, ec);
			if (ec) {
				if (errorOut) {
					*errorOut = "Failed to promote download partial: " + ec.message();
				}
				return false;
			}
			std::filesystem::remove(partialPath, ec);
		}
		if (!statePath.empty()) {
			std::filesystem::remove(statePath, ec);
			std::filesystem::remove(statePath.parent_path(), ec);
		}
	}
	return true;
}

} //namespace sdk_internal
} //namespace coldstorage
