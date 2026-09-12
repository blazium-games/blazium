/**************************************************************************/
/*  steam_inventory.cpp                                                   */
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

#include "steam.h"

#include "core/io/http_client.h"
#include "core/io/image.h"
#include "core/os/os.h"
#include "core/os/time.h"
#include "steam_types.h"

bool Steam::_ensure_inventory_ready() const {
	return initialized && steam_inventory && loader.has_inventory_support();
}

bool Steam::_wait_for_inventory_result(SteamInventoryResult_t p_result, double p_timeout_sec) {
	if (!_ensure_inventory_ready() || p_result == STEAM_INVENTORY_RESULT_INVALID) {
		return false;
	}

	const double start_usec = Time::get_singleton()->get_ticks_usec();
	while (true) {
		_dispatch_callbacks();

		if (inventory_pending_results.has(p_result)) {
			const int status = inventory_pending_results[p_result];
			if (status != STEAM_RESULT_PENDING) {
				return status == STEAM_RESULT_OK || status == STEAM_RESULT_EXPIRED;
			}
		}

		const int api_status = loader.inventory_get_result_status(steam_inventory, p_result);
		if (api_status != STEAM_RESULT_PENDING && api_status != 0) {
			return api_status == STEAM_RESULT_OK || api_status == STEAM_RESULT_EXPIRED;
		}

		if ((Time::get_singleton()->get_ticks_usec() - start_usec) / 1000000.0 > p_timeout_sec) {
			_log_debug(vformat("Timed out waiting for inventory result handle=%d", (int)p_result));
			return false;
		}
		::OS::get_singleton()->delay_usec(1000);
	}
}

bool Steam::_wait_for_inventory_definitions(double p_timeout_sec) {
	if (!_ensure_inventory_ready()) {
		return false;
	}
	if (inventory_definitions_loaded) {
		return true;
	}

	const double start_usec = Time::get_singleton()->get_ticks_usec();
	while (!inventory_definitions_loaded) {
		_dispatch_callbacks();
		const Vector<int32_t> ids = loader.inventory_get_item_definition_ids(steam_inventory);
		if (!ids.is_empty()) {
			inventory_definitions_loaded = true;
			inventory_definitions_pending = false;
			emit_signal("inventory_definitions_updated");
			return true;
		}
		if ((Time::get_singleton()->get_ticks_usec() - start_usec) / 1000000.0 > p_timeout_sec) {
			_log_debug("Timed out waiting for inventory item definitions");
			return false;
		}
		::OS::get_singleton()->delay_usec(1000);
	}
	return true;
}

Ref<SteamInventoryItem> Steam::_build_inventory_item(const SteamItemDetails &p_details, SteamInventoryResult_t p_result, uint32_t p_item_index) const {
	Ref<SteamInventoryItem> item;
	item.instantiate();
	item->set_instance_id(p_details.item_id);
	item->set_def_id((int)p_details.definition);
	item->set_quantity((int)p_details.quantity);
	item->set_flags((int)p_details.flags);

	Dictionary props;
	const String property_list = loader.inventory_get_result_item_property(steam_inventory, p_result, p_item_index, nullptr);
	if (!property_list.is_empty()) {
		PackedStringArray names = property_list.split(",");
		for (int i = 0; i < names.size(); i++) {
			const String name = names[i].strip_edges();
			if (name.is_empty()) {
				continue;
			}
			CharString name_utf8 = name.utf8();
			props[name] = loader.inventory_get_result_item_property(steam_inventory, p_result, p_item_index, name_utf8.get_data());
		}
	}
	item->set_properties(props);
	return item;
}

