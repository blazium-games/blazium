(function (root) {
	const P = root.BlaziumPortals;
	const config = P.config();
	root.GD_OPTIONS = {
		gameId: config.gameId || '',
		onEvent: function (event) {
			const name = event && event.name ? event.name : '';
			if (name === 'SDK_GAME_PAUSE') {
				P.emitPaused();
			} else if (name === 'SDK_GAME_START') {
				P.emitResumed();
			}
		},
	};
	P.handlers.gamedistribution = {
		showAd: function () {
			if (root.gdsdk && typeof root.gdsdk.showAd === 'function') {
				root.gdsdk.showAd();
			}
			return '';
		},
	};
	P._ready = true;
})(globalThis);
