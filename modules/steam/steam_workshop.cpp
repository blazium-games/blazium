/**************************************************************************/
/*  steam_workshop.cpp                                                    */
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

// Steam Workshop (ISteamUGC) support for the Steam singleton.

#include "steam.h"
#include "steam_types.h"

#include "core/config/project_settings.h"
#include "core/object/class_db.h"

#include <cstring>

namespace {

String _workshop_fixed_string(const char *p_buffer, size_t p_capacity) {
	size_t length = 0;
	while (length < p_capacity && p_buffer[length] != '\0') {
		length++;
	}
	return String::utf8(p_buffer, (int)length);
}

String _workshop_native_path(const String &p_path) {
	if (p_path.begins_with("res://") || p_path.begins_with("user://")) {
		return ProjectSettings::get_singleton()->globalize_path(p_path);
	}
	return p_path;
}

Vector<SteamPublishedFileId_t> _workshop_ids_from_array(const PackedInt64Array &p_ids) {
	Vector<SteamPublishedFileId_t> ids;
	for (int i = 0; i < p_ids.size(); i++) {
		if (p_ids[i] != 0) {
			ids.push_back((SteamPublishedFileId_t)p_ids[i]);
		}
	}
	return ids;
}

struct WorkshopStatisticName {
	int id;
	const char *name;
};

// EItemStatistic values that are meaningful for a single query result.
const WorkshopStatisticName WORKSHOP_STATISTICS[] = {
	{ 0, "num_subscriptions" },
	{ 1, "num_favorites" },
	{ 2, "num_followers" },
	{ 3, "num_unique_subscriptions" },
	{ 4, "num_unique_favorites" },
	{ 5, "num_unique_followers" },
	{ 6, "num_unique_website_views" },
	{ 7, "report_score" },
	{ 8, "num_seconds_played" },
	{ 9, "num_playtime_sessions" },
	{ 10, "num_comments" },
	{ 11, "num_seconds_played_during_time_period" },
	{ 12, "num_playtime_sessions_during_time_period" },
};

} // namespace