Array Steam::_parse_inventory_result(SteamInventoryResult_t p_result) {
	Array out;
	if (!_ensure_inventory_ready() || p_result == STEAM_INVENTORY_RESULT_INVALID) {
		return out;
	}

	Vector<SteamItemDetails> details;
	if (!loader.inventory_get_result_items(steam_inventory, p_result, details)) {
		return out;
	}

	for (int i = 0; i < details.size(); i++) {
		if (details[i].flags & STEAM_ITEM_REMOVED) {
			continue;
		}
		Ref<SteamInventoryItem> item = _build_inventory_item(details[i], p_result, (uint32_t)i);
		if (item.is_valid()) {
			out.push_back(item);
		}
	}
	return out;
}

Ref<SteamItemDefinition> Steam::_build_item_definition(int p_def_id) const {
	Ref<SteamItemDefinition> def;
	if (!_ensure_inventory_ready() || p_def_id <= 0) {
		return def;
	}

	def.instantiate();
	def->set_def_id(p_def_id);
	const Dictionary props = loader.inventory_get_item_definition_properties(steam_inventory, (SteamItemDef_t)p_def_id);
	def->set_properties(props);
	def->set_name(props.has("name") ? String(props["name"]) : String());
	def->set_description(props.has("description") ? String(props["description"]) : String());
	def->set_type(props.has("type") ? String(props["type"]) : String());
	def->set_icon_url(props.has("icon_url") ? String(props["icon_url"]) : String());
	return def;
}

Ref<Image> Steam::_fetch_image_from_url(const String &p_url) const {
	if (p_url.is_empty()) {
		return Ref<Image>();
	}

	Ref<HTTPClient> http = HTTPClient::create();
	http->set_blocking_mode(true);

	String host;
	int port = 80;
	String scheme = "http";
	String path = "/";

	int scheme_end = p_url.find("://");
	if (scheme_end == -1) {
		return Ref<Image>();
	}
	scheme = p_url.substr(0, scheme_end).to_lower();
	String remainder = p_url.substr(scheme_end + 3);
	int slash = remainder.find("/");
	String authority = slash == -1 ? remainder : remainder.substr(0, slash);
	if (slash != -1) {
		path = remainder.substr(slash);
	}
	int colon = authority.find(":");
	if (colon != -1) {
		host = authority.substr(0, colon);
		port = authority.substr(colon + 1).to_int();
	} else {
		host = authority;
		port = scheme == "https" ? 443 : 80;
	}

	Error err = http->connect_to_host(host, port, scheme == "https" ? TLSOptions::client() : Ref<TLSOptions>());
	if (err != OK) {
		return Ref<Image>();
	}

	const double connect_timeout_sec = 15.0;
	const double connect_start_usec = Time::get_singleton()->get_ticks_usec();
	while (http->get_status() != HTTPClient::STATUS_CONNECTED) {
		http->poll();
		HTTPClient::Status status = http->get_status();
		if (status == HTTPClient::STATUS_CANT_CONNECT ||
				status == HTTPClient::STATUS_CANT_RESOLVE ||
				status == HTTPClient::STATUS_CONNECTION_ERROR ||
				status == HTTPClient::STATUS_TLS_HANDSHAKE_ERROR) {
			http->close();
			return Ref<Image>();
		}
		if ((Time::get_singleton()->get_ticks_usec() - connect_start_usec) / 1000000.0 > connect_timeout_sec) {
			http->close();
			return Ref<Image>();
		}
		::OS::get_singleton()->delay_usec(1000);
	}

	err = http->request(HTTPClient::METHOD_GET, p_url, Vector<String>(), nullptr, 0);
	if (err != OK) {
		return Ref<Image>();
	}

	PackedByteArray response_bytes;
	while (true) {
		http->poll();
		HTTPClient::Status status = http->get_status();
		if (status == HTTPClient::STATUS_BODY || status == HTTPClient::STATUS_CONNECTED) {
			if (http->has_response()) {
				const int code = http->get_response_code();
				if (code < 200 || code >= 300) {
					http->close();
					return Ref<Image>();
				}
				while (http->get_status() == HTTPClient::STATUS_BODY) {
					response_bytes.append_array(http->read_response_body_chunk());
					http->poll();
				}
				break;
			}
		} else if (status == HTTPClient::STATUS_CONNECTION_ERROR ||
				status == HTTPClient::STATUS_CANT_CONNECT ||
				status == HTTPClient::STATUS_CANT_RESOLVE ||
				status == HTTPClient::STATUS_TLS_HANDSHAKE_ERROR) {
			http->close();
			return Ref<Image>();
		}
		::OS::get_singleton()->delay_usec(1000);
	}
	http->close();

	if (response_bytes.is_empty()) {
		return Ref<Image>();
	}

	Ref<Image> image;
	image.instantiate();
	err = image->load_png_from_buffer(response_bytes);
	if (err != OK) {
		err = image->load_jpg_from_buffer(response_bytes);
	}
	if (err != OK) {
		err = image->load_webp_from_buffer(response_bytes);
	}
	if (err != OK) {
		return Ref<Image>();
	}
	return image;
}

