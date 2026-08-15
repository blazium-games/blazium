/**************************************************************************/
/*  steam_types.h                                                         */
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

#include "core/typedefs.h"

// Minimal Steamworks types used by the module at runtime.
// Kept local so CI/builds succeed without the official Steam SDK headers.

enum SteamEResult {
	STEAM_RESULT_OK = 1,
	STEAM_RESULT_PENDING = 22,
	STEAM_RESULT_EXPIRED = 16,
};

enum {
	STEAM_USER_CALLBACKS_BASE = 100,
	STEAM_GET_TICKET_FOR_WEB_API_RESPONSE_CALLBACK = STEAM_USER_CALLBACKS_BASE + 68,
	STEAM_USER_STATS_CALLBACKS_BASE = 1100,
	STEAM_USER_STATS_RECEIVED_CALLBACK = STEAM_USER_STATS_CALLBACKS_BASE + 1,
	STEAM_USER_STATS_STORED_CALLBACK = STEAM_USER_STATS_CALLBACKS_BASE + 2,
	STEAM_UTILS_CALLBACKS_BASE = 700,
	STEAM_API_CALL_COMPLETED_CALLBACK = STEAM_UTILS_CALLBACKS_BASE + 3,
	STEAM_INVENTORY_CALLBACKS_BASE = 4700,
	STEAM_INVENTORY_RESULT_READY_CALLBACK = STEAM_INVENTORY_CALLBACKS_BASE + 0,
	STEAM_INVENTORY_FULL_UPDATE_CALLBACK = STEAM_INVENTORY_CALLBACKS_BASE + 1,
	STEAM_INVENTORY_DEFINITION_UPDATE_CALLBACK = STEAM_INVENTORY_CALLBACKS_BASE + 2,
};

typedef uint64_t SteamItemInstanceID_t;
typedef int32_t SteamItemDef_t;
typedef int32_t SteamInventoryResult_t;
typedef uint64_t SteamInventoryUpdateHandle_t;

static constexpr SteamItemInstanceID_t STEAM_ITEM_INSTANCE_ID_INVALID = ~(SteamItemInstanceID_t)0;
static constexpr SteamInventoryResult_t STEAM_INVENTORY_RESULT_INVALID = -1;
static constexpr SteamInventoryUpdateHandle_t STEAM_INVENTORY_UPDATE_HANDLE_INVALID = 0xffffffffffffffffULL;

enum SteamItemFlags {
	STEAM_ITEM_NO_TRADE = 1 << 0,
	STEAM_ITEM_REMOVED = 1 << 8,
	STEAM_ITEM_CONSUMED = 1 << 9,
};

struct SteamItemDetails {
	SteamItemInstanceID_t item_id = STEAM_ITEM_INSTANCE_ID_INVALID;
	SteamItemDef_t definition = 0;
	uint16_t quantity = 0;
	uint16_t flags = 0;
};

typedef uint32_t SteamHAuthTicket;
typedef int32_t SteamHSteamUser;
typedef int32_t SteamHSteamPipe;
typedef uint64_t SteamCSteamID;

#pragma pack(push, 8)

struct SteamCallbackMsg {
	SteamHSteamUser m_hSteamUser = 0;
	int m_iCallback = 0;
	uint8_t *m_pubParam = nullptr;
	int m_cubParam = 0;
};

struct SteamAPICallCompleted {
	enum { k_iCallback = STEAM_API_CALL_COMPLETED_CALLBACK };

	uint64_t m_hAsyncCall = 0;
	int m_iCallback = 0;
	uint32_t m_cubParam = 0;
};

struct SteamGetTicketForWebApiResponse {
	enum {
		k_iCallback = STEAM_GET_TICKET_FOR_WEB_API_RESPONSE_CALLBACK,
		k_nCubTicketMaxLength = 2560,
	};

	SteamHAuthTicket m_hAuthTicket = 0;
	int m_eResult = 0;
	int m_cubTicket = 0;
	uint8_t m_rgubTicket[k_nCubTicketMaxLength];
};

struct SteamUserStatsReceived {
	enum { k_iCallback = STEAM_USER_STATS_RECEIVED_CALLBACK };

	uint64_t m_nGameID = 0;
	int m_eResult = 0;
	SteamCSteamID m_steamIDUser = 0;
};

struct SteamUserStatsStored {
	enum { k_iCallback = STEAM_USER_STATS_STORED_CALLBACK };

	uint64_t m_nGameID = 0;
	int m_eResult = 0;
};

struct SteamInventoryResultReady {
	enum { k_iCallback = STEAM_INVENTORY_RESULT_READY_CALLBACK };

	SteamInventoryResult_t m_handle = STEAM_INVENTORY_RESULT_INVALID;
	int m_result = 0;
};

struct SteamInventoryFullUpdate {
	enum { k_iCallback = STEAM_INVENTORY_FULL_UPDATE_CALLBACK };

	SteamInventoryResult_t m_handle = STEAM_INVENTORY_RESULT_INVALID;
};

struct SteamInventoryDefinitionUpdate {
	enum { k_iCallback = STEAM_INVENTORY_DEFINITION_UPDATE_CALLBACK };
};

#pragma pack(pop)