void Steam::_bind_workshop_methods() {
	ClassDB::bind_method(D_METHOD("has_workshop_support"), &Steam::has_workshop_support);
	ClassDB::bind_method(D_METHOD("set_workshop_app_ids", "consumer_app_id", "creator_app_id"), &Steam::set_workshop_app_ids, DEFVAL(0));
	ClassDB::bind_method(D_METHOD("get_workshop_consumer_app_id"), &Steam::get_workshop_consumer_app_id);
	ClassDB::bind_method(D_METHOD("get_workshop_creator_app_id"), &Steam::get_workshop_creator_app_id);
	ClassDB::bind_method(D_METHOD("get_num_subscribed_items", "include_locally_disabled"), &Steam::get_num_subscribed_items, DEFVAL(false));
	ClassDB::bind_method(D_METHOD("get_subscribed_items", "include_locally_disabled"), &Steam::get_subscribed_items, DEFVAL(false));
	ClassDB::bind_method(D_METHOD("get_item_state", "published_file_id"), &Steam::get_item_state);
	ClassDB::bind_method(D_METHOD("get_item_install_info", "published_file_id"), &Steam::get_item_install_info);
	ClassDB::bind_method(D_METHOD("get_item_download_info", "published_file_id"), &Steam::get_item_download_info);
	ClassDB::bind_method(D_METHOD("download_item", "published_file_id", "high_priority"), &Steam::download_item, DEFVAL(false));
	ClassDB::bind_method(D_METHOD("suspend_downloads", "suspend"), &Steam::suspend_downloads);
	ClassDB::bind_method(D_METHOD("subscribe_item", "published_file_id"), &Steam::subscribe_item);
	ClassDB::bind_method(D_METHOD("unsubscribe_item", "published_file_id"), &Steam::unsubscribe_item);

	ClassDB::bind_method(D_METHOD("query_workshop_items", "query_type", "matching_type", "page", "options"), &Steam::query_workshop_items, DEFVAL(WORKSHOP_QUERY_RANKED_BY_VOTE), DEFVAL(WORKSHOP_MATCHING_ITEMS_READY_TO_USE), DEFVAL(1), DEFVAL(Dictionary()));
	ClassDB::bind_method(D_METHOD("query_user_workshop_items", "list_type", "matching_type", "sort_order", "page", "steam_id", "options"), &Steam::query_user_workshop_items, DEFVAL(WORKSHOP_USER_LIST_PUBLISHED), DEFVAL(WORKSHOP_MATCHING_ITEMS_READY_TO_USE), DEFVAL(WORKSHOP_SORT_CREATION_ORDER_DESC), DEFVAL(1), DEFVAL(0), DEFVAL(Dictionary()));
	ClassDB::bind_method(D_METHOD("query_workshop_item_details", "published_file_ids", "options"), &Steam::query_workshop_item_details, DEFVAL(Dictionary()));

	ClassDB::bind_method(D_METHOD("create_workshop_item", "file_type"), &Steam::create_workshop_item, DEFVAL(WORKSHOP_FILE_TYPE_COMMUNITY));
	ClassDB::bind_method(D_METHOD("start_item_update", "published_file_id"), &Steam::start_item_update);
	ClassDB::bind_method(D_METHOD("set_item_title", "update_handle", "title"), &Steam::set_item_title);
	ClassDB::bind_method(D_METHOD("set_item_description", "update_handle", "description"), &Steam::set_item_description);
	ClassDB::bind_method(D_METHOD("set_item_update_language", "update_handle", "language"), &Steam::set_item_update_language);
	ClassDB::bind_method(D_METHOD("set_item_metadata", "update_handle", "metadata"), &Steam::set_item_metadata);
	ClassDB::bind_method(D_METHOD("set_item_visibility", "update_handle", "visibility"), &Steam::set_item_visibility);
	ClassDB::bind_method(D_METHOD("set_item_tags", "update_handle", "tags", "allow_admin_tags"), &Steam::set_item_tags, DEFVAL(false));
	ClassDB::bind_method(D_METHOD("set_item_content", "update_handle", "content_folder"), &Steam::set_item_content);
	ClassDB::bind_method(D_METHOD("set_item_preview", "update_handle", "preview_file"), &Steam::set_item_preview);
	ClassDB::bind_method(D_METHOD("add_item_key_value_tag", "update_handle", "key", "value"), &Steam::add_item_key_value_tag);
	ClassDB::bind_method(D_METHOD("remove_item_key_value_tags", "update_handle", "key"), &Steam::remove_item_key_value_tags);
	ClassDB::bind_method(D_METHOD("remove_all_item_key_value_tags", "update_handle"), &Steam::remove_all_item_key_value_tags);
	ClassDB::bind_method(D_METHOD("add_item_preview_file", "update_handle", "preview_file", "type"), &Steam::add_item_preview_file, DEFVAL(WORKSHOP_PREVIEW_TYPE_IMAGE));
	ClassDB::bind_method(D_METHOD("add_item_preview_video", "update_handle", "video_id"), &Steam::add_item_preview_video);
	ClassDB::bind_method(D_METHOD("remove_item_preview", "update_handle", "index"), &Steam::remove_item_preview);
	ClassDB::bind_method(D_METHOD("submit_item_update", "update_handle", "change_note"), &Steam::submit_item_update, DEFVAL(String()));
	ClassDB::bind_method(D_METHOD("get_item_update_progress", "update_handle"), &Steam::get_item_update_progress);
	ClassDB::bind_method(D_METHOD("delete_workshop_item", "published_file_id"), &Steam::delete_workshop_item);

	ClassDB::bind_method(D_METHOD("set_user_item_vote", "published_file_id", "vote_up"), &Steam::set_user_item_vote);
	ClassDB::bind_method(D_METHOD("add_item_to_favorites", "published_file_id"), &Steam::add_item_to_favorites);
	ClassDB::bind_method(D_METHOD("remove_item_from_favorites", "published_file_id"), &Steam::remove_item_from_favorites);
	ClassDB::bind_method(D_METHOD("start_playtime_tracking", "published_file_ids"), &Steam::start_playtime_tracking);
	ClassDB::bind_method(D_METHOD("stop_playtime_tracking", "published_file_ids"), &Steam::stop_playtime_tracking);
	ClassDB::bind_method(D_METHOD("stop_playtime_tracking_for_all_items"), &Steam::stop_playtime_tracking_for_all_items);
	ClassDB::bind_method(D_METHOD("show_workshop_eula"), &Steam::show_workshop_eula);

	ADD_SIGNAL(MethodInfo("workshop_query_completed", PropertyInfo(Variant::INT, "query_handle"), PropertyInfo(Variant::INT, "result"), PropertyInfo(Variant::ARRAY, "items", PROPERTY_HINT_ARRAY_TYPE, "SteamWorkshopItem"), PropertyInfo(Variant::INT, "total_matching_results"), PropertyInfo(Variant::STRING, "next_cursor")));
	ADD_SIGNAL(MethodInfo("workshop_item_created", PropertyInfo(Variant::INT, "published_file_id"), PropertyInfo(Variant::INT, "result"), PropertyInfo(Variant::BOOL, "needs_to_accept_agreement")));
	ADD_SIGNAL(MethodInfo("workshop_item_updated", PropertyInfo(Variant::INT, "published_file_id"), PropertyInfo(Variant::INT, "result"), PropertyInfo(Variant::BOOL, "needs_to_accept_agreement")));
	ADD_SIGNAL(MethodInfo("workshop_item_deleted", PropertyInfo(Variant::INT, "published_file_id"), PropertyInfo(Variant::INT, "result")));
	ADD_SIGNAL(MethodInfo("workshop_item_subscribed", PropertyInfo(Variant::INT, "published_file_id"), PropertyInfo(Variant::INT, "result")));
	ADD_SIGNAL(MethodInfo("workshop_item_unsubscribed", PropertyInfo(Variant::INT, "published_file_id"), PropertyInfo(Variant::INT, "result")));
	ADD_SIGNAL(MethodInfo("workshop_item_installed", PropertyInfo(Variant::INT, "published_file_id")));
	ADD_SIGNAL(MethodInfo("workshop_item_downloaded", PropertyInfo(Variant::INT, "published_file_id"), PropertyInfo(Variant::INT, "result")));
	ADD_SIGNAL(MethodInfo("workshop_item_vote_set", PropertyInfo(Variant::INT, "published_file_id"), PropertyInfo(Variant::INT, "result"), PropertyInfo(Variant::BOOL, "vote_up")));
	ADD_SIGNAL(MethodInfo("workshop_favorites_changed", PropertyInfo(Variant::INT, "published_file_id"), PropertyInfo(Variant::INT, "result"), PropertyInfo(Variant::BOOL, "was_add_request")));
	ADD_SIGNAL(MethodInfo("workshop_playtime_tracking_updated", PropertyInfo(Variant::INT, "result"), PropertyInfo(Variant::BOOL, "started")));
	ADD_SIGNAL(MethodInfo("workshop_subscriptions_changed"));

	BIND_ENUM_CONSTANT(WORKSHOP_FILE_TYPE_COMMUNITY);
	BIND_ENUM_CONSTANT(WORKSHOP_FILE_TYPE_MICROTRANSACTION);
	BIND_ENUM_CONSTANT(WORKSHOP_FILE_TYPE_COLLECTION);
	BIND_ENUM_CONSTANT(WORKSHOP_FILE_TYPE_ART);
	BIND_ENUM_CONSTANT(WORKSHOP_FILE_TYPE_VIDEO);
	BIND_ENUM_CONSTANT(WORKSHOP_FILE_TYPE_SCREENSHOT);
	BIND_ENUM_CONSTANT(WORKSHOP_FILE_TYPE_WEB_GUIDE);
	BIND_ENUM_CONSTANT(WORKSHOP_FILE_TYPE_INTEGRATED_GUIDE);
	BIND_ENUM_CONSTANT(WORKSHOP_FILE_TYPE_MERCH);
	BIND_ENUM_CONSTANT(WORKSHOP_FILE_TYPE_CONTROLLER_BINDING);
	BIND_ENUM_CONSTANT(WORKSHOP_FILE_TYPE_STEAM_VIDEO);
	BIND_ENUM_CONSTANT(WORKSHOP_FILE_TYPE_GAME_MANAGED_ITEM);

	BIND_ENUM_CONSTANT(WORKSHOP_VISIBILITY_PUBLIC);
	BIND_ENUM_CONSTANT(WORKSHOP_VISIBILITY_FRIENDS_ONLY);
	BIND_ENUM_CONSTANT(WORKSHOP_VISIBILITY_PRIVATE);
	BIND_ENUM_CONSTANT(WORKSHOP_VISIBILITY_UNLISTED);

	BIND_BITFIELD_FLAG(WORKSHOP_ITEM_STATE_NONE);
	BIND_BITFIELD_FLAG(WORKSHOP_ITEM_STATE_SUBSCRIBED);
	BIND_BITFIELD_FLAG(WORKSHOP_ITEM_STATE_LEGACY_ITEM);
	BIND_BITFIELD_FLAG(WORKSHOP_ITEM_STATE_INSTALLED);
	BIND_BITFIELD_FLAG(WORKSHOP_ITEM_STATE_NEEDS_UPDATE);
	BIND_BITFIELD_FLAG(WORKSHOP_ITEM_STATE_DOWNLOADING);
	BIND_BITFIELD_FLAG(WORKSHOP_ITEM_STATE_DOWNLOAD_PENDING);
	BIND_BITFIELD_FLAG(WORKSHOP_ITEM_STATE_DISABLED_LOCALLY);

	BIND_ENUM_CONSTANT(WORKSHOP_QUERY_RANKED_BY_VOTE);
	BIND_ENUM_CONSTANT(WORKSHOP_QUERY_RANKED_BY_PUBLICATION_DATE);
	BIND_ENUM_CONSTANT(WORKSHOP_QUERY_ACCEPTED_FOR_GAME_RANKED_BY_ACCEPTANCE_DATE);
	BIND_ENUM_CONSTANT(WORKSHOP_QUERY_RANKED_BY_TREND);
	BIND_ENUM_CONSTANT(WORKSHOP_QUERY_FAVORITED_BY_FRIENDS_RANKED_BY_PUBLICATION_DATE);
	BIND_ENUM_CONSTANT(WORKSHOP_QUERY_CREATED_BY_FRIENDS_RANKED_BY_PUBLICATION_DATE);
	BIND_ENUM_CONSTANT(WORKSHOP_QUERY_RANKED_BY_NUM_TIMES_REPORTED);
	BIND_ENUM_CONSTANT(WORKSHOP_QUERY_CREATED_BY_FOLLOWED_USERS_RANKED_BY_PUBLICATION_DATE);
	BIND_ENUM_CONSTANT(WORKSHOP_QUERY_NOT_YET_RATED);
	BIND_ENUM_CONSTANT(WORKSHOP_QUERY_RANKED_BY_TOTAL_VOTES_ASC);
	BIND_ENUM_CONSTANT(WORKSHOP_QUERY_RANKED_BY_VOTES_UP);
	BIND_ENUM_CONSTANT(WORKSHOP_QUERY_RANKED_BY_TEXT_SEARCH);
	BIND_ENUM_CONSTANT(WORKSHOP_QUERY_RANKED_BY_TOTAL_UNIQUE_SUBSCRIPTIONS);
	BIND_ENUM_CONSTANT(WORKSHOP_QUERY_RANKED_BY_PLAYTIME_TREND);
	BIND_ENUM_CONSTANT(WORKSHOP_QUERY_RANKED_BY_TOTAL_PLAYTIME);
	BIND_ENUM_CONSTANT(WORKSHOP_QUERY_RANKED_BY_AVERAGE_PLAYTIME_TREND);
	BIND_ENUM_CONSTANT(WORKSHOP_QUERY_RANKED_BY_LIFETIME_AVERAGE_PLAYTIME);
	BIND_ENUM_CONSTANT(WORKSHOP_QUERY_RANKED_BY_PLAYTIME_SESSIONS_TREND);
	BIND_ENUM_CONSTANT(WORKSHOP_QUERY_RANKED_BY_LIFETIME_PLAYTIME_SESSIONS);
	BIND_ENUM_CONSTANT(WORKSHOP_QUERY_RANKED_BY_LAST_UPDATED_DATE);

	BIND_ENUM_CONSTANT(WORKSHOP_MATCHING_ITEMS);
	BIND_ENUM_CONSTANT(WORKSHOP_MATCHING_ITEMS_MTX);
	BIND_ENUM_CONSTANT(WORKSHOP_MATCHING_ITEMS_READY_TO_USE);
	BIND_ENUM_CONSTANT(WORKSHOP_MATCHING_COLLECTIONS);
	BIND_ENUM_CONSTANT(WORKSHOP_MATCHING_ARTWORK);
	BIND_ENUM_CONSTANT(WORKSHOP_MATCHING_VIDEOS);
	BIND_ENUM_CONSTANT(WORKSHOP_MATCHING_SCREENSHOTS);
	BIND_ENUM_CONSTANT(WORKSHOP_MATCHING_ALL_GUIDES);
	BIND_ENUM_CONSTANT(WORKSHOP_MATCHING_WEB_GUIDES);
	BIND_ENUM_CONSTANT(WORKSHOP_MATCHING_INTEGRATED_GUIDES);
	BIND_ENUM_CONSTANT(WORKSHOP_MATCHING_USABLE_IN_GAME);
	BIND_ENUM_CONSTANT(WORKSHOP_MATCHING_CONTROLLER_BINDINGS);
	BIND_ENUM_CONSTANT(WORKSHOP_MATCHING_GAME_MANAGED_ITEMS);
	BIND_ENUM_CONSTANT(WORKSHOP_MATCHING_ALL);

	BIND_ENUM_CONSTANT(WORKSHOP_USER_LIST_PUBLISHED);
	BIND_ENUM_CONSTANT(WORKSHOP_USER_LIST_VOTED_ON);
	BIND_ENUM_CONSTANT(WORKSHOP_USER_LIST_VOTED_UP);
	BIND_ENUM_CONSTANT(WORKSHOP_USER_LIST_VOTED_DOWN);
	BIND_ENUM_CONSTANT(WORKSHOP_USER_LIST_WILL_VOTE_LATER);
	BIND_ENUM_CONSTANT(WORKSHOP_USER_LIST_FAVORITED);
	BIND_ENUM_CONSTANT(WORKSHOP_USER_LIST_SUBSCRIBED);
	BIND_ENUM_CONSTANT(WORKSHOP_USER_LIST_USED_OR_PLAYED);
	BIND_ENUM_CONSTANT(WORKSHOP_USER_LIST_FOLLOWED);

	BIND_ENUM_CONSTANT(WORKSHOP_SORT_CREATION_ORDER_DESC);
	BIND_ENUM_CONSTANT(WORKSHOP_SORT_CREATION_ORDER_ASC);
	BIND_ENUM_CONSTANT(WORKSHOP_SORT_TITLE_ASC);
	BIND_ENUM_CONSTANT(WORKSHOP_SORT_LAST_UPDATED_DESC);
	BIND_ENUM_CONSTANT(WORKSHOP_SORT_SUBSCRIPTION_DATE_DESC);
	BIND_ENUM_CONSTANT(WORKSHOP_SORT_VOTE_SCORE_DESC);
	BIND_ENUM_CONSTANT(WORKSHOP_SORT_FOR_MODERATION);

	BIND_ENUM_CONSTANT(WORKSHOP_UPDATE_STATUS_INVALID);
	BIND_ENUM_CONSTANT(WORKSHOP_UPDATE_STATUS_PREPARING_CONFIG);
	BIND_ENUM_CONSTANT(WORKSHOP_UPDATE_STATUS_PREPARING_CONTENT);
	BIND_ENUM_CONSTANT(WORKSHOP_UPDATE_STATUS_UPLOADING_CONTENT);
	BIND_ENUM_CONSTANT(WORKSHOP_UPDATE_STATUS_UPLOADING_PREVIEW_FILE);
	BIND_ENUM_CONSTANT(WORKSHOP_UPDATE_STATUS_COMMITTING_CHANGES);

	BIND_ENUM_CONSTANT(WORKSHOP_PREVIEW_TYPE_IMAGE);
	BIND_ENUM_CONSTANT(WORKSHOP_PREVIEW_TYPE_YOUTUBE_VIDEO);
	BIND_ENUM_CONSTANT(WORKSHOP_PREVIEW_TYPE_SKETCHFAB);
	BIND_ENUM_CONSTANT(WORKSHOP_PREVIEW_TYPE_ENVIRONMENT_MAP_HORIZONTAL_CROSS);
	BIND_ENUM_CONSTANT(WORKSHOP_PREVIEW_TYPE_ENVIRONMENT_MAP_LAT_LONG);
	BIND_ENUM_CONSTANT(WORKSHOP_PREVIEW_TYPE_CLIP);
}

