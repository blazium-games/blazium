import assert from "node:assert/strict";
import fs from "node:fs";
import path from "node:path";
import vm from "node:vm";
import { fileURLToPath } from "node:url";

const here = path.dirname(fileURLToPath(import.meta.url));
const rootDir = path.resolve(here, "..");

function loadRuntime() {
	const context = vm.createContext({
		console,
		setTimeout,
		clearTimeout,
		Date,
		Promise,
		JSON,
		Uint8Array,
		ArrayBuffer,
		Float32Array,
		TextEncoder,
		btoa: (value) => Buffer.from(value, "binary").toString("base64"),
		atob: (value) => Buffer.from(value, "base64").toString("binary"),
	});
	context.globalThis = context;
	const files = [
		"blazium_adapter.js",
		"blazium_fetch.js",
		"blazium_runtime.js",
		"blazium_host.js",
		"blazium_loader.js",
	];
	for (const file of files) {
		const source = fs.readFileSync(path.join(rootDir, file), "utf8");
		vm.runInContext(source, context, { filename: file });
	}
	return context;
}

const runtimeFiles = [
	"blazium_adapter.js",
	"blazium_fetch.js",
	"blazium_runtime.js",
	"blazium_host.js",
	"blazium_loader.js",
	"tiktok_entry.js",
];
for (const file of runtimeFiles) {
	const source = fs.readFileSync(path.join(rootDir, file), "utf8");
	assert.equal(source.includes("eval("), false, `${file} contains eval(`);
}

const tiktokEntry = fs.readFileSync(path.join(rootDir, "tiktok_entry.js"), "utf8");
assert.ok(tiktokEntry.trim().length > 0);
assert.ok(tiktokEntry.includes("/*"));

const ctx = loadRuntime();
assert.equal(ctx.BlaziumHostAdapter.installed, true);
assert.ok(ctx.document.getElementById("canvas"));
assert.equal(typeof ctx.AudioContext, "function");

const handshake = ctx.blaziumHostBridge.handshake();
assert.equal(handshake.abi, 1);
assert.equal(handshake.brand, "blazium-host-bridge");

assert.equal(ctx.BlaziumHostLoader.PATHS.executable, "engine/blazium");
assert.equal(ctx.BlaziumHostLoader.PATHS.wasm, "engine/blazium.wasm.br");
assert.equal(ctx.BlaziumHostLoader.PATHS.pack, "engine/blazium.zip");
assert.equal(ctx.BlaziumHostLoader.PATHS.js, "engine/blazium.js");

ctx.wx = { name: "wx" };
ctx.tt = { name: "tt" };
ctx.TTMinis = { game: { name: "ttminis" } };
assert.equal(ctx.BlaziumHostRuntime.select("tiktok").api.name, "ttminis");
assert.equal(ctx.BlaziumHostRuntime.select("douyin").api.name, "tt");
assert.equal(ctx.BlaziumHostRuntime.select("wechat").api.name, "wx");
assert.equal(ctx.blaziumHostBridge.useProvider("tiktok"), true);
assert.equal(ctx.blaziumHostBridge.provider, "tiktok");
assert.equal(ctx.blaziumHostBridge.api.name, "ttminis");

ctx.WXWebAssembly = {
	instantiate(filePath) {
		assert.equal(typeof filePath, "string");
		assert.equal(filePath, "engine/blazium.wasm.br");
		return Promise.resolve({ instance: { exports: {} }, module: { provider: "wechat" } });
	},
};
ctx.TTWebAssembly = {
	instantiate(filePath) {
		assert.equal(typeof filePath, "string");
		return Promise.resolve({ instance: { exports: {} }, module: { provider: "tt", path: filePath } });
	},
};
ctx.blaziumHostBridge.useProvider("wechat");
const wechatResult = await ctx.BlaziumHostLoader.instantiateWasm({}, () => {});
assert.equal(wechatResult.module.provider, "wechat");
assert.equal(ctx.BlaziumHostLoader.lastCall.provider, "wechat");
assert.equal(ctx.BlaziumHostLoader.lastCall.argument, "path");

ctx.blaziumHostBridge.useProvider("douyin");
const douyinResult = await ctx.BlaziumHostLoader.instantiateWasm({}, () => {});
assert.equal(douyinResult.module.provider, "tt");

ctx.blaziumHostBridge.useProvider("tiktok");
const tiktokResult = await ctx.BlaziumHostLoader.instantiateWasm({}, () => {});
assert.equal(tiktokResult.module.path, "engine/blazium.wasm.br");
assert.equal(ctx.BlaziumHostLoader.compilerFor("tiktok"), ctx.TTWebAssembly);
assert.equal(ctx.BlaziumHostLoader.compilerFor("wechat"), ctx.WXWebAssembly);

const reads = [];
ctx.wx = {
	name: "wx",
	getFileSystemManager() {
		return {
			readFile(options) {
				reads.push(options.filePath);
				options.success({ data: new Uint8Array([1, 2, 3]) });
			},
			readFileSync() {
				return new Uint8Array([4]);
			},
		};
	},
	loadSubpackage(options) {
		assert.equal(options.name, "engine");
		options.success();
	},
};
ctx.tt = null;
ctx.TTMinis = null;
const dirs = [];
ctx.BlaziumEngine = function BlaziumEngine(config) {
	this.config = config;
	this.init = async () => {};
	this.start = async () => {};
	this.ensureFSDirectory = (dir) => {
		dirs.push(dir);
	};
	this.copyToFS = () => {};
};
await ctx.BlaziumHostLoader.start({ provider: "wechat" });
assert.equal(ctx.blaziumHostBridge.engine.config.executable, "engine/blazium");
assert.equal(ctx.blaziumHostBridge.engine.config.mainPack, "engine/blazium.zip");
assert.deepEqual(dirs, ["/userfs"]);
assert.equal(ctx.BlaziumHostFetch.installed, true);
const response = await ctx.fetch("engine/blazium.wasm");
assert.deepEqual(reads, ["engine/blazium.wasm.br"]);
const bytes = new Uint8Array(await response.arrayBuffer());
assert.deepEqual(Array.from(bytes), [1, 2, 3]);

console.log("blazium host runtime tests passed");
