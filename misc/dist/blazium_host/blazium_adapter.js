(function (root) {
	function noop() {}
	function element() {
		return {
			style: {},
			appendChild: noop,
			removeChild: noop,
			setAttribute: noop,
			addEventListener: noop,
			removeEventListener: noop,
			getContext: function () {
				return canvasContext();
			},
		};
	}
	function canvasContext() {
		return {
			canvas: null,
			fillStyle: "",
			drawImage: noop,
			fillRect: noop,
			clearRect: noop,
			getImageData: function () {
				return { data: new Uint8Array(4) };
			},
			putImageData: noop,
			createImageData: function () {
				return { data: new Uint8Array(4) };
			},
			getParameter: function () {
				return 0;
			},
			getExtension: function () {
				return null;
			},
			viewport: noop,
			bindBuffer: noop,
			bufferData: noop,
			enable: noop,
			disable: noop,
			blendFunc: noop,
			useProgram: noop,
			uniform1i: noop,
			uniform1f: noop,
			uniform2f: noop,
			uniform4f: noop,
			createTexture: function () {
				return {};
			},
			createBuffer: function () {
				return {};
			},
			createProgram: function () {
				return {};
			},
			createShader: function () {
				return {};
			},
		};
	}
	if (typeof root.document === "undefined") {
		const canvas = element();
		canvas.id = "canvas";
		canvas.width = 300;
		canvas.height = 150;
		canvas.getBoundingClientRect = function () {
			return { left: 0, top: 0, width: canvas.width, height: canvas.height };
		};
		canvas.focus = noop;
		const body = element();
		const documentElement = element();
		root.document = {
			getElementById: function (id) {
				return id === "canvas" ? canvas : null;
			},
			querySelector: function () {
				return canvas;
			},
			querySelectorAll: function () {
				return [canvas];
			},
			createElement: function (tag) {
				return tag === "canvas" ? canvas : element();
			},
			createElementNS: function () {
				return element();
			},
			createTextNode: function () {
				return {};
			},
			addEventListener: noop,
			removeEventListener: noop,
			hasFocus: function () {
				return true;
			},
			body: body,
			head: element(),
			documentElement: documentElement,
			hidden: false,
			visibilityState: "visible",
		};
	}
	if (typeof root.window === "undefined") {
		root.window = root;
	}
	if (typeof root.requestAnimationFrame !== "function") {
		root.requestAnimationFrame = function (callback) {
			return setTimeout(function () {
				callback(Date.now());
			}, 16);
		};
		root.cancelAnimationFrame = function (id) {
			clearTimeout(id);
		};
	}
	if (typeof root.devicePixelRatio !== "number") {
		root.devicePixelRatio = 1;
	}
	if (!root.navigator) {
		root.navigator = { userAgent: "BlaziumHost", language: "en", languages: ["en"] };
	}
	if (!root.location) {
		root.location = { href: "", hostname: "", protocol: "https:", search: "" };
	}
	if (!root.performance) {
		root.performance = { now: function () { return Date.now(); } };
	}
	if (typeof root.AudioContext === "undefined" && typeof root.webkitAudioContext === "undefined") {
		function AudioNodeStub() {
			this.connect = noop;
			this.disconnect = noop;
			this.gain = { value: 1 };
		}
		function AudioContextStub() {
			this.state = "running";
			this.sampleRate = 44100;
			this.currentTime = 0;
			this.destination = new AudioNodeStub();
			this.listener = {};
			this.audioWorklet = { addModule: function () { return Promise.resolve(); } };
			this.createGain = function () { return new AudioNodeStub(); };
			this.createBufferSource = function () {
				const node = new AudioNodeStub();
				node.start = noop;
				node.stop = noop;
				node.buffer = null;
				return node;
			};
			this.createBuffer = function () { return { getChannelData: function () { return new Float32Array(1); } }; };
			this.createOscillator = function () { return new AudioNodeStub(); };
			this.createChannelSplitter = function () { return new AudioNodeStub(); };
			this.createChannelMerger = function () { return new AudioNodeStub(); };
			this.createDynamicsCompressor = function () { return new AudioNodeStub(); };
			this.createBiquadFilter = function () { return new AudioNodeStub(); };
			this.decodeAudioData = function () { return Promise.reject(new Error("audio data unavailable")); };
			this.resume = function () { return Promise.resolve(); };
			this.suspend = function () { return Promise.resolve(); };
			this.close = function () { return Promise.resolve(); };
			this.addEventListener = noop;
		}
		root.AudioContext = AudioContextStub;
	}
	if (typeof root.btoa !== "function") {
		root.btoa = function (value) {
			const chars = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
			let output = "";
			for (let i = 0; i < value.length; i += 3) {
				const a = value.charCodeAt(i);
				const b = i + 1 < value.length ? value.charCodeAt(i + 1) : 0;
				const c = i + 2 < value.length ? value.charCodeAt(i + 2) : 0;
				const triple = (a << 16) | (b << 8) | c;
				output += chars[(triple >> 18) & 63];
				output += chars[(triple >> 12) & 63];
				output += i + 1 < value.length ? chars[(triple >> 6) & 63] : "=";
				output += i + 2 < value.length ? chars[triple & 63] : "=";
			}
			return output;
		};
	}
	if (typeof root.atob !== "function") {
		root.atob = function (value) {
			const chars = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
			const clean = value.replace(/=+$/, "");
			let output = "";
			for (let i = 0; i < clean.length; i += 4) {
				const a = chars.indexOf(clean[i]);
				const b = chars.indexOf(clean[i + 1]);
				const c = chars.indexOf(clean[i + 2]);
				const d = chars.indexOf(clean[i + 3]);
				const triple = (a << 18) | (b << 12) | ((c & 63) << 6) | (d & 63);
				output += String.fromCharCode((triple >> 16) & 255);
				if (clean[i + 2]) {
					output += String.fromCharCode((triple >> 8) & 255);
				}
				if (clean[i + 3]) {
					output += String.fromCharCode(triple & 255);
				}
			}
			return output;
		};
	}
	if (typeof root.Response === "undefined") {
		root.Response = function (body) {
			this.body = body;
			this.ok = true;
			this.status = 200;
			this.clone = function () { return this; };
			this.arrayBuffer = function () {
				const data = body && body.buffer ? body.buffer : body;
				return Promise.resolve(data);
			};
		};
	}
	root.BlaziumHostAdapter = { installed: true };
})(typeof globalThis !== "undefined" ? globalThis : this);
