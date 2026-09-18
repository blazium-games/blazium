/**************************************************************************/
/*  register_types.h                                                      */
/**************************************************************************/
/*                         This file is part of:                          */
/*                             BLAZIUM ENGINE                             */
/*                          https://blazium.app                           */
/**************************************************************************/

#pragma once

#include "modules/register_module_types.h"

void initialize_anticheat_module(ModuleInitializationLevel p_level);
void uninitialize_anticheat_module(ModuleInitializationLevel p_level);
void anticheat_ensure_frame_hook();