bool Steam::load_item_definitions() {
	if (!_ensure_inventory_ready()) {
		_log_debug("load_item_definitions unavailable");
		return false;
	}
	inventory_definitions_loaded = false;
	inventory_definitions_pending = true;
	if (!loader.inventory_load_item_definitions(steam_inventory)) {
		inventory_definitions_pending = false;
		_log_debug("LoadItemDefinitions call failed");
		return false;
	}
	_log_debug("LoadItemDefinitions requested");
	return true;
}

bool Steam::request_item_definitions(double p_timeout_sec) {
	if (!load_item_definitions()) {
		return false;
	}
	return _wait_for_inventory_definitions(p_timeout_sec);
}

PackedInt32Array Steam::get_item_definition_ids() {
	PackedInt32Array out;
	if (!_ensure_inventory_ready()) {
		return out;
	}
	const Vector<int32_t> ids = loader.inventory_get_item_definition_ids(steam_inventory);
	for (int i = 0; i < ids.size(); i++) {
		out.push_back(ids[i]);
	}
	return out;
}

Ref<SteamItemDefinition> Steam::get_item_definition(int p_def_id) {
	return _build_item_definition(p_def_id);
}

Array Steam::get_all_item_definitions() {
	Array out;
	if (!_ensure_inventory_ready()) {
		return out;
	}
	const Vector<int32_t> ids = loader.inventory_get_item_definition_ids(steam_inventory);
	for (int i = 0; i < ids.size(); i++) {
		Ref<SteamItemDefinition> def = _build_item_definition((int)ids[i]);
		if (def.is_valid()) {
			out.push_back(def);
		}
	}
	return out;
}

Array Steam::get_all_items() {
	Array out;
	if (!_ensure_inventory_ready()) {
		return out;
	}

	SteamInventoryResult_t result = STEAM_INVENTORY_RESULT_INVALID;
	if (!loader.inventory_get_all_items(steam_inventory, result)) {
		return out;
	}
	if (!_wait_for_inventory_result(result, 45.0)) {
		loader.inventory_destroy_result(steam_inventory, result);
		return out;
	}
	out = _parse_inventory_result(result);
	loader.inventory_destroy_result(steam_inventory, result);
	return out;
}

Array Steam::get_items_by_id(const PackedInt64Array &p_instance_ids) {
	Array out;
	if (!_ensure_inventory_ready() || p_instance_ids.is_empty()) {
		return out;
	}

	Vector<uint64_t> ids;
	for (int i = 0; i < p_instance_ids.size(); i++) {
		ids.push_back((uint64_t)p_instance_ids[i]);
	}

	SteamInventoryResult_t result = STEAM_INVENTORY_RESULT_INVALID;
	if (!loader.inventory_get_items_by_id(steam_inventory, result, ids)) {
		return out;
	}
	if (!_wait_for_inventory_result(result, 45.0)) {
		loader.inventory_destroy_result(steam_inventory, result);
		return out;
	}
	out = _parse_inventory_result(result);
	loader.inventory_destroy_result(steam_inventory, result);
	return out;
}

