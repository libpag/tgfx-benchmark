#!/usr/bin/env node
process.chdir(__dirname);

const fs = require("fs");
const path = require("path");
const {execFileSync} = require("child_process");

const webDir = path.resolve(__dirname, "..");
const rootStatePath = path.join(webDir, ".tgfx-root");

function resolveTgfxRoot() {
    if (process.env.TGFX_ROOT) {
        return path.resolve(process.env.TGFX_ROOT);
    }
    if (fs.existsSync(rootStatePath)) {
        return path.resolve(fs.readFileSync(rootStatePath, "utf8").trim());
    }
    return path.resolve(webDir, "../third_party/tgfx");
}

const tgfxRoot = resolveTgfxRoot();
const requiredFiles = ["web/src/binding.ts", "web/src/tgfx.ts"];
for (const relativePath of requiredFiles) {
    if (!fs.existsSync(path.join(tgfxRoot, relativePath))) {
        throw new Error(`Invalid TGFX_ROOT '${tgfxRoot}': missing ${relativePath}`);
    }
}

const relativeTgfxRoot = path.relative(webDir, tgfxRoot);
const generatedTsConfigPath = path.join(webDir, "tsconfig.tgfx.generated.json");
const generatedTsConfig = {
    extends: "./tsconfig.type.json",
    compilerOptions: {
        baseUrl: ".",
        paths: {
            "@tgfx/*": [path.join(relativeTgfxRoot, "web/src/*")],
        },
    },
    include: [path.join(relativeTgfxRoot, "web/src/tgfx.ts")],
};
fs.writeFileSync(generatedTsConfigPath, `${JSON.stringify(generatedTsConfig, null, 2)}\n`);

fs.rmSync(path.join(webDir, "lib"), {recursive: true, force: true});
fs.rmSync(path.join(webDir, "types"), {recursive: true, force: true});

const tsc = path.join(webDir, "node_modules/typescript/bin/tsc");
const rollup = path.join(webDir, "node_modules/rollup/dist/bin/rollup");
const environment = {...process.env, TGFX_ROOT: tgfxRoot};

console.log(`TGFX web source: ${tgfxRoot}`);
execFileSync(process.execPath, [tsc, "-p", generatedTsConfigPath], {
    stdio: "inherit",
    env: environment,
    cwd: webDir,
});
execFileSync(process.execPath, [
    rollup,
    "-c",
    "./script/rollup.tgfx.js",
], {stdio: "inherit", env: environment, cwd: webDir});
