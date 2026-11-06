#!/usr/bin/env node
'use strict';

const crypto = require('crypto');
const fs = require('fs');
const os = require('os');
const path = require('path');
const {execFileSync} = require('child_process');
const {
  candidatePackageId: makeCandidatePackageId,
  normalizeTag,
  writePackageCatalog,
} = require('./package-catalog');

const webDir = path.resolve(__dirname, '..');
const workspaceDir = path.resolve(webDir, '../..');
const packagesDir = path.join(workspaceDir, 'packages');
const benchmarkRoot = path.resolve(webDir, '..');
const compatibilityPatchesDir = path.join(webDir, 'patches/compatibility');
const buildLockPath = path.join(packagesDir, '.build.lock');
const aotSource = 'sources/tgfx-aot';
const aotRemoteRef = 'refs/remotes/origin/feature/shenxianfei_aot_simplify';
const definitions = {
  'aot-webgl': {
    backend: 'webgl',
    codegen: 'aot',
    source: aotSource,
    remoteRef: aotRemoteRef,
  },
  'aot-webgpu': {
    backend: 'webgpu',
    codegen: 'aot',
    source: aotSource,
    remoteRef: aotRemoteRef,
  },
  'jit-webgl': {
    backend: 'webgl',
    codegen: 'jit',
    source: 'tgfx-main-perf',
    remoteRef: 'refs/remotes/origin/main',
  },
  'jit-webgpu': {
    backend: 'webgpu',
    codegen: 'jit',
    source: 'tgfx-main-perf',
    remoteRef: 'refs/remotes/origin/main',
  },
};
const packageOrder = ['aot-webgl', 'aot-webgpu', 'jit-webgl', 'jit-webgpu'];

function parseArgs(argv) {
  const args = {
    all: false,
    candidate: false,
    packageId: null,
    tgfxRoot: null,
    tag: null,
  };
  for (let index = 2; index < argv.length; index++) {
    const argument = argv[index];
    if (argument === '--all') {
      args.all = true;
    } else if (argument === '--candidate') {
      args.candidate = true;
    } else if (argument.startsWith('--package=')) {
      args.packageId = argument.slice('--package='.length);
    } else if (argument.startsWith('--tgfx-root=')) {
      args.tgfxRoot = argument.slice('--tgfx-root='.length);
    } else if (argument.startsWith('--tag=') || argument.startsWith('--label=')) {
      args.tag = argument.slice(argument.indexOf('=') + 1);
    } else {
      throw new Error(`Unknown argument: ${argument}`);
    }
  }
  if (!args.all && !args.packageId) {
    throw new Error(
        'usage: node script/build-package.js --all | ' +
        '--package=<id> [--tgfx-root=<path>] [--candidate] [--tag=<name>]');
  }
  if (args.all && args.candidate) {
    throw new Error('--candidate requires one --package=<id>');
  }
  if (!args.candidate && args.tag != null) {
    throw new Error('--tag/--label requires --candidate');
  }
  args.tag = normalizeTag(args.tag);
  return args;
}

function git(root, ...args) {
  return execFileSync('git', ['-C', root, ...args], {encoding: 'utf8'}).trim();
}

function sha256Buffer(buffer) {
  return crypto.createHash('sha256').update(buffer).digest('hex');
}

function sha256(file) {
  return sha256Buffer(fs.readFileSync(file));
}

function fileInfo(root, relativePath) {
  const file = path.join(root, relativePath);
  if (!fs.existsSync(file)) {
    throw new Error(`Missing build artifact: ${file}`);
  }
  return {
    path: relativePath,
    bytes: fs.statSync(file).size,
    sha256: sha256(file),
  };
}

function parseManifest(file) {
  const values = {};
  for (const line of fs.readFileSync(file, 'utf8').split(/\r?\n/)) {
    if (!line || line.startsWith('#')) continue;
    const separator = line.indexOf('=');
    if (separator > 0) {
      values[line.slice(0, separator)] = line.slice(separator + 1);
    }
  }
  return values;
}

