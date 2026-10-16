/**************************************************************************/
/*  rsx_context.cpp                                                       */
/**************************************************************************/

#include "rsx_context.h"

#include "core/string/print_string.h"
#include "servers/rendering/dummy/rasterizer_dummy.h"

#ifdef RSX_ENABLED
#include <sysmodule/sysmodule.h>
#include <rsx/rsx.h>
#include <gcm_sys.h>
#endif

static bool g_rsx_initialized = false;

void RSXContext::make_current() {
#ifdef RSX_ENABLED
	if (!g_rsx_initialized) {
		sysModuleLoad(SYSMODULE_GCM_SYS);
		sysModuleLoad(SYSMODULE_RSX);
		gcmInitBody(GCM_CONTEXT_RSX);
		rsxInit(0x200000, 0x1000000, 0, GCM_CONTEXT_RSX);
		g_rsx_initialized = true;
		print_line("PS3: RSX/GCM context initialized (experimental).");
	}
	RasterizerDummy::make_current();
#else
	RasterizerDummy::make_current();
#endif
}

bool RSXContext::is_available() {
#ifdef RSX_ENABLED
	return g_rsx_initialized;
#else
	return false;
#endif
}

void RSXContext::present_stub() {
#ifdef RSX_ENABLED
	if (g_rsx_initialized) {
		rsxSetFlipStatus(GCM_TRUE);
	}
#endif
}
