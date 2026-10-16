/**************************************************************************/
/*  ntex_writer.h                                                         */
/**************************************************************************/
/*                         This file is part of:                          */
/*                               BLAZIUM                                  */
/*                        https://blazium.app                             */
/**************************************************************************/
/* Copyright (c) 2024-present Blazium Engine contributors.                */
/* SPDX-License-Identifier: MIT                                           */
/**************************************************************************/

#pragma once

#include "core/error/error_list.h"
#include "core/io/image.h"
#include "core/templates/vector.h"

namespace n64_cook {

Error write_ntex_blob(const Ref<Image> &p_image, Vector<uint8_t> &r_out);

} // namespace n64_cook
