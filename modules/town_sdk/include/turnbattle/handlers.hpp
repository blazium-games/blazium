/**************************************************************************/
/*  handlers.hpp                                                          */
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
#include "core/variant/variant.h"
#include "types.hpp"
#include <functional>

namespace turnbattle {

using VariantCallback = std::function<void(const Variant &)>;

using OnSnapshotCallback = VariantCallback;
using OnMoveStateCallback = VariantCallback;
using OnBattleStartCallback = VariantCallback;
using OnBattleStateCallback = VariantCallback;
using OnBattleLogCallback = std::function<void(const std::string &log)>;
using OnBattleEndCallback = VariantCallback;
using OnBattleIndicatorSpawnCallback = VariantCallback;
using OnBattleIndicatorDespawnCallback = VariantCallback;
using OnErrorCallback = std::function<void(const std::string &error)>;
using OnDisconnectCallback = std::function<void(const std::string &reason)>;

// Reconnection callbacks
using OnReconnectingCallback = std::function<void(int attempt, float next_delay)>;
using OnReconnectedCallback = VariantCallback;
using OnReconnectFailedCallback = std::function<void(const std::string &reason)>;

// Admin callbacks
using OnAdminReloadCallback = VariantCallback;
using OnAdminKickCallback = VariantCallback;
using OnAdminStatsCallback = VariantCallback;
using OnAdminBroadcastCallback = VariantCallback;

} // namespace turnbattle
