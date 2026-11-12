/**************************************************************************/
/*  portal_scripts.inc                                                    */
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

/* Embedded portal and host scripts. Generated from misc/dist. */
static const int portal_script_count = 19;
static const char *portal_script_names[] = {
	"blazium-betacade.js",
	"blazium-crazygames.js",
	"blazium-discord.js",
	"blazium-gamedistribution.js",
	"blazium-meta.js",
	"blazium-newgrounds.js",
	"blazium-poki.js",
	"blazium-portals.js",
	"blazium-reddit.js",
	"blazium-telegram.js",
	"blazium-youtube.js",
	"discord-embed.js",
	"youtube-playables.js",
	"blazium_adapter.js",
	"blazium_fetch.js",
	"blazium_host.js",
	"blazium_loader.js",
	"blazium_runtime.js",
	"tiktok_entry.js",
};
static const char *portal_script_bodies[] = {
	R"BLZ_EMBED_9f3a((function (root) {
	const P = root.BlaziumPortals;
	const state = {
		json: null,
		dirty: false,
		snapshot: false,
		replay: false,
	};

	function host() {
		try {
			const page = root.wt;
			return page && typeof page === 'object' ? page : null;
		} catch (err) {
			return null;
		}
	}

	function quiet(result) {
		if (result && typeof result.catch === 'function') {
			result.catch(function () {});
		}
	}

	function ensureSnapshot(page) {
		if (state.snapshot || typeof page.onSnapshot !== 'function') {
			return;
		}
		state.snapshot = true;
		page.onSnapshot(function () {
			if (!state.dirty) {
				return undefined;
			}
			state.dirty = false;
			try {
				return JSON.parse(state.json);
			} catch (err) {
				return undefined;
			}
		});
	}

	function ensureReplay(page) {
		if (state.replay || typeof page.onReplay !== 'function') {
			return;
		}
		state.replay = true;
		page.onReplay({
			enter: function () {
				P.emitSlot('replay_entered', '');
			},
			state: function (next) {
				P.emitSlot('replay_state', JSON.stringify(next === undefined ? null : next));
			},
			exit: function () {
				P.emitSlot('replay_exited', '');
			},
		});
	}

	if (typeof root.addEventListener === 'function') {
		root.addEventListener('wt:pause', function () {
			P.emitPaused();
		});
		root.addEventListener('wt:resume', function () {
			P.emitResumed();
		});
	}

	P.handlers.betacade = {
		available: function () {
			const page = host();
			return page && typeof page.track === 'function' ? 'true' : 'false';
		},
		replaying: function () {
			const page = host();
			try {
				return page && page.replaying ? 'true' : 'false';
			} catch (err) {
				return 'false';
			}
		},
		setState: function (args) {
			state.json = typeof args.json === 'string' ? args.json : JSON.stringify(args.json);
			state.dirty = true;
			const page = host();
			if (!page) {
				return '';
			}
			ensureSnapshot(page);
			if (args.replay) {
				ensureReplay(page);
			}
			return '';
		},
		track: function (args) {
			const page = host();
			if (!page || typeof page.track !== 'function') {
				return '';
			}
			page.track(args.name, args.fields || {});
			return '';
		},
		submitScore: function (args) {
			const page = host();
			if (!page || typeof page.submitScore !== 'function') {
				return '';
			}
			quiet(page.submitScore(args.score));
			return '';
		},
		achieve: function (args) {
			const page = host();
			if (!page || typeof page.achieve !== 'function') {
				return '';
			}
			quiet(page.achieve(args.key));
			return '';
		},
		storageSet: function (args) {
			try {
				root.localStorage.setItem(String(args.key), String(args.value));
				return 'true';
			} catch (err) {
				return 'false';
			}
		},
		storageGet: function (args) {
			try {
				const value = root.localStorage.getItem(String(args.key));
				if (value === null) {
					return { found: false };
				}
				return { found: true, value: value };
			} catch (err) {
				return { found: false };
			}
		},
		storageRemove: function (args) {
			try {
				root.localStorage.removeItem(String(args.key));
			} catch (err) {
				// Leave the game running.
			}
			return '';
		},
	};
	P._ready = true;
})(globalThis);
)BLZ_EMBED_9f3a",
	R"BLZ_EMBED_9f3a((function (root) {
	const P = root.BlaziumPortals;
	const state = { environment: '' };
	function sdk() {
		return root.CrazyGames && root.CrazyGames.SDK ? root.CrazyGames.SDK : null;
	}
	function requestAd(kind) {
		const current = sdk();
		if (!current || !current.ad || !current.ad.requestAd) {
			return '';
		}
		current.ad.requestAd(kind, {
			adFinished: function () {
				P.emitResult(kind, 'finished');
				P.emitResumed();
			},
			adError: function (err) {
				P.emitResult(kind, err && err.message ? err.message : 'error');
				P.emitResumed();
			},
			adStarted: function () {
				P.emitPaused();
			},
		});
		return '';
	}
	P.handlers.crazygames = {
		gameplayStart: function () {
			const current = sdk();
			if (current && current.game && current.game.gameplayStart) {
				current.game.gameplayStart();
			}
			return '';
		},
		gameplayStop: function () {
			const current = sdk();
			if (current && current.game && current.game.gameplayStop) {
				current.game.gameplayStop();
			}
			return '';
		},
		requestMidgameAd: function () {
			return requestAd('midgame');
		},
		requestRewardedAd: function () {
			return requestAd('rewarded');
		},
		getEnvironment: function () {
			if (state.environment) {
				return state.environment;
			}
			const current = sdk();
			return current && current.environment ? String(current.environment) : '';
		},
	};
	P.boot = function () {
		const current = sdk();
		if (!current || typeof current.init !== 'function') {
			return Promise.reject(new Error('CrazyGames SDK missing'));
		}
		return current.init().then(function () {
			state.environment = current.environment ? String(current.environment) : 'crazygames';
			P._ready = true;
		});
	};
})(globalThis);
)BLZ_EMBED_9f3a",
	R"BLZ_EMBED_9f3a((function (root) {
	const P = root.BlaziumPortals;
	const state = { sdk: null, user: null };
	P.handlers.discord = {
		ready: function () {
			return '';
		},
		getUserId: function () {
			return state.user && state.user.id ? String(state.user.id) : '';
		},
		getUsername: function () {
			return state.user && state.user.username ? String(state.user.username) : '';
		},
		setActivity: function (args) {
			if (!state.sdk || !state.sdk.commands || !state.sdk.commands.setActivity) {
				return '';
			}
			state.sdk.commands.setActivity({
				activity: {
					type: 0,
					details: args.details || '',
					state: args.state || '',
				},
			});
			return '';
		},
		clearActivity: function () {
			if (!state.sdk || !state.sdk.commands || !state.sdk.commands.setActivity) {
				return '';
			}
			state.sdk.commands.setActivity({ activity: { type: 0, details: '', state: '' } });
			return '';
		},
	};
	P.boot = function () {
		const config = P.config();
		if (!config.clientId) {
			return Promise.reject(new Error('Discord client id is required'));
		}
		const loaded = root.DiscordSDK
			? Promise.resolve(root.DiscordSDK)
			: import('https://cdn.jsdelivr.net/npm/@discord/embedded-app-sdk@1.4.1/+esm').then(function (mod) {
				return mod.DiscordSDK;
			});
		return loaded.then(function (DiscordSDK) {
			const sdk = new DiscordSDK(config.clientId);
			state.sdk = sdk;
			return sdk.ready().then(function () {
				if (!sdk.commands || !sdk.commands.authenticate) {
					P._ready = true;
					return Promise.resolve();
				}
				return sdk.commands.authenticate({
					client_id: config.clientId,
					response_type: 'code',
					state: '',
					prompt: 'none',
					scope: ['identify'],
				}).then(function (auth) {
					state.user = auth && auth.user ? auth.user : null;
					P._ready = true;
				}, function () {
					P._ready = true;
				});
			});
		});
	};
})(globalThis);
)BLZ_EMBED_9f3a",
	R"BLZ_EMBED_9f3a((function (root) {
	const P = root.BlaziumPortals;
	const config = P.config();
	root.GD_OPTIONS = {
		gameId: config.gameId || '',
		onEvent: function (event) {
			const name = event && event.name ? event.name : '';
			if (name === 'SDK_GAME_PAUSE') {
				P.emitPaused();
			} else if (name === 'SDK_GAME_START') {
				P.emitResumed();
			}
		},
	};
	P.handlers.gamedistribution = {
		showAd: function () {
			if (root.gdsdk && typeof root.gdsdk.showAd === 'function') {
				root.gdsdk.showAd();
			}
			return '';
		},
	};
	P._ready = true;
})(globalThis);
)BLZ_EMBED_9f3a",
	R"BLZ_EMBED_9f3a((function (root) {
	const P = root.BlaziumPortals;
	function player() {
		return root.FBInstant && root.FBInstant.player ? root.FBInstant.player : null;
	}
	P.handlers.meta = {
		getPlayerId: function () {
			const current = player();
			return current && current.getID ? String(current.getID() || '') : '';
		},
		getPlayerName: function () {
			const current = player();
			return current && current.getName ? String(current.getName() || '') : '';
		},
		getPlayerPhoto: function () {
			const current = player();
			return current && current.getPhoto ? String(current.getPhoto() || '') : '';
		},
		getLocale: function () {
			return root.FBInstant && root.FBInstant.getLocale ? String(root.FBInstant.getLocale() || '') : '';
		},
		getEntryPointData: function () {
			if (!root.FBInstant || !root.FBInstant.getEntryPointData) {
				return '';
			}
			return JSON.stringify(root.FBInstant.getEntryPointData() || {});
		},
		getContextId: function () {
			if (!root.FBInstant || !root.FBInstant.context || !root.FBInstant.context.getID) {
				return '';
			}
			return String(root.FBInstant.context.getID() || '');
		},
	};
	P.reportProgress = function (current, total) {
		if (!root.FBInstant || typeof root.FBInstant.setLoadingProgress !== 'function') {
			return;
		}
		const percent = total > 0 ? Math.round((current / total) * 100) : 0;
		root.FBInstant.setLoadingProgress(percent);
	};
	P.boot = function () {
		if (!root.FBInstant || typeof root.FBInstant.initializeAsync !== 'function') {
			return Promise.reject(new Error('Meta instant runtime missing'));
		}
		return root.FBInstant.initializeAsync().then(function () {
			return root.FBInstant.startGameAsync();
		}).then(function () {
			P._ready = true;
		});
	};
})(globalThis);
)BLZ_EMBED_9f3a",
	R"BLZ_EMBED_9f3a((function (root) {
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
)BLZ_EMBED_9f3a",
	R"BLZ_EMBED_9f3a((function (root) {
	const P = root.BlaziumPortals;
	const state = { shareableUrl: '' };
	function finish(kind) {
		P.emitSlot('break_done', kind);
		P.emitResumed();
	}
	P.handlers.poki = {
		gameplayStart: function () {
			if (root.PokiSDK && root.PokiSDK.gameplayStart) {
				root.PokiSDK.gameplayStart();
			}
			return '';
		},
		gameplayStop: function () {
			if (root.PokiSDK && root.PokiSDK.gameplayStop) {
				root.PokiSDK.gameplayStop();
			}
			return '';
		},
		commercialBreak: function () {
			if (!root.PokiSDK || !root.PokiSDK.commercialBreak) {
				return '';
			}
			P.emitPaused();
			root.PokiSDK.commercialBreak().then(function () {
				finish('commercial');
			}, function () {
				finish('commercial');
			});
			return '';
		},
		rewardedBreak: function () {
			if (!root.PokiSDK || !root.PokiSDK.rewardedBreak) {
				return '';
			}
			P.emitPaused();
			root.PokiSDK.rewardedBreak().then(function () {
				finish('rewarded');
			}, function () {
				finish('rewarded');
			});
			return '';
		},
		getShareableUrl: function () {
			if (state.shareableUrl) {
				return state.shareableUrl;
			}
			if (root.PokiSDK && root.PokiSDK.shareableURL) {
				root.PokiSDK.shareableURL({}).then(function (url) {
					state.shareableUrl = String(url || '');
					P.emitResult('shareableUrl', state.shareableUrl);
				});
			}
			return state.shareableUrl;
		},
	};
	P.boot = function () {
		if (!root.PokiSDK || typeof root.PokiSDK.init !== 'function') {
			return Promise.reject(new Error('Poki SDK missing'));
		}
		const host = root.location && root.location.hostname;
		if ((host === 'localhost' || host === '127.0.0.1') && typeof root.PokiSDK.setDebug === 'function') {
			root.PokiSDK.setDebug(true);
		}
		return root.PokiSDK.init().then(function () {
			P._ready = true;
		});
	};
})(globalThis);
)BLZ_EMBED_9f3a",
	R"BLZ_EMBED_9f3a((function (root) {
	const portals = {
		_ready: false,
		handlers: {},
		_slots: {},
		config: function () {
			return root.BlaziumPortalConfig || { host: '' };
		},
		available: function (host) {
			return portals.config().host === host && portals._ready === true;
		},
		bind: function (slot, callback) {
			portals._slots[slot] = callback;
		},
		emitPaused: function () {
			if (portals._slots.paused) {
				portals._slots.paused();
			}
		},
		emitResumed: function () {
			if (portals._slots.resumed) {
				portals._slots.resumed();
			}
		},
		emitResult: function (method, payload) {
			if (portals._slots.result) {
				portals._slots.result(JSON.stringify({ method: method, payload: payload }));
			}
		},
		emitSlot: function (slot, payload) {
			if (portals._slots[slot]) {
				portals._slots[slot](payload);
			}
		},
		call: function (host, method, arg) {
			if (portals.config().host !== host) {
				return '';
			}
			const table = portals.handlers[host];
			if (!table || typeof table[method] !== 'function') {
				return '';
			}
			let parsed = {};
			if (typeof arg === 'string' && arg.length) {
				try {
					parsed = JSON.parse(arg);
				} catch (err) {
					parsed = {};
				}
			}
			const result = table[method](parsed || {});
			if (result === undefined || result === null) {
				return '';
			}
			if (typeof result === 'string') {
				return result;
			}
			if (typeof result === 'number' || typeof result === 'boolean') {
				return String(result);
			}
			return JSON.stringify(result);
		},
		boot: function () {
			portals._ready = true;
			return Promise.resolve();
		},
		reportProgress: function () {},
	};
	root.BlaziumPortals = portals;
})(globalThis);
)BLZ_EMBED_9f3a",
	R"BLZ_EMBED_9f3a((function (root) {
	const P = root.BlaziumPortals;
	const state = { username: '' };
	root.addEventListener('message', function (event) {
		const data = event.data || {};
		const nested = data.data || {};
		const username = data.username || nested.username || '';
		if (username) {
			state.username = String(username);
		}
	});
	P.handlers.reddit = {
		getUsername: function () {
			if (!state.username && root.parent && root.parent !== root) {
				root.parent.postMessage({ type: 'request-user' }, '*');
			}
			return state.username;
		},
		submitScore: function (args) {
			if (root.parent) {
				root.parent.postMessage({ type: 'score', score: Number(args.score || 0) }, '*');
			}
			return '';
		},
	};
	P._ready = true;
})(globalThis);
)BLZ_EMBED_9f3a",
	R"BLZ_EMBED_9f3a((function (root) {
	const P = root.BlaziumPortals;
	const state = { app: null };
	function app() {
		return state.app || (root.Telegram && root.Telegram.WebApp) || null;
	}
	P.handlers.telegram = {
		getInitData: function () {
			const current = app();
			return current && current.initData ? String(current.initData) : '';
		},
		getTheme: function () {
			const current = app();
			return current && current.themeParams ? current.themeParams : {};
		},
		getViewport: function () {
			const current = app();
			if (!current) {
				return {};
			}
			return {
				height: current.viewportHeight || 0,
				stableHeight: current.viewportStableHeight || 0,
				width: current.viewportWidth || 0,
			};
		},
		getSafeArea: function () {
			const current = app();
			if (!current) {
				return {};
			}
			return current.safeAreaInset || current.contentSafeAreaInset || {};
		},
		requestFullscreen: function () {
			const current = app();
			if (current && typeof current.requestFullscreen === 'function') {
				current.requestFullscreen();
			} else if (current && typeof current.expand === 'function') {
				current.expand();
			}
			return '';
		},
		sendData: function (args) {
			const current = app();
			if (current && typeof current.sendData === 'function') {
				current.sendData(String(args.data || ''));
			}
			return '';
		},
	};
	P.boot = function () {
		const current = root.Telegram && root.Telegram.WebApp;
		if (!current) {
			return Promise.reject(new Error('Telegram WebApp missing'));
		}
		state.app = current;
		if (typeof current.ready === 'function') {
			current.ready();
		}
		if (typeof current.expand === 'function') {
			current.expand();
		}
		P._ready = true;
		return Promise.resolve();
	};
})(globalThis);
)BLZ_EMBED_9f3a",
	R"BLZ_EMBED_9f3a((function (root) {
	const P = root.BlaziumPortals;
	function api() {
		return root.ytgame || null;
	}
	P.handlers.youtube = {
		firstFrame: function () {
			const game = api();
			if (game && game.game && game.game.firstFrameReady) {
				game.game.firstFrameReady();
			}
			return '';
		},
		gameReady: function () {
			const game = api();
			if (game && game.game && game.game.gameReady) {
				game.game.gameReady();
			}
			return '';
		},
		sendScore: function (args) {
			const game = api();
			if (game && game.engagement && game.engagement.sendScore) {
				game.engagement.sendScore({ value: Number(args.score || 0) });
			}
			return '';
		},
		saveData: function (args) {
			const game = api();
			if (game && game.game && game.game.saveData) {
				game.game.saveData(String(args.data || ''));
			}
			return '';
		},
		loadData: function () {
			const game = api();
			if (game && game.game && game.game.loadData) {
				game.game.loadData().then(function (data) {
					P.emitResult('loadData', data || '');
				}, function () {
					P.emitResult('loadData', '');
				});
			}
			return '';
		},
		getLanguage: function () {
			const game = api();
			if (game && game.system && game.system.getLanguage) {
				const language = game.system.getLanguage();
				return language && language.then ? '' : String(language || '');
			}
			return '';
		},
		isAudioEnabled: function () {
			const game = api();
			if (game && game.system && typeof game.system.isAudioEnabled === 'function') {
				return game.system.isAudioEnabled() ? 'true' : 'false';
			}
			return 'true';
		},
	};
	root.YoutubePlayables ||= {};
	root.YoutubePlayables.gameReady = function () {
		P.handlers.youtube.gameReady();
	};
	const game = api();
	if (game && game.system) {
		if (typeof game.system.onPause === 'function') {
			game.system.onPause(function () {
				P.emitPaused();
			});
		}
		if (typeof game.system.onResume === 'function') {
			game.system.onResume(function () {
				P.emitResumed();
			});
		}
	}
	P._ready = !!game;
})(globalThis);
)BLZ_EMBED_9f3a",
	R"BLZ_EMBED_9f3a(const DISCORD_AUTODETECT = $BLAZIUM_DISCORD_AUTODETECT;
