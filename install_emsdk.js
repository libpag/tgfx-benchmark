const { spawn } = require('child_process');

const isWindows = process.platform === 'win32';
const isMacOS = process.platform === 'darwin';

if (!isMacOS && !isWindows) {
    console.error(`Unsupported OS: ${process.platform}`);
    process.exit(1);
}


const command = isWindows ? 'emsdk.bat' : './emsdk';
const args = ['install', 'latest'];
const child = spawn(command, args, { stdio: 'inherit' });

child.on('close', (code) => {
    if (code !== 0) {
        console.error(`Command failed with code ${code}`);
        return;
    }

    const activateArgs = ['activate', 'latest'];
    const activateChild = spawn(command, activateArgs, { stdio: 'inherit' });

    activateChild.on('close', (activateCode) => {
        if (activateCode !== 0) {
            console.error(`Activation failed with code ${activateCode}`);
            return;
        }
        console.log('emsdk installed and activated successfully.');
    });
});