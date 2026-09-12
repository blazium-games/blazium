/**************************************************************************/
/*  sdk_print.cpp                                                         */
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
#include "common/protocol/messages.h"
#include "common/util/sanitize.h"
#include <filesystem>
#include <fstream>

namespace coldstorage {
namespace {

bool parsePrintSpec(const std::string &pathSpec, std::string &depotPath, int64_t &revNum,
		std::string &error) {
	depotPath = sanitize::trim(pathSpec);
	revNum = 0;
	if (depotPath.empty()) {
		error = "Empty print path";
		return false;
	}
	auto hash = depotPath.rfind('#');
	if (hash == std::string::npos) {
		return true;
	}
	std::string revPart = depotPath.substr(hash + 1);
	depotPath = depotPath.substr(0, hash);
	if (depotPath.empty()) {
		error = "Invalid print path";
		return false;
	}
	if (revPart.empty() || revPart == "head") {
		revNum = 0;
		return true;
	}
	try {
		size_t idx = 0;
		long long v = std::stoll(revPart, &idx, 10);
		if (idx != revPart.size() || v <= 0) {
			error = "Invalid revision in print path: #" + revPart;
			return false;
		}
		revNum = static_cast<int64_t>(v);
	} catch (...) {
		error = "Invalid revision in print path: #" + revPart;
		return false;
	}
	return true;
}

} //namespace

std::string ColdStorageClient::print(const std::string &pathSpec) {
	static constexpr size_t kPrintAssembleCap = 32ULL * 1024 * 1024;
	lastError_.clear();
	std::string assembled;
	bool ok = printStreaming(pathSpec, [&](const uint8_t *data, size_t len) {
		if (assembled.size() + len > kPrintAssembleCap) {
			lastError_ = "print() exceeded 32MiB assemble cap; use printToFile or printStreaming";
			return false;
		}
		assembled.append(reinterpret_cast<const char *>(data), len);
		return true;
	});
	if (!ok) {
		if (lastError_.empty()) {
			lastError_ = "print failed";
		}
		return "";
	}
	return assembled;
}

bool ColdStorageClient::printStreaming(const std::string &pathSpec, PrintChunkFn callback) {
	if (!callback) {
		return false;
	}
	lastError_.clear();
	std::string depotPath;
	int64_t revNum = 0;
	std::string parseErr;
	if (!parsePrintSpec(pathSpec, depotPath, revNum, parseErr)) {
		lastError_ = parseErr;
		return false;
	}
	nlohmann::json args = { { "depot_path", depotPath } };
	if (revNum > 0) {
		args["rev_num"] = revNum;
	}
	auto cmd = makeCommand("print", args);
	if (!conn_->sendMessage(cmd)) {
		lastError_ = "Send failed";
		return false;
	}

	while (true) {
		auto instr = conn_->readInstruction();
		if (!instr) {
			lastError_ = "Connection lost";
			return false;
		}

		if (instr->op == InstructionOp::WriteFile) {
			if (!instr->payload.empty()) {
				if (!callback(instr->payload.data(), instr->payload.size())) {
					return false;
				}
			}
		} else if (instr->op == InstructionOp::ChunkBegin) {
			const bool resumeSupported = instr->data.value("resume_supported", false);
			if (resumeSupported) {
				Instruction reply;
				reply.op = InstructionOp::ResumeDownload;
				reply.data = {
					{ "download_id", instr->data.value("download_id", "") },
					{ "resume_from", 0 },
				};
				if (!conn_->sendInstruction(reply)) {
					return false;
				}
			}
			while (true) {
				auto chunk = conn_->readInstruction();
				if (!chunk) {
					return false;
				}
				if (chunk->op == InstructionOp::ChunkEnd) {
					break;
				}
				if (chunk->op != InstructionOp::ChunkData) {
					return false;
				}
				if (!chunk->payload.empty()) {
					if (!callback(chunk->payload.data(), chunk->payload.size())) {
						return false;
					}
				}
			}
		} else if (instr->op == InstructionOp::Release) {
			bool ok = instr->data.value("success", false);
			if (!ok && lastError_.empty()) {
				lastError_ = "print failed";
			}
			return ok;
		} else if (instr->op == InstructionOp::Error) {
			lastError_ = instr->data.value("text", "print failed");
			return false;
		}
	}
}

bool ColdStorageClient::printToFile(const std::string &pathSpec, const std::string &filePath) {
	std::filesystem::path p(filePath);
	if (p.has_parent_path()) {
		std::filesystem::create_directories(p.parent_path());
	}
	std::ofstream out(filePath, std::ios::binary | std::ios::trunc);
	if (!out.is_open()) {
		return false;
	}
	return printStreaming(pathSpec, [&](const uint8_t *data, size_t len) {
		out.write(reinterpret_cast<const char *>(data), static_cast<std::streamsize>(len));
		return static_cast<bool>(out);
	});
}

bool ColdStorageClient::printAtRev(const std::string &depotPath, int64_t revNum, PrintChunkFn callback) {
	if (!callback || revNum <= 0) {
		return false;
	}
	lastError_.clear();
	auto cmd = makeCommand("print", { { "depot_path", depotPath }, { "rev_num", revNum } });
	if (!conn_->sendMessage(cmd)) {
		lastError_ = "Send failed";
		return false;
	}

	while (true) {
		auto instr = conn_->readInstruction();
		if (!instr) {
			lastError_ = "Connection lost";
			return false;
		}

		if (instr->op == InstructionOp::WriteFile) {
			if (!instr->payload.empty()) {
				if (!callback(instr->payload.data(), instr->payload.size())) {
					return false;
				}
			}
		} else if (instr->op == InstructionOp::ChunkBegin) {
			const bool resumeSupported = instr->data.value("resume_supported", false);
			if (resumeSupported) {
				Instruction reply;
				reply.op = InstructionOp::ResumeDownload;
				reply.data = {
					{ "download_id", instr->data.value("download_id", "") },
					{ "resume_from", 0 },
				};
				if (!conn_->sendInstruction(reply)) {
					return false;
				}
			}
			while (true) {
				auto chunk = conn_->readInstruction();
				if (!chunk) {
					return false;
				}
				if (chunk->op == InstructionOp::ChunkEnd) {
					break;
				}
				if (chunk->op != InstructionOp::ChunkData) {
					return false;
				}
				if (!chunk->payload.empty()) {
					if (!callback(chunk->payload.data(), chunk->payload.size())) {
						return false;
					}
				}
			}
		} else if (instr->op == InstructionOp::Release) {
			bool ok = instr->data.value("success", false);
			if (!ok && lastError_.empty()) {
				lastError_ = "print failed";
			}
			return ok;
		} else if (instr->op == InstructionOp::Error) {
			lastError_ = instr->data.value("text", "print failed");
			return false;
		}
	}
}

} //namespace coldstorage
