(function (root) {
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
