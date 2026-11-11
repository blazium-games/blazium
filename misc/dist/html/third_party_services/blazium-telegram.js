(function (root) {
	const P = root.BlaziumPortals;
	const state = { app: null };
	function app() {
		return state.app || (root.Telegram && root.Telegram.WebApp) || null;
	}
	P.handlers.telegram = {
		getInitData: function () {
			const current = app();
			return current && current.initData ? String(current.initData) : "";
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
			if (current && typeof current.requestFullscreen === "function") {
				current.requestFullscreen();
			} else if (current && typeof current.expand === "function") {
				current.expand();
			}
			return "";
		},
		sendData: function (args) {
			const current = app();
			if (current && typeof current.sendData === "function") {
				current.sendData(String(args.data || ""));
			}
			return "";
		},
	};
	P.boot = function () {
		const current = root.Telegram && root.Telegram.WebApp;
		if (!current) {
			return Promise.reject(new Error("Telegram WebApp missing"));
		}
		state.app = current;
		if (typeof current.ready === "function") {
			current.ready();
		}
		if (typeof current.expand === "function") {
			current.expand();
		}
		P._ready = true;
		return Promise.resolve();
	};
})(typeof globalThis !== "undefined" ? globalThis : this);
