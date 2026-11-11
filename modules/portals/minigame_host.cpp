/**************************************************************************/
/*  minigame_host.cpp                                                     */
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

#include "minigame_host.h"

#include "portal_runtime.h"

#include "core/crypto/crypto_core.h"
#include "core/object/callable_method_pointer.h"
#include "core/object/class_db.h"

template <class T>
void MiniGameApi<T>::bind_shared() {
	PortalApi<T>::bind_common();
	ClassDB::bind_method(D_METHOD("login"), &T::login);
	ClassDB::bind_method(D_METHOD("check_session"), &T::check_session);
	ClassDB::bind_method(D_METHOD("get_profile"), &T::get_profile);
	ClassDB::bind_method(D_METHOD("set_storage", "key", "value"), &T::set_storage);
	ClassDB::bind_method(D_METHOD("get_storage", "key"), &T::get_storage);
	ClassDB::bind_method(D_METHOD("remove_storage", "key"), &T::remove_storage);
	ClassDB::bind_method(D_METHOD("clear_storage"), &T::clear_storage);
	ClassDB::bind_method(D_METHOD("show_rewarded_ad", "ad_unit_id"), &T::show_rewarded_ad);
	ClassDB::bind_method(D_METHOD("share", "payload"), &T::share);
	ClassDB::bind_method(D_METHOD("set_clipboard", "text"), &T::set_clipboard);
	ClassDB::bind_method(D_METHOD("get_clipboard"), &T::get_clipboard);
	ClassDB::bind_method(D_METHOD("show_toast", "message"), &T::show_toast);
	ClassDB::bind_method(D_METHOD("show_modal", "title", "content"), &T::show_modal);
	ClassDB::bind_method(D_METHOD("get_system_info"), &T::get_system_info);
	ClassDB::bind_method(D_METHOD("get_launch_info"), &T::get_launch_info);
	ClassDB::bind_method(D_METHOD("load_subpackage", "name"), &T::load_subpackage);
	ClassDB::bind_method(D_METHOD("read_host_file", "path"), &T::read_host_file);
	ClassDB::bind_method(D_METHOD("write_host_file", "path", "data"), &T::write_host_file);
	ClassDB::bind_method(D_METHOD("play_inner_audio", "path"), &T::play_inner_audio);

	ClassDB::add_signal(T::get_class_static(), MethodInfo("shown"));
	ClassDB::add_signal(T::get_class_static(), MethodInfo("hidden"));
	ClassDB::add_signal(T::get_class_static(), MethodInfo("host_error", PropertyInfo(Variant::STRING, "message")));
}

template <class T>
void MiniGameApi<T>::bind_host_extra() {
	T *owner = static_cast<T *>(this);
	shown_callback = PortalRuntime::make_callback(callable_mp(owner, &T::_portal_shown));
	hidden_callback = PortalRuntime::make_callback(callable_mp(owner, &T::_portal_hidden));
	error_callback = PortalRuntime::make_callback(callable_mp(owner, &T::_portal_host_error));
	PortalRuntime::bind(this->js_interface, "shown", shown_callback);
	PortalRuntime::bind(this->js_interface, "hidden", hidden_callback);
	PortalRuntime::bind(this->js_interface, "error", error_callback);
}

template <class T>
void MiniGameApi<T>::login() {
	this->host_call("login");
}

template <class T>
void MiniGameApi<T>::check_session() {
	this->host_call("checkSession");
}

template <class T>
void MiniGameApi<T>::get_profile() {
	this->host_call("getProfile");
}

template <class T>
void MiniGameApi<T>::set_storage(const String &p_key, const String &p_value) {
	Dictionary args;
	args["key"] = p_key;
	args["value"] = p_value;
	this->host_call("setStorage", args);
}

template <class T>
String MiniGameApi<T>::get_storage(const String &p_key) {
	Dictionary args;
	args["key"] = p_key;
	return this->host_call("getStorage", args);
}

template <class T>
void MiniGameApi<T>::remove_storage(const String &p_key) {
	Dictionary args;
	args["key"] = p_key;
	this->host_call("removeStorage", args);
}