Array Steam::find_items_by_def_id(int p_def_id) {
	Array out;
	const Array items = get_all_items();
	for (int i = 0; i < items.size(); i++) {
		Ref<SteamInventoryItem> item = items[i];
		if (item.is_valid() && item->get_def_id() == p_def_id) {
			out.push_back(item);
		}
	}
	return out;
}

Ref<Image> Steam::get_item_definition_icon(int p_def_id) {
	Ref<SteamItemDefinition> def = get_item_definition(p_def_id);
	if (def.is_null()) {
		return Ref<Image>();
	}
	return _fetch_image_from_url(def->get_icon_url());
}

bool Steam::consume_item(uint64_t p_instance_id, int p_quantity) {
	if (!_ensure_inventory_ready() || p_instance_id == 0 || p_quantity <= 0) {
		return false;
	}
	SteamInventoryResult_t result = STEAM_INVENTORY_RESULT_INVALID;
	if (!loader.inventory_consume_item(steam_inventory, result, p_instance_id, (uint32_t)p_quantity)) {
		return false;
	}
	const bool ok = _wait_for_inventory_result(result, 45.0);
	loader.inventory_destroy_result(steam_inventory, result);
	_log_debug(vformat("consume_item(%s, %d) -> %s", String::num_uint64(p_instance_id), p_quantity, ok ? "true" : "false"));
	return ok;
}

bool Steam::exchange_items(const PackedInt32Array &p_generate_def_ids, const PackedInt32Array &p_generate_quantities, const PackedInt64Array &p_destroy_instance_ids, const PackedInt32Array &p_destroy_quantities) {
	if (!_ensure_inventory_ready()) {
		return false;
	}
	Vector<int32_t> generate_defs;
	Vector<int32_t> generate_quantities;
	Vector<uint64_t> destroy_ids;
	Vector<int32_t> destroy_quantities;
	for (int i = 0; i < p_generate_def_ids.size(); i++) {
		generate_defs.push_back(p_generate_def_ids[i]);
	}
	for (int i = 0; i < p_generate_quantities.size(); i++) {
		generate_quantities.push_back(p_generate_quantities[i]);
	}
	for (int i = 0; i < p_destroy_instance_ids.size(); i++) {
		destroy_ids.push_back((uint64_t)p_destroy_instance_ids[i]);
	}
	for (int i = 0; i < p_destroy_quantities.size(); i++) {
		destroy_quantities.push_back(p_destroy_quantities[i]);
	}

	SteamInventoryResult_t result = STEAM_INVENTORY_RESULT_INVALID;
	if (!loader.inventory_exchange_items(steam_inventory, result, generate_defs, generate_quantities, destroy_ids, destroy_quantities)) {
		return false;
	}
	const bool ok = _wait_for_inventory_result(result, 45.0);
	loader.inventory_destroy_result(steam_inventory, result);
	return ok;
}

bool Steam::trigger_item_drop(int p_drop_list_definition) {
	if (!_ensure_inventory_ready() || p_drop_list_definition <= 0) {
		return false;
	}
	SteamInventoryResult_t result = STEAM_INVENTORY_RESULT_INVALID;
	if (!loader.inventory_trigger_item_drop(steam_inventory, result, (SteamItemDef_t)p_drop_list_definition)) {
		return false;
	}
	const bool ok = _wait_for_inventory_result(result, 45.0);
	loader.inventory_destroy_result(steam_inventory, result);
	_log_debug(vformat("trigger_item_drop(%d) -> %s", p_drop_list_definition, ok ? "true" : "false"));
	return ok;
}

bool Steam::transfer_item_quantity(uint64_t p_source_instance_id, int p_quantity, uint64_t p_dest_instance_id) {
	if (!_ensure_inventory_ready() || p_source_instance_id == 0 || p_quantity <= 0) {
		return false;
	}
	SteamInventoryResult_t result = STEAM_INVENTORY_RESULT_INVALID;
	if (!loader.inventory_transfer_item_quantity(steam_inventory, result, p_source_instance_id, (uint32_t)p_quantity, p_dest_instance_id)) {
		return false;
	}
	const bool ok = _wait_for_inventory_result(result, 45.0);
	loader.inventory_destroy_result(steam_inventory, result);
	return ok;
}