function verifyOfficialSource(definition, tgfxRoot) {
  const status = git(tgfxRoot, 'status', '--porcelain');
  const workspaceCommit = git(tgfxRoot, 'rev-parse', 'HEAD');
  const commit = git(tgfxRoot, 'rev-parse', definition.remoteRef);
  return {
    commit,
    dirty: status.length > 0,
    status,
    workspaceCommit,
    remoteRef: definition.remoteRef,
    remoteCommit: commit,
    atRemoteRef: workspaceCommit === commit,
  };
}

function materializeHeadSnapshot(tgfxRoot, commit) {
  const snapshotRoot = fs.mkdtempSync(path.join(os.tmpdir(), 'tgfx-official-head-'));
  const archive = execFileSync(
      'git',
      ['-C', tgfxRoot, 'archive', '--format=tar', commit],
      {encoding: 'buffer', maxBuffer: 512 * 1024 * 1024});
  execFileSync('tar', ['-xf', '-', '-C', snapshotRoot], {input: archive});
  const thirdPartyRoot = path.join(tgfxRoot, 'third_party');
  if (fs.existsSync(thirdPartyRoot)) {
    fs.symlinkSync(fs.realpathSync(thirdPartyRoot), path.join(snapshotRoot, 'third_party'), 'dir');
  }
  return snapshotRoot;
}

function createBuildToolRoot() {
  const root = fs.mkdtempSync(path.join(os.tmpdir(), 'tgfx-build-tools-'));
  const binaryDir = path.join(root, 'bin');
  fs.mkdirSync(binaryDir);
  const depsyncPath = path.join(binaryDir, 'depsync');
  fs.writeFileSync(depsyncPath, '#!/bin/sh\nexit 0\n');
  fs.chmodSync(depsyncPath, 0o755);
  return {root, binaryDir};
}

function captureSourcePatch(tgfxRoot) {
  const temporaryIndex = path.join(
      fs.mkdtempSync(path.join(os.tmpdir(), 'tgfx-candidate-index-')), 'index');
  const environment = {...process.env, GIT_INDEX_FILE: temporaryIndex};
  try {
    execFileSync('git', ['-C', tgfxRoot, 'read-tree', 'HEAD'], {
      env: environment,
      stdio: 'pipe',
    });
    execFileSync('git', ['-C', tgfxRoot, 'add', '-A', '--', '.'], {
      env: environment,
      stdio: 'pipe',
    });
    return execFileSync(
        'git',
        ['-C', tgfxRoot, 'diff', '--cached', '--binary', 'HEAD', '--'],
        {env: environment, encoding: 'buffer', maxBuffer: 256 * 1024 * 1024});
  } finally {
    fs.rmSync(path.dirname(temporaryIndex), {recursive: true, force: true});
  }
}

function verifyCandidateSource(definition, tgfxRoot) {
  const status = git(tgfxRoot, 'status', '--porcelain');
  const commit = git(tgfxRoot, 'rev-parse', 'HEAD');
  const remoteCommit = git(tgfxRoot, 'rev-parse', definition.remoteRef);
  const patch = captureSourcePatch(tgfxRoot);
  return {
    commit,
    dirty: status.length > 0,
    status,
    remoteRef: definition.remoteRef,
    remoteCommit,
    atRemoteRef: commit === remoteCommit,
    sourceDiff: {
      format: 'git-binary-patch',
      path: 'candidate-source.patch',
      bytes: patch.length,
      sha256: sha256Buffer(patch),
    },
    patch,
  };
}

function run(command, args, environment) {
  execFileSync(command, args, {
    cwd: webDir,
    env: environment,
    stdio: 'inherit',
  });
}

