(function (root) {
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
