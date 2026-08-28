/**************************************************************************/
/*  script_vm.cpp                                                         */
/**************************************************************************/
/*                         This file is part of:                          */
/*                               BLAZIUM                                  */
/*                        https://blazium.app                             */
/**************************************************************************/

#include "script_vm.h"

static const uint8_t *g_gdbc = nullptr;
static size_t g_gdbc_size = 0;
static const uint8_t *g_luau = nullptr;
static size_t g_luau_size = 0;
static int g_ready = 0;

enum {
	kOpReturn = 0,
	kOpLoadConstF = 1,
	kOpLoadArg = 2,
	kOpMul = 3,
	kOpAdd = 4,
	kOpCallSelf = 5
};

static uint16_t ru16(const uint8_t *p) {
	return uint16_t(p[0] | (p[1] << 8));
}

static float rf32(const uint8_t *p) {
	union {
		float f;
		uint32_t u;
	} x;
	x.u = uint32_t(p[0]) | (uint32_t(p[1]) << 8) | (uint32_t(p[2]) << 16) | (uint32_t(p[3]) << 24);
	return x.f;
}

void script_vm_init(const uint8_t *gdbc, size_t gdbc_size, const uint8_t *luau, size_t luau_size) {
	g_gdbc = gdbc;
	g_gdbc_size = gdbc_size;
	g_luau = luau;
	g_luau_size = luau_size;
	g_ready = 1;
}

static int run_gdbc(const uint8_t *blob, size_t size, float delta, const ScriptVMHost *host) {
	if (size < 8 || blob[0] != 'G' || blob[1] != 'D' || blob[2] != 'B' || blob[3] != 'C') {
		return 0;
	}
	const uint16_t npaths = ru16(blob + 6);
	const uint8_t *p = blob + 8;
	const uint8_t *end = blob + size;
	for (uint16_t i = 0; i < npaths; i++) {
		if (p >= end) {
			return 0;
		}
		const uint8_t nlen = *p++;
		if (p + nlen > end) {
			return 0;
		}
		p += nlen;
	}
	if (p + 2 > end) {
		return 0;
	}
	const uint16_t nfunc = ru16(p);
	p += 2;
	int ran = 0;
	for (uint16_t f = 0; f < nfunc; f++) {
		if (p >= end) {
			break;
		}
		const uint8_t nlen = *p++;
		if (p + nlen > end) {
			break;
		}
		p += nlen;
		if (p + 5 > end) {
			break;
		}
		p++; // nargs
		const uint16_t nconst = ru16(p);
		p += 2;
		const uint16_t ncode = ru16(p);
		p += 2;
		if (p + nconst * 4 > end) {
			break;
		}
		float consts[8];
		for (uint16_t c = 0; c < nconst && c < 8; c++) {
			consts[c] = rf32(p);
			p += 4;
		}
		if (nconst > 8) {
			p += (nconst - 8) * 4;
		}
		if (p >= end) {
			break;
		}
		const uint8_t nnames = *p++;
		char names[4][32];
		uint8_t nstore = 0;
		for (uint8_t n = 0; n < nnames; n++) {
			if (p >= end) {
				break;
			}
			const uint8_t sl = *p++;
			if (p + sl > end) {
				break;
			}
			if (nstore < 4) {
				uint8_t cpy = sl < 31 ? sl : 31;
				for (uint8_t k = 0; k < cpy; k++) {
					names[nstore][k] = char(p[k]);
				}
				names[nstore][cpy] = 0;
				nstore++;
			}
			p += sl;
		}
		if (p + ncode > end) {
			break;
		}
		const uint8_t *code = p;
		p += ncode;
		float stack[8];
		int sp = 0;
		size_t ip = 0;
		while (ip < ncode) {
			const uint8_t op = code[ip++];
			if (op == kOpReturn) {
				break;
			}
			if (op == kOpLoadConstF && ip < ncode) {
				const uint8_t ci = code[ip++];
				if (sp < 8 && ci < nconst && ci < 8) {
					stack[sp++] = consts[ci];
				}
			} else if (op == kOpLoadArg && ip < ncode) {
				ip++;
				if (sp < 8) {
					stack[sp++] = delta;
				}
			} else if (op == kOpMul && sp >= 2) {
				const float b = stack[--sp];
				const float a = stack[--sp];
				stack[sp++] = a * b;
			} else if (op == kOpAdd && sp >= 2) {
				const float b = stack[--sp];
				const float a = stack[--sp];
				stack[sp++] = a + b;
			} else if (op == kOpCallSelf && ip < ncode) {
				const uint8_t ni = code[ip++];
				const float arg = sp > 0 ? stack[--sp] : 0.0f;
				int step = int(arg * 4096.0f / (2.0f * 3.14159265f));
				if (step == 0 && arg != 0.0f) {
					step = arg > 0.0f ? 1 : -1;
				}
				const char *nm = (ni < nstore) ? names[ni] : "rotate_y";
				if (host) {
					if (nm[0] == 'r' && nm[7] == 'x' && host->rot_x) {
						*host->rot_x += int16_t(step);
					} else if (nm[0] == 'r' && nm[7] == 'z' && host->rot_z) {
						*host->rot_z += int16_t(step);
					} else if (nm[0] == 't' && host->pos_z) {
						*host->pos_z += int32_t(arg);
					} else if (host->rot_y) {
						*host->rot_y += int16_t(step);
					}
				}
				ran = 1;
			}
		}
	}
	return ran;
}

int script_vm_process(float delta, const ScriptVMHost *host) {
	if (!g_ready) {
		return 0;
	}
	int ran = 0;
	if (g_gdbc && g_gdbc_size) {
		ran |= run_gdbc(g_gdbc, g_gdbc_size, delta, host);
	}
	if (!ran && g_luau && g_luau_size > 8) {
		const uint8_t *p = g_luau;
		if (p[0] == 'L' && p[1] == 'U' && p[2] == 'B' && p[3] == 'C') {
			const uint16_t official = ru16(p + 4);
			const uint8_t *gdbc = p + 6 + official;
			const size_t left = g_luau_size - size_t(gdbc - p);
			ran |= run_gdbc(gdbc, left, delta, host);
		}
	}
	return ran;
}
