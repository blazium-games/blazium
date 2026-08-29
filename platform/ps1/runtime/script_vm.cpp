/**************************************************************************/
/*  script_vm.cpp                                                         */
/**************************************************************************/
/*                         This file is part of:                          */
/*                               BLAZIUM                                  */
/*                        https://blazium.app                             */
/**************************************************************************/

#include "script_vm.h"

#include <psxpad.h>

static const uint8_t *g_gdbc = nullptr;
static size_t g_gdbc_size = 0;
static const uint8_t *g_luau = nullptr;
static size_t g_luau_size = 0;
static int g_ready = 0;
static int g_did_ready = 0;
static char g_err[8] = "";

enum {
	kTapeReturn = 0,
	kTapeLoadConstF = 1,
	kTapeLoadArg = 2,
	kTapeMul = 3,
	kTapeAdd = 4,
	kTapeCallSelf = 5
};

// Must match GDScriptFunction::Opcode numeric values.
enum {
	OP_OPERATOR = 0,
	OP_SET_MEMBER = 16,
	OP_GET_MEMBER = 17,
	OP_ASSIGN = 20,
	OP_ASSIGN_NULL = 21,
	OP_ASSIGN_TRUE = 22,
	OP_ASSIGN_FALSE = 23,
	OP_ASSIGN_TYPED_BUILTIN = 24,
	OP_CONSTRUCT = 31,
	OP_CONSTRUCT_VALIDATED = 32,
	OP_CALL = 36,
	OP_CALL_RETURN = 37,
	OP_CALL_METHOD_BIND = 44,
	OP_CALL_METHOD_BIND_RET = 45,
	OP_CALL_NATIVE_STATIC = 47,
	OP_CALL_NATIVE_STATIC_VALIDATED_RETURN = 48,
	OP_CALL_NATIVE_STATIC_VALIDATED_NO_RETURN = 49,
	OP_CALL_METHOD_BIND_VALIDATED_RETURN = 50,
	OP_CALL_METHOD_BIND_VALIDATED_NO_RETURN = 51,
	OP_JUMP = 56,
	OP_JUMP_IF = 57,
	OP_JUMP_IF_NOT = 58,
	OP_JUMP_TO_DEF_ARGUMENT = 59,
	OP_JUMP_IF_SHARED = 60,
	OP_RETURN = 61,
	OP_RETURN_TYPED_BUILTIN = 62,
	OP_TYPE_ADJUST_BOOL = 110,
	OP_TYPE_ADJUST_INT = 111,
	OP_TYPE_ADJUST_FLOAT = 112,
	OP_TYPE_ADJUST_STRING = 113,
	OP_TYPE_ADJUST_VECTOR2 = 114,
	OP_TYPE_ADJUST_VECTOR3 = 118,
	OP_ASSERT = 148,
	OP_BREAKPOINT = 149,
	OP_LINE = 150,
	OP_END = 151
};

enum { V_NIL, V_BOOL, V_INT, V_FLOAT, V_STR, V_V2, V_V3 };

struct GVar {
	uint8_t type;
	int32_t i;
	float f;
	float x, y, z;
	char s[32];
};

static uint16_t ru16(const uint8_t *p) {
	return uint16_t(p[0] | (p[1] << 8));
}

static int32_t ri32(const uint8_t *p) {
	return int32_t(uint32_t(p[0]) | (uint32_t(p[1]) << 8) | (uint32_t(p[2]) << 16) | (uint32_t(p[3]) << 24));
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
	g_did_ready = 0;
	g_err[0] = 0;
}

const char *script_vm_last_error() {
	return g_err;
}

static void set_vm_err() {
	g_err[0] = 'V';
	g_err[1] = 'M';
	g_err[2] = ' ';
	g_err[3] = 'E';
	g_err[4] = 'R';
	g_err[5] = 'R';
	g_err[6] = 0;
}

static GVar gv_nil() {
	GVar v{};
	v.type = V_NIL;
	return v;
}

static GVar gv_float(float f) {
	GVar v = gv_nil();
	v.type = V_FLOAT;
	v.f = f;
	return v;
}

static GVar gv_bool(int b) {
	GVar v = gv_nil();
	v.type = V_BOOL;
	v.i = b ? 1 : 0;
	return v;
}

