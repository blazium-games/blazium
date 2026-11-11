(function (root) {
	const P = root.BlaziumPortals;
	const state = { environment: "" };
	function sdk() {
		return root.CrazyGames && root.CrazyGames.SDK ? root.CrazyGames.SDK : null;
	}
	function requestAd(kind) {
		const current = sdk();
		if (!current || !current.ad || !current.ad.requestAd) {
			return "";
		}
		current.ad.requestAd(kind, {
			adFinished: function () {
				P.emitResult(kind, "finished");
				P.emitResumed();
			},
			adError: function (err) {
				P.emitResult(kind, err && err.message ? err.message : "error");
				P.emitResumed();
			},
			adStarted: function () {
				P.emitPaused();
			},
		});
		return "";
	}
	P.handlers.crazygames = {
		gameplayStart: function () {
			const current = sdk();
			if (current && current.game && current.game.gameplayStart) {
				current.game.gameplayStart();
			}
			return "";
		},
		gameplayStop: function () {
			const current = sdk();
			if (current && current.game && current.game.gameplayStop) {
				current.game.gameplayStop();
			}
			return "";
		},
		requestMidgameAd: function () {
			return requestAd("midgame");
		},
		requestRewardedAd: function () {
			return requestAd("rewarded");
		},
		getEnvironment: function () {
			if (state.environment) {
				return state.environment;
			}
			const current = sdk();
			return current && current.environment ? String(current.environment) : "";
		},
	};
	P.boot = function () {
		const current = sdk();
		if (!current || typeof current.init !== "function") {
			return Promise.reject(new Error("CrazyGames SDK missing"));
		}
		return current.init().then(function () {
			state.environment = current.environment ? String(current.environment) : "crazygames";
			P._ready = true;
		});
	};
})(typeof globalThis !== "undefined" ? globalThis : this);