// -----------------------------------------------------------------------------
// Internal helpers
// -----------------------------------------------------------------------------

bool Steam::_ensure_workshop_ready() const {
	return initialized && steam_ugc && loader.has_workshop_support();
}

uint32_t Steam::_get_workshop_app_id() const {
	if (workshop_consumer_app_id > 0) {
		return (uint32_t)workshop_consumer_app_id;
	}
	if (app_id > 0) {
		return (uint32_t)app_id;
	}
	return loader.get_app_id();
}

uint32_t Steam::_get_workshop_creator_app_id() const {
	if (workshop_creator_app_id > 0) {
		return (uint32_t)workshop_creator_app_id;
	}
	return _get_workshop_app_id();
}

void Steam::set_workshop_app_ids(int p_consumer_app_id, int p_creator_app_id) {
	ERR_FAIL_COND_MSG(p_consumer_app_id < 0 || p_creator_app_id < 0, "Workshop app IDs can't be negative.");
	workshop_consumer_app_id = p_consumer_app_id;
	workshop_creator_app_id = p_creator_app_id;
	_log_debug(vformat("Workshop app IDs: consumer=%d creator=%d", (int64_t)_get_workshop_app_id(), (int64_t)_get_workshop_creator_app_id()));
}

int Steam::get_workshop_consumer_app_id() const {
	return (int)_get_workshop_app_id();
}

int Steam::get_workshop_creator_app_id() const {
	return (int)_get_workshop_creator_app_id();
}

