const { spawn } = require('child_process');
const path = require('path');
const fs = require('fs');

const isWindows = process.platform === 'win32';
const isMacOS = process.platform === 'darwin';

function setupEnvironment() {
    const emsdkPath = path.resolve(__dirname, '../../third_party/emsdk');
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

function runCommands(commands, index = 0) {
    if (index >= commands.length) {
        console.log('All commands executed successfully.');
        return;
    }

    const [command, args] = commands[index];
    console.log(`Executing: ${command} ${args.join(' ')}`);

    const child = spawn(command, args, {
        stdio: 'inherit',
        env: { ...process.env, PATH: process.env.PATH }
    });

    child.on('close', (code) => {
        if (code !== 0) {
            console.error(`Command failed with code ${code}: ${command} ${args.join(' ')}`);
            return;
        }
        runCommands(commands, index + 1);
    });
}

function build() {
    if (!isMacOS && !isWindows) {
        console.error(`Unsupported OS: ${process.platform}`);
        return;
    }
    setupEnvironment();

    const args = process.argv.slice(2);
    const arch = args[0] || 'wasm-mt';
    const debug = args.includes('debug');
    const commands = commandSets(arch, debug);
    runCommands(commands);
}

build();