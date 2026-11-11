(function (root) {
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
			return "";
		},
		gameReady: function () {
			const game = api();
			if (game && game.game && game.game.gameReady) {
				game.game.gameReady();
			}
			return "";
		},
		sendScore: function (args) {
			const game = api();
			if (game && game.engagement && game.engagement.sendScore) {
				game.engagement.sendScore({ value: Number(args.score || 0) });
			}
			return "";
		},
		saveData: function (args) {
			const game = api();
			if (game && game.game && game.game.saveData) {
				game.game.saveData(String(args.data || ""));
			}
			return "";
		},
		loadData: function () {
			const game = api();
			if (game && game.game && game.game.loadData) {
				game.game.loadData().then(function (data) {
					P.emitResult("loadData", data || "");
				}, function () {
					P.emitResult("loadData", "");
				});
			}
			return "";
		},
		getLanguage: function () {
			const game = api();
			if (game && game.system && game.system.getLanguage) {
				const language = game.system.getLanguage();
				return language && language.then ? "" : String(language || "");
			}
			return "";
		},
		isAudioEnabled: function () {
			const game = api();
			if (game && game.system && typeof game.system.isAudioEnabled === "function") {
				return game.system.isAudioEnabled() ? "true" : "false";
			}
			return "true";
		},
	};
	root.YoutubePlayables = root.YoutubePlayables || {};
	root.YoutubePlayables.gameReady = function () {
		P.handlers.youtube.gameReady();
	};
	const game = api();
	if (game && game.system) {
		if (typeof game.system.onPause === "function") {
			game.system.onPause(function () {
				P.emitPaused();
			});
		}
		if (typeof game.system.onResume === "function") {
			game.system.onResume(function () {
				P.emitResumed();
			});
		}
	}
	P._ready = !!game;
})(typeof globalThis !== "undefined" ? globalThis : this);