bool Steam::_track_workshop_call(SteamAPICallHandle_t p_call, int p_callback_id, uint64_t p_published_file_id, uint64_t p_query_handle, bool p_flag) {
	if (p_call == STEAM_API_CALL_INVALID) {
		return false;
	}
	WorkshopPendingCall pending;
	pending.callback_id = p_callback_id;
	pending.published_file_id = p_published_file_id;
	pending.query_handle = p_query_handle;
	pending.flag = p_flag;
	workshop_pending_calls[p_call] = pending;
	return true;
}

void Steam::_clear_workshop_state() {
	workshop_pending_calls.clear();
	workshop_query_return_flags.clear();
}

void Steam::_handle_workshop_call_failure(const WorkshopPendingCall &p_call) {
	const int result = STEAM_RESULT_IO_FAILURE;
	const int64_t file_id = (int64_t)p_call.published_file_id;
	_log_debug(vformat("Workshop call (callback %d) failed with an IO failure", p_call.callback_id));

	switch (p_call.callback_id) {
		case SteamUGCQueryCompleted::k_iCallback: {
			if (_ensure_workshop_ready()) {
				loader.ugc_release_query_request(steam_ugc, p_call.query_handle);
			}
			workshop_query_return_flags.erase(p_call.query_handle);
			emit_signal("workshop_query_completed", (int64_t)p_call.query_handle, result, Array(), 0, String());
		} break;
		case SteamUGCCreateItemResult::k_iCallback: {
			emit_signal("workshop_item_created", (int64_t)0, result, false);
		} break;
		case SteamUGCSubmitItemUpdateResult::k_iCallback: {
			emit_signal("workshop_item_updated", file_id, result, false);
		} break;
		case SteamUGCDeleteItemResult::k_iCallback: {
			emit_signal("workshop_item_deleted", file_id, result);
		} break;
		case SteamRemoteStorageSubscribePublishedFileResult::k_iCallback: {
			emit_signal("workshop_item_subscribed", file_id, result);
		} break;
		case SteamRemoteStorageUnsubscribePublishedFileResult::k_iCallback: {
			emit_signal("workshop_item_unsubscribed", file_id, result);
		} break;
		case SteamUGCSetUserItemVoteResult::k_iCallback: {
			emit_signal("workshop_item_vote_set", file_id, result, p_call.flag);
		} break;
		case SteamUGCUserFavoriteItemsListChanged::k_iCallback: {
			emit_signal("workshop_favorites_changed", file_id, result, p_call.flag);
		} break;
		case STEAM_UGC_START_PLAYTIME_TRACKING_RESULT_CALLBACK:
		case STEAM_UGC_STOP_PLAYTIME_TRACKING_RESULT_CALLBACK: {
			emit_signal("workshop_playtime_tracking_updated", result, p_call.flag);
		} break;
		default:
			break;
	}
}

bool Steam::_handle_workshop_callback(int p_callback_id, const void *p_data, int p_size) {
	switch (p_callback_id) {
		case SteamUGCQueryCompleted::k_iCallback: {
			if (p_size < (int)sizeof(SteamUGCQueryCompleted)) {
				return true;
			}
			const SteamUGCQueryCompleted *response = (const SteamUGCQueryCompleted *)p_data;
			const SteamUGCQueryHandle_t handle = response->m_handle;
			Array items;
			if (response->m_eResult == STEAM_RESULT_OK && _ensure_workshop_ready()) {
				items = _parse_workshop_query_results(handle, response->m_unNumResultsReturned);
			}
			if (_ensure_workshop_ready()) {
				loader.ugc_release_query_request(steam_ugc, handle);
			}
			workshop_query_return_flags.erase(handle);
			const String next_cursor = _workshop_fixed_string(response->m_rgchNextCursor, sizeof(response->m_rgchNextCursor));
			_log_debug(vformat("Workshop query %d completed (result=%d, returned=%d, total=%d)", (int64_t)handle, response->m_eResult, (int)response->m_unNumResultsReturned, (int)response->m_unTotalMatchingResults));
			emit_signal("workshop_query_completed", (int64_t)handle, response->m_eResult, items, (int64_t)response->m_unTotalMatchingResults, next_cursor);
			return true;
		}
		case SteamUGCCreateItemResult::k_iCallback: {
			if (p_size < (int)sizeof(SteamUGCCreateItemResult)) {
				return true;
			}
			const SteamUGCCreateItemResult *response = (const SteamUGCCreateItemResult *)p_data;
			_log_debug(vformat("Workshop item created id=%d result=%d", (int64_t)response->m_nPublishedFileId, response->m_eResult));
			emit_signal("workshop_item_created", (int64_t)response->m_nPublishedFileId, response->m_eResult, response->m_bUserNeedsToAcceptWorkshopLegalAgreement);
			return true;
		}
		case SteamUGCSubmitItemUpdateResult::k_iCallback: {
			if (p_size < (int)sizeof(SteamUGCSubmitItemUpdateResult)) {
				return true;
			}
			const SteamUGCSubmitItemUpdateResult *response = (const SteamUGCSubmitItemUpdateResult *)p_data;
			_log_debug(vformat("Workshop item updated id=%d result=%d", (int64_t)response->m_nPublishedFileId, response->m_eResult));
			emit_signal("workshop_item_updated", (int64_t)response->m_nPublishedFileId, response->m_eResult, response->m_bUserNeedsToAcceptWorkshopLegalAgreement);
			return true;
		}
		case SteamUGCDeleteItemResult::k_iCallback: {
			if (p_size < (int)sizeof(SteamUGCDeleteItemResult)) {
				return true;
			}
			const SteamUGCDeleteItemResult *response = (const SteamUGCDeleteItemResult *)p_data;
			emit_signal("workshop_item_deleted", (int64_t)response->m_nPublishedFileId, response->m_eResult);
			return true;
		}
		case SteamRemoteStorageSubscribePublishedFileResult::k_iCallback: {
			if (p_size < (int)sizeof(SteamRemoteStorageSubscribePublishedFileResult)) {
				return true;
			}
			const SteamRemoteStorageSubscribePublishedFileResult *response = (const SteamRemoteStorageSubscribePublishedFileResult *)p_data;
			_log_debug(vformat("Workshop subscribe id=%d result=%d", (int64_t)response->m_nPublishedFileId, response->m_eResult));
			emit_signal("workshop_item_subscribed", (int64_t)response->m_nPublishedFileId, response->m_eResult);
			return true;
		}
		case SteamRemoteStorageUnsubscribePublishedFileResult::k_iCallback: {
			if (p_size < (int)sizeof(SteamRemoteStorageUnsubscribePublishedFileResult)) {
				return true;
			}
			const SteamRemoteStorageUnsubscribePublishedFileResult *response = (const SteamRemoteStorageUnsubscribePublishedFileResult *)p_data;
			_log_debug(vformat("Workshop unsubscribe id=%d result=%d", (int64_t)response->m_nPublishedFileId, response->m_eResult));
			emit_signal("workshop_item_unsubscribed", (int64_t)response->m_nPublishedFileId, response->m_eResult);
			return true;
		}
		case SteamUGCSetUserItemVoteResult::k_iCallback: {
			if (p_size < (int)sizeof(SteamUGCSetUserItemVoteResult)) {
				return true;
			}
			const SteamUGCSetUserItemVoteResult *response = (const SteamUGCSetUserItemVoteResult *)p_data;
			emit_signal("workshop_item_vote_set", (int64_t)response->m_nPublishedFileId, response->m_eResult, response->m_bVoteUp);
			return true;
		}
		case SteamUGCUserFavoriteItemsListChanged::k_iCallback: {
			if (p_size < (int)sizeof(SteamUGCUserFavoriteItemsListChanged)) {
				return true;
			}
			const SteamUGCUserFavoriteItemsListChanged *response = (const SteamUGCUserFavoriteItemsListChanged *)p_data;
			emit_signal("workshop_favorites_changed", (int64_t)response->m_nPublishedFileId, response->m_eResult, response->m_bWasAddRequest);
			return true;
		}
		case STEAM_UGC_START_PLAYTIME_TRACKING_RESULT_CALLBACK:
		case STEAM_UGC_STOP_PLAYTIME_TRACKING_RESULT_CALLBACK: {
			if (p_size < (int)sizeof(SteamUGCPlaytimeTrackingResult)) {
				return true;
			}
			const SteamUGCPlaytimeTrackingResult *response = (const SteamUGCPlaytimeTrackingResult *)p_data;
			emit_signal("workshop_playtime_tracking_updated", response->m_eResult, p_callback_id == STEAM_UGC_START_PLAYTIME_TRACKING_RESULT_CALLBACK);
			return true;
		}
		case SteamUGCItemInstalled::k_iCallback: {
			if (p_size < (int)sizeof(SteamUGCItemInstalled)) {
				return true;
			}
			const SteamUGCItemInstalled *response = (const SteamUGCItemInstalled *)p_data;
			const uint32_t our_app_id = _get_workshop_app_id();
			if (our_app_id != 0 && response->m_unAppID != our_app_id) {
				return true;
			}
			_log_debug(vformat("Workshop item installed id=%d", (int64_t)response->m_nPublishedFileId));
			emit_signal("workshop_item_installed", (int64_t)response->m_nPublishedFileId);
			return true;
		}
		case SteamUGCDownloadItemResult::k_iCallback: {
			if (p_size < (int)sizeof(SteamUGCDownloadItemResult)) {
				return true;
			}
			const SteamUGCDownloadItemResult *response = (const SteamUGCDownloadItemResult *)p_data;
			const uint32_t our_app_id = _get_workshop_app_id();
			if (our_app_id != 0 && response->m_unAppID != our_app_id) {
				return true;
			}
			_log_debug(vformat("Workshop item downloaded id=%d result=%d", (int64_t)response->m_nPublishedFileId, response->m_eResult));
			emit_signal("workshop_item_downloaded", (int64_t)response->m_nPublishedFileId, response->m_eResult);
			return true;
		}
		case SteamUGCUserSubscribedItemsListChanged::k_iCallback: {
			if (p_size < (int)sizeof(SteamUGCUserSubscribedItemsListChanged)) {
				return true;
			}
			const SteamUGCUserSubscribedItemsListChanged *response = (const SteamUGCUserSubscribedItemsListChanged *)p_data;
			const uint32_t our_app_id = _get_workshop_app_id();
			if (our_app_id != 0 && response->m_nAppID != our_app_id) {
				return true;
			}
			emit_signal("workshop_subscriptions_changed");
			return true;
		}
		default:
			return false;
	}
}

