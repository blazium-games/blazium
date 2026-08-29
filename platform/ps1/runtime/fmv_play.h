/**************************************************************************/
/*  fmv_play.h                                                            */
/**************************************************************************/
/*                         This file is part of:                          */
/*                               BLAZIUM                                  */
/*                        https://blazium.app                             */
/**************************************************************************/

#pragma once

#include <cstddef>
#include <cstdint>

void fmv_play_embedded(const uint8_t *str, size_t str_size, int screen_w, int screen_h, const uint8_t *pad34, const uint8_t *xa, size_t xa_size);