function applyCompatibilityPatches(tgfxRoot, codegen, callback) {
  if (!fs.existsSync(compatibilityPatchesDir)) {
    return callback([]);
  }
  const configPath = path.join(compatibilityPatchesDir, 'index.json');
  const config = JSON.parse(fs.readFileSync(configPath, 'utf8'));
  if (config.schemaVersion !== 'tgfx-benchmark.compatibility-patches.v1' ||
      !Array.isArray(config.patches)) {
    throw new Error(`Invalid compatibility patch manifest: ${configPath}`);
  }
  const configured = new Map(config.patches.map(entry => [entry.file, entry]));
  const patchFiles = fs.readdirSync(compatibilityPatchesDir)
      .filter(file => file.endsWith('.patch'))
      .sort();
  for (const file of patchFiles) {
    if (!configured.has(file)) {
      throw new Error(`Compatibility patch '${file}' is missing from ${configPath}`);
    }
  }
  const selectedPatches = config.patches
      .filter(entry => (entry.codegens || ['aot']).includes(codegen))
      .sort((left, right) => left.file.localeCompare(right.file));
  if (selectedPatches.length === 0) {
    return callback([]);
  }
  const records = [];
  const applied = [];
  try {
    for (const {file} of selectedPatches) {
      const patchPath = path.join(compatibilityPatchesDir, file);
      const record = {
        id: path.basename(file, '.patch'),
        path: path.relative(webDir, patchPath),
        sha256: sha256(patchPath),
      };
      try {
        execFileSync('git', ['-C', tgfxRoot, 'apply', '--check', patchPath], {
          encoding: 'utf8',
          stdio: 'pipe',
        });
        execFileSync('git', ['-C', tgfxRoot, 'apply', patchPath], {
          encoding: 'utf8',
          stdio: 'pipe',
        });
        record.state = 'applied';
        applied.push(patchPath);
      } catch (applyError) {
        const markers = configured.get(file).integratedMarkers || [];
        const integrated = markers.some(marker => {
          const sourcePath = path.join(tgfxRoot, marker.file);
          return fs.existsSync(sourcePath) &&
            fs.readFileSync(sourcePath, 'utf8').includes(marker.contains);
        });
        if (integrated) {
          record.state = 'already-integrated';
          records.push(record);
          continue;
        }
        try {
          execFileSync('git', ['-C', tgfxRoot, 'apply', '--reverse', '--check', patchPath], {
            encoding: 'utf8',
            stdio: 'pipe',
          });
        } catch (reverseError) {
          throw new Error(
              `Compatibility patch '${file}' neither applies nor is already integrated: ` +
              `${applyError.message}\n${reverseError.message}`);
        }
        record.state = 'already-integrated';
      }
      records.push(record);
    }
    return callback(records);
  } finally {
    for (const patchPath of applied.reverse()) {
      git(tgfxRoot, 'apply', '-R', patchPath);
    }
  }
}

function acquireBuildLock() {
  fs.mkdirSync(packagesDir, {recursive: true});
  const token = crypto.randomBytes(8).toString('hex');
  const payload = JSON.stringify({
    pid: process.pid,
    token,
    startedAt: new Date().toISOString(),
  });
  const writeLock = () => {
    const descriptor = fs.openSync(buildLockPath, 'wx');
    fs.writeFileSync(descriptor, `${payload}\n`);
    fs.closeSync(descriptor);
  };
  try {
    writeLock();
  } catch (error) {
    if (error.code !== 'EEXIST') throw error;
    let owner = null;
    try {
      owner = JSON.parse(fs.readFileSync(buildLockPath, 'utf8'));
    } catch (readError) {
      // An unreadable lock cannot protect a live build.
    }
    let ownerAlive = false;
    if (Number.isInteger(owner?.pid) && owner.pid > 0) {
      try {
        process.kill(owner.pid, 0);
        ownerAlive = true;
      } catch (killError) {
        ownerAlive = killError.code !== 'ESRCH';
      }
    }
    if (!ownerAlive) {
      fs.rmSync(buildLockPath, {force: true});
      writeLock();
    } else {
      throw new Error(
          `Another package build is active${owner?.pid ? ` (pid ${owner.pid})` : ''}. ` +
          `Wait for it to finish or remove ${buildLockPath} after confirming it is stale.`);
    }
  }
  return () => {
    try {
      const owner = JSON.parse(fs.readFileSync(buildLockPath, 'utf8'));
      if (owner.token === token) {
        fs.rmSync(buildLockPath, {force: true});
      }
    } catch (error) {
      // The lock may already have been removed after a process-level failure.
    }
  };
}

