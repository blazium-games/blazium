/**************************************************************************/
/*  workspace.cpp                                                         */
/**************************************************************************/
/*                         This file is part of:                          */
/*                             BLAZIUM ENGINE                             */
/*                          https://blazium.app                           */
/**************************************************************************/
/* Copyright (c) 2024-present Blazium Engine contributors.                */
/*                                                                        */
/* Permission is hereby granted, free of charge, to any person obtaining  */
/* a copy of this software and associated documentation files (the        */
/* "Software"), to deal in the Software without restriction, including    */
/* without limitation the rights to use, copy, modify, merge, publish,    */
/* distribute, sublicense, and/or sell copies of the Software, and to     */
/* permit persons to whom the Software is furnished to do so, subject to  */
/* the following conditions:                                              */
/*                                                                        */
/* The above copyright notice and this permission notice shall be         */
/* included in all copies or substantial portions of the Software.        */
/*                                                                        */
/* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,        */
/* EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF     */
/* MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. */
/* IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY   */
/* CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,   */
/* TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE      */
/* SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.                 */
/**************************************************************************/

#include "client/sdk/workspace.h"
#include <algorithm>

namespace coldstorage {

Workspace::Workspace(const std::string &name, const std::string &root, const std::vector<ViewMapping> &view) :
		name_(name), root_(root), view_(view) {}

const std::string &Workspace::name() const {
	return name_;
}
const std::string &Workspace::root() const {
	return root_;
}
const std::vector<ViewMapping> &Workspace::view() const {
	return view_;
}

std::string Workspace::depotToLocal(const std::string &depotPath) const {
	for (const auto &m : view_) {
		if (m.isExclusion) {
			continue;
		}
		std::string depotPrefix = m.depotPattern;
		if (depotPrefix.size() >= 3 && depotPrefix.substr(depotPrefix.size() - 3) == "...") {
			depotPrefix = depotPrefix.substr(0, depotPrefix.size() - 3);
		}
		if (depotPath.substr(0, depotPrefix.size()) == depotPrefix) {
			std::string localPrefix = m.localPattern;
			if (localPrefix.size() >= 3 && localPrefix.substr(localPrefix.size() - 3) == "...") {
				localPrefix = localPrefix.substr(0, localPrefix.size() - 3);
			}
			return localPrefix + depotPath.substr(depotPrefix.size());
		}
	}
	return depotPath;
}

std::string Workspace::localToDepot(const std::string &localPath) const {
	for (const auto &m : view_) {
		if (m.isExclusion) {
			continue;
		}
		std::string localPrefix = m.localPattern;
		if (localPrefix.size() >= 3 && localPrefix.substr(localPrefix.size() - 3) == "...") {
			localPrefix = localPrefix.substr(0, localPrefix.size() - 3);
		}
		if (localPath.substr(0, localPrefix.size()) == localPrefix) {
			std::string depotPrefix = m.depotPattern;
			if (depotPrefix.size() >= 3 && depotPrefix.substr(depotPrefix.size() - 3) == "...") {
				depotPrefix = depotPrefix.substr(0, depotPrefix.size() - 3);
			}
			return depotPrefix + localPath.substr(localPrefix.size());
		}
	}
	return localPath;
}

bool Workspace::isPathInView(const std::string &depotPath) const {
	bool inView = false;
	for (const auto &m : view_) {
		std::string prefix = m.depotPattern;
		if (prefix.size() >= 3 && prefix.substr(prefix.size() - 3) == "...") {
			prefix = prefix.substr(0, prefix.size() - 3);
		}
		if (depotPath.substr(0, prefix.size()) == prefix) {
			inView = !m.isExclusion;
		}
	}
	return inView;
}

std::vector<ViewMapping> Workspace::parseView(const nlohmann::json &viewJson) {
	std::vector<ViewMapping> result;
	if (!viewJson.is_array()) {
		return result;
	}
	for (const auto &entry : viewJson) {
		ViewMapping m;
		m.depotPattern = entry.value("depot", "");
		m.localPattern = entry.value("local", "");
		m.isExclusion = entry.value("exclusion", false);
		result.push_back(m);
	}
	return result;
}

} //namespace coldstorage
