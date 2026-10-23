/**************************************************************************/
/*  inp_writer.h                                                          */
/**************************************************************************/
/*                         This file is part of:                          */
/*                               BLAZIUM                                  */
/*                        https://blazium.app                             */
/**************************************************************************/
/* Copyright (c) 2024-present Blazium Engine contributors.                */
/* SPDX-License-Identifier: MIT                                           */
/**************************************************************************/

#pragma once

#include "core/templates/vector.h"
#include "core/error/error_list.h"

namespace n64_cook {

Error write_inp_blob(Vector<uint8_t> &r_out, int &r_count);

} // namespace n64_cook
