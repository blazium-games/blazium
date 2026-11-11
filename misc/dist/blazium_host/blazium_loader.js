(function (root) {
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
