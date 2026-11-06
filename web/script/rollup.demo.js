import esbuild from 'rollup-plugin-esbuild';
import resolve from '@rollup/plugin-node-resolve';
import commonJs from '@rollup/plugin-commonjs';
import json from '@rollup/plugin-json';
import path from "path";
import {readFileSync} from "node:fs";

const fileHeaderPath = path.resolve(__dirname, '../../.idea/fileTemplates/includes/tgfx-benchmark File Header.h');
const banner = readFileSync(fileHeaderPath, 'utf-8').replace(/\$\{YEAR\}/g, new Date().getFullYear());

const arch = process.env.ARCH;
const backend = process.env.BACKEND || 'webgl';
const packageId = process.env.BENCHMARK_PACKAGE_ID || '';
const archDir = (arch === 'wasm-mt' ? 'wasm-mt' : 'wasm');
const backendDir = backend === 'webgpu' ? 'webgpu' : 'webgl';
const filePath = `${backendDir}/${archDir}`;
const fileName = backend === 'webgpu'
    ? (arch === 'wasm-mt' ? 'index-webgpu' : 'index-webgpu-st')
    : (arch === 'wasm-mt' ? 'index' : 'index-st');
const packageRoot = packageId
    ? process.env.BENCHMARK_PACKAGE_ROOT
        ? path.resolve(process.env.BENCHMARK_PACKAGE_ROOT)
        : path.resolve(__dirname, '../../../packages', packageId)
    : null;
if (packageId && !packageId.endsWith(`-${backendDir}`)) {
    throw new Error(`Package '${packageId}' does not match backend '${backendDir}'`);
}
const outputFile = packageRoot
    ? path.join(packageRoot, filePath, `${fileName}.js`)
    : `demo/${filePath}/${fileName}.js`;
const packageBenchmarkModule = packageRoot
    ? path.join(packageRoot, filePath, 'benchmark.js')
    : null;
const packageModulePlugin = {
    name: 'benchmark-package-module',
    resolveId(source) {
        if (packageBenchmarkModule !== null && source === `./${backendDir}/wasm/benchmark`) {
            return packageBenchmarkModule;
        }
        return null;
    },
};

const plugins = [
    packageModulePlugin,
    esbuild({tsconfig: "tsconfig.json", minify: false, target: 'es2020'}),
    json(),
    resolve(),
    commonJs(),
    {
        name: 'preserve-import-meta-url',
        resolveImportMeta(property, options) {
            // Preserve the original behavior of `import.meta.url`.
            if (property === 'url') {
                return 'import.meta.url';
            }
            return null;
        },
    },
];

export default [
    {
        input: `demo/${fileName}.ts`,
        output: {
            banner,
            file: outputFile,
            format: 'esm',
            sourcemap: true
        },
        plugins: plugins,
    }
];