window.DiscordEmbed = {
	isDiscordEmbed: () => {
		if (DISCORD_AUTODETECT) {
			return ['discord.com', 'discordapp.com', 'discordsays.com', 'discordapp.net', 'discordsez.com'].some((s) => window.location.hostname.includes(s));
		}
		return true;
	},
};
)BLZ_EMBED_9f3a",
	R"BLZ_EMBED_9f3a(window.YoutubePlayables = undefined;
if ((typeof ytgame) !== 'undefined') {
	function returnError(c) {
		return (e) => {
			c(`{"error":"${e.toString()}"}`);
		};
	};
	function returnEmpty(c) {
		return () => {
			c('{}');
		};
	};
	window.YoutubePlayables = {
		isYoutubePlayables: () => ytgame?.IN_PLAYABLES_ENV ?? false,
		getSdkVersion: () => ytgame?.SDK_VERSION ?? 'unloaded',
		sendScore: (v, c) => {
			ytgame.engagement.sendScore({ value: v }).then(returnEmpty(c), returnError(c));
		},
		openYTContent: (v, c) => {
			ytgame.engagement.openYTContent({ id: v }).then(returnEmpty(c), returnError(c));
		},
		loadData: (c) => {
			ytgame.game.loadData().then((d) => c(`{"data":${d}}`), returnError(c));
		},
		saveData: (v, c) => {
			ytgame.game.saveData(v).then(returnEmpty(c), returnError(c));
		},
		logWarning: () => {
			ytgame.health.logWarning();
		},
		logError: () => {
			ytgame.health.logError();
		},
		onAudioEnabledChange: (c) => {
			ytgame.system.onAudioEnabledChange(c);
		},
		onPause: (c) => {
			ytgame.system.onPause(c);
		},
		onResume: (c) => {
			ytgame.system.onResume(c);
		},
		getLanguage: (c) => {
			ytgame.system.getLanguage().then((l) => c(`{"data":"${l}"}`), returnError(c));
		},
		gameReady: () => {
			ytgame.game.gameReady();
		},
	};
	ytgame.game.firstFrameReady();
}
)BLZ_EMBED_9f3a",
	R"BLZ_EMBED_9f3a((function (root) {
	function noop() {}
	function element() {
		return {
			style: {},
			appendChild: noop,
			removeChild: noop,
			setAttribute: noop,
			addEventListener: noop,
			removeEventListener: noop,
			getContext: function () {
				return canvasContext();
			},
		};
	}
	function canvasContext() {
		return {
			canvas: null,
			fillStyle: "",
			drawImage: noop,
			fillRect: noop,
			clearRect: noop,
			getImageData: function () {
				return { data: new Uint8Array(4) };
			},
			putImageData: noop,
			createImageData: function () {
				return { data: new Uint8Array(4) };
			},
			getParameter: function () {
				return 0;
			},
			getExtension: function () {
				return null;
			},
			viewport: noop,
			bindBuffer: noop,
			bufferData: noop,
			enable: noop,
			disable: noop,
			blendFunc: noop,
			useProgram: noop,
			uniform1i: noop,
			uniform1f: noop,
			uniform2f: noop,
			uniform4f: noop,
			createTexture: function () {
				return {};
			},
			createBuffer: function () {
				return {};
			},
			createProgram: function () {
				return {};
			},
			createShader: function () {
				return {};
			},
		};
	}
	if (typeof root.document === "undefined") {
		const canvas = element();
		canvas.id = "canvas";
		canvas.width = 300;
		canvas.height = 150;
		canvas.getBoundingClientRect = function () {
			return { left: 0, top: 0, width: canvas.width, height: canvas.height };
		};
		canvas.focus = noop;
		const body = element();
		const documentElement = element();
		root.document = {
			getElementById: function (id) {
				return id === "canvas" ? canvas : null;
			},
			querySelector: function () {
				return canvas;
			},
			querySelectorAll: function () {
				return [canvas];
			},
			createElement: function (tag) {
				return tag === "canvas" ? canvas : element();
			},
			createElementNS: function () {
				return element();
			},
			createTextNode: function () {
				return {};
			},
			addEventListener: noop,
			removeEventListener: noop,
			hasFocus: function () {
				return true;
			},
			body: body,
			head: element(),
			documentElement: documentElement,
			hidden: false,
			visibilityState: "visible",
		};
	}
	if (typeof root.window === "undefined") {
		root.window = root;
	}
	if (typeof root.requestAnimationFrame !== "function") {
		root.requestAnimationFrame = function (callback) {
			return setTimeout(function () {
				callback(Date.now());
			}, 16);
		};
		root.cancelAnimationFrame = function (id) {
			clearTimeout(id);
		};
	}
	if (typeof root.devicePixelRatio !== "number") {
		root.devicePixelRatio = 1;
	}
	if (!root.navigator) {
		root.navigator = { userAgent: "BlaziumHost", language: "en", languages: ["en"] };
	}
	if (!root.location) {
		root.location = { href: "", hostname: "", protocol: "https:", search: "" };
	}
	if (!root.performance) {
		root.performance = { now: function () { return Date.now(); } };
	}
	if (typeof root.AudioContext === "undefined" && typeof root.webkitAudioContext === "undefined") {
		function AudioNodeStub() {
			this.connect = noop;
			this.disconnect = noop;
			this.gain = { value: 1 };
		}
		function AudioContextStub() {
			this.state = "running";
			this.sampleRate = 44100;
			this.currentTime = 0;
			this.destination = new AudioNodeStub();
			this.listener = {};
			this.audioWorklet = { addModule: function () { return Promise.resolve(); } };
			this.createGain = function () { return new AudioNodeStub(); };
			this.createBufferSource = function () {
				const node = new AudioNodeStub();
				node.start = noop;
				node.stop = noop;
				node.buffer = null;
				return node;
			};
			this.createBuffer = function () { return { getChannelData: function () { return new Float32Array(1); } }; };
			this.createOscillator = function () { return new AudioNodeStub(); };
			this.createChannelSplitter = function () { return new AudioNodeStub(); };
			this.createChannelMerger = function () { return new AudioNodeStub(); };
			this.createDynamicsCompressor = function () { return new AudioNodeStub(); };
			this.createBiquadFilter = function () { return new AudioNodeStub(); };
			this.decodeAudioData = function () { return Promise.reject(new Error("audio data unavailable")); };
			this.resume = function () { return Promise.resolve(); };
			this.suspend = function () { return Promise.resolve(); };
			this.close = function () { return Promise.resolve(); };
			this.addEventListener = noop;
		}
		root.AudioContext = AudioContextStub;
	}
	if (typeof root.btoa !== "function") {
		root.btoa = function (value) {
			const chars = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
			let output = "";
			for (let i = 0; i < value.length; i += 3) {
				const a = value.charCodeAt(i);
				const b = i + 1 < value.length ? value.charCodeAt(i + 1) : 0;
				const c = i + 2 < value.length ? value.charCodeAt(i + 2) : 0;
				const triple = (a << 16) | (b << 8) | c;
				output += chars[(triple >> 18) & 63];
				output += chars[(triple >> 12) & 63];
				output += i + 1 < value.length ? chars[(triple >> 6) & 63] : "=";
				output += i + 2 < value.length ? chars[triple & 63] : "=";
			}
			return output;
		};
	}
	if (typeof root.atob !== "function") {
		root.atob = function (value) {
			const chars = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
			const clean = value.replace(/=+$/, "");
			let output = "";
			for (let i = 0; i < clean.length; i += 4) {
				const a = chars.indexOf(clean[i]);
				const b = chars.indexOf(clean[i + 1]);
				const c = chars.indexOf(clean[i + 2]);
				const d = chars.indexOf(clean[i + 3]);
				const triple = (a << 18) | (b << 12) | ((c & 63) << 6) | (d & 63);
				output += String.fromCharCode((triple >> 16) & 255);
				if (clean[i + 2]) {
					output += String.fromCharCode((triple >> 8) & 255);
				}
				if (clean[i + 3]) {
					output += String.fromCharCode(triple & 255);
				}
			}
			return output;
		};
	}
	if (typeof root.Response === "undefined") {
		root.Response = function (body) {
			this.body = body;
			this.ok = true;
			this.status = 200;
			this.clone = function () { return this; };
			this.arrayBuffer = function () {
				const data = body && body.buffer ? body.buffer : body;
				return Promise.resolve(data);
			};
		};
	}
	root.BlaziumHostAdapter = { installed: true };
})(typeof globalThis !== "undefined" ? globalThis : this);
)BLZ_EMBED_9f3a",
	R"BLZ_EMBED_9f3a((function (root) {
	function toBuffer(data) {
		if (!data) {
			return new ArrayBuffer(0);
		}
		if (data instanceof ArrayBuffer) {
			return data;
		}
		if (data.buffer) {
			return data.buffer;
		}
		return data;
	}
	const fetchApi = {
		installed: false,
		install: function (api) {
			const fs = api && typeof api.getFileSystemManager === "function" ? api.getFileSystemManager() : null;
			if (!fs) {
				return false;
			}
			root.fetch = function (url) {
				let filePath = String(url);
				if (filePath.endsWith(".wasm")) {
					filePath += ".br";
				}
				return new Promise(function (resolve, reject) {
					fs.readFile({
						filePath: filePath,
						success: function (res) {
							const data = res.data;
							resolve({
								ok: true,
								status: 200,
								body: data,
								clone: function () {
									return this;
								},
								arrayBuffer: function () {
									return Promise.resolve(toBuffer(data));
								},
							});
						},
						fail: function (err) {
							reject(err || new Error("host file read failed"));
						},
					});
				});
			};
			fetchApi.installed = true;
			return true;
		},
	};
	root.BlaziumHostFetch = fetchApi;
})(typeof globalThis !== "undefined" ? globalThis : this);
)BLZ_EMBED_9f3a",
	R"BLZ_EMBED_9f3a((function (root) {
	function text(value) {
		if (value === undefined || value === null) {
			return "";
		}
		return String(value);
	}
	function failMessage(err) {
		if (!err) {
			return "host call failed";
		}
		return text(err.errMsg || err.message || err);
	}
	function bytesToBase64(data) {
		const bytes = data instanceof Uint8Array ? data : new Uint8Array(data || []);
		let binary = "";
		for (let i = 0; i < bytes.length; i++) {
			binary += String.fromCharCode(bytes[i]);
		}
		return root.btoa(binary);
	}
	function base64ToBytes(value) {
		const binary = root.atob(value || "");
		const bytes = new Uint8Array(binary.length);
		for (let i = 0; i < binary.length; i++) {
			bytes[i] = binary.charCodeAt(i);
		}
		return bytes;
	}
	const bridge = {
		abi: 1,
		brand: "blazium-host-bridge",
		provider: "",
		api: null,
		engine: null,
		_slots: {},
		_attached: false,
		handshake: function () {
			return { abi: bridge.abi, brand: bridge.brand, provider: bridge.provider };
		},
		available: function (host) {
			return bridge.provider === host && bridge.api !== null;
		},
		bind: function (slot, callback) {
			bridge._slots[slot] = callback;
		},
		emitResult: function (method, payload) {
			if (bridge._slots.result) {
				bridge._slots.result(JSON.stringify({ method: method, payload: payload }));
			}
		},
		useProvider: function (name) {
			const selected = root.BlaziumHostRuntime.select(name);
			bridge.provider = selected.name;
			bridge.api = selected.api;
			bridge.attach();
			return bridge.api !== null;
		},
		attach: function () {
			const api = bridge.api;
			if (!api || bridge._attached) {
				return;
			}
			bridge._attached = true;
			if (typeof api.onShow === "function") {
				api.onShow(function () {
					if (bridge._slots.shown) {
						bridge._slots.shown();
					}
				});
			}
			if (typeof api.onHide === "function") {
				api.onHide(function () {
					bridge.flushUser();
					if (bridge._slots.hidden) {
						bridge._slots.hidden();
					}
				});
			}
			if (typeof api.onError === "function") {
				api.onError(function (err) {
					if (bridge._slots.error) {
						bridge._slots.error(failMessage(err));
					}
				});
			}
		},
		flushUser: function () {
			const engine = bridge.engine;
			if (!engine || typeof engine.copyFSToAdapter !== "function") {
				return;
			}
			try {
				engine.copyFSToAdapter("/userfs/save.bin");
			} catch (err) {
				// The save file is created by the game when it writes user://.
			}
		},
		restoreUser: function () {
			const engine = bridge.engine;
			if (!engine || typeof engine.ensureFSDirectory !== "function") {
				return;
			}
			engine.ensureFSDirectory("/userfs");
			const fs = bridge.api && typeof bridge.api.getFileSystemManager === "function" ? bridge.api.getFileSystemManager() : null;
			if (!fs || typeof fs.readFileSync !== "function" || typeof engine.copyToFS !== "function") {
				return;
			}
			try {
				engine.copyToFS("/userfs/save.bin", fs.readFileSync("user/save.bin"));
			} catch (err) {
				// A new game has no saved file yet.
			}
		},
		writeHostFile: function (path, data) {
			const fs = bridge.api && typeof bridge.api.getFileSystemManager === "function" ? bridge.api.getFileSystemManager() : null;
			if (!fs || typeof fs.writeFileSync !== "function") {
				return;
			}
			const payload = typeof data === "string" ? base64ToBytes(data) : data;
			fs.writeFileSync(path, payload);
		},
		call: function (host, method, arg) {
			if (bridge.provider !== host || !bridge.api || typeof bridge.methods[method] !== "function") {
				return "";
			}
			let parsed = {};
			if (typeof arg === "string" && arg.length) {
				try {
					parsed = JSON.parse(arg);
				} catch (err) {
					parsed = {};
				}
			}
			const result = bridge.methods[method](parsed || {});
			if (result === undefined || result === null) {
				return "";
			}
			if (typeof result === "string") {
				return result;
			}
			return JSON.stringify(result);
		},
		methods: {
			login: function () {
				bridge.api.login({
					success: function (res) {
						bridge.emitResult("login", { ok: true, code: res && res.code ? res.code : "" });
					},
					fail: function (err) {
						bridge.emitResult("login", { ok: false, error: failMessage(err) });
					},
				});
				return "";
			},
			checkSession: function () {
				bridge.api.checkSession({
					success: function () {
						bridge.emitResult("checkSession", { ok: true });
					},
					fail: function () {
						bridge.emitResult("checkSession", { ok: false });
					},
				});
				return "";
			},
			getProfile: function () {
				bridge.api.getUserInfo({
					success: function (res) {
						bridge.emitResult("profile", res && res.userInfo ? res.userInfo : {});
					},
					fail: function (err) {
						bridge.emitResult("profile", { error: failMessage(err) });
					},
				});
				return "";
			},
			setStorage: function (args) {
				bridge.api.setStorageSync(args.key, args.value);
				return "";
			},
			getStorage: function (args) {
				const value = bridge.api.getStorageSync(args.key);
				return value == null ? "" : text(value);
			},
			removeStorage: function (args) {
				bridge.api.removeStorageSync(args.key);
				return "";
			},
			clearStorage: function () {
				bridge.api.clearStorageSync();
				return "";
			},
			showRewardedAd: function (args) {
				const ad = bridge.api.createRewardedVideoAd({ adUnitId: args.adUnitId || "" });
				if (ad.onClose) {
					ad.onClose(function (res) {
						bridge.emitResult("rewarded", { isEnded: !res || res.isEnded !== false });
					});
				}
				ad.show();
				return "";
			},
			share: function (args) {
				bridge.api.shareAppMessage({
					title: args.title || "",
					imageUrl: args.imageUrl || "",
					query: args.query || "",
				});
				return "";
			},
			setClipboard: function (args) {
				bridge.api.setClipboardData({ data: args.text || "" });
				return "";
			},
			getClipboard: function () {
				if (typeof bridge.api.getClipboardData !== "function") {
					return "";
				}
				bridge.api.getClipboardData({
					success: function (res) {
						bridge.emitResult("clipboard", res && res.data ? res.data : "");
					},
					fail: function () {
						bridge.emitResult("clipboard", "");
					},
				});
				return "";
			},
			showToast: function (args) {
				bridge.api.showToast({ title: args.message || "", icon: "none" });
				return "";
			},
			showModal: function (args) {
				bridge.api.showModal({ title: args.title || "", content: args.content || "" });
				return "";
			},
			getSystemInfo: function () {
				const info = typeof bridge.api.getSystemInfoSync === "function" ? bridge.api.getSystemInfoSync() : {};
				return JSON.stringify(info || {});
			},
			getLaunchInfo: function () {
				const info = typeof bridge.api.getLaunchOptionsSync === "function" ? bridge.api.getLaunchOptionsSync() : {};
				return JSON.stringify(info || {});
			},
			loadSubpackage: function (args) {
				const name = args.name || "engine";
				bridge.api.loadSubpackage({
					name: name,
					success: function () {
						bridge.emitResult("subpackage", { name: name, ok: true });
					},
					fail: function (err) {
						bridge.emitResult("subpackage", { name: name, ok: false, error: failMessage(err) });
					},
				});
				return "";
			},
			readHostFile: function (args) {
				const fs = bridge.api.getFileSystemManager();
				const data = fs.readFileSync(args.path);
				return bytesToBase64(data);
			},
			writeHostFile: function (args) {
				bridge.writeHostFile(args.path, args.data || "");
				return "";
			},
			playInnerAudio: function (args) {
				const audio = bridge.api.createInnerAudioContext();
				audio.src = args.path || "";
				audio.play();
				return "";
			},
			requestMidasPayment: function (args) {
				if (bridge.provider !== "wechat" || typeof bridge.api.requestMidasPayment !== "function") {
					return JSON.stringify({ ok: false, error: "Not on this host" });
				}
				bridge.api.requestMidasPayment(args || {});
				return "";
			},
			requestGamePayment: function (args) {
				if (bridge.provider !== "douyin" || typeof bridge.api.pay !== "function") {
					return JSON.stringify({ ok: false, error: "Not on this host" });
				}
				bridge.api.pay(args || {});
				return "";
			},
		},
	};
	root.blaziumHostBridge = bridge;
})(typeof globalThis !== "undefined" ? globalThis : this);
)BLZ_EMBED_9f3a",
	R"BLZ_EMBED_9f3a((function (root) {
	const loader = {
		PATHS: {
			js: "engine/blazium.js",
			wasm: "engine/blazium.wasm.br",
			pack: "engine/blazium.zip",
			executable: "engine/blazium",
		},
		lastCall: null,
		compilerFor: function (provider) {
			if (provider === "wechat") {
				return root.WXWebAssembly || null;
			}
			if (provider === "douyin" || provider === "tiktok") {
				return root.TTWebAssembly || null;
			}
			return null;
		},
		instantiateWasm: function (imports, onSuccess) {
			const provider = root.blaziumHostBridge ? root.blaziumHostBridge.provider : "";
			const compiler = loader.compilerFor(provider);
			const path = loader.PATHS.wasm;
			loader.lastCall = { provider: provider, path: path, argument: "path" };
			if (!compiler || typeof compiler.instantiate !== "function") {
				return Promise.reject(new Error("wasm compiler missing"));
			}
			if (typeof path !== "string") {
				return Promise.reject(new Error("wasm path must be a package path"));
			}
			return compiler.instantiate(path, imports || {}).then(function (result) {
				if (typeof onSuccess === "function") {
					onSuccess(result.instance, result.module);
				}
				return result;
			}, function (err) {
				return Promise.reject(err);
			});
		},
		loadSubpackage: function (api, name) {
			if (!api || typeof api.loadSubpackage !== "function") {
				return Promise.resolve();
			}
			return new Promise(function (resolve, reject) {
				api.loadSubpackage({
					name: name,
					success: function () {
						resolve();
					},
					fail: function (err) {
						reject(err || new Error("subpackage failed"));
					},
				});
			});
		},
		start: async function (options) {
			options = options || {};
			const selected = root.BlaziumHostRuntime.select(options.provider || "");
			if (!selected.api) {
				throw new Error("host api missing");
			}
			root.blaziumHostBridge.useProvider(selected.name);
			if (root.BlaziumHostFetch) {
				root.BlaziumHostFetch.install(selected.api);
			}
			await loader.loadSubpackage(selected.api, "engine");
			if (typeof root.BlaziumEngine === "undefined") {
				throw new Error("BlaziumEngine missing");
			}
			const engine = new root.BlaziumEngine({
				executable: loader.PATHS.executable,
				mainPack: loader.PATHS.pack,
				canvasResizePolicy: 2,
				focusCanvas: false,
				args: [],
			});
			root.blaziumHostBridge.engine = engine;
			await engine.init(loader.PATHS.executable);
			root.blaziumHostBridge.restoreUser();
			await engine.start();
			return engine;
		},
	};
	root.BlaziumHostLoader = loader;
})(typeof globalThis !== "undefined" ? globalThis : this);
)BLZ_EMBED_9f3a",
	R"BLZ_EMBED_9f3a((function (root) {
	function present(value) {
		return typeof value !== "undefined" && value !== null;
	}
	const providers = {
		wechat: function () {
			return present(root.wx) ? root.wx : null;
		},
		douyin: function () {
			return present(root.tt) ? root.tt : null;
		},
		tiktok: function () {
			return root.TTMinis && root.TTMinis.game ? root.TTMinis.game : null;
		},
	};
	function select(name) {
		if (name && providers[name]) {
			return { name: name, api: providers[name]() };
		}
		const order = ["wechat", "douyin", "tiktok"];
		for (let i = 0; i < order.length; i++) {
			const api = providers[order[i]]();
			if (api) {
				return { name: order[i], api: api };
			}
		}
		return { name: "", api: null };
	}
	root.BlaziumHostRuntime = {
		providers: providers,
		select: select,
	};
})(typeof globalThis !== "undefined" ? globalThis : this);
)BLZ_EMBED_9f3a",
	R"BLZ_EMBED_9f3a(/* Blazium host entry. */
)BLZ_EMBED_9f3a",
};