static float as_float(const GVar &v) {
	if (v.type == V_FLOAT) {
		return v.f;
	}
	if (v.type == V_INT || v.type == V_BOOL) {
		return float(v.i);
	}
	return 0.0f;
}

static int as_truth(const GVar &v) {
	if (v.type == V_NIL) {
		return 0;
	}
	if (v.type == V_BOOL || v.type == V_INT) {
		return v.i != 0;
	}
	if (v.type == V_FLOAT) {
		return v.f != 0.0f;
	}
	return 1;
}

static int name_is(const char *n, const char *w) {
	int i = 0;
	for (; w[i]; i++) {
		if (n[i] != w[i]) {
			return 0;
		}
	}
	return n[i] == 0;
}

static void apply_rot(const ScriptVMHost *host, const char *name, float arg) {
	if (!host || name_is(name, "get_node")) {
		return;
	}
	int step = int(arg * 4096.0f / (2.0f * 3.14159265f));
	if (step == 0 && arg != 0.0f) {
		step = arg > 0.0f ? 1 : -1;
	}
	if (name_is(name, "rotate_x") && host->rot_x) {
		*host->rot_x += int16_t(step);
	} else if (name_is(name, "rotate_z") && host->rot_z) {
		*host->rot_z += int16_t(step);
	} else if (name_is(name, "translate") && host->pos_z) {
		*host->pos_z += int32_t(arg);
	} else if (host->rot_y) {
		*host->rot_y += int16_t(step);
	}
}

static int pad_pressed(const ScriptVMHost *host, const char *action) {
	if (!host || !host->pad34) {
		return 0;
	}
	const PADTYPE *pad = (const PADTYPE *)host->pad34;
	if (pad->stat != 0) {
		return 0;
	}
	uint16_t mask = 0;
	if (name_is(action, "ui_left")) {
		mask = PAD_LEFT;
	} else if (name_is(action, "ui_right")) {
		mask = PAD_RIGHT;
	} else if (name_is(action, "ui_up")) {
		mask = PAD_UP;
	} else if (name_is(action, "ui_down")) {
		mask = PAD_DOWN;
	} else if (name_is(action, "ui_accept")) {
		mask = PAD_CROSS;
	}
	return mask && !(pad->btn & mask);
}

static GVar *slot(GVar *stack, int nstack, GVar *cvars, int nc, uint32_t addr) {
	const int type = int(addr >> 24);
	const int idx = int(addr & 0xffffffu);
	if (type == 0 && idx >= 0 && idx < nstack) {
		return &stack[idx];
	}
	if (type == 1 && idx >= 0 && idx < nc) {
		return &cvars[idx];
	}
	return nullptr;
}

static int is_call(int op) {
	return op == OP_CALL || op == OP_CALL_RETURN || op == OP_CALL_METHOD_BIND || op == OP_CALL_METHOD_BIND_RET ||
			op == OP_CALL_METHOD_BIND_VALIDATED_RETURN || op == OP_CALL_METHOD_BIND_VALIDATED_NO_RETURN ||
			op == OP_CALL_NATIVE_STATIC || op == OP_CALL_NATIVE_STATIC_VALIDATED_RETURN ||
			op == OP_CALL_NATIVE_STATIC_VALIDATED_NO_RETURN;
}

static int is_bind_call(int op) {
	return op == OP_CALL_METHOD_BIND || op == OP_CALL_METHOD_BIND_RET ||
			op == OP_CALL_METHOD_BIND_VALIDATED_RETURN || op == OP_CALL_METHOD_BIND_VALIDATED_NO_RETURN ||
			op == OP_CALL_NATIVE_STATIC || op == OP_CALL_NATIVE_STATIC_VALIDATED_RETURN ||
			op == OP_CALL_NATIVE_STATIC_VALIDATED_NO_RETURN;
}

static int is_input_name(const char *nm) {
	return name_is(nm, "is_action_pressed") || name_is(nm, "is_action_just_pressed");
}

