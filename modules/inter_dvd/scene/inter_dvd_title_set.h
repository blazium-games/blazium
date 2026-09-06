/**************************************************************************/
/*  inter_dvd_title_set.h                                                 */
/**************************************************************************/
/*                         This file is part of:                          */
/*                             BLAZIUM ENGINE                             */
/*                          https://blazium.app                           */
/**************************************************************************/

#pragma once

#include "modules/inter_dvd/author/inter_dvd_project.h"
#include "scene/main/node.h"

class InterDVDTitle;
class InterDVDMenuPage;

class InterDVDTitleSet : public Node {
	GDCLASS(InterDVDTitleSet, Node);

protected:
	static void _bind_methods();

public:
	InterDVDTitle *add_title(const String &p_name = "Title");
	InterDVDMenuPage *add_root_menu(const String &p_name = "RootMenu");
};
