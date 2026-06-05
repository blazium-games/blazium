/**************************************************************************/
/*  gdk_gdk_stubs.h                                                       */
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

#ifndef XBOX_MODULE_GDK_ENABLED

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

#ifdef _WIN32
#undef min
#undef max
#undef ERROR
#undef DELETE
#undef MessageBox
#undef Error
#undef OK
#undef CONNECT_DEFERRED
#undef MemoryBarrier
#undef MONO_FONT
#endif

#include <cstdint>

struct XUserLocalId {
	uint64_t value = 0;
};

using XUserHandle = void *;
using XTaskQueueHandle = void *;
using XblContextHandle = void *;
using XblSocialManagerUserGroupHandle = void *;
using XPackageMountHandle = void *;
using XStoreContextHandle = void *;
using XDisplayTimeoutDeferralHandle = void *;

struct XAsyncBlock {
	void *queue = nullptr;
	void *context = nullptr;
	void *callback = nullptr;
};

struct XTaskQueueRegistrationToken {};

struct XClosedCaptionProperties {};

enum class XHighContrastMode : uint32_t {
	Off = 0,
	Dark = 1,
	Light = 2,
	Other = 3,
};

enum class XGameActivationType : uint32_t {
	Protocol = 0,
	File = 1,
	PendingGameInvite = 2,
	AcceptedGameInvite = 3,
};

struct XGameActivationInfo {};

enum class XAppCaptureMetadataPriority : int32_t {
	Informational = 0,
	Important = 1,
};

enum class XDisplayHdrModeResult : uint32_t {
	Unknown = 0,
	Enabled = 1,
	Disabled = 2,
};

enum class XDisplayHdrModePreference : uint32_t {
	PreferHdr = 0,
	PreferRefreshRate = 1,
};

enum class XErrorOptions : uint32_t {
	OutputDebugStringOnError = 1,
	DebugBreakOnError = 2,
	FailFastOnError = 4,
};

using XErrorCallback = bool (*)(HRESULT, const char *, void *);

enum class XPackageKind : int {
	Game = 0,
	Content = 1,
};

enum class XPackageEnumerationScope : int {
	ThisPublisher = 0,
	ThisAndRelated = 1,
};

enum class XUserAddOptions : uint32_t {
	None = 0,
	AllowGuests = 1,
	AddDefaultUserSilently = 2,
};

enum class XUserChangeEvent : uint32_t {
	SignedInAgain = 0,
	SignedOut = 1,
	Gamertag = 2,
	GamerPicture = 3,
	Privileges = 4,
	TrackingState = 5,
};

struct XblAchievement {};
struct XblSocialManagerUser {};
struct XblSocialManagerPresenceRecord {};
struct XblUserProfile {};
struct XblTitleStorageBlobMetadata {};
struct XblLeaderboardColumn {};
struct XblLeaderboardRow {};
struct XblLeaderboardResult {};
struct XblMultiplayerActivityInfo {};
struct XblUserStatisticsResult {};
struct XblStatisticChangeEventArgs {};

using XblTitleStorageBlobMetadataResultHandle = void *;
using XblPresenceRecordHandle = void *;
struct XblFunctionContext {};

enum class XblPresenceDeviceType : uint32_t {
	Unknown = 0,
};

enum class XblPresenceTitleState : uint32_t {
	Unknown = 0,
};

enum class XblPresenceBroadcastProvider : uint32_t {
	Unknown = 0,
	Twitch = 1,
};

struct XblPresenceTitleRecord {
	uint32_t titleId = 0;
	const char *titleName = nullptr;
	uint64_t lastModified = 0;
	bool titleActive = false;
	const char *richPresenceString = nullptr;
	uint32_t viewState = 0;
	void *broadcastRecord = nullptr;
};

struct XblSocialManagerPresenceTitleRecord {
	uint32_t titleId = 0;
	const char *titleName = nullptr;
	bool isTitleActive = false;
	const char *presenceText = nullptr;
	XblPresenceDeviceType deviceType = XblPresenceDeviceType::Unknown;
	bool isBroadcasting = false;
	bool isPrimary = false;
};

enum class XblMultiplayerActivityJoinRestriction : uint32_t {
	None = 0,
};

enum class XblMultiplayerActivityPlatform : uint32_t {
	Unknown = 0,
};

enum class XblMultiplayerActivityEncounterType : uint32_t {
	None = 0,
};

enum class XblSocialGroupType : uint32_t {
	None = 0,
};

enum class XblLeaderboardQueryType : uint32_t {
	Global = 0,
};

#endif
