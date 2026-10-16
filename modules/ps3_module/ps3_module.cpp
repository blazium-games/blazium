/**************************************************************************/
/*  ps3_module.cpp                                                        */
/**************************************************************************/

#include "ps3_module.h"

#include "core/object/class_db.h"

#ifdef PS3_ENABLED
#include <sysutil/sysutil.h>
extern "C" {
const char *ps3_boot_last_event(void);
}
#endif

void PS3Module::_bind_methods() {
	ClassDB::bind_method(D_METHOD("get_sdk_version"), &PS3Module::get_sdk_version);
	ClassDB::bind_method(D_METHOD("get_firmware_version"), &PS3Module::get_firmware_version);
	ClassDB::bind_method(D_METHOD("get_boot_phase"), &PS3Module::get_boot_phase);
	ClassDB::bind_method(D_METHOD("get_memory_available_mb"), &PS3Module::get_memory_available_mb);
}

String PS3Module::get_sdk_version() const {
	return "PSL1GHT";
}

String PS3Module::get_firmware_version() const {
#ifdef PS3_ENABLED
	char version[64] = {};
	if (sysUtilGetSystemParamString(0x101, version, sizeof(version)) == 0) {
		return String(version);
	}
#endif
	return "PS3";
}

String PS3Module::get_boot_phase() const {
#ifdef PS3_ENABLED
	return String(ps3_boot_last_event());
#else
	return "editor";
#endif
}

int PS3Module::get_memory_available_mb() const {
#ifdef PS3_ENABLED
	// Typical LV2 user memory budget for homebrew; exact query varies by firmware.
	return 227;
#else
	return 0;
#endif
}
