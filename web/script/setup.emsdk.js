const path = require('path');
const Utils = require("../../third_party/vendor_tools/lib/Utils");

const emsdkPath = path.resolve(__dirname, '../../third_party/emsdk');
const emscriptenPath = path.resolve(emsdkPath, 'upstream/emscripten');
process.env.PATH = process.platform === 'win32'
    ? `${emsdkPath};${emscriptenPath};${process.env.PATH}`
    : `${emsdkPath}:${emscriptenPath}:${process.env.PATH}`;

// The vendored emsdk checkout must be at (or after) this tag; otherwise emsdk_manifest.json
// won't know this SDK version and `emsdk install` fails with "tool or SDK not found".
const emscriptenVersion = "4.0.15";
Utils.exec(`emsdk install ${emscriptenVersion}`, emsdkPath);
Utils.exec(`emsdk activate ${emscriptenVersion}`, emsdkPath);

const emsdkEnv = process.platform === 'win32' ? "emsdk_env.bat" : "source emsdk_env.sh";
let result = Utils.execSafe(emsdkEnv, emsdkPath);
let lines = result.split("\n");
for (let line of lines) {
    let values = line.split("=");
    if (values.length > 1) {
        process.stdout.write(line);
        let key = values[0].trim();
        process.env[key] = values[1].trim();
    }
}