static int run_official(const uint8_t *blob, size_t size, const char *want, float delta, const ScriptVMHost *host) {
	if (size < 8 || blob[0] != 'G' || blob[1] != 'D' || blob[2] != 'B' || blob[3] != 'C') {
		return 0;
	}
	if (ru16(blob + 4) != 5) {
		return 0;
	}
	const uint16_t nscripts = ru16(blob + 6);
	const uint8_t *p = blob + 8;
	const uint8_t *end = blob + size;
	int ran = 0;
	for (uint16_t s = 0; s < nscripts && p < end; s++) {
		if (p >= end) {
			break;
		}
		const uint8_t plen = *p++;
		if (p + plen > end) {
			break;
		}
		p += plen;
		if (p + 2 > end) {
			break;
		}
		const uint16_t nfn = ru16(p);
		p += 2;
		for (uint16_t f = 0; f < nfn && p < end; f++) {
			const uint8_t nlen = *p++;
			if (p + nlen + 3 > end) {
				return ran;
			}
			char fname[40];
			uint8_t cpy = nlen < 39 ? nlen : 39;
			for (uint8_t k = 0; k < cpy; k++) {
				fname[k] = char(p[k]);
			}
			fname[cpy] = 0;
			p += nlen;
			p++; // argc
			const uint16_t stack_size = ru16(p);
			p += 2;
			const uint16_t ncode = ru16(p);
			p += 2;
			if (p + ncode * 4 > end) {
				return ran;
			}
			const uint8_t *codeb = p;
			p += ncode * 4;
			const uint16_t nconst = ru16(p);
			p += 2;
			GVar cvars[16];
			int nc = 0;
			for (uint16_t c = 0; c < nconst; c++) {
				if (p >= end) {
					break;
				}
				const uint8_t t = *p++;
				GVar v = gv_nil();
				if (t == 1) {
					v = gv_bool(*p++);
				} else if (t == 2) {
					v.type = V_INT;
					v.i = ri32(p);
					p += 4;
				} else if (t == 3) {
					v = gv_float(rf32(p));
					p += 4;
				} else if (t == 4 || t == 21) {
					const uint8_t sl = *p++;
					v.type = V_STR;
					uint8_t n = sl < 31 ? sl : 31;
					for (uint8_t k = 0; k < n; k++) {
						v.s[k] = char(p[k]);
					}
					v.s[n] = 0;
					p += sl;
				} else if (t == 5) {
					v.type = V_V2;
					v.x = rf32(p);
					v.y = rf32(p + 4);
					p += 8;
				} else if (t == 9) {
					v.type = V_V3;
					v.x = rf32(p);
					v.y = rf32(p + 4);
					v.z = rf32(p + 8);
					p += 12;
				}
				if (nc < 16) {
					cvars[nc++] = v;
				}
			}
			const uint16_t nnames = ru16(p);
			p += 2;
			char names[12][32];
			int nn = 0;
			for (uint16_t g = 0; g < nnames; g++) {
				if (p >= end) {
					break;
				}
				const uint8_t sl = *p++;
				if (nn < 12) {
					uint8_t n = sl < 31 ? sl : 31;
					for (uint8_t k = 0; k < n && p + k < end; k++) {
						names[nn][k] = char(p[k]);
					}
					names[nn][n] = 0;
					nn++;
				}
				p += sl;
			}
			const uint16_t nmeth = (p + 2 <= end) ? ru16(p) : 0;
			p += 2;
			char mnames[8][32];
			int nmethods = 0;
			for (uint16_t g = 0; g < nmeth; g++) {
				if (p >= end) {
					break;
				}
				const uint8_t sl = *p++;
				if (nmethods < 8) {
					uint8_t n = sl < 31 ? sl : 31;
					for (uint8_t k = 0; k < n && p + k < end; k++) {
						mnames[nmethods][k] = char(p[k]);
					}
					mnames[nmethods][n] = 0;
					nmethods++;
				}
				p += sl;
			}
			if (!name_is(fname, want)) {
				continue;
			}
			const int nstack = stack_size > 4 && stack_size < 48 ? int(stack_size) : 16;
			GVar stack[48];
			for (int i = 0; i < 48; i++) {
				stack[i] = gv_nil();
			}
			stack[3] = gv_float(delta);
			int ip = 0;
			while (ip < int(ncode)) {
				const int32_t op = ri32(codeb + ip * 4);
				if (op == OP_END || op == OP_RETURN || op == OP_RETURN_TYPED_BUILTIN) {
					ran = 1;
					break;
				}
				if (op == OP_LINE || op == OP_BREAKPOINT) {
					ip += op == OP_LINE ? 2 : 1;
					continue;
				}
				if (op == OP_ASSIGN || op == OP_ASSIGN_TYPED_BUILTIN) {
					const uint32_t da = uint32_t(ri32(codeb + (ip + 1) * 4));
					const uint32_t sa = uint32_t(ri32(codeb + (ip + 2) * 4));
					GVar *d = slot(stack, nstack, cvars, nc, da);
					GVar *s = slot(stack, nstack, cvars, nc, sa);
					if (d && s) {
						*d = *s;
					}
					ip += op == OP_ASSIGN ? 3 : 4;
					continue;
				}
				if (op == OP_ASSIGN_TRUE || op == OP_ASSIGN_FALSE || op == OP_ASSIGN_NULL) {
					const uint32_t da = uint32_t(ri32(codeb + (ip + 1) * 4));
					GVar *d = slot(stack, nstack, cvars, nc, da);
					if (d) {
						*d = op == OP_ASSIGN_NULL ? gv_nil() : gv_bool(op == OP_ASSIGN_TRUE);
					}
					ip += 2;
					continue;
				}
				if (op == OP_OPERATOR) {
					const uint32_t aa = uint32_t(ri32(codeb + (ip + 1) * 4));
					const uint32_t ba = uint32_t(ri32(codeb + (ip + 2) * 4));
					const uint32_t da = uint32_t(ri32(codeb + (ip + 3) * 4));
					const int vop = ri32(codeb + (ip + 4) * 4);
					GVar *a = slot(stack, nstack, cvars, nc, aa);
					GVar *b = slot(stack, nstack, cvars, nc, ba);
					GVar *d = slot(stack, nstack, cvars, nc, da);
					if (a && b && d) {
						const float fa = as_float(*a);
						const float fb = as_float(*b);
						if (vop == 6) {
							*d = gv_float(fa + fb);
						} else if (vop == 7) {
							*d = gv_float(fa - fb);
						} else if (vop == 8) {
							*d = gv_float(fa * fb);
						} else if (vop == 9 && fb != 0.0f) {
							*d = gv_float(fa / fb);
						} else if (vop <= 5) {
							int t = 0;
							if (vop == 0) {
								t = fa == fb;
							} else if (vop == 1) {
								t = fa != fb;
							} else if (vop == 2) {
								t = fa < fb;
							} else if (vop == 3) {
								t = fa <= fb;
							} else if (vop == 4) {
								t = fa > fb;
							} else {
								t = fa >= fb;
							}
							*d = gv_bool(t);
						}
					}
					ip += 5;
					continue;
				}
				if (op == OP_JUMP) {
					ip = ri32(codeb + (ip + 1) * 4);
					continue;
				}
				if (op == OP_JUMP_IF || op == OP_JUMP_IF_NOT || op == OP_JUMP_IF_SHARED) {
					const uint32_t a = uint32_t(ri32(codeb + (ip + 1) * 4));
					const int to = ri32(codeb + (ip + 2) * 4);
					GVar *s = slot(stack, nstack, cvars, nc, a);
					const int t = s ? as_truth(*s) : 0;
					if ((op == OP_JUMP_IF && t) || (op != OP_JUMP_IF && !t)) {
						ip = to;
					} else {
						ip += 3;
					}
					continue;
				}
				if (is_call(op)) {
					const int iac = ri32(codeb + (ip + 1) * 4);
					const int native = op == OP_CALL_NATIVE_STATIC || op == OP_CALL_NATIVE_STATIC_VALIDATED_RETURN ||
							op == OP_CALL_NATIVE_STATIC_VALIDATED_NO_RETURN;
					const int argc = native ? ri32(codeb + (ip + 3 + iac) * 4) : ri32(codeb + (ip + 2 + iac) * 4);
					const int namei = native ? ri32(codeb + (ip + 2 + iac) * 4) : ri32(codeb + (ip + 3 + iac) * 4);
					const char *meth = "rotate_y";
					if (is_bind_call(op) && namei >= 0 && namei < nmethods) {
						meth = mnames[namei];
					} else if (!is_bind_call(op) && namei >= 0 && namei < nn) {
						meth = names[namei];
					}
					float arg = 0.0f;
					if (argc > 0) {
						const uint32_t aa = uint32_t(ri32(codeb + (ip + 2) * 4));
						GVar *s = slot(stack, nstack, cvars, nc, aa);
						if (s) {
							if (s->type == V_STR) {
								arg = float(pad_pressed(host, s->s));
							} else {
								arg = as_float(*s);
							}
						}
					}
					if (is_input_name(meth)) {
						const uint32_t ra = uint32_t(ri32(codeb + (ip + 3 + argc) * 4));
						GVar *d = slot(stack, nstack, cvars, nc, ra);
						if (d) {
							*d = gv_bool(int(arg));
						}
					} else {
						apply_rot(host, meth, arg);
					}
					ran = 1;
					ip += 4 + iac;
					continue;
				}
				if (op == OP_TYPE_ADJUST_BOOL || op == OP_TYPE_ADJUST_INT || op == OP_TYPE_ADJUST_FLOAT ||
						op == OP_TYPE_ADJUST_STRING || op == OP_TYPE_ADJUST_VECTOR2 || op == OP_TYPE_ADJUST_VECTOR3 ||
						op == OP_GET_MEMBER || op == OP_SET_MEMBER || op == OP_ASSERT ||
						op == OP_CONSTRUCT || op == OP_CONSTRUCT_VALIDATED || op == OP_JUMP_TO_DEF_ARGUMENT) {
					if (op == OP_CONSTRUCT || op == OP_CONSTRUCT_VALIDATED) {
						const int iac = ri32(codeb + (ip + 1) * 4);
						ip += 4 + iac;
					} else if (op == OP_GET_MEMBER || op == OP_SET_MEMBER || op == OP_ASSERT) {
						ip += 3;
					} else if (op == OP_JUMP_TO_DEF_ARGUMENT) {
						ip += 1;
					} else {
						ip += 2;
					}
					continue;
				}
				set_vm_err();
				break;
			}
		}
	}
	return ran;
}