Ref<SteamWorkshopItem> Steam::_build_workshop_item(SteamUGCQueryHandle_t p_handle, uint32_t p_index, uint32_t p_return_flags) const {
	Ref<SteamWorkshopItem> item;
	SteamUGCDetails details;
	if (!loader.ugc_get_query_result(steam_ugc, p_handle, p_index, details)) {
		return item;
	}

	item.instantiate();
	item->set_published_file_id(details.m_nPublishedFileId);
	item->set_result(details.m_eResult);
	item->set_file_type(details.m_eFileType);
	item->set_creator_app_id(details.m_nCreatorAppID);
	item->set_consumer_app_id(details.m_nConsumerAppID);
	item->set_title(_workshop_fixed_string(details.m_rgchTitle, sizeof(details.m_rgchTitle)));
	item->set_description(_workshop_fixed_string(details.m_rgchDescription, sizeof(details.m_rgchDescription)));
	item->set_owner_steam_id(details.m_ulSteamIDOwner);
	item->set_time_created((int64_t)details.m_rtimeCreated);
	item->set_time_updated((int64_t)details.m_rtimeUpdated);
	item->set_time_added_to_user_list((int64_t)details.m_rtimeAddedToUserList);
	item->set_visibility(details.m_eVisibility);
	item->set_banned(details.m_bBanned);
	item->set_accepted_for_use(details.m_bAcceptedForUse);
	item->set_tags_truncated(details.m_bTagsTruncated);
	item->set_file_name(_workshop_fixed_string(details.m_pchFileName, sizeof(details.m_pchFileName)));
	item->set_file_size((int64_t)details.m_nFileSize);
	item->set_preview_file_size((int64_t)details.m_nPreviewFileSize);
	item->set_total_files_size(details.m_ulTotalFilesSize);
	item->set_url(_workshop_fixed_string(details.m_rgchURL, sizeof(details.m_rgchURL)));
	item->set_votes_up((int)details.m_unVotesUp);
	item->set_votes_down((int)details.m_unVotesDown);
	item->set_score(details.m_flScore);
	item->set_num_children((int)details.m_unNumChildren);

	PackedStringArray tags;
	const PackedStringArray raw_tags = _workshop_fixed_string(details.m_rgchTags, sizeof(details.m_rgchTags)).split(",", false);
	for (int i = 0; i < raw_tags.size(); i++) {
		const String tag = raw_tags[i].strip_edges();
		if (!tag.is_empty()) {
			tags.push_back(tag);
		}
	}
	item->set_tags(tags);

	item->set_preview_url(loader.ugc_get_query_preview_url(steam_ugc, p_handle, p_index));

	Dictionary statistics;
	for (const WorkshopStatisticName &stat : WORKSHOP_STATISTICS) {
		uint64_t value = 0;
		if (loader.ugc_get_query_statistic(steam_ugc, p_handle, p_index, stat.id, value)) {
			statistics[stat.name] = (int64_t)value;
		}
	}
	item->set_statistics(statistics);

	if (p_return_flags & WORKSHOP_QUERY_RETURN_METADATA) {
		item->set_metadata(loader.ugc_get_query_metadata(steam_ugc, p_handle, p_index));
	}

	if (p_return_flags & WORKSHOP_QUERY_RETURN_KEY_VALUE_TAGS) {
		// Keys may repeat, so every key maps to an array of values.
		Dictionary key_value_tags;
		const uint32_t count = loader.ugc_get_query_num_key_value_tags(steam_ugc, p_handle, p_index);
		for (uint32_t i = 0; i < count; i++) {
			String key;
			String value;
			if (!loader.ugc_get_query_key_value_tag(steam_ugc, p_handle, p_index, i, key, value)) {
				continue;
			}
			PackedStringArray values;
			if (key_value_tags.has(key)) {
				values = key_value_tags[key];
			}
			values.push_back(value);
			key_value_tags[key] = values;
		}
		item->set_key_value_tags(key_value_tags);
	}

	if ((p_return_flags & WORKSHOP_QUERY_RETURN_CHILDREN) && details.m_unNumChildren > 0) {
		const Vector<SteamPublishedFileId_t> children = loader.ugc_get_query_children(steam_ugc, p_handle, p_index, details.m_unNumChildren);
		PackedInt64Array child_ids;
		for (int i = 0; i < children.size(); i++) {
			child_ids.push_back((int64_t)children[i]);
		}
		item->set_children(child_ids);
	}

	if (p_return_flags & WORKSHOP_QUERY_RETURN_ADDITIONAL_PREVIEWS) {
		Array previews;
		const uint32_t count = loader.ugc_get_query_num_additional_previews(steam_ugc, p_handle, p_index);
		for (uint32_t i = 0; i < count; i++) {
			String url_or_video_id;
			String original_file_name;
			int preview_type = 0;
			if (!loader.ugc_get_query_additional_preview(steam_ugc, p_handle, p_index, i, url_or_video_id, original_file_name, preview_type)) {
				continue;
			}
			Dictionary preview;
			preview["url_or_video_id"] = url_or_video_id;
			preview["original_file_name"] = original_file_name;
			preview["type"] = preview_type;
			previews.push_back(preview);
		}
		item->set_additional_previews(previews);
	}

	return item;
}

