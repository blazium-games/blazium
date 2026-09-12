/**************************************************************************/
/*  messages.cpp                                                          */
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

#include "common/protocol/messages.h"

#include <algorithm>
#include <cstdint>
#include <stdexcept>

namespace coldstorage {

const char *instructionOpToString(InstructionOp op) {
	switch (op) {
		case InstructionOp::WriteFile:
			return "write_file";
		case InstructionOp::DeleteFile:
			return "delete_file";
		case InstructionOp::SendFile:
			return "send_file";
		case InstructionOp::Info:
			return "info";
		case InstructionOp::Error:
			return "error";
		case InstructionOp::Release:
			return "release";
		case InstructionOp::ChunkBegin:
			return "chunk_begin";
		case InstructionOp::ChunkData:
			return "chunk_data";
		case InstructionOp::ChunkEnd:
			return "chunk_end";
		case InstructionOp::ResumeDownload:
			return "resume_download";
	}
	return "unknown";
}

InstructionOp instructionOpFromString(const std::string &str) {
	if (str == "write_file") {
		return InstructionOp::WriteFile;
	}
	if (str == "delete_file") {
		return InstructionOp::DeleteFile;
	}
	if (str == "send_file") {
		return InstructionOp::SendFile;
	}
	if (str == "info") {
		return InstructionOp::Info;
	}
	if (str == "error") {
		return InstructionOp::Error;
	}
	if (str == "release") {
		return InstructionOp::Release;
	}
	if (str == "chunk_begin") {
		return InstructionOp::ChunkBegin;
	}
	if (str == "chunk_data") {
		return InstructionOp::ChunkData;
	}
	if (str == "chunk_end") {
		return InstructionOp::ChunkEnd;
	}
	if (str == "resume_download") {
		return InstructionOp::ResumeDownload;
	}
	throw std::invalid_argument("Unknown InstructionOp: " + str);
}

std::vector<uint8_t> Message::serialize() const {
	nlohmann::json j;
	j["func"] = func;
	j["args"] = args;

	std::string s = j.dump();
	return std::vector<uint8_t>(s.begin(), s.end());
}

std::optional<Message> Message::deserialize(const std::vector<uint8_t> &data) {
	try {
		std::string s(data.begin(), data.end());
		nlohmann::json j = nlohmann::json::parse(s);

		Message msg;
		msg.func = j.at("func").get<std::string>();
		msg.args = j.value("args", nlohmann::json::object());
		return msg;
	} catch (const nlohmann::json::exception &) {
		return std::nullopt;
	}
}

std::vector<uint8_t> Instruction::serialize() const {
	nlohmann::json j;
	j["op"] = instructionOpToString(op);
	j["data"] = data;
	j["payload_size"] = payload.size();

	std::string header = j.dump();

	std::vector<uint8_t> result;
	result.reserve(header.size() + 1 + payload.size());

	result.insert(result.end(), header.begin(), header.end());

	result.push_back(0x00);

	if (!payload.empty()) {
		result.insert(result.end(), payload.begin(), payload.end());
	}

	return result;
}

std::optional<Instruction> Instruction::deserialize(const std::vector<uint8_t> &data) {
	try {
		auto sep = std::find(data.begin(), data.end(), uint8_t{ 0 });
		if (sep == data.end()) {
			return std::nullopt;
		}

		std::string headerStr(data.begin(), sep);
		nlohmann::json j = nlohmann::json::parse(headerStr);

		Instruction instr;
		instr.op = instructionOpFromString(j.at("op").get<std::string>());
		instr.data = j.value("data", nlohmann::json::object());

		size_t payloadSize = j.value("payload_size", static_cast<size_t>(0));
		auto payloadBegin = sep + 1;
		const size_t available = static_cast<size_t>(data.end() - payloadBegin);

		if (payloadSize != available) {
			return std::nullopt;
		}
		if (payloadSize > 0) {
			instr.payload.assign(payloadBegin, payloadBegin + payloadSize);
		}

		return instr;
	} catch (const std::exception &) {
		return std::nullopt;
	}
}

} //namespace coldstorage
