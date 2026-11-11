(function (root) {
	const P = root.BlaziumPortals;
	const state = { shareableUrl: "" };
	function finish(kind) {
		P.emitSlot("break_done", kind);
		P.emitResumed();
	}
	P.handlers.poki = {
		gameplayStart: function () {
			if (root.PokiSDK && root.PokiSDK.gameplayStart) {
				root.PokiSDK.gameplayStart();
			}
			return "";
		},
		gameplayStop: function () {
			if (root.PokiSDK && root.PokiSDK.gameplayStop) {
				root.PokiSDK.gameplayStop();
			}
			return "";
		},
		commercialBreak: function () {
			if (!root.PokiSDK || !root.PokiSDK.commercialBreak) {
				return "";
			}
			P.emitPaused();
			root.PokiSDK.commercialBreak().then(function () {
				finish("commercial");
			}, function () {
				finish("commercial");
			});
			return "";
		},
		rewardedBreak: function () {
			if (!root.PokiSDK || !root.PokiSDK.rewardedBreak) {
				return "";
			}
			P.emitPaused();
			root.PokiSDK.rewardedBreak().then(function () {
				finish("rewarded");
			}, function () {
				finish("rewarded");
			});
			return "";
		},
		getShareableUrl: function () {
			if (state.shareableUrl) {
				return state.shareableUrl;
			}
			if (root.PokiSDK && root.PokiSDK.shareableURL) {
				root.PokiSDK.shareableURL({}).then(function (url) {
					state.shareableUrl = String(url || "");
					P.emitResult("shareableUrl", state.shareableUrl);
				});
			}
			return state.shareableUrl;
		},
	};
	P.boot = function () {
		if (!root.PokiSDK || typeof root.PokiSDK.init !== "function") {
			return Promise.reject(new Error("Poki SDK missing"));
		}
		const host = root.location && root.location.hostname;
		if ((host === "localhost" || host === "127.0.0.1") && typeof root.PokiSDK.setDebug === "function") {
			root.PokiSDK.setDebug(true);
		}
		return root.PokiSDK.init().then(function () {
			P._ready = true;
		});
	};
})(typeof globalThis !== "undefined" ? globalThis : this);
