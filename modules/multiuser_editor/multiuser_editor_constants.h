/**************************************************************************/
/*  multiuser_editor_constants.h                                          */
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

#ifdef TOOLS_ENABLED

#include <cstdint>

namespace multiuser_editor {

static constexpr const char *kRoleViewer = "Viewer";
static constexpr const char *kRoleEditor = "Editor";
static constexpr const char *kRoleAdmin = "Admin";
static constexpr const char *kRoleAny = "@any";

static constexpr const char *kActionHandshake = "handshake";
static constexpr const char *kActionAuthChallenge = "auth_challenge";
static constexpr const char *kActionChat = "chat";
static constexpr const char *kActionCursorUpdate = "cursor_update";
static constexpr const char *kActionSelect = "select";
static constexpr const char *kActionTelemetry = "telemetry";
static constexpr const char *kActionFsSnapshotDone = "fs_snapshot_done";
static constexpr const char *kActionFileReject = "file_reject";
static constexpr const char *kActionProjectSettingsSnapshot = "project_settings_snapshot";
static constexpr const char *kActionProperty = "property";
static constexpr const char *kActionNodeAdd = "node_add";
static constexpr const char *kActionNodeDelete = "node_delete";
static constexpr const char *kActionCrdt = "crdt";
static constexpr const char *kActionCrdtSync = "crdt_sync";
static constexpr const char *kActionScriptAttach = "script_attach";
static constexpr const char *kActionScriptDetach = "script_detach";
static constexpr const char *kActionFilePropose = "file_propose";
static constexpr const char *kActionFileApply = "file_apply";
static constexpr const char *kActionResourceSync = "resource_sync";
static constexpr const char *kActionTileSync = "tile_sync";
static constexpr const char *kActionVfxRestart = "vfx_restart";
static constexpr const char *kActionShaderAction = "shader_action";
static constexpr const char *kActionUnlockAll = "unlock_all";
static constexpr const char *kActionMagicRepairRequest = "magic_repair_request";
static constexpr const char *kActionMagicRepairStart = "magic_repair_start";
static constexpr const char *kActionGitRequest = "git_request";
static constexpr const char *kActionGitResponse = "git_response";
static constexpr const char *kActionProjectSetting = "project_setting";
static constexpr const char *kActionSceneSync = "scene_sync";
static constexpr const char *kActionFsOp = "fs_op";
static constexpr const char *kActionFsMove = "fs_move";
static constexpr const char *kActionFsRemove = "fs_remove";
static constexpr const char *kActionFsRefresh = "fs_refresh";
static constexpr const char *kActionTeamPlayStart = "team_play_start";
static constexpr const char *kActionTeamPlayStop = "team_play_stop";
static constexpr const char *kActionAutoworkTrigger = "autowork_trigger";
static constexpr const char *kActionGlobalUndo = "global_undo";

static constexpr const char *kActionHandshakeAck = "handshake_ack";
static constexpr const char *kActionFileProposeBegin = "file_propose_begin";
static constexpr const char *kActionFileProposeChunk = "file_propose_chunk";
static constexpr const char *kActionFileProposeEnd = "file_propose_end";
static constexpr const char *kActionFileProposeDelete = "file_propose_delete";
static constexpr const char *kActionFileProposeMove = "file_propose_move";
static constexpr const char *kActionFileApplyBegin = "file_apply_begin";
static constexpr const char *kActionFileApplyChunk = "file_apply_chunk";
static constexpr const char *kActionFileApplyEnd = "file_apply_end";
static constexpr const char *kActionFileApplyDelete = "file_apply_delete";
static constexpr const char *kActionFileApplyMove = "file_apply_move";

static constexpr const char *kDefaultAccessListPath = "res://.multiuser_access_list.json";

static constexpr int kPortMin = 1024;
static constexpr int kPortMax = 65535;
static constexpr int kDefaultPort = 7654;

// Validator caps (multiuser_editor_action_interceptor.cpp)
static constexpr int kPathLengthMax = 1024;
static constexpr int kPropertyNameMax = 256;
static constexpr int kBranchNameMax = 200;
static constexpr int kRemoteNameMax = 64;
static constexpr int kNodeNameMax = 128;
static constexpr int kCommitMessageMax = 4096;
static constexpr int64_t kRemoteValueDefaultByteCap = 4 * 1024 * 1024;
static constexpr int kRemoteStringMaxChars = 1024 * 1024;
static constexpr int kPackedArrayMaxElems = 1000000;
static constexpr int kArrayMaxLen = 65536;
static constexpr int kDictionaryMaxKeys = 65536;

// Packet sanity (multiuser_editor_plugin.cpp _packet_is_sane / _is_safe_simple_value)
static constexpr int kPacketTopLevelKeysMax = 32;
static constexpr int kPacketTypeStringMax = 64;
static constexpr int kPacketRecursionDepthMax = 8;
static constexpr int kSimpleValueStringMax = 1 * 1024 * 1024;
static constexpr int kSimpleValuePackedByteMax = 4 * 1024 * 1024;
static constexpr int kSimpleValuePackedElemMax = 1024 * 1024;
static constexpr int kSimpleValueDictMaxKeys = 256;
static constexpr int kSimpleValueArrayMaxLen = 4096;

// Auth/JWT/peer fields
static constexpr int kChatMessageMax = 4096;
static constexpr int kJWTLengthMin = 8;
static constexpr int kJWTLengthMax = 4096;
static constexpr int kJWTJTIMax = 256;
static constexpr int kJTIFingerprintHexLength = 8;
static constexpr int kSHA256HexLength = 64;
static constexpr int kPeerIdMax = 64;
static constexpr int kRoleFieldMax = 32;
// Telemetry packets historically allow a slightly looser inner-role cap than the strict on-wire role field;
// kept as a separate symbol so tightening does not silently change protocol behavior.
static constexpr int kRoleFieldTelemetryMax = 64;
static constexpr int kURIHostMax = 253;

// CRDT (multiuser_editor_crdt_text_buffer.cpp/.h)
static constexpr int kCRDTSiteIDMax = 64;
static constexpr uint64_t kCRDTClockMax = (uint64_t)1 << 48;
static constexpr int kCRDTPositionFractionMax = 32767;

// Lock manager defaults
static constexpr int kLockManagerDefaultLockTTLMsec = 30000;

// Filesystem sync
static constexpr int kFilesystemSyncRecentApplyMax = 200;
static constexpr int kFilesystemSyncTransferIdHexWidth = 16;
static constexpr int kFilesystemSyncTotalChunksMax = 1000000;
static constexpr int kFilesystemSyncHashReadChunk = 65536;

// Access list (mirror of MultiuserEditorAccessList::MAX_* enum)
static constexpr int kAccessListCodenameMax = 64;
static constexpr int kAccessListPasswordMax = 1024;
static constexpr int kAccessListFileMax = 1024 * 1024;
static constexpr int kAccessListGitignoreMax = 1024 * 1024;
static constexpr int kAccessListMaxEntriesCeiling = 4096;

// Network policy ceilings
static constexpr int kNetworkMaxClientsCeiling = 256;
static constexpr int kNetworkPacketsPerPollCeiling = 100000;

// Git operation defaults
static constexpr int kGitOutputDefaultMax = 8192;

// MAX(...) floors used when reading per-policy chunk-size editor settings.
static constexpr int kFilesystemSyncChunkFloor = 8192;
static constexpr int kScriptSyncFloor = 1024;

} // namespace multiuser_editor

#endif