function withBuildLock(callback) {
  const release = acquireBuildLock();
  try {
    return callback();
  } finally {
    release();
  }
}

function createStagingRoot(packageId) {
  const stagingParent = path.join(packagesDir, '.staging');
  fs.mkdirSync(stagingParent, {recursive: true});
  return fs.mkdtempSync(path.join(stagingParent, `${packageId}-`));
}

function buildWorkRoot(basePackageId) {
  const root = path.join(packagesDir, '.cache', 'build', basePackageId);
  fs.mkdirSync(root, {recursive: true});
  return root;
}

function publishStagedPackage(stagingRoot, packageRoot) {
  fs.mkdirSync(path.dirname(packageRoot), {recursive: true});
  const backupRoot = `${packageRoot}.previous-${process.pid}-${Date.now()}`;
  const hadPrevious = fs.existsSync(packageRoot);
  if (hadPrevious) {
    fs.renameSync(packageRoot, backupRoot);
  }
  try {
    fs.renameSync(stagingRoot, packageRoot);
  } catch (error) {
    if (hadPrevious && !fs.existsSync(packageRoot)) {
      fs.renameSync(backupRoot, packageRoot);
    }
    throw error;
  }
  if (hadPrevious) {
    fs.rmSync(backupRoot, {recursive: true, force: true});
  }
}

function captureBenchmarkSource(packageRoot) {
  const patch = captureSourcePatch(benchmarkRoot);
  if (patch.length === 0) {
    return null;
  }
  const fileName = 'benchmark-source.patch';
  fs.writeFileSync(path.join(packageRoot, fileName), patch);
  return {
    format: 'git-binary-patch',
    path: fileName,
    bytes: patch.length,
    sha256: sha256Buffer(patch),
  };
}

function hashBenchmarkBuildInputs(root = benchmarkRoot) {
  const inputs = [
    'CMakeLists.txt',
    'cmake',
    'src',
    'web/demo',
    'web/script/build-tgfx.js',
    'web/script/cmake.demo.js',
    'web/script/rollup.demo.js',
    'web/script/rollup.tgfx.js',
    'web/script/setup.emsdk.js',
  ];
  const excluded = new Set([
    'web/demo/benchmark-ui.css',
    'web/demo/benchmark-ui.html',
    'web/demo/benchmark-ui.js',
  ]);
  const listFiles = extraArgs => execFileSync(
      'git',
      ['-C', root, 'ls-files', '-z', ...extraArgs, '--', ...inputs],
      {encoding: 'utf8', maxBuffer: 32 * 1024 * 1024});
  const fileSet = new Set([
    ...listFiles([]).split('\0'),
    ...listFiles(['--others', '--exclude-standard']).split('\0'),
  ]);
  const relativeFiles = [...fileSet]
      .filter(file => file && !excluded.has(file) && fs.existsSync(path.join(root, file)))
      .sort();
  const hash = crypto.createHash('sha256');
  for (const file of relativeFiles) {
    hash.update(file);
    hash.update('\0');
    hash.update(fs.readFileSync(path.join(root, file)));
    hash.update('\0');
  }
  return {
    sha256: hash.digest('hex'),
    fileCount: relativeFiles.length,
  };
}

