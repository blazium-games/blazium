(function (root) {
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
