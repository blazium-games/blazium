(function (root) {
	const P = root.BlaziumPortals;
	const state = { sessionId: '', queue: [] };
	function config() {
		return P.config();
	}
	function readSession() {
		try {
			const params = new URLSearchParams(root.location.search);
			return params.get('ngio_session_id') || params.get('session_id') || '';
		} catch (err) {
			return '';
		}
	}
	function bytesToBase64(bytes) {
		let text = '';
		for (let i = 0; i < bytes.length; i++) {
			text += String.fromCharCode(bytes[i]);
		}
		return root.btoa(text);
	}
	async function encryptCall(payload, keyText) {
		const encoder = new TextEncoder();
		const keyBytes = encoder.encode(keyText || '');
		const raw = new Uint8Array(16);
		raw.set(keyBytes.subarray(0, 16));
		const iv = root.crypto.getRandomValues(new Uint8Array(16));
		const key = await root.crypto.subtle.importKey('raw', raw, { name: 'AES-CBC' }, false, ['encrypt']);
		const cipher = new Uint8Array(await root.crypto.subtle.encrypt({ name: 'AES-CBC', iv: iv }, key, encoder.encode(JSON.stringify(payload))));
		const out = new Uint8Array(iv.length + cipher.length);
		out.set(iv, 0);
		out.set(cipher, iv.length);
		return bytesToBase64(out);
	}
	async function gateway(component, parameters, encrypted) {
		const current = config();
		const call = { component: component, parameters: parameters || {} };
		const body = { app_id: current.appId || '', session_id: state.sessionId || '', call: call };
		if (encrypted && current.encryptionKey && root.crypto && root.crypto.subtle) {
			body.call = { secure: await encryptCall(call, current.encryptionKey) };
		}
		const response = await root.fetch('https://www.newgrounds.io/gateway_v3.php', {
			method: 'POST',
			headers: { 'Content-Type': 'application/json' },
			body: JSON.stringify(body),
		});
		return response.json();
	}
	function secure(component, parameters) {
		if (!state.sessionId) {
			state.queue.push({ component: component, parameters: parameters });
			return Promise.resolve({ queued: true });
		}
		return gateway(component, parameters, true);
	}
	function flush() {
		const pending = state.queue.splice(0);
		return pending.reduce(function (chain, item) {
			return chain.then(function () {
				return gateway(item.component, item.parameters, true);
			});
		}, Promise.resolve());
	}
	state.sessionId = readSession();
	P.handlers.newgrounds = {
		logIn: function () {
			const current = config();
			const url = `https://www.newgrounds.com/passport?app_id=${encodeURIComponent(current.appId || '')}`;
			if (root.open) {
				root.open(url, 'ngpassport', 'width=800,height=600');
			}
			return '';
		},
		unlockMedal: function (args) {
			secure('Medal.unlock', { id: args.id });
			return state.sessionId ? '' : 'queued';
		},
		submitScore: function (args) {
			secure('ScoreBoard.postScore', { id: args.board, value: args.score });
			return state.sessionId ? '' : 'queued';
		},
		cloudSave: function (args) {
			secure('CloudSave.set', { id: args.key, data: args.value });
			return state.sessionId ? '' : 'queued';
		},
		cloudLoad: function (args) {
			if (!state.sessionId) {
				state.queue.push({ component: 'CloudSave.load', parameters: { id: args.key } });
				return '';
			}
			gateway('CloudSave.load', { id: args.key }, true).then(function (result) {
				P.emitResult('cloudLoad', result || {});
			});
			return '';
		},
	};
	P.boot = function () {
		state.sessionId ||= readSession();
		P._ready = true;
		if (state.sessionId) {
			return flush();
		}
		return Promise.resolve();
	};
	if (state.sessionId) {
		P._ready = true;
		flush();
	} else {
		P._ready = true;
	}
})(globalThis);