function bundleInfo(definition, tgfxRoot, packageRoot) {
  if (definition.codegen !== 'aot') {
    return null;
  }
  const profile = definition.backend === 'webgpu' ? 'webgpu' : 'opengles';
  const buildRoot = path.join(
    definition.workRoot, definition.backend, 'build-benchmark/wasm');
  const generatedRoot = path.join(buildRoot, 'generated/shaders');
  const generatedBundle = path.join(generatedRoot, `shader_bundle.${profile}.bin`);
  const generatedManifest =
    path.join(generatedRoot, `shader_bundle.${profile}.manifest`);
  const sourceBundle = path.join(
    tgfxRoot, `resources/shaders/shader_bundle.${profile}.bin`);
  const hasGeneratedBundle = fs.existsSync(generatedBundle);
  const hasGeneratedManifest = fs.existsSync(generatedManifest);
  if (hasGeneratedBundle && !hasGeneratedManifest) {
    if (!fs.existsSync(sourceBundle) ||
        sha256(generatedBundle) !== sha256(sourceBundle)) {
      throw new Error(
          `Generated shader bundle ${generatedBundle} has no manifest and does not match ` +
          `the source-published bundle ${sourceBundle}`);
    }
  }
  const useGenerated = hasGeneratedBundle && hasGeneratedManifest;
  const bundleRoot = useGenerated ? buildRoot : tgfxRoot;
  const relativeManifest = useGenerated
    ? `generated/shaders/shader_bundle.${profile}.manifest`
    : `resources/shaders/shader_bundle.${profile}.manifest`;
  const manifest = parseManifest(path.join(bundleRoot, relativeManifest));
  const relativeBundle = useGenerated
    ? `generated/shaders/shader_bundle.${profile}.bin`
    : `resources/shaders/shader_bundle.${profile}.bin`;
  const bundle = fileInfo(bundleRoot, relativeBundle);
  const bundleBytes = fs.readFileSync(path.join(bundleRoot, relativeBundle));
  const wasmPath = path.join(packageRoot, definition.backend, 'wasm/benchmark.wasm');
  const wasmBytes = fs.readFileSync(wasmPath);
  const embeddedByteOffset = wasmBytes.indexOf(bundleBytes);
  const embeddedSourcePath = path.join(
    buildRoot, `generated/embedded/EmbeddedShaderBundle_${profile}.cpp`);
  let embeddedSourceMatchesBundle = null;
  if (fs.existsSync(embeddedSourcePath)) {
    const source = fs.readFileSync(embeddedSourcePath, 'utf8');
    const arrayStart = source.indexOf('[] = {');
    const arrayEnd = source.indexOf('};', arrayStart);
    if (arrayStart >= 0 && arrayEnd > arrayStart) {
      const values = source.slice(arrayStart, arrayEnd).match(/0x[0-9a-f]{2}/gi) || [];
      embeddedSourceMatchesBundle =
        sha256Buffer(Buffer.from(values.map(value => Number.parseInt(value.slice(2), 16)))) ===
        bundle.sha256;
    } else {
      embeddedSourceMatchesBundle = false;
    }
  }
  const sourceResourceBundle = path.join(
    tgfxRoot, `resources/shaders/shader_bundle.${profile}.bin`);
  return {
    ...bundle,
    effectiveSource: useGenerated ? 'build-generated' : 'source-published-resource',
    identityHash: manifest.identityHash || null,
    toolchainABI: manifest.toolchainABI || null,
    formatVersion: manifest.formatVersion || null,
    shaderCount: Number(manifest.shaderCount || 0),
    stageCount: Number(manifest.stageCount || 0),
    embeddedInWasm: embeddedByteOffset >= 0,
    embeddedByteOffset,
    contiguousEmbeddedBytesFound: embeddedByteOffset >= 0,
    embeddedSourceMatchesBundle,
    embeddedSource: fs.existsSync(embeddedSourcePath)
      ? fileInfo(
          buildRoot,
          path.relative(buildRoot, embeddedSourcePath))
      : null,
    sourcePublishedResource: fs.existsSync(sourceResourceBundle)
      ? {
          ...fileInfo(tgfxRoot, `resources/shaders/shader_bundle.${profile}.bin`),
          matchesEffective: sha256(sourceResourceBundle) === bundle.sha256,
        }
      : null,
  };
}