bool Steam::grant_promo_items() {
	if (!_ensure_inventory_ready()) {
		return false;
	}
	SteamInventoryResult_t result = STEAM_INVENTORY_RESULT_INVALID;
	if (!loader.inventory_grant_promo_items(steam_inventory, result)) {
		return false;
	}
	const bool ok = _wait_for_inventory_result(result, 45.0);
	loader.inventory_destroy_result(steam_inventory, result);
	return ok;
}

bool Steam::add_promo_item(int p_def_id) {
	if (!_ensure_inventory_ready() || p_def_id <= 0) {
		return false;
	}
	SteamInventoryResult_t result = STEAM_INVENTORY_RESULT_INVALID;
	if (!loader.inventory_add_promo_item(steam_inventory, result, (SteamItemDef_t)p_def_id)) {
		return false;
	}
	const bool ok = _wait_for_inventory_result(result, 45.0);
	loader.inventory_destroy_result(steam_inventory, result);
	_log_debug(vformat("add_promo_item(%d) -> %s", p_def_id, ok ? "true" : "false"));
	return ok;
}

bool Steam::add_promo_items(const PackedInt32Array &p_def_ids) {
	if (!_ensure_inventory_ready() || p_def_ids.is_empty()) {
		return false;
	}
	Vector<int32_t> defs;
	for (int i = 0; i < p_def_ids.size(); i++) {
		defs.push_back(p_def_ids[i]);
	}
	SteamInventoryResult_t result = STEAM_INVENTORY_RESULT_INVALID;
	if (!loader.inventory_add_promo_items(steam_inventory, result, defs)) {
		return false;
	}
	const bool ok = _wait_for_inventory_result(result, 45.0);
	loader.inventory_destroy_result(steam_inventory, result);
	return ok;
}

bool Steam::start_property_update() {
	if (!_ensure_inventory_ready()) {
		return false;
	}
	inventory_property_update_handle = loader.inventory_start_update_properties(steam_inventory);
	return inventory_property_update_handle != STEAM_INVENTORY_UPDATE_HANDLE_INVALID;
}

bool Steam::set_property_string(uint64_t p_instance_id, const String &p_property_name, const String &p_value) {
	if (!_ensure_inventory_ready() || inventory_property_update_handle == STEAM_INVENTORY_UPDATE_HANDLE_INVALID || p_property_name.is_empty()) {
		return false;
	}
	CharString name_utf8 = p_property_name.utf8();
	CharString value_utf8 = p_value.utf8();
	return loader.inventory_set_property_string(steam_inventory, inventory_property_update_handle, p_instance_id, name_utf8.get_data(), value_utf8.get_data());
}

bool Steam::set_property_bool(uint64_t p_instance_id, const String &p_property_name, bool p_value) {
	if (!_ensure_inventory_ready() || inventory_property_update_handle == STEAM_INVENTORY_UPDATE_HANDLE_INVALID || p_property_name.is_empty()) {
		return false;
	}
	CharString name_utf8 = p_property_name.utf8();
	return loader.inventory_set_property_bool(steam_inventory, inventory_property_update_handle, p_instance_id, name_utf8.get_data(), p_value);
}

bool Steam::set_property_int64(uint64_t p_instance_id, const String &p_property_name, int p_value) {
	if (!_ensure_inventory_ready() || inventory_property_update_handle == STEAM_INVENTORY_UPDATE_HANDLE_INVALID || p_property_name.is_empty()) {
		return false;
	}
	CharString name_utf8 = p_property_name.utf8();
	return loader.inventory_set_property_int64(steam_inventory, inventory_property_update_handle, p_instance_id, name_utf8.get_data(), (int64_t)p_value);
}

