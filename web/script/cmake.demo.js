#!/usr/bin/env node
process.chdir(__dirname);

const fs = require("fs");
const path = require("path");
const {execFileSync} = require("child_process");

const useWebGPU = process.argv.includes("--webgpu");
if (useWebGPU) {
    process.argv = process.argv.filter(arg => arg !== "--webgpu");
}

function readTgfxRootArgument() {
    const prefix = "--tgfx-root=";
    const argument = process.argv.find(arg => arg.startsWith(prefix));
    return argument === undefined ? undefined : argument.slice(prefix.length);
}

function readPackageIdArgument() {
    const prefix = "--package-id=";
    const argument = process.argv.find(arg => arg.startsWith(prefix));
    return argument === undefined ? undefined : argument.slice(prefix.length);
}

function resolveTgfxRoot() {
    const explicitRoot = readTgfxRootArgument() || process.env.TGFX_ROOT;
    if (explicitRoot === undefined || explicitRoot === "") {
        return path.resolve(__dirname, "../../third_party/tgfx");
    }
    return path.resolve(explicitRoot);
}

function validateTgfxRoot(root) {
    const requiredFiles = [
        "CMakeLists.txt",
        "build_tgfx",
        "include/tgfx",
        "web/src/binding.ts",
    ];
    for (const relativePath of requiredFiles) {
        if (!fs.existsSync(path.join(root, relativePath))) {
            throw new Error(`Invalid TGFX_ROOT '${root}': missing ${relativePath}`);
        }
    }
}

function gitIdentity(root) {
    const identity = {commit: null, dirty: null};
    try {
        identity.commit = execFileSync("git", ["-C", root, "rev-parse", "HEAD"], {encoding: "utf8"})
            .trim();
        identity.dirty = execFileSync("git", ["-C", root, "status", "--porcelain"], {encoding: "utf8"})
            .trim().length > 0;
    } catch (error) {
        // A source archive without Git metadata is still usable; the manifest records the path.
    }
    return identity;
}

const packageId = readPackageIdArgument() || process.env.BENCHMARK_PACKAGE_ID || null;
const packageMatch = packageId === null
    ? null
    : /^(?:candidate(?:-[a-z0-9][a-z0-9._-]*)?-)?(aot|jit)-(webgl|webgpu)$/.exec(packageId);
if (packageId !== null && packageMatch === null) {
    throw new Error(`Invalid BENCHMARK_PACKAGE_ID '${packageId}'`);
}
process.argv = process.argv.filter(arg => !arg.startsWith("--package-id="));

const tgfxRoot = resolveTgfxRoot();
validateTgfxRoot(tgfxRoot);
process.env.TGFX_ROOT = tgfxRoot;
process.argv = process.argv.filter(arg => !arg.startsWith("--tgfx-root="));
fs.writeFileSync(path.resolve(__dirname, "../.tgfx-root"), `${tgfxRoot}\n`);

if (process.argv.includes("--print-tgfx-root")) {
    console.log(tgfxRoot);
    process.exit(0);
}

const backendDir = useWebGPU ? "webgpu" : "webgl";
if (packageId !== null && !packageId.endsWith(`-${backendDir}`)) {
    throw new Error(`Package '${packageId}' does not match backend '${backendDir}'`);
}
const packageRoot = packageId === null
    ? null
    : process.env.BENCHMARK_PACKAGE_ROOT
        ? path.resolve(process.env.BENCHMARK_PACKAGE_ROOT)
        : path.resolve(__dirname, "../../../packages", packageId);
// Package builds keep the existing per-backend relative asset structure so the checked-in TS
// entry points and Emscripten locateFile callbacks remain unchanged.
const outputPath = packageRoot === null
    ? path.resolve(__dirname, `../demo/${backendDir}`)
    : path.join(packageRoot, backendDir);
fs.mkdirSync(outputPath, {recursive: true});

process.argv.push("-s");
process.argv.push("../demo");
process.argv.push("-o");
process.argv.push(outputPath);
process.argv.push("-p");
process.argv.push("web");
process.argv.push(`-DTGFX_DIR=${tgfxRoot}`);
process.argv.push(`-DBENCHMARK_BACKEND=${useWebGPU ? "WEBGPU" : "WEBGL"}`);
process.argv.push("benchmark");
require("./setup.emsdk");
const vendorBuild = path.join(tgfxRoot, "third_party/vendor_tools/lib-build");
if (process.env.TGFX_SKIP_DEPSYNC === "1" && fs.existsSync(vendorBuild)) {
    require(vendorBuild);
} else {
    require(path.join(tgfxRoot, "build_tgfx"));
}

const buildIdentity = {
    schemaVersion: "tgfx-benchmark.build.v2",
    packageId,
    backend: useWebGPU ? "webgpu" : "webgl",
    codegen: packageMatch === null ? null : packageMatch[1],
    tgfxRoot,
    ...gitIdentity(tgfxRoot),
    builtAt: new Date().toISOString(),
};
fs.writeFileSync(
    packageRoot === null
        ? path.join(outputPath, "tgfx-build.json")
        : path.join(packageRoot, "tgfx-build.json"),
    `${JSON.stringify(buildIdentity, null, 2)}\n`);
