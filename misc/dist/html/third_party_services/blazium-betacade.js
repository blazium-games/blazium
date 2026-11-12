(function (root) {
	const P = root.BlaziumPortals;
	const state = {
		json: null,
		dirty: false,
		snapshot: false,
		replay: false,
	};

	function host() {
		try {
			const page = root.wt;
			return page && typeof page === 'object' ? page : null;
		} catch (err) {
			return null;
		}
	}

	function quiet(result) {
		if (result && typeof result.catch === 'function') {
			result.catch(function () {});
		}
	}

	function ensureSnapshot(page) {
		if (state.snapshot || typeof page.onSnapshot !== 'function') {
			return;
		}
		state.snapshot = true;
		page.onSnapshot(function () {
			if (!state.dirty) {
				return undefined;
			}
			state.dirty = false;
			try {
				return JSON.parse(state.json);
			} catch (err) {
				return undefined;
			}
		});
	}

	function ensureReplay(page) {
		if (state.replay || typeof page.onReplay !== 'function') {
			return;
		}
		state.replay = true;
		page.onReplay({
			enter: function () {
				P.emitSlot('replay_entered', '');
			},
			state: function (next) {
				P.emitSlot('replay_state', JSON.stringify(next === undefined ? null : next));
			},
			exit: function () {
				P.emitSlot('replay_exited', '');
			},
		});
	}

	if (typeof root.addEventListener === 'function') {
		root.addEventListener('wt:pause', function () {
			P.emitPaused();
		});
		root.addEventListener('wt:resume', function () {
			P.emitResumed();
		});
	}

	P.handlers.betacade = {
		available: function () {
			const page = host();
			return page && typeof page.track === 'function' ? 'true' : 'false';
		},
		replaying: function () {
			const page = host();
			try {
				return page && page.replaying ? 'true' : 'false';
			} catch (err) {
				return 'false';
			}
		},
		setState: function (args) {
			state.json = typeof args.json === 'string' ? args.json : JSON.stringify(args.json);
			state.dirty = true;
			const page = host();
			if (!page) {
				return '';
			}
			ensureSnapshot(page);
			if (args.replay) {
				ensureReplay(page);
			}
			return '';
		},
		track: function (args) {
			const page = host();
			if (!page || typeof page.track !== 'function') {
				return '';
			}
			page.track(args.name, args.fields || {});
			return '';
		},
		submitScore: function (args) {
			const page = host();
			if (!page || typeof page.submitScore !== 'function') {
				return '';
			}
			quiet(page.submitScore(args.score));
			return '';
		},
		achieve: function (args) {
			const page = host();
			if (!page || typeof page.achieve !== 'function') {
				return '';
			}
			quiet(page.achieve(args.key));
			return '';
		},
		storageSet: function (args) {
			try {
				root.localStorage.setItem(String(args.key), String(args.value));
				return 'true';
			} catch (err) {
				return 'false';
			}
		},
		storageGet: function (args) {
			try {
				const value = root.localStorage.getItem(String(args.key));
				if (value === null) {
					return { found: false };
				}
				return { found: true, value: value };
			} catch (err) {
				return { found: false };
			}
		},
		storageRemove: function (args) {
			try {
				root.localStorage.removeItem(String(args.key));
			} catch (err) {
				// Leave the game running.
			}
			return '';
		},
	};
	P._ready = true;
})(globalThis);