Array Steam::_parse_workshop_query_results(SteamUGCQueryHandle_t p_handle, uint32_t p_num_results) {
	Array items;
	const uint32_t *flags = workshop_query_return_flags.getptr(p_handle);
	const uint32_t return_flags = flags ? *flags : 0;
	for (uint32_t i = 0; i < p_num_results; i++) {
		Ref<SteamWorkshopItem> item = _build_workshop_item(p_handle, i, return_flags);
		if (item.is_valid()) {
			items.push_back(item);
		}
	}
	return items;
}

int64_t Steam::_send_workshop_query(SteamUGCQueryHandle_t p_handle, const Dictionary &p_options) {
	if (p_handle == STEAM_UGC_QUERY_HANDLE_INVALID) {
		_log_debug("Failed to create Workshop query");
		return -1;
	}

	uint32_t return_flags = 0;

	if (p_options.has("required_tags")) {
		const PackedStringArray tags = p_options["required_tags"];
		for (int i = 0; i < tags.size(); i++) {
			loader.ugc_add_required_tag(steam_ugc, p_handle, tags[i].utf8().get_data());
		}
	}
	if (p_options.has("excluded_tags")) {
		const PackedStringArray tags = p_options["excluded_tags"];
		for (int i = 0; i < tags.size(); i++) {
			loader.ugc_add_excluded_tag(steam_ugc, p_handle, tags[i].utf8().get_data());
		}
	}
	if (p_options.has("required_key_value_tags")) {
		const Dictionary key_values = p_options["required_key_value_tags"];
		const Array keys = key_values.keys();
		for (int i = 0; i < keys.size(); i++) {
			const String key = keys[i];
			const String value = key_values[keys[i]];
			loader.ugc_add_required_key_value_tag(steam_ugc, p_handle, key.utf8().get_data(), value.utf8().get_data());
		}
	}
	if (p_options.has("match_any_tag")) {
		loader.ugc_set_match_any_tag(steam_ugc, p_handle, (bool)p_options["match_any_tag"]);
	}
	if (p_options.has("search_text")) {
		const String text = p_options["search_text"];
		loader.ugc_set_search_text(steam_ugc, p_handle, text.utf8().get_data());
	}
	if (p_options.has("ranked_by_trend_days")) {
		loader.ugc_set_ranked_by_trend_days(steam_ugc, p_handle, (uint32_t)(int)p_options["ranked_by_trend_days"]);
	}
	if (p_options.has("cloud_file_name_filter")) {
		const String file_name = p_options["cloud_file_name_filter"];
		loader.ugc_set_cloud_file_name_filter(steam_ugc, p_handle, file_name.utf8().get_data());
	}
	if (p_options.has("language")) {
		const String language = p_options["language"];
		loader.ugc_set_language(steam_ugc, p_handle, language.utf8().get_data());
	}
	if (p_options.has("allow_cached_response")) {
		loader.ugc_set_allow_cached_response(steam_ugc, p_handle, (uint32_t)(int)p_options["allow_cached_response"]);
	}
	if (p_options.has("return_only_ids")) {
		loader.ugc_set_return_only_ids(steam_ugc, p_handle, (bool)p_options["return_only_ids"]);
	}
	if (p_options.has("return_total_only")) {
		loader.ugc_set_return_total_only(steam_ugc, p_handle, (bool)p_options["return_total_only"]);
	}
	if (p_options.has("return_long_description")) {
		loader.ugc_set_return_long_description(steam_ugc, p_handle, (bool)p_options["return_long_description"]);
	}
	if ((bool)p_options.get("return_metadata", false)) {
		loader.ugc_set_return_metadata(steam_ugc, p_handle, true);
		return_flags |= WORKSHOP_QUERY_RETURN_METADATA;
	}
	if ((bool)p_options.get("return_key_value_tags", false)) {
		loader.ugc_set_return_key_value_tags(steam_ugc, p_handle, true);
		return_flags |= WORKSHOP_QUERY_RETURN_KEY_VALUE_TAGS;
	}
	if ((bool)p_options.get("return_children", false)) {
		loader.ugc_set_return_children(steam_ugc, p_handle, true);
		return_flags |= WORKSHOP_QUERY_RETURN_CHILDREN;
	}
	if ((bool)p_options.get("return_additional_previews", false)) {
		loader.ugc_set_return_additional_previews(steam_ugc, p_handle, true);
		return_flags |= WORKSHOP_QUERY_RETURN_ADDITIONAL_PREVIEWS;
	}

	const SteamAPICallHandle_t call = loader.ugc_send_query_request(steam_ugc, p_handle);
	if (!_track_workshop_call(call, SteamUGCQueryCompleted::k_iCallback, 0, p_handle)) {
		_log_debug("SendQueryUGCRequest failed");
		loader.ugc_release_query_request(steam_ugc, p_handle);
		return -1;
	}
	workshop_query_return_flags[p_handle] = return_flags;
	_log_debug(vformat("Workshop query %d sent", (int64_t)p_handle));
	return (int64_t)p_handle;
}

// -----------------------------------------------------------------------------
// Public API
// -----------------------------------------------------------------------------

bool Steam::has_workshop_support() const {
	return loader.has_workshop_support();
}

int Steam::get_num_subscribed_items(bool p_include_locally_disabled) const {
	if (!_ensure_workshop_ready()) {
		return 0;
	}
	return (int)loader.ugc_get_num_subscribed_items(steam_ugc, p_include_locally_disabled);
}

PackedInt64Array Steam::get_subscribed_items(bool p_include_locally_disabled) const {
	PackedInt64Array out;
	if (!_ensure_workshop_ready()) {
		return out;
	}
	const Vector<SteamPublishedFileId_t> items = loader.ugc_get_subscribed_items(steam_ugc, p_include_locally_disabled);
	for (int i = 0; i < items.size(); i++) {
		out.push_back((int64_t)items[i]);
	}
	return out;
}

BitField<Steam::WorkshopItemState> Steam::get_item_state(uint64_t p_published_file_id) const {
	if (!_ensure_workshop_ready() || p_published_file_id == 0) {
		return WORKSHOP_ITEM_STATE_NONE;
	}
	return (int64_t)loader.ugc_get_item_state(steam_ugc, p_published_file_id);
}

Dictionary Steam::get_item_install_info(uint64_t p_published_file_id) const {
	Dictionary info;
	if (!_ensure_workshop_ready() || p_published_file_id == 0) {
		return info;
	}
	uint64_t size_on_disk = 0;
	String folder;
	uint32_t timestamp = 0;
	if (!loader.ugc_get_item_install_info(steam_ugc, p_published_file_id, size_on_disk, folder, timestamp)) {
		return info;
	}
	info["size_on_disk"] = (int64_t)size_on_disk;
	info["folder"] = folder;
	info["timestamp"] = (int64_t)timestamp;
	return info;
}

