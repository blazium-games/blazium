window.YoutubePlayables = undefined;
if ((typeof ytgame) !== 'undefined') {
	function returnError(c) {
		return (e) => {
			c(`{"error":"${e.toString()}"}`);
		};
	};
	function returnEmpty(c) {
		return () => {
			c('{}');
		};
	};
	window.YoutubePlayables = {
		isYoutubePlayables: () => ytgame?.IN_PLAYABLES_ENV ?? false,
		getSdkVersion: () => ytgame?.SDK_VERSION ?? 'unloaded',
		sendScore: (v, c) => {
			ytgame.engagement.sendScore({ value: v }).then(returnEmpty(c), returnError(c));
		},
		openYTContent: (v, c) => {
			ytgame.engagement.openYTContent({ id: v }).then(returnEmpty(c), returnError(c));
		},
		loadData: (c) => {
			ytgame.game.loadData().then((d) => c(`{"data":${d}}`), returnError(c));
		},
		saveData: (v, c) => {
			ytgame.game.saveData(v).then(returnEmpty(c), returnError(c));
		},
		logWarning: () => {
			ytgame.health.logWarning();
		},
		logError: () => {
			ytgame.health.logError();
		},
		onAudioEnabledChange: (c) => {
			ytgame.system.onAudioEnabledChange(c);
		},
		onPause: (c) => {
			ytgame.system.onPause(c);
		},
		onResume: (c) => {
			ytgame.system.onResume(c);
		},
		getLanguage: (c) => {
			ytgame.system.getLanguage().then((l) => c(`{"data":"${l}"}`), returnError(c));
		},
		gameReady: () => {
			ytgame.game.gameReady();
		},
	};
	ytgame.game.firstFrameReady();
}