bool Steam::set_property_float(uint64_t p_instance_id, const String &p_property_name, float p_value) {
	if (!_ensure_inventory_ready() || inventory_property_update_handle == STEAM_INVENTORY_UPDATE_HANDLE_INVALID || p_property_name.is_empty()) {
		return false;
	}
	CharString name_utf8 = p_property_name.utf8();
	return loader.inventory_set_property_float(steam_inventory, inventory_property_update_handle, p_instance_id, name_utf8.get_data(), p_value);
}

bool Steam::remove_property(uint64_t p_instance_id, const String &p_property_name) {
	if (!_ensure_inventory_ready() || inventory_property_update_handle == STEAM_INVENTORY_UPDATE_HANDLE_INVALID || p_property_name.is_empty()) {
		return false;
	}
	CharString name_utf8 = p_property_name.utf8();
	return loader.inventory_remove_property(steam_inventory, inventory_property_update_handle, p_instance_id, name_utf8.get_data());
}

bool Steam::submit_property_update() {
	if (!_ensure_inventory_ready() || inventory_property_update_handle == STEAM_INVENTORY_UPDATE_HANDLE_INVALID) {
		return false;
	}
	SteamInventoryResult_t result = STEAM_INVENTORY_RESULT_INVALID;
	if (!loader.inventory_submit_update_properties(steam_inventory, inventory_property_update_handle, result)) {
		return false;
	}
	inventory_property_update_handle = STEAM_INVENTORY_UPDATE_HANDLE_INVALID;
	const bool ok = _wait_for_inventory_result(result, 45.0);
	loader.inventory_destroy_result(steam_inventory, result);
	return ok;
}

PackedByteArray Steam::serialize_inventory_result(int p_result_handle) {
	PackedByteArray out;
	if (!_ensure_inventory_ready() || p_result_handle < 0) {
		return out;
	}
	loader.inventory_serialize_result(steam_inventory, (SteamInventoryResult_t)p_result_handle, out);
	return out;
}

int Steam::deserialize_inventory(const PackedByteArray &p_bytes, uint64_t p_expected_steam_id) {
	if (!_ensure_inventory_ready() || p_bytes.is_empty()) {
		return STEAM_INVENTORY_RESULT_INVALID;
	}
	SteamInventoryResult_t result = STEAM_INVENTORY_RESULT_INVALID;
	if (!loader.inventory_deserialize_result(steam_inventory, result, p_bytes, false)) {
		return STEAM_INVENTORY_RESULT_INVALID;
	}
	if (p_expected_steam_id != 0 && !loader.inventory_check_result_steam_id(steam_inventory, result, p_expected_steam_id)) {
		loader.inventory_destroy_result(steam_inventory, result);
		return STEAM_INVENTORY_RESULT_INVALID;
	}
	return (int)result;
}

PackedInt32Array Steam::get_eligible_promo_item_definition_ids(uint64_t p_steam_id) {
	PackedInt32Array out;
	if (!_ensure_inventory_ready()) {
		return out;
	}
	uint64_t steam_id = p_steam_id;
	if (steam_id == 0) {
		steam_id = get_local_steam_id();
	}
	if (steam_id == 0) {
		return out;
	}
	const Vector<int32_t> ids = loader.inventory_get_eligible_promo_item_definition_ids(steam_inventory, steam_id);
	for (int i = 0; i < ids.size(); i++) {
		out.push_back(ids[i]);
	}
	return out;
}

int Steam::get_inventory_result_status(int p_result_handle) {
	if (!_ensure_inventory_ready() || p_result_handle < 0) {
		return 0;
	}
	return loader.inventory_get_result_status(steam_inventory, (SteamInventoryResult_t)p_result_handle);
}

Array Steam::get_inventory_result_items(int p_result_handle) {
	if (!_ensure_inventory_ready() || p_result_handle < 0) {
		return Array();
	}
	return _parse_inventory_result((SteamInventoryResult_t)p_result_handle);
}
