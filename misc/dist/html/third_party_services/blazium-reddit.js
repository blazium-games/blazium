(function (root) {
	const P = root.BlaziumPortals;
	const state = { username: '' };
	root.addEventListener('message', function (event) {
		const data = event.data || {};
		const nested = data.data || {};
		const username = data.username || nested.username || '';
		if (username) {
			state.username = String(username);
		}
	});
	P.handlers.reddit = {
		getUsername: function () {
			if (!state.username && root.parent && root.parent !== root) {
				root.parent.postMessage({ type: 'request-user' }, '*');
			}
			return state.username;
		},
		submitScore: function (args) {
			if (root.parent) {
				root.parent.postMessage({ type: 'score', score: Number(args.score || 0) }, '*');
			}
			return '';
		},
	};
	P._ready = true;
})(globalThis);
