/**************************************************************************/
/*  xa_writer.cpp                                                         */
/**************************************************************************/
/*                         This file is part of:                          */
/*                               BLAZIUM                                  */
/*                        https://blazium.app                             */
/**************************************************************************/

#include "xa_writer.h"

namespace ps1_cook {

Error write_xa_pcm(const Vector<int16_t> &p_pcm, int p_sample_rate, XaBlob &r_out) {
	if (p_pcm.is_empty()) {
		return ERR_INVALID_PARAMETER;
	}
	const int rate = p_sample_rate > 0 ? p_sample_rate : 37800;
	const int samples_per_sector = 18 * 28;
	const int sectors = MAX(1, (p_pcm.size() + samples_per_sector - 1) / samples_per_sector);
	Vector<uint8_t> out;
	int src = 0;
	for (int s = 0; s < sectors; s++) {
		out.push_back(0x01);
		out.push_back(0x00);
		out.push_back(uint8_t(s & 0xff));
		out.push_back(uint8_t(s >> 8));
		out.push_back(uint8_t(rate & 0xff));
		out.push_back(uint8_t((rate >> 8) & 0xff));
		out.push_back(0);
		out.push_back(s == sectors - 1 ? 0x80 : 0x00);
		for (int g = 0; g < 18; g++) {
			int16_t smp[28];
			int max_abs = 1;
			for (int i = 0; i < 28; i++) {
				smp[i] = src < p_pcm.size() ? p_pcm[src++] : 0;
				const int a = smp[i] < 0 ? -int(smp[i]) : int(smp[i]);
				if (a > max_abs) {
					max_abs = a;
				}
			}
			int shift = 12;
			while (shift > 0 && (max_abs >> (12 - shift)) > 7) {
				shift--;
			}
			out.push_back(uint8_t(shift & 0x0f));
			out.push_back(0x00);
			for (int i = 0; i < 14; i++) {
				const int a = CLAMP(int(smp[i * 2]) >> shift, -8, 7) & 0x0f;
				const int c = CLAMP(int(smp[i * 2 + 1]) >> shift, -8, 7) & 0x0f;
				out.push_back(uint8_t(a | (c << 4)));
			}
		}
		while ((out.size() % 2336) != 0) {
			out.push_back(0);
		}
	}
	r_out.bytes = out;
	r_out.sample_rate = rate;
	return OK;
}

} // namespace ps1_cook
