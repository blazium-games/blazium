/**************************************************************************/
/*  cold_storage_async.cpp                                                */
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

#ifdef TOOLS_ENABLED

#include "cold_storage_async.h"

#include "cold_storage_vcs.h"

// Include Godot headers before the SDK: winsock2.h defines CONNECT/IGNORE, which
// break Godot enums if those headers are parsed afterward.
#include "core/object/object.h"
#include "core/object/worker_thread_pool.h"
#include "core/os/mutex.h"

#include "client/sdk/client_sdk.h"
#include "client/sdk/tls_options.h"
#ifdef CONNECT
#undef CONNECT
#endif
#ifdef IGNORE
#undef IGNORE
#endif

#include <exception>
#include <memory>
#include <utility>

namespace {

Mutex busy_mutex;
bool busy = false;
std::unique_ptr<coldstorage::ColdStorageClient> pending_client;

struct Job {
	ColdStorageConnectRequest req;
	bool ok = false;
	String error;
	std::unique_ptr<coldstorage::ColdStorageClient> client;
};

void _cold_storage_async_complete(ObjectID p_caller_id, const StringName &p_method, bool p_ok, const String &p_error, int p_kind);

String _connect_auth(coldstorage::ColdStorageClient &client, const ColdStorageConnectionConfig &cfg, const String &project_path) {
	coldstorage::TlsOptions tls;
	tls.enabled = cfg.use_tls;
	tls.verifyPeer = !cfg.tls_insecure;
	tls.caFile = String(cfg.ca_file).utf8().get_data();

	const std::string host = String(cfg.host).utf8().get_data();
	if (!client.connect(host, cfg.port, tls)) {
		return "Failed to connect to " + cfg.host + ":" + itos(cfg.port);
	}

	client.setWorkspace(String(cfg.workspace).utf8().get_data());
	client.setRepo(String(cfg.repo).utf8().get_data());

	String root = cfg.workspace_root;
	if (root.is_empty()) {
		root = project_path;
	}
	client.setWorkspaceRoot(String(root).utf8().get_data());

	if (!cfg.jwt.is_empty()) {
		if (!client.authenticateWithJWT(String(cfg.jwt).utf8().get_data())) {
			client.disconnect();
			return "JWT authentication failed";
		}
	} else if (!cfg.ticket.is_empty()) {
		client.setTicket(String(cfg.ticket).utf8().get_data());
	} else if (!cfg.user.is_empty()) {
		std::string ticket = client.login(String(cfg.user).utf8().get_data(), String(cfg.password).utf8().get_data());
		if (ticket.empty()) {
			String err = "Login failed";
			if (!client.lastError().empty()) {
				err += ": " + String(client.lastError().c_str());
			}
			client.disconnect();
			return err;
		}
	}

	client.ensureWorkspaceView();
	return String();
}

void _worker(void *p_userdata) {
	Job *job = static_cast<Job *>(p_userdata);
	try {
		auto client = std::make_unique<coldstorage::ColdStorageClient>();
		const String auth_err = _connect_auth(*client, job->req.cfg, job->req.project_path);
		if (!auth_err.is_empty()) {
			job->ok = false;
			job->error = auth_err;
		} else if (job->req.validate) {
			auto info = client->info();
			if (info.name.empty() && info.version.empty()) {
				job->ok = false;
				job->error = "Server info() failed";
				if (!client->lastError().empty()) {
					job->error += ": " + String(client->lastError().c_str());
				}
				client->disconnect();
			} else {
				job->ok = true;
			}
		} else {
			job->ok = true;
		}

		if (job->ok && job->req.auto_pull) {
			auto r = client->syncAll("#head");
			if (!r.success) {
				job->ok = false;
				job->error = String(r.error.c_str());
				if (job->error.is_empty()) {
					job->error = "syncAll failed";
				}
				client->disconnect();
			}
		}

		if (job->ok) {
			if (job->req.kind == ColdStorageConnectRequest::Kind::TEST_JOB) {
				client->disconnect();
				client.reset();
			} else {
				job->client = std::move(client);
			}
		}
	} catch (const std::exception &e) {
		job->ok = false;
		job->error = String("ColdStorage exception: ") + e.what();
		job->client.reset();
	} catch (...) {
		job->ok = false;
		job->error = "ColdStorage unknown exception during connect";
		job->client.reset();
	}

	{
		MutexLock lock(busy_mutex);
		pending_client = std::move(job->client);
	}

	const ObjectID caller_id = job->req.caller_id;
	const bool ok = job->ok;
	const String error = job->error;
	const int kind = (int)job->req.kind;
	const StringName method(job->req.complete_method);
	memdelete(job);

	// Keep busy=true until the main-thread trampoline (or caller) takes/discards.
	// Completion must not be bound to the caller's lifetime: if the Object is
	// freed between queue and flush, Object-bound deferred calls are dropped and
	// busy would latch forever.
	callable_mp_static(_cold_storage_async_complete).call_deferred(caller_id, method, ok, error, kind);
}

void _cold_storage_async_complete(ObjectID p_caller_id, const StringName &p_method, bool p_ok, const String &p_error, int p_kind) {
	Object *caller = ObjectDB::get_instance(p_caller_id);
	if (caller && p_method != StringName()) {
		caller->call(p_method, p_ok, p_error, p_kind);
	}

	// Safety: clear latch if caller was freed or forgot to take/discard.
	MutexLock lock(busy_mutex);
	if (busy) {
		busy = false;
		if (pending_client) {
			pending_client->disconnect();
			pending_client.reset();
		}
	}
}

} //namespace

bool cold_storage_connect_busy() {
	MutexLock lock(busy_mutex);
	return busy;
}

static std::unique_ptr<coldstorage::ColdStorageClient> _take_connected_client() {
	MutexLock lock(busy_mutex);
	busy = false;
	return std::move(pending_client);
}

void cold_storage_discard_connected_client() {
	std::unique_ptr<coldstorage::ColdStorageClient> client = _take_connected_client();
	if (client) {
		client->disconnect();
	}
}

bool cold_storage_has_pending_client() {
	MutexLock lock(busy_mutex);
	return pending_client != nullptr;
}

bool cold_storage_adopt_connected_client_into(ColdStorageVCS *p_vcs, const ColdStorageConnectionConfig &p_cfg, const String &p_project_path) {
	std::unique_ptr<coldstorage::ColdStorageClient> client = _take_connected_client();
	if (!p_vcs || !client) {
		return false;
	}
	return p_vcs->adopt_connected_client(std::move(client), p_cfg, p_project_path);
}

bool cold_storage_begin_connect_async(const ColdStorageConnectRequest &p_request) {
	{
		MutexLock lock(busy_mutex);
		if (busy) {
			return false;
		}
		busy = true;
		pending_client.reset();
	}

	Job *job = memnew(Job);
	job->req = p_request;
	WorkerThreadPool::get_singleton()->add_native_task(&_worker, job, true, "ColdStorage Connect");
	return true;
}

#endif
