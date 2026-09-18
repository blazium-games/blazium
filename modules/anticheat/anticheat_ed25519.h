/**************************************************************************/
/*  anticheat_ed25519.h                                                   */
/**************************************************************************/

#pragma once

#include "core/string/ustring.h"

#include <cstddef>
#include <cstdint>

bool anticheat_parse_runtime_sig(const String &p_body, String &r_sha256_hex, String &r_sig_b64);
int anticheat_b64_decode(const String &p_s, uint8_t *p_out, int p_cap);
int anticheat_hex_decode(const String &p_s, uint8_t *p_out, int p_want);
bool anticheat_ed25519_verify(const uint8_t p_pk[32], const uint8_t *p_msg, size_t p_msg_len, const uint8_t p_sig[64]);
