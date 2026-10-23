/**************************************************************************/
/*  fmv_play.cpp                                                          */
/**************************************************************************/
/*                         This file is part of:                          */
/*                               BLAZIUM                                  */
/*                        https://blazium.app                             */
/**************************************************************************/

#include "fmv_play.h"

#include <psxcd.h>
#include <psxgpu.h>
#include <psxpad.h>
#include <psxpress.h>
#include <psxspu.h>

static uint16_t ru16(const uint8_t *p) {
	return uint16_t(p[0] | (p[1] << 8));
}

static void play_xa(const uint8_t *xa, size_t xa_size) {
	if (!xa || xa_size < 24) {
		return;
	}
	uint8_t adpcm[4096];
	size_t out = 0;
	uint16_t rate = 37800;
	for (size_t off = 0; off + 2336 <= xa_size && out + 288 <= sizeof(adpcm); off += 2336) {
		if (off == 0) {
			rate = ru16(xa + 4);
		}
		const uint8_t *g = xa + off + 8;
		for (int i = 0; i < 18 && out + 16 <= sizeof(adpcm); i++) {
			for (int b = 0; b < 16; b++) {
				adpcm[out++] = g[i * 16 + b];
			}
		}
	}
	if (out < 16) {
		return;
	}
	adpcm[1] |= 0x01;
	SpuInit();
	SpuSetCommonMasterVolume(0x3fff, 0x3fff);
	uint32_t xfer = uint32_t((out + 63) & ~size_t(63));
	if (xfer > sizeof(adpcm)) {
		xfer = sizeof(adpcm);
	}
	const uint32_t addr = 0x2010;
	SpuSetTransferMode(SPU_TRANSFER_BY_DMA);
	SpuSetTransferStartAddr(addr);
	SpuWrite((const uint32_t *)adpcm, xfer);
	SpuIsTransferCompleted(SPU_TRANSFER_WAIT);
	SpuSetKey(0, 1 << 1);
	SpuSetVoiceVolume(1, 0x3fff, 0x3fff);
	SpuSetVoicePitch(1, getSPUSampleRate(int(rate ? rate : 37800)));
	SpuSetVoiceStartAddr(1, addr);
	SPU_CH_ADSR1(1) = 0x00ff;
	SPU_CH_ADSR2(1) = 0x0000;
	SpuSetKey(1, 1 << 1);
}

void fmv_play_embedded(const uint8_t *str, size_t str_size, int screen_w, int screen_h, const uint8_t *pad34, const uint8_t *xa, size_t xa_size) {
	DecDCTReset(0);
	if (!str || str_size < 16 || ru16(str) != 0x0160) {
		return;
	}
	play_xa(xa, xa_size);

	static uint8_t frame_bytes[32768];
	static uint32_t pixels[160 * 128 / 2];
	size_t assembled = 0;
	int fw = 160;
	int fh = 128;
	const uint8_t *p = str;
	const uint8_t *end = str + str_size;
	while (p + 2048 <= end) {
		if (ru16(p) != 0x0160) {
			break;
		}
		const uint16_t chunk = ru16(p + 2);
		const uint16_t chunks = ru16(p + 4);
		fw = int(ru16(p + 8));
		fh = int(ru16(p + 10));
		if (fw <= 0 || fh <= 0) {
			fw = 160;
			fh = 128;
		}
		if (chunk == 0) {
			assembled = 0;
		}
		const int n = 2032;
		if (assembled + size_t(n) > sizeof(frame_bytes)) {
			break;
		}
		for (int i = 0; i < n; i++) {
			frame_bytes[assembled + i] = p[16 + i];
		}
		assembled += size_t(n);
		p += 2048;
		if (chunk + 1 < chunks) {
			continue;
		}
		DecDCTReset(0);
		DecDCTin((const uint32_t *)frame_bytes, DECDCT_MODE_16BPP);
		const int words = (fw * fh) / 2;
		if (words > int(sizeof(pixels) / sizeof(pixels[0]))) {
			break;
		}
		DecDCTout(pixels, size_t(words));
		DecDCToutSync(0);
		RECT dst{ short((screen_w - fw) / 2), short((screen_h - fh) / 2), short(fw), short(fh) };
		if (dst.x < 0) {
			dst.x = 0;
		}
		if (dst.y < 0) {
			dst.y = 0;
		}
		LoadImage(&dst, pixels);
		DrawSync(0);
		VSync(0);
		if (pad34) {
			const PADTYPE *pad = (const PADTYPE *)pad34;
			if (pad->stat == 0 && !(pad->btn & PAD_START)) {
				break;
			}
		}
	}
}

int fmv_play_cd(const char *iso_name, int screen_w, int screen_h, const uint8_t *pad34) {
	if (!iso_name || !iso_name[0]) {
		return 0;
	}
	if (!CdInit()) {
		return 0;
	}
	char path[32];
	path[0] = '\\';
	int i = 0;
	while (iso_name[i] && i < 20) {
		path[1 + i] = iso_name[i];
		i++;
	}
	path[1 + i] = ';';
	path[2 + i] = '1';
	path[3 + i] = 0;
	CdlFILE fp;
	if (!CdSearchFile(&fp, path)) {
		return 0;
	}
	DecDCTReset(0);
	static uint8_t frame_bytes[32768];
	static uint32_t pixels[160 * 128 / 2];
	uint8_t sector[2048];
	size_t assembled = 0;
	int fw = 160;
	int fh = 128;
	uint32_t left = fp.size;
	CdControl(CdlSetloc, (uint8_t *)&fp.pos, 0);
	while (left >= 2048) {
		if (CdRead(1, (uint32_t *)sector, CdlModeSpeed) <= 0) {
			break;
		}
		if (CdReadSync(0, 0) < 0) {
			break;
		}
		left -= 2048;
		if (ru16(sector) != 0x0160) {
			break;
		}
		const uint16_t chunk = ru16(sector + 2);
		const uint16_t chunks = ru16(sector + 4);
		fw = int(ru16(sector + 8));
		fh = int(ru16(sector + 10));
		if (fw <= 0 || fh <= 0) {
			fw = 160;
			fh = 128;
		}
		if (chunk == 0) {
			assembled = 0;
		}
		const int n = 2032;
		if (assembled + size_t(n) > sizeof(frame_bytes)) {
			break;
		}
		for (int k = 0; k < n; k++) {
			frame_bytes[assembled + k] = sector[16 + k];
		}
		assembled += size_t(n);
		if (chunk + 1 < chunks) {
			continue;
		}
		DecDCTReset(0);
		DecDCTin((const uint32_t *)frame_bytes, DECDCT_MODE_16BPP);
		const int words = (fw * fh) / 2;
		if (words > int(sizeof(pixels) / sizeof(pixels[0]))) {
			break;
		}
		DecDCTout(pixels, size_t(words));
		DecDCToutSync(0);
		RECT dst{ short((screen_w - fw) / 2), short((screen_h - fh) / 2), short(fw), short(fh) };
		if (dst.x < 0) {
			dst.x = 0;
		}
		if (dst.y < 0) {
			dst.y = 0;
		}
		LoadImage(&dst, pixels);
		DrawSync(0);
		VSync(0);
		if (pad34) {
			const PADTYPE *pad = (const PADTYPE *)pad34;
			if (pad->stat == 0 && !(pad->btn & PAD_START)) {
				break;
			}
		}
	}
	CdControl(CdlPause, 0, 0);
	return 1;
}
