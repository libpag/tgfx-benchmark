#!/usr/bin/env node
process.chdir(__dirname);

const useWebGPU = process.argv.includes("--webgpu");
if (useWebGPU) {
    process.argv = process.argv.filter(arg => arg !== "--webgpu");
}

process.argv.push("-s");
process.argv.push("../demo");
process.argv.push("-o");
process.argv.push("../demo");
process.argv.push("-p");
process.argv.push("web");
process.argv.push(`-DBENCHMARK_BACKEND=${useWebGPU ? "WEBGPU" : "WEBGL"}`);
process.argv.push("benchmark");
require("./setup.emsdk");
require("../../third_party/tgfx/build_tgfx");