Dictionary Steam::get_item_download_info(uint64_t p_published_file_id) const {
	Dictionary info;
	if (!_ensure_workshop_ready() || p_published_file_id == 0) {
		return info;
	}
	uint64_t downloaded = 0;
	uint64_t total = 0;
	if (!loader.ugc_get_item_download_info(steam_ugc, p_published_file_id, downloaded, total)) {
		return info;
	}
	info["bytes_downloaded"] = (int64_t)downloaded;
	info["bytes_total"] = (int64_t)total;
	info["progress"] = total > 0 ? (double)downloaded / (double)total : 0.0;
	return info;
}

bool Steam::download_item(uint64_t p_published_file_id, bool p_high_priority) {
	if (!_ensure_workshop_ready() || p_published_file_id == 0) {
		return false;
	}
	const bool ok = loader.ugc_download_item(steam_ugc, p_published_file_id, p_high_priority);
	_log_debug(vformat("download_item(%d, %s) -> %s", (int64_t)p_published_file_id, p_high_priority ? "high" : "normal", ok ? "true" : "false"));
	return ok;
}

void Steam::suspend_downloads(bool p_suspend) {
	if (!_ensure_workshop_ready()) {
		return;
	}
	loader.ugc_suspend_downloads(steam_ugc, p_suspend);
}

bool Steam::subscribe_item(uint64_t p_published_file_id) {
	if (!_ensure_workshop_ready() || p_published_file_id == 0) {
		return false;
	}
	return _track_workshop_call(loader.ugc_subscribe_item(steam_ugc, p_published_file_id), SteamRemoteStorageSubscribePublishedFileResult::k_iCallback, p_published_file_id);
}

bool Steam::unsubscribe_item(uint64_t p_published_file_id) {
	if (!_ensure_workshop_ready() || p_published_file_id == 0) {
		return false;
	}
	return _track_workshop_call(loader.ugc_unsubscribe_item(steam_ugc, p_published_file_id), SteamRemoteStorageUnsubscribePublishedFileResult::k_iCallback, p_published_file_id);
}

int64_t Steam::query_workshop_items(WorkshopQueryType p_query_type, WorkshopMatchingType p_matching_type, int p_page, const Dictionary &p_options) {
	if (!_ensure_workshop_ready()) {
		return -1;
	}
	const uint32_t workshop_app_id = _get_workshop_app_id();
	const uint32_t creator_app_id = _get_workshop_creator_app_id();
	SteamUGCQueryHandle_t handle = STEAM_UGC_QUERY_HANDLE_INVALID;
	if (p_options.has("cursor")) {
		// Cursor-based paging; pass "*" for the first page.
		const String cursor = p_options["cursor"];
		handle = loader.ugc_create_query_all_request_cursor(steam_ugc, (int)p_query_type, (int)p_matching_type, creator_app_id, workshop_app_id, cursor.utf8().get_data());
	} else {
		handle = loader.ugc_create_query_all_request_page(steam_ugc, (int)p_query_type, (int)p_matching_type, creator_app_id, workshop_app_id, (uint32_t)MAX(p_page, 1));
	}
	return _send_workshop_query(handle, p_options);
}

int64_t Steam::query_user_workshop_items(WorkshopUserList p_list_type, WorkshopMatchingType p_matching_type, WorkshopUserListSortOrder p_sort_order, int p_page, uint64_t p_steam_id, const Dictionary &p_options) {
	if (!_ensure_workshop_ready()) {
		return -1;
	}
	const uint64_t steam_id = p_steam_id != 0 ? p_steam_id : get_local_steam_id();
	if (steam_id == 0) {
		return -1;
	}
	// The account ID is the low 32 bits of a 64-bit Steam ID.
	const uint32_t account_id = (uint32_t)(steam_id & 0xFFFFFFFFULL);
	const uint32_t workshop_app_id = _get_workshop_app_id();
	const uint32_t creator_app_id = _get_workshop_creator_app_id();
	const SteamUGCQueryHandle_t handle = loader.ugc_create_query_user_request(steam_ugc, account_id, (int)p_list_type, (int)p_matching_type, (int)p_sort_order, creator_app_id, workshop_app_id, (uint32_t)MAX(p_page, 1));
	return _send_workshop_query(handle, p_options);
}

int64_t Steam::query_workshop_item_details(const PackedInt64Array &p_published_file_ids, const Dictionary &p_options) {
	if (!_ensure_workshop_ready()) {
		return -1;
	}
	const Vector<SteamPublishedFileId_t> ids = _workshop_ids_from_array(p_published_file_ids);
	if (ids.is_empty()) {
		return -1;
	}
	return _send_workshop_query(loader.ugc_create_query_details_request(steam_ugc, ids), p_options);
}

bool Steam::create_workshop_item(WorkshopFileType p_file_type) {
	if (!_ensure_workshop_ready()) {
		return false;
	}
	const uint32_t workshop_app_id = _get_workshop_app_id();
	if (workshop_app_id == 0) {
		_log_debug("create_workshop_item: unknown app ID; pass it to Steam.initialize()");
		return false;
	}
	return _track_workshop_call(loader.ugc_create_item(steam_ugc, workshop_app_id, (int)p_file_type), SteamUGCCreateItemResult::k_iCallback);
}

int64_t Steam::start_item_update(uint64_t p_published_file_id) {
	if (!_ensure_workshop_ready() || p_published_file_id == 0) {
		return -1;
	}
	const uint32_t workshop_app_id = _get_workshop_app_id();
	if (workshop_app_id == 0) {
		_log_debug("start_item_update: unknown app ID; pass it to Steam.initialize()");
		return -1;
	}
	const SteamUGCUpdateHandle_t handle = loader.ugc_start_item_update(steam_ugc, workshop_app_id, p_published_file_id);
	_log_debug(vformat("start_item_update(%d) -> %d", (int64_t)p_published_file_id, (int64_t)handle));
	return (int64_t)handle;
}

#define WORKSHOP_CHECK_UPDATE_HANDLE() \
	if (!_ensure_workshop_ready() || (SteamUGCUpdateHandle_t)p_update_handle == STEAM_UGC_UPDATE_HANDLE_INVALID) { \
		return false; \
	}

bool Steam::set_item_title(uint64_t p_update_handle, const String &p_title) {
	WORKSHOP_CHECK_UPDATE_HANDLE();
	return loader.ugc_set_item_title(steam_ugc, p_update_handle, p_title.utf8().get_data());
}

bool Steam::set_item_description(uint64_t p_update_handle, const String &p_description) {
	WORKSHOP_CHECK_UPDATE_HANDLE();
	return loader.ugc_set_item_description(steam_ugc, p_update_handle, p_description.utf8().get_data());
}

bool Steam::set_item_update_language(uint64_t p_update_handle, const String &p_language) {
	WORKSHOP_CHECK_UPDATE_HANDLE();
	return loader.ugc_set_item_update_language(steam_ugc, p_update_handle, p_language.utf8().get_data());
}

bool Steam::set_item_metadata(uint64_t p_update_handle, const String &p_metadata) {
	WORKSHOP_CHECK_UPDATE_HANDLE();
	const CharString metadata_utf8 = p_metadata.utf8();
	if ((uint32_t)metadata_utf8.length() > STEAM_UGC_DEVELOPER_METADATA_MAX) {
		_log_debug(vformat("set_item_metadata: metadata is %d bytes, Steam allows at most %d", metadata_utf8.length(), (int)STEAM_UGC_DEVELOPER_METADATA_MAX));
		return false;
	}
	return loader.ugc_set_item_metadata(steam_ugc, p_update_handle, metadata_utf8.get_data());
}

bool Steam::set_item_visibility(uint64_t p_update_handle, WorkshopVisibility p_visibility) {
	WORKSHOP_CHECK_UPDATE_HANDLE();
	return loader.ugc_set_item_visibility(steam_ugc, p_update_handle, (int)p_visibility);
}

