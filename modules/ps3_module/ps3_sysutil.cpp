/**************************************************************************/
/*  ps3_sysutil.cpp                                                       */
/**************************************************************************/

#include "ps3_sysutil.h"

#include "core/object/class_db.h"
#include "core/string/print_string.h"

#ifdef PS3_ENABLED
#include <sysutil/msg.h>
#include <sysutil/sysutil.h>
#endif

#ifdef PS3_ENABLED
static bool g_msg_dialog_open = false;
#endif

void PS3SystemUtil::_bind_methods() {
	ClassDB::bind_method(D_METHOD("show_message", "message"), &PS3SystemUtil::show_message);
	ClassDB::bind_method(D_METHOD("show_message_async", "message"), &PS3SystemUtil::show_message_async);
	ClassDB::bind_method(D_METHOD("poll_message_async"), &PS3SystemUtil::poll_message_async);
}

void PS3SystemUtil::show_message(const String &p_message) const {
#ifdef PS3_ENABLED
	CharString message = p_message.utf8();
	msgDialogOpen2(MSG_DIALOG_NORMAL, message.get_data(), nullptr, nullptr, nullptr);
	while (!msgDialogAbort() && !msgDialogClose(0.0f)) {
		sysUtilCheckCallback();
	}
	g_msg_dialog_open = false;
#else
	print_line(vformat("PS3SystemUtil: %s", p_message));
#endif
}

int PS3SystemUtil::show_message_async(const String &p_message) const {
#ifdef PS3_ENABLED
	if (g_msg_dialog_open) {
		return 0;
	}
	CharString message = p_message.utf8();
	msgDialogOpen2(MSG_DIALOG_NORMAL, message.get_data(), nullptr, nullptr, nullptr);
	g_msg_dialog_open = true;
	return 0;
#else
	print_line(vformat("PS3SystemUtil: %s", p_message));
	return 0;
#endif
}

int PS3SystemUtil::poll_message_async() const {
#ifdef PS3_ENABLED
	if (!g_msg_dialog_open) {
		return -1;
	}
	sysUtilCheckCallback();
	if (msgDialogAbort()) {
		g_msg_dialog_open = false;
		return 2;
	}
	if (msgDialogClose(0.0f)) {
		g_msg_dialog_open = false;
		return 1;
	}
	return 0;
#else
	return -1;
#endif
}

PS3SystemUtil::PS3SystemUtil() {
}