static int run_tape(const uint8_t *blob, size_t size, float delta, const ScriptVMHost *host) {
	if (size < 8 || blob[0] != 'G' || blob[1] != 'D' || blob[2] != 'B' || blob[3] != 'C') {
		return 0;
	}
	if (ru16(blob + 4) != 4) {
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
		p += nlen;
		if (p + 5 > end) {
			break;
		}
		p++;
		const uint16_t nconst = ru16(p);
		p += 2;
		const uint16_t ncode = ru16(p);
		p += 2;
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
			const uint8_t sl = *p++;
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
		const uint8_t *code = p;
		p += ncode;
		float stack[8];
		int sp = 0;
		size_t ip = 0;
		while (ip < ncode) {
			const uint8_t op = code[ip++];
			if (op == kTapeReturn) {
				break;
			}
			if (op == kTapeLoadConstF && ip < ncode) {
				const uint8_t ci = code[ip++];
				if (sp < 8 && ci < 8) {
					stack[sp++] = consts[ci];
				}
			} else if (op == kTapeLoadArg && ip < ncode) {
				ip++;
				if (sp < 8) {
					stack[sp++] = delta;
				}
			} else if (op == kTapeMul && sp >= 2) {
				const float b = stack[--sp];
				const float a = stack[--sp];
				stack[sp++] = a * b;
			} else if (op == kTapeAdd && sp >= 2) {
				const float b = stack[--sp];
				const float a = stack[--sp];
				stack[sp++] = a + b;
			} else if (op == kTapeCallSelf && ip < ncode) {
				const uint8_t ni = code[ip++];
				const float arg = sp > 0 ? stack[--sp] : 0.0f;
				apply_rot(host, (ni < nstore) ? names[ni] : "rotate_y", arg);
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
		if (!g_did_ready) {
			run_official(g_gdbc, g_gdbc_size, "_ready", 0.0f, host);
			g_did_ready = 1;
		}
		ran |= run_official(g_gdbc, g_gdbc_size, "_process", delta, host);
	}
	if (!ran && g_luau && g_luau_size > 8) {
		const uint8_t *p = g_luau;
		if (p[0] == 'L' && p[1] == 'U' && p[2] == 'B' && p[3] == 'C') {
			const uint16_t official = ru16(p + 4);
			const uint8_t *tape = p + 6 + official;
			const size_t left = g_luau_size - size_t(tape - p);
			ran |= run_tape(tape, left, delta, host);
		}
	}
	return ran;
}