template <class T>
void MiniGameApi<T>::clear_storage() {
	this->host_call("clearStorage");
}

template <class T>
void MiniGameApi<T>::show_rewarded_ad(const String &p_ad_unit_id) {
	Dictionary args;
	args["adUnitId"] = p_ad_unit_id;
	this->host_call("showRewardedAd", args);
}

template <class T>
void MiniGameApi<T>::share(const Dictionary &p_payload) {
	this->host_call("share", p_payload);
}

template <class T>
void MiniGameApi<T>::set_clipboard(const String &p_text) {
	Dictionary args;
	args["text"] = p_text;
	this->host_call("setClipboard", args);
}

template <class T>
String MiniGameApi<T>::get_clipboard() {
	return this->host_call("getClipboard");
}

template <class T>
void MiniGameApi<T>::show_toast(const String &p_message) {
	Dictionary args;
	args["message"] = p_message;
	this->host_call("showToast", args);
}

template <class T>
void MiniGameApi<T>::show_modal(const String &p_title, const String &p_content) {
	Dictionary args;
	args["title"] = p_title;
	args["content"] = p_content;
	this->host_call("showModal", args);
}

template <class T>
Dictionary MiniGameApi<T>::get_system_info() {
	return this->host_dictionary(this->host_call("getSystemInfo"));
}

template <class T>
Dictionary MiniGameApi<T>::get_launch_info() {
	return this->host_dictionary(this->host_call("getLaunchInfo"));
}

template <class T>
void MiniGameApi<T>::load_subpackage(const String &p_name) {
	Dictionary args;
	args["name"] = p_name;
	this->host_call("loadSubpackage", args);
}

template <class T>
PackedByteArray MiniGameApi<T>::read_host_file(const String &p_path) {
	Dictionary args;
	args["path"] = p_path;
	String encoded = this->host_call("readHostFile", args);
	if (encoded.is_empty()) {
		return PackedByteArray();
	}
	CharString ascii = encoded.ascii();
	Vector<uint8_t> buf;
	buf.resize(encoded.length());
	size_t len = 0;
	if (CryptoCore::b64_decode(buf.ptrw(), buf.size(), &len, (const uint8_t *)ascii.get_data(), ascii.length()) != OK) {
		return PackedByteArray();
	}
	buf.resize(len);
	return buf;
}

template <class T>
void MiniGameApi<T>::write_host_file(const String &p_path, const PackedByteArray &p_data) {
	Dictionary args;
	args["path"] = p_path;
	args["data"] = p_data.is_empty() ? String() : CryptoCore::b64_encode_str(p_data.ptr(), p_data.size());
	this->host_call("writeHostFile", args);
}

template <class T>
void MiniGameApi<T>::play_inner_audio(const String &p_path) {
	Dictionary args;
	args["path"] = p_path;
	this->host_call("playInnerAudio", args);
}

WeChat::WeChat() {
	host_id = "wechat";
}

void WeChat::_bind_methods() {
	MiniGameApi<WeChat>::bind_shared();
	ClassDB::bind_method(D_METHOD("request_midas_payment", "options"), &WeChat::request_midas_payment);
}

void WeChat::request_midas_payment(const Dictionary &p_options) {
	host_call("requestMidasPayment", p_options);
}

Douyin::Douyin() {
	host_id = "douyin";
}

void Douyin::_bind_methods() {
	MiniGameApi<Douyin>::bind_shared();
	ClassDB::bind_method(D_METHOD("request_game_payment", "options"), &Douyin::request_game_payment);
}

void Douyin::request_game_payment(const Dictionary &p_options) {
	host_call("requestGamePayment", p_options);
}

TikTok::TikTok() {
	host_id = "tiktok";
}

void TikTok::_bind_methods() {
	MiniGameApi<TikTok>::bind_shared();
}

template class MiniGameApi<WeChat>;
template class MiniGameApi<Douyin>;
template class MiniGameApi<TikTok>;