function writePackageReadme(packageRoot, definition, tgfxRoot, identity) {
  const lines = [
    `# ${definition.packageId}`,
    '',
    '- Status: built',
    `- Package role: ${identity.packageRole}`,
    `- Source: \`${tgfxRoot}\``,
    `- TGFX commit: \`${identity.commit}\``,
    `- Backend: ${definition.backend}`,
    `- Codegen: ${definition.codegen}`,
    '- Profile: Release, single-thread',
    `- Built at: ${identity.builtAt}`,
  ];
  if (identity.packageRole === 'candidate') {
    lines.push(
        `- Base package: \`${identity.basePackageId}\``,
        ...(definition.tag ? [`- Candidate tag: \`${definition.tag}\``] : []),
        `- Source patch: \`${identity.sourceDiff.path}\``,
        `- Source patch SHA-256: \`${identity.sourceDiff.sha256}\``,
        '- Visibility: benchmark UI package-source selector, command-line and direct URL.');
  } else {
    lines.push(
        '- Source mode: clean Git HEAD snapshot',
        `- Compatibility patches: ${identity.compatibilityPatches.length}`);
  }
  lines.push(
      '',
      'Build command:',
      '',
      '```sh',
      'cd tgfx-benchmark/web',
      `node script/build-package.js --package=${identity.basePackageId || definition.packageId}` +
        `${identity.packageRole === 'candidate' ? ' --candidate' : ''}` +
        `${identity.packageRole === 'candidate' ? ` --tgfx-root=${tgfxRoot}` : ''}` +
        `${identity.packageRole === 'candidate' && definition.tag ? ` --tag=${definition.tag}` : ''}`,
      '```',
      '',
      'The generated `tgfx-build.json` is the authoritative package identity.',
      '');
  fs.writeFileSync(path.join(packageRoot, 'README.md'), lines.join('\n'));
}

