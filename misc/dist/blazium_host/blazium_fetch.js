(function (root) {
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
