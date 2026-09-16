/**************************************************************************/
/*  ctex_trailer.cpp                                                      */
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

#include "io/ctex_trailer.h"

#include "core/io/file_access.h"

#include <cstring>

Error ObfuscationCtexTrailer::append(const String &p_path, const PackedByteArray &p_claim_id, const PackedByteArray &p_path_hmac, const PackedByteArray &p_sig) {
	Ref<FileAccess> f = FileAccess::open(p_path, FileAccess::READ_WRITE);
	if (f.is_null()) {
		return ERR_CANT_OPEN;
	}
	f->seek_end();
	f->store_8('O');
	f->store_8('B');
	f->store_8('C');
	f->store_8('L');
	uint32_t len = (uint32_t)(16 + 32 + p_sig.size());
	f->store_32(len);
	PackedByteArray cid = p_claim_id;
	cid.resize(16);
	PackedByteArray hmac = p_path_hmac;
	hmac.resize(32);
	f->store_buffer(cid.ptr(), 16);
	f->store_buffer(hmac.ptr(), 32);
	f->store_32(p_sig.size());
	if (!p_sig.is_empty()) {
		f->store_buffer(p_sig.ptr(), p_sig.size());
	}
	return OK;
}

bool ObfuscationCtexTrailer::read(const String &p_path, PackedByteArray &r_claim_id, PackedByteArray &r_path_hmac, PackedByteArray &r_sig) {
	Ref<FileAccess> f = FileAccess::open(p_path, FileAccess::READ);
	if (f.is_null()) {
		return false;
	}
	int64_t sz = f->get_length();
	if (sz < 12) {
		return false;
	}
	f->seek(sz - 8);
	// Walk back is unreliable; scan last 4k.
	int64_t start = MAX((int64_t)0, sz - 4096);
	f->seek(start);
	PackedByteArray buf = f->get_buffer(sz - start);
	for (int i = buf.size() - 8; i >= 0; i--) {
		if (buf[i] == 'O' && i + 3 < buf.size() && buf[i + 1] == 'B' && buf[i + 2] == 'C' && buf[i + 3] == 'L') {
			if (i + 8 > buf.size()) {
				continue;
			}
			uint32_t len = buf[i + 4] | ((uint32_t)buf[i + 5] << 8) | ((uint32_t)buf[i + 6] << 16) | ((uint32_t)buf[i + 7] << 24);
			int payload_off = i + 8;
			if (payload_off + 16 + 32 + 4 > buf.size()) {
				continue;
			}
			r_claim_id.resize(16);
			memcpy(r_claim_id.ptrw(), buf.ptr() + payload_off, 16);
			r_path_hmac.resize(32);
			memcpy(r_path_hmac.ptrw(), buf.ptr() + payload_off + 16, 32);
			int sigoff = payload_off + 48;
			uint32_t siglen = buf[sigoff] | ((uint32_t)buf[sigoff + 1] << 8) | ((uint32_t)buf[sigoff + 2] << 16) | ((uint32_t)buf[sigoff + 3] << 24);
			if (sigoff + 4 + (int)siglen > buf.size()) {
				continue;
			}
			r_sig.resize(siglen);
			if (siglen) {
				memcpy(r_sig.ptrw(), buf.ptr() + sigoff + 4, siglen);
			}
			(void)len;
			return true;
		}
	}
	return false;
}