function buildPackage(definition) {
  const packageRoot = definition.stagingRoot;
  const workRoot = definition.workRoot;
  const tgfxRoot = path.resolve(definition.tgfxRoot);
  const source = definition.source;
  const originalTgfxRoot = path.resolve(definition.originalTgfxRoot);
  const initialStatus = source.status || '';
  const environment = {
    ...process.env,
    BENCHMARK_PACKAGE_ID: definition.packageId,
    BENCHMARK_PACKAGE_ROOT: workRoot,
    NODE_OPTIONS: [
      process.env.NODE_OPTIONS || '',
      '--preserve-symlinks',
      '--preserve-symlinks-main',
    ].filter(Boolean).join(' '),
    TGFX_ROOT: tgfxRoot,
    TGFX_SKIP_DEPSYNC: '1',
    PATH: `${definition.buildToolRoot.binaryDir}${path.delimiter}${process.env.PATH || ''}`,
  };
  const backendDir = path.join(workRoot, definition.backend);
  const wasmDir = path.join(backendDir, 'wasm');
  const templateName = definition.backend === 'webgpu' ? 'index-webgpu-st.html' : 'index-st.html';
  const appliedPatches = [];

  console.log(`\n===== ${definition.packageId} =====`);
  console.log(
      `TGFX ${source.commit} (${path.basename(originalTgfxRoot)}, ` +
      `${definition.candidate ? 'candidate worktree' : 'clean HEAD snapshot'})`);
  fs.rmSync(wasmDir, {recursive: true, force: true});
  fs.rmSync(path.join(backendDir, '.benchmark.wasm.md5'), {force: true});
  fs.mkdirSync(wasmDir, {recursive: true});

  const cmakeArgs = ['script/cmake.demo.js', '-a', 'wasm',
    `--package-id=${definition.packageId}`, `--tgfx-root=${tgfxRoot}`];
  if (definition.candidate) {
    cmakeArgs.push('-i');
  }
  if (definition.backend === 'webgpu') {
    cmakeArgs.push('--webgpu');
  }
  const extraCxxFlags = (process.env.TGFX_BENCHMARK_CXX_FLAGS || '').trim();
  if (extraCxxFlags) {
    // Candidate builds can opt into compile-time experiments without changing the production
    // default. The flag is part of the separately recorded build identity.
    cmakeArgs.push(`-DCMAKE_CXX_FLAGS="-fno-rtti ${extraCxxFlags}"`);
  }
  const buildCMake = () => run(process.execPath, cmakeArgs, environment);
  applyCompatibilityPatches(tgfxRoot, definition.codegen, patches => {
    appliedPatches.push(...patches);
    buildCMake();
  });
  if (definition.candidate && appliedPatches.length > 0) {
    const sourceStatus = git(tgfxRoot, 'status', '--porcelain');
    if (sourceStatus !== initialStatus) {
      throw new Error(`Temporary build patches did not restore ${tgfxRoot}: ${sourceStatus}`);
    }
  }
  run(process.execPath, ['script/build-tgfx.js'], environment);
  run(process.execPath, [
    path.join(webDir, 'node_modules/rollup/dist/bin/rollup'),
    '-c',
    'script/rollup.demo.js',
    '--environment',
    `ARCH:wasm,BACKEND:${definition.backend}`,
  ], environment);

  const publishedBackendDir = path.join(packageRoot, definition.backend);
  fs.mkdirSync(publishedBackendDir, {recursive: true});
  const publishedWasmDir = path.join(publishedBackendDir, 'wasm');
  if (path.resolve(wasmDir) !== path.resolve(publishedWasmDir)) {
    fs.cpSync(wasmDir, publishedWasmDir, {recursive: true});
  }
  const wasmMd5 = path.join(backendDir, '.benchmark.wasm.md5');
  const publishedWasmMd5 = path.join(publishedBackendDir, '.benchmark.wasm.md5');
  if (fs.existsSync(wasmMd5) && path.resolve(wasmMd5) !== path.resolve(publishedWasmMd5)) {
    fs.copyFileSync(wasmMd5, publishedWasmMd5);
  }
  fs.copyFileSync(path.join(webDir, 'demo/index.css'), path.join(packageRoot, 'index.css'));
  fs.copyFileSync(path.join(webDir, 'demo', templateName), path.join(packageRoot, 'index.html'));
  fs.cpSync(path.join(webDir, 'demo/static'), path.join(packageRoot, 'static'), {recursive: true});

  if (definition.candidate) {
    fs.writeFileSync(path.join(packageRoot, source.sourceDiff.path), source.patch);
  }
  const benchmarkSourceDiff = captureBenchmarkSource(packageRoot);
  const identity = {
    schemaVersion: 'tgfx-benchmark.build.v2',
    packageId: definition.packageId,
    packageRole: definition.candidate ? 'candidate' : 'official',
    sourceMode: definition.candidate ? 'worktree' : 'head-snapshot',
    ...(definition.tag ? {tag: definition.tag} : {}),
    basePackageId: definition.basePackageId,
    baseCommit: source.commit,
    remoteRef: source.remoteRef,
    remoteCommit: source.remoteCommit,
    atRemoteRef: source.atRemoteRef,
    ...(definition.candidate ? {
      sourceDiff: source.sourceDiff,
    } : {}),
    backend: definition.backend,
    codegen: definition.codegen,
    threadMode: 'st',
    buildType: 'release',
    tgfxRoot: originalTgfxRoot,
    commit: source.commit,
    dirty: definition.candidate ? source.dirty : false,
    sourceWorkspaceDirty: source.dirty,
    sourceWorkspaceCommit: source.workspaceCommit || source.commit,
    sourceName: path.basename(originalTgfxRoot),
    compatibilityPatches: appliedPatches,
    buildPatches: appliedPatches,
    builtAt: new Date().toISOString(),
    benchmark: {
      commit: git(benchmarkRoot, 'rev-parse', 'HEAD'),
      dirty: git(benchmarkRoot, 'status', '--porcelain').length > 0,
      sourceDiff: benchmarkSourceDiff,
      buildInputs: hashBenchmarkBuildInputs(),
    },
    artifacts: {
      entry: fileInfo(
          packageRoot,
          `${definition.backend}/wasm/` +
            `${definition.backend === 'webgpu' ? 'index-webgpu-st' : 'index-st'}.js`),
      glue: fileInfo(packageRoot, `${definition.backend}/wasm/benchmark.js`),
      wasm: fileInfo(packageRoot, `${definition.backend}/wasm/benchmark.wasm`),
      shaderBundle: bundleInfo(definition, tgfxRoot, packageRoot),
    },
  };
  fs.writeFileSync(
      path.join(packageRoot, 'tgfx-build.json'),
      `${JSON.stringify(identity, null, 2)}\n`);
  writePackageReadme(packageRoot, {
    ...definition,
    packageId: definition.packageId,
  }, originalTgfxRoot, identity);
  console.log(`Built ${definition.packageId}: ${path.join(packageRoot, 'index.html')}`);
}

