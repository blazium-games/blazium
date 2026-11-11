(function (root) {
	const P = root.BlaziumPortals;
	function player() {
		return root.FBInstant && root.FBInstant.player ? root.FBInstant.player : null;
	}
	P.handlers.meta = {
		getPlayerId: function () {
			const current = player();
			return current && current.getID ? String(current.getID() || '') : '';
		},
		getPlayerName: function () {
			const current = player();
			return current && current.getName ? String(current.getName() || '') : '';
		},
		getPlayerPhoto: function () {
			const current = player();
			return current && current.getPhoto ? String(current.getPhoto() || '') : '';
		},
		getLocale: function () {
			return root.FBInstant && root.FBInstant.getLocale ? String(root.FBInstant.getLocale() || '') : '';
		},
		getEntryPointData: function () {
			if (!root.FBInstant || !root.FBInstant.getEntryPointData) {
				return '';
			}
			return JSON.stringify(root.FBInstant.getEntryPointData() || {});
		},
		getContextId: function () {
			if (!root.FBInstant || !root.FBInstant.context || !root.FBInstant.context.getID) {
				return '';
			}
			return String(root.FBInstant.context.getID() || '');
		},
	};
	P.reportProgress = function (current, total) {
		if (!root.FBInstant || typeof root.FBInstant.setLoadingProgress !== 'function') {
			return;
		}
		const percent = total > 0 ? Math.round((current / total) * 100) : 0;
		root.FBInstant.setLoadingProgress(percent);
	};
	P.boot = function () {
		if (!root.FBInstant || typeof root.FBInstant.initializeAsync !== 'function') {
			return Promise.reject(new Error('Meta instant runtime missing'));
		}
		return root.FBInstant.initializeAsync().then(function () {
			return root.FBInstant.startGameAsync();
		}).then(function () {
			P._ready = true;
		});
	};
})(globalThis);
