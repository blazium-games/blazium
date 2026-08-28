/**************************************************************************/
/*  fmv_play.cpp                                                          */
/**************************************************************************/
/*                         This file is part of:                          */
/*                               BLAZIUM                                  */
/*                        https://blazium.app                             */
/**************************************************************************/

#include "fmv_play.h"

#include <psxgpu.h>
#include <psxpad.h>
#include <psxpress.h>

void fmv_play_embedded(const uint8_t *str, size_t str_size, int screen_w, int screen_h, const uint8_t *pad34) {
	DecDCTReset(0);
	if (str_size < 12 || str[0] != 'F' || str[1] != 'S' || str[2] != 'T' || str[3] != 'R') {
		return;
	}
	const uint16_t frames = uint16_t(str[4] | (str[5] << 8));
	const uint16_t fw = uint16_t(str[6] | (str[7] << 8));
	const uint16_t fh = uint16_t(str[8] | (str[9] << 8));
	const uint8_t *p = str + 12;
	const uint8_t *end = str + str_size;
	for (uint16_t f = 0; f < frames; f++) {
		if (p + 4 > end) {
			break;
		}
		const uint32_t nbytes = uint32_t(p[0] | (p[1] << 8) | (p[2] << 16) | (p[3] << 24));
		p += 4;
		if (p + nbytes > end) {
			break;
		}
		RECT dst{ short((screen_w - int(fw)) / 2), short((screen_h - int(fh)) / 2), short(fw), short(fh) };
		if (dst.x < 0) {
			dst.x = 0;
		}
		if (dst.y < 0) {
			dst.y = 0;
		}
		LoadImage(&dst, (const uint32_t *)p);
		DrawSync(0);
		VSync(0);
		p += nbytes;
		if (pad34) {
			const PADTYPE *pad = (const PADTYPE *)pad34;
			if (pad->stat == 0 && !(pad->btn & PAD_START)) {
				break;
			}
		}
	}
}