bool Steam::set_item_tags(uint64_t p_update_handle, const PackedStringArray &p_tags, bool p_allow_admin_tags) {
	WORKSHOP_CHECK_UPDATE_HANDLE();
	Vector<String> tags;
	for (int i = 0; i < p_tags.size(); i++) {
		tags.push_back(p_tags[i]);
	}
	return loader.ugc_set_item_tags(steam_ugc, p_update_handle, tags, p_allow_admin_tags);
}

bool Steam::set_item_content(uint64_t p_update_handle, const String &p_content_folder) {
	WORKSHOP_CHECK_UPDATE_HANDLE();
	const String path = _workshop_native_path(p_content_folder);
	return loader.ugc_set_item_content(steam_ugc, p_update_handle, path.utf8().get_data());
}

bool Steam::set_item_preview(uint64_t p_update_handle, const String &p_preview_file) {
	WORKSHOP_CHECK_UPDATE_HANDLE();
	const String path = _workshop_native_path(p_preview_file);
	return loader.ugc_set_item_preview(steam_ugc, p_update_handle, path.utf8().get_data());
}

bool Steam::add_item_key_value_tag(uint64_t p_update_handle, const String &p_key, const String &p_value) {
	WORKSHOP_CHECK_UPDATE_HANDLE();
	return loader.ugc_add_item_key_value_tag(steam_ugc, p_update_handle, p_key.utf8().get_data(), p_value.utf8().get_data());
}

bool Steam::remove_item_key_value_tags(uint64_t p_update_handle, const String &p_key) {
	WORKSHOP_CHECK_UPDATE_HANDLE();
	return loader.ugc_remove_item_key_value_tags(steam_ugc, p_update_handle, p_key.utf8().get_data());
}

bool Steam::remove_all_item_key_value_tags(uint64_t p_update_handle) {
	WORKSHOP_CHECK_UPDATE_HANDLE();
	return loader.ugc_remove_all_item_key_value_tags(steam_ugc, p_update_handle);
}

bool Steam::add_item_preview_file(uint64_t p_update_handle, const String &p_preview_file, WorkshopPreviewType p_type) {
	WORKSHOP_CHECK_UPDATE_HANDLE();
	const String path = _workshop_native_path(p_preview_file);
	return loader.ugc_add_item_preview_file(steam_ugc, p_update_handle, path.utf8().get_data(), (int)p_type);
}

bool Steam::add_item_preview_video(uint64_t p_update_handle, const String &p_video_id) {
	WORKSHOP_CHECK_UPDATE_HANDLE();
	return loader.ugc_add_item_preview_video(steam_ugc, p_update_handle, p_video_id.utf8().get_data());
}

bool Steam::remove_item_preview(uint64_t p_update_handle, int p_index) {
	WORKSHOP_CHECK_UPDATE_HANDLE();
	if (p_index < 0) {
		return false;
	}
	return loader.ugc_remove_item_preview(steam_ugc, p_update_handle, (uint32_t)p_index);
}

bool Steam::submit_item_update(uint64_t p_update_handle, const String &p_change_note) {
	WORKSHOP_CHECK_UPDATE_HANDLE();
	const CharString change_note = p_change_note.utf8();
	const SteamAPICallHandle_t call = loader.ugc_submit_item_update(steam_ugc, p_update_handle, p_change_note.is_empty() ? nullptr : change_note.get_data());
	_log_debug(vformat("submit_item_update(%d) -> call %d", (int64_t)p_update_handle, (int64_t)call));
	return _track_workshop_call(call, SteamUGCSubmitItemUpdateResult::k_iCallback);
}

#undef WORKSHOP_CHECK_UPDATE_HANDLE

Dictionary Steam::get_item_update_progress(uint64_t p_update_handle) const {
	Dictionary progress;
	uint64_t processed = 0;
	uint64_t total = 0;
	int status = WORKSHOP_UPDATE_STATUS_INVALID;
	if (_ensure_workshop_ready() && (SteamUGCUpdateHandle_t)p_update_handle != STEAM_UGC_UPDATE_HANDLE_INVALID) {
		status = loader.ugc_get_item_update_progress(steam_ugc, p_update_handle, processed, total);
	}
	progress["status"] = status;
	progress["bytes_processed"] = (int64_t)processed;
	progress["bytes_total"] = (int64_t)total;
	progress["progress"] = total > 0 ? (double)processed / (double)total : 0.0;
	return progress;
}

bool Steam::delete_workshop_item(uint64_t p_published_file_id) {
	if (!_ensure_workshop_ready() || p_published_file_id == 0) {
		return false;
	}
	return _track_workshop_call(loader.ugc_delete_item(steam_ugc, p_published_file_id), SteamUGCDeleteItemResult::k_iCallback, p_published_file_id);
}

bool Steam::set_user_item_vote(uint64_t p_published_file_id, bool p_vote_up) {
	if (!_ensure_workshop_ready() || p_published_file_id == 0) {
		return false;
	}
	return _track_workshop_call(loader.ugc_set_user_item_vote(steam_ugc, p_published_file_id, p_vote_up), SteamUGCSetUserItemVoteResult::k_iCallback, p_published_file_id, 0, p_vote_up);
}

bool Steam::add_item_to_favorites(uint64_t p_published_file_id) {
	if (!_ensure_workshop_ready() || p_published_file_id == 0) {
		return false;
	}
	return _track_workshop_call(loader.ugc_add_item_to_favorites(steam_ugc, _get_workshop_app_id(), p_published_file_id), SteamUGCUserFavoriteItemsListChanged::k_iCallback, p_published_file_id, 0, true);
}

bool Steam::remove_item_from_favorites(uint64_t p_published_file_id) {
	if (!_ensure_workshop_ready() || p_published_file_id == 0) {
		return false;
	}
	return _track_workshop_call(loader.ugc_remove_item_from_favorites(steam_ugc, _get_workshop_app_id(), p_published_file_id), SteamUGCUserFavoriteItemsListChanged::k_iCallback, p_published_file_id, 0, false);
}

bool Steam::start_playtime_tracking(const PackedInt64Array &p_published_file_ids) {
	if (!_ensure_workshop_ready()) {
		return false;
	}
	const Vector<SteamPublishedFileId_t> ids = _workshop_ids_from_array(p_published_file_ids);
	return _track_workshop_call(loader.ugc_start_playtime_tracking(steam_ugc, ids), STEAM_UGC_START_PLAYTIME_TRACKING_RESULT_CALLBACK, 0, 0, true);
}

bool Steam::stop_playtime_tracking(const PackedInt64Array &p_published_file_ids) {
	if (!_ensure_workshop_ready()) {
		return false;
	}
	const Vector<SteamPublishedFileId_t> ids = _workshop_ids_from_array(p_published_file_ids);
	return _track_workshop_call(loader.ugc_stop_playtime_tracking(steam_ugc, ids), STEAM_UGC_STOP_PLAYTIME_TRACKING_RESULT_CALLBACK, 0, 0, false);
}

bool Steam::stop_playtime_tracking_for_all_items() {
	if (!_ensure_workshop_ready()) {
		return false;
	}
	return _track_workshop_call(loader.ugc_stop_playtime_tracking_for_all_items(steam_ugc), STEAM_UGC_STOP_PLAYTIME_TRACKING_RESULT_CALLBACK, 0, 0, false);
}

bool Steam::show_workshop_eula() {
	if (!_ensure_workshop_ready()) {
		return false;
	}
	return loader.ugc_show_workshop_eula(steam_ugc);
}
