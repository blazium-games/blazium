(function (root) {
	const P = root.BlaziumPortals;
	const state = { sdk: null, user: null };
	P.handlers.discord = {
		ready: function () {
			return "";
		},
		getUserId: function () {
			return state.user && state.user.id ? String(state.user.id) : "";
		},
		getUsername: function () {
			return state.user && state.user.username ? String(state.user.username) : "";
		},
		setActivity: function (args) {
			if (!state.sdk || !state.sdk.commands || !state.sdk.commands.setActivity) {
				return "";
			}
			state.sdk.commands.setActivity({
				activity: {
					type: 0,
					details: args.details || "",
					state: args.state || "",
				},
			});
			return "";
		},
		clearActivity: function () {
			if (!state.sdk || !state.sdk.commands || !state.sdk.commands.setActivity) {
				return "";
			}
			state.sdk.commands.setActivity({ activity: { type: 0, details: "", state: "" } });
			return "";
		},
	};
	P.boot = function () {
		const config = P.config();
		if (!config.clientId) {
			return Promise.reject(new Error("Discord client id is required"));
		}
		const loaded = root.DiscordSDK
			? Promise.resolve(root.DiscordSDK)
			: import("https://cdn.jsdelivr.net/npm/@discord/embedded-app-sdk@1.4.1/+esm").then(function (mod) {
				return mod.DiscordSDK;
			});
		return loaded.then(function (DiscordSDK) {
			const sdk = new DiscordSDK(config.clientId);
			state.sdk = sdk;
			return sdk.ready().then(function () {
				if (!sdk.commands || !sdk.commands.authenticate) {
					P._ready = true;
					return;
				}
				return sdk.commands.authenticate({
					client_id: config.clientId,
					response_type: "code",
					state: "",
					prompt: "none",
					scope: ["identify"],
				}).then(function (auth) {
					state.user = auth && auth.user ? auth.user : null;
					P._ready = true;
				}, function () {
					P._ready = true;
				});
			});
		});
	};
})(typeof globalThis !== "undefined" ? globalThis : this);