function buildOnePackage(basePackageId, args) {
  const definition = definitions[basePackageId];
  if (!definition) {
    throw new Error(`Unknown package: ${basePackageId}`);
  }
  const originalTgfxRoot = path.resolve(
      args.tgfxRoot || path.join(workspaceDir, definition.source));
  const packageId = args.candidate
    ? makeCandidatePackageId(basePackageId, args.tag)
    : basePackageId;
  const source = args.candidate
    ? verifyCandidateSource(definition, originalTgfxRoot)
    : verifyOfficialSource(definition, originalTgfxRoot);
  let buildSourceRoot = originalTgfxRoot;
  if (!args.candidate) {
    buildSourceRoot = materializeHeadSnapshot(originalTgfxRoot, source.commit);
  }
  const stagingRoot = createStagingRoot(packageId);
  const buildToolRoot = createBuildToolRoot();
  const packageRoot = path.join(packagesDir, packageId);
  let published = false;
  try {
    buildPackage({
      ...definition,
      basePackageId,
      candidate: args.candidate,
      packageId,
      tag: args.tag,
      source,
      tgfxRoot: buildSourceRoot,
      originalTgfxRoot,
      stagingRoot,
      workRoot: args.candidate
        ? buildWorkRoot(
            `candidate-${args.tag ? `${args.tag}-` : ''}${basePackageId}`)
        : stagingRoot,
      buildToolRoot,
    });
    publishStagedPackage(stagingRoot, packageRoot);
    published = true;
  } finally {
    fs.rmSync(stagingRoot, {recursive: true, force: true});
    fs.rmSync(buildToolRoot.root, {recursive: true, force: true});
    if (!args.candidate && buildSourceRoot !== originalTgfxRoot) {
      fs.rmSync(buildSourceRoot, {recursive: true, force: true});
    }
  }
  return {packageId, published, packageRoot};
}

function main() {
  const args = parseArgs(process.argv);
  const ids = args.all ? packageOrder : [args.packageId];
  withBuildLock(() => {
    for (const packageId of ids) {
      buildOnePackage(packageId, args);
    }
    writePackageCatalog();
  });
}

module.exports = {
  applyCompatibilityPatches,
  captureSourcePatch,
  createStagingRoot,
  hashBenchmarkBuildInputs,
  materializeHeadSnapshot,
  parseArgs,
  publishStagedPackage,
  verifyCandidateSource,
  verifyOfficialSource,
  verifySource: verifyOfficialSource,
};

if (require.main === module) {
  main();
}
