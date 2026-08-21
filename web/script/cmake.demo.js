#!/usr/bin/env node
process.chdir(__dirname);

const fs = require("fs");
const path = require("path");

const useWebGPU = process.argv.includes("--webgpu");
if (useWebGPU) {
    process.argv = process.argv.filter(arg => arg !== "--webgpu");
}

// Resolve the arch (wasm / wasm-mt) so we can relocate the emscripten artifacts into a
// backend-specific directory afterwards. This keeps WebGL and WebGPU builds isolated so they no
// longer overwrite each other in the same output directory.
const archIndex = process.argv.indexOf("-a");
const arch = archIndex !== -1 ? process.argv[archIndex + 1] : "wasm-mt";

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

// The vendored build always emits to `../demo/<arch>`. For WebGPU builds, move the emitted
// benchmark artifacts into `<arch>-webgpu` so WebGL and WebGPU outputs stay isolated.
if (useWebGPU) {
    const srcDir = path.resolve(__dirname, "../demo", arch);
    const dstDir = path.resolve(__dirname, "../demo", `${arch}-webgpu`);
    fs.mkdirSync(dstDir, {recursive: true});
    for (const file of ["benchmark.js", "benchmark.wasm"]) {
        const srcFile = path.join(srcDir, file);
        if (fs.existsSync(srcFile)) {
            fs.renameSync(srcFile, path.join(dstDir, file));
        }
    }
}
