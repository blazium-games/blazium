/**************************************************************************/
/*  anticheat_export_plugin.h                                             */
/**************************************************************************/
/*                         This file is part of:                          */
/*                             BLAZIUM ENGINE                             */
/*                          https://blazium.app                           */
/**************************************************************************/

#pragma once

#ifdef TOOLS_ENABLED

#include "core/string/ustring.h"
#include "core/templates/hash_set.h"
#include "editor/export/editor_export_plugin.h"

class AnticheatExportPlugin : public EditorExportPlugin {
	GDCLASS(AnticheatExportPlugin, EditorExportPlugin);

	String export_path;

protected:
	virtual void _export_begin(const HashSet<String> &p_features, bool p_debug, const String &p_path, int p_flags) override;
	virtual void _export_end() override;

public:
	virtual String get_name() const override { return "Anticheat"; }
};

#endif
