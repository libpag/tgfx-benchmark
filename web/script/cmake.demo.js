#!/usr/bin/env node
process.chdir(__dirname);

const useWebGPU = process.argv.includes("--webgpu");
if (useWebGPU) {
    process.argv = process.argv.filter(arg => arg !== "--webgpu");
}

// Emit each backend into its own top-level output directory (demo/webgl or demo/webgpu) so the
// WebGL and WebGPU build artifacts never share a directory. The vendored build appends the arch
// (wasm / wasm-mt) as a sub-directory, giving e.g. demo/webgpu/wasm-mt/benchmark.js.
const backendDir = useWebGPU ? "webgpu" : "webgl";

process.argv.push("-s");
process.argv.push("../demo");
process.argv.push("-o");
process.argv.push(`../demo/${backendDir}`);
process.argv.push("-p");
process.argv.push("web");
process.argv.push(`-DBENCHMARK_BACKEND=${useWebGPU ? "WEBGPU" : "WEBGL"}`);
process.argv.push("benchmark");
require("./setup.emsdk");
require("../../third_party/tgfx/build_tgfx");
