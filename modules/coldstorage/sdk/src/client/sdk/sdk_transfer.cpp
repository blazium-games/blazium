/**************************************************************************/
/*  sdk_transfer.cpp                                                      */
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
#include "client/sdk/workspace_filters.h"
#include "common/protocol/messages.h"
#include <algorithm>
#include <fstream>
#include <iterator>

namespace coldstorage {

bool ColdStorageClient::respondSendFile(const Instruction &instr) {
	std::string depotPath = instr.data.value("path", "");
	bool chunkedRequested = instr.data.value("chunked", false);
	int64_t resumeOffset = instr.data.value("resume_offset", static_cast<int64_t>(0));
	std::string uploadId = instr.data.value("upload_id", "");
	int64_t segmentBytes = instr.data.value("segment_bytes", static_cast<int64_t>(0));
	std::string localPath = sdk_internal::depotToLocalPath(depotPath, workspaceRoot_);

	static constexpr size_t kClientChunkSize = 1 * 1024 * 1024;

	std::ifstream file(localPath, std::ios::binary);
	if (!file.is_open()) {
		Instruction resp;
		resp.op = InstructionOp::SendFile;
		resp.data = { { "path", depotPath } };
		return conn_->sendInstruction(resp);
	}

	file.seekg(0, std::ios::end);
	auto fileSize = static_cast<size_t>(file.tellg());
	file.seekg(0, std::ios::beg);

	if (resumeOffset < 0 || static_cast<size_t>(resumeOffset) > fileSize) {
		return false;
	}

	const bool useChunked = chunkedRequested ||
			sdk_internal::shouldForceChunkedUpload(fileSize) ||
			resumeOffset > 0 ||
			(segmentBytes > 0 && fileSize > static_cast<size_t>(segmentBytes));
	if (useChunked) {
		if (resumeOffset > 0) {
			file.seekg(static_cast<std::streamoff>(resumeOffset), std::ios::beg);
		}
		Instruction beginInstr;
		beginInstr.op = InstructionOp::ChunkBegin;
		beginInstr.data = { { "path", depotPath },
			{ "total_size", static_cast<int64_t>(fileSize) },
			{ "chunk_size", kClientChunkSize },
			{ "resume_from", resumeOffset } };
		if (!uploadId.empty()) {
			beginInstr.data["upload_id"] = uploadId;
		}
		if (!conn_->sendInstruction(beginInstr)) {
			return false;
		}

		size_t offset = static_cast<size_t>(resumeOffset);
		int chunkNum = 0;
		while (offset < fileSize) {
			size_t remaining = fileSize - offset;
			size_t thisChunk = std::min(remaining, kClientChunkSize);
			Instruction chunkInstr;
			chunkInstr.op = InstructionOp::ChunkData;
			chunkInstr.data = { { "chunk_num", chunkNum }, { "offset", static_cast<int64_t>(offset) } };
			chunkInstr.payload.resize(thisChunk);
			file.read(reinterpret_cast<char *>(chunkInstr.payload.data()),
					static_cast<std::streamsize>(thisChunk));
			auto got = static_cast<size_t>(file.gcount());
			if (got == 0) {
				break;
			}
			chunkInstr.payload.resize(got);
			if (!conn_->sendInstruction(chunkInstr)) {
				return false;
			}
			offset += got;
			chunkNum++;
		}
		Instruction endInstr;
		endInstr.op = InstructionOp::ChunkEnd;
		endInstr.data = { { "path", depotPath },
			{ "total_size", static_cast<int64_t>(offset) },
			{ "chunks", chunkNum } };
		return conn_->sendInstruction(endInstr);
	}

	Instruction resp;
	resp.op = InstructionOp::SendFile;
	resp.data = { { "path", depotPath } };
	resp.payload.assign(std::istreambuf_iterator<char>(file),
			std::istreambuf_iterator<char>());
	if (filters_) {
		resp.payload = filters_->normalizeForSubmit(depotPath, resp.payload);
	}
	return conn_->sendInstruction(resp);
}

bool ColdStorageClient::shelve(const std::string &name, const std::string &description) {
	lastError_.clear();
	auto cmd = makeCommand("shelve", { { "name", name }, { "description", description } });
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

		if (instr->op == InstructionOp::SendFile) {
			if (!respondSendFile(*instr)) {
				lastError_ = "Failed to upload shelf content";
				return false;
			}
		} else if (instr->op == InstructionOp::Release) {
			return instr->data.value("success", false);
		} else if (instr->op == InstructionOp::Error) {
			lastError_ = instr->data.value("text", "shelve failed");
			return false;
		}
	}
}

bool ColdStorageClient::unshelve(const std::string &name, bool keep) {
	lastError_.clear();
	auto cmd = makeCommand("unshelve", { { "name", name }, { "keep", keep } });
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

		if (instr->op == InstructionOp::WriteFile || instr->op == InstructionOp::ChunkBegin) {
			std::string err;
			if (!sdk_internal::writeInstructionToLocalFile(*conn_, *instr, workspaceRoot_, &err,
						maxDownloadBytes_, filters_.get())) {
				lastError_ = err.empty() ? "Failed to write unshelve file" : err;
				return false;
			}
		} else if (instr->op == InstructionOp::Release) {
			return instr->data.value("success", false);
		} else if (instr->op == InstructionOp::Error) {
			lastError_ = instr->data.value("text", "unshelve failed");
			return false;
		}
	}
}

} //namespace coldstorage
