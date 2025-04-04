const { spawnSync } = require('child_process');
const path = require('path');
const fs = require('fs');

const isWindows = process.platform === 'win32';
const isMacOS = process.platform === 'darwin';

const emsdkPath = path.resolve(__dirname, '../../third_party/emsdk');
const emsdkCommandPath = path.resolve(emsdkPath, isWindows ? 'emsdk.bat' : 'emsdk');
const emsdkCommandSets = [
    [emsdkCommandPath, ['install', 'latest']],
    [emsdkCommandPath, ['activate', 'latest']]
];

function setupEnvironment() {
    const emscriptenPath = path.resolve(__dirname, '../../third_party/emsdk/upstream/emscripten');
    process.env.PATH = isWindows
        ? `${emsdkPath};${emscriptenPath};${process.env.PATH}`
        : `${emsdkPath}:${emscriptenPath}:${process.env.PATH}`;
}

const rollupPath = path.resolve(__dirname, `../node_modules/.bin/rollup${isWindows ? '.cmd' : ''}`);

if (!fs.existsSync(rollupPath)) {
    console.error('Rollup is not installed. Please run `npm install rollup --save-dev`.');
    process.exit(1);
}

const commandSets = (arch, debug) => [
    ['node', ['script/cmake.demo.js', '-a', arch, ...(debug ? ['--debug'] : [])]],
    [isWindows ? 'npm.cmd' : 'npm', ['run', 'build:tgfx']],
    [rollupPath, ['-c', './script/rollup.demo.js', '--environment', `ARCH:${arch}`]]
];

function runCommands(commands, verbose = true) {
    for (const [command, args] of commands) {
        console.log(`Executing: ${command} ${args.join(' ')}`);
        const result = spawnSync(command, args, {
            stdio: verbose ? 'inherit' : 'ignore',
            env: { ...process.env, PATH: process.env.PATH }
        });

        if (result.error) {
            console.error(`Command failed with error: ${result.error.message}`);
            return;
        }

        if (result.status !== 0) {
            console.error(`Command failed with code ${result.status}: ${command} ${args.join(' ')}`);
            return;
        }
    }
    console.log('All commands executed successfully.');
}

function build() {
    if (!isMacOS && !isWindows) {
        console.error(`Unsupported OS: ${process.platform}`);
        return;
    }
    runCommands(emsdkCommandSets, false);

    setupEnvironment();

    const args = process.argv.slice(2);
    const arch = args[0] || 'wasm-mt';
    const debug = args.includes('debug');
    const commands = commandSets(arch, debug);
    runCommands(commands);
}

build();