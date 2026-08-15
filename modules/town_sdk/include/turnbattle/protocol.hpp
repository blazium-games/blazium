/**************************************************************************/
/*  protocol.hpp                                                          */
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

#include <cstdint>
#include <string>
#include <utility>

namespace turnbattle {
namespace protocol {

// Message type constants (must match server)
namespace MessageType {
inline constexpr uint16_t VERSION_CHECK = 0;
inline constexpr uint16_t HELLO = 1;
inline constexpr uint16_t HELLO_ACK = 2;
inline constexpr uint16_t DISCONNECT = 3;
inline constexpr uint16_t VERSION_MISMATCH = 4;

inline constexpr uint16_t REGION_ENTER = 10;
inline constexpr uint16_t REGION_LEAVE = 11;
inline constexpr uint16_t MOVE_INPUT = 12;

inline constexpr uint16_t REGION_SNAPSHOT = 20;
inline constexpr uint16_t MOVE_STATE = 23;

inline constexpr uint16_t BATTLE_INDICATOR_SPAWN = 30;
inline constexpr uint16_t BATTLE_INDICATOR_DESPAWN = 31;
inline constexpr uint16_t BATTLE_START = 34;
inline constexpr uint16_t BATTLE_STATE = 35;
inline constexpr uint16_t BATTLE_ACTION = 36;
inline constexpr uint16_t BATTLE_RESULT = 37;
inline constexpr uint16_t BATTLE_END = 38;
inline constexpr uint16_t BATTLE_LOG = 39;
inline constexpr uint16_t BATTLE_LEAVE = 33;

inline constexpr uint16_t ADMIN_RELOAD = 100;
inline constexpr uint16_t RELOAD_OK = 101;
inline constexpr uint16_t ADMIN_KICK = 102;
inline constexpr uint16_t ADMIN_STATS_REQUEST = 103;
inline constexpr uint16_t ADMIN_STATS_RESPONSE = 104;
inline constexpr uint16_t ADMIN_BROADCAST = 105;

inline constexpr uint16_t ERROR_MSG = 200;
} //namespace MessageType

// Channel constants
namespace Channel {
inline constexpr uint8_t CONTROL = 0;
inline constexpr uint8_t REGION = 1;
inline constexpr uint8_t BATTLE = 2;
inline constexpr uint8_t NOTIFICATIONS = 3;
} //namespace Channel

// Encode message: [type:uint16][len:uint16] + payload
std::string encode_message(uint16_t type, const std::string &payload);

// Decode message header (returns {type, payload_length})
std::pair<uint16_t, uint16_t> decode_header(const char *data, size_t len);

} // namespace protocol
} // namespace turnbattle
