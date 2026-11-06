#!/usr/bin/env node
'use strict';

const crypto = require('crypto');
const fs = require('fs');
const path = require('path');
const {execFileSync} = require('child_process');

const repoRoot = path.resolve(__dirname, '../..');
const packagesRoot = path.resolve(repoRoot, '../packages');
const baseUrl = (process.argv[2] || 'http://localhost:8061').replace(/\/+$/, '');
const allowStale = process.env.BENCHMARK_ALLOW_STALE === '1';

function packageSpec(packageId) {
  const manifestPath = path.join(packagesRoot, packageId, 'tgfx-build.json');
  if (!fs.existsSync(manifestPath)) {
    throw new Error(`Package manifest is missing: ${manifestPath}`);
  }
  const identity = JSON.parse(fs.readFileSync(manifestPath, 'utf8'));
  if (!['aot', 'jit'].includes(identity.codegen) ||
      !['webgl', 'webgpu'].includes(identity.backend)) {
    throw new Error(`Unsupported package identity: ${packageId}`);
  }
  const basePackageId = identity.basePackageId || `${identity.codegen}-${identity.backend}`;
  return {
    packageId,
    basePackageId,
    codegen: identity.codegen,
    backend: identity.backend,
    packageRole: identity.packageRole === 'candidate' ? 'candidate' : 'official',
    entry: identity.backend === 'webgpu'
      ? 'webgpu/wasm/index-webgpu-st.js'
      : 'webgl/wasm/index-st.js',
    glue: `${identity.backend}/wasm/benchmark.js`,
    wasm: `${identity.backend}/wasm/benchmark.wasm`,
  };
}

function sha256(buffer) {
  return crypto.createHash('sha256').update(buffer).digest('hex');
}

async function fetchBuffer(url) {
  const response = await fetch(url);
  if (!response.ok) {
    throw new Error(`${url} returned ${response.status}`);
  }
  return Buffer.from(await response.arrayBuffer());
}

function assert(condition, message) {
  if (!condition) {
    throw new Error(message);
  }
}

function verifySourceDiff(packageRoot, sourceDiff, label) {
  assert(sourceDiff?.format === 'git-binary-patch' && sourceDiff.path,
      `${label}: source patch metadata is missing`);
  const patchPath = path.join(packageRoot, sourceDiff.path);
  assert(fs.existsSync(patchPath), `${label}: source patch is missing`);
  const patch = fs.readFileSync(patchPath);
  assert(patch.length === sourceDiff.bytes, `${label}: source patch size mismatch`);
  assert(sha256(patch) === sourceDiff.sha256, `${label}: source patch hash mismatch`);
}

function verifySourceIdentity(identity, spec, packageRoot) {
  const configuredRoot = identity.tgfxRoot;
  const localSourceRoot = path.resolve(
    repoRoot, '../sources', identity.sourceName || '');
  const tgfxRoot = configuredRoot && fs.existsSync(configuredRoot)
    ? configuredRoot
    : localSourceRoot;
  assert(tgfxRoot && fs.existsSync(tgfxRoot), `${spec.packageId}: missing TGFX source root`);
  const commit = execFileSync('git', ['-C', tgfxRoot, 'rev-parse', 'HEAD'], {encoding: 'utf8'})
      .trim();
  const status = execFileSync('git', ['-C', tgfxRoot, 'status', '--porcelain'], {encoding: 'utf8'})
      .trim();
  const remoteRef = identity.remoteRef || (spec.codegen === 'aot'
    ? 'refs/remotes/origin/feature/shenxianfei_aot_simplify'
    : 'refs/remotes/origin/main');
  const latest = execFileSync('git', ['-C', tgfxRoot, 'rev-parse', remoteRef], {encoding: 'utf8'})
      .trim();
  if (spec.packageRole === 'candidate') {
    assert(commit === identity.commit,
        `${spec.packageId}: manifest commit does not match source HEAD`);
    assert(identity.packageRole === 'candidate',
        `${spec.packageId}: candidate manifest has the wrong package role`);
    assert((status.length > 0) === identity.dirty,
        `${spec.packageId}: manifest dirty state does not match source status`);
    assert(identity.basePackageId === spec.basePackageId,
        `${spec.packageId}: candidate base package mismatch`);
    assert(identity.remoteRef === remoteRef, `${spec.packageId}: candidate remote ref mismatch`);
    assert(allowStale || identity.remoteCommit === latest,
        `${spec.packageId}: candidate remote commit is stale`);
    assert(identity.atRemoteRef === (commit === latest),
        `${spec.packageId}: candidate remote position mismatch`);
    verifySourceDiff(packageRoot, identity.sourceDiff, spec.packageId);
    return {
      commit,
      latest,
      remoteRef,
      clean: status === '',
      atRemoteRef: identity.atRemoteRef,
      sourcePatchVerified: true,
      stale: commit !== latest,
      sourceRoot: tgfxRoot,
    };
  }
  assert(identity.packageRole == null || identity.packageRole === 'official',
      `${spec.packageId}: official manifest has the wrong package role`);
  assert(allowStale || latest === identity.commit,
      `${spec.packageId}: package is stale; remote=${latest}, manifest=${identity.commit}`);
  if (identity.sourceMode === 'head-snapshot') {
    assert(identity.dirty === false,
        `${spec.packageId}: clean HEAD snapshot package is marked dirty`);
  } else {
    assert(commit === identity.commit,
        `${spec.packageId}: legacy manifest commit does not match source HEAD`);
    assert(status === '', `${spec.packageId}: legacy official build used a dirty source`);
    assert((status.length > 0) === identity.dirty,
        `${spec.packageId}: manifest dirty state does not match source status`);
  }
  for (const patch of identity.compatibilityPatches || []) {
    const patchPath = path.resolve(repoRoot, 'web', patch.path);
    assert(fs.existsSync(patchPath), `${spec.packageId}: compatibility patch is missing: ${patch.path}`);
    assert(sha256(fs.readFileSync(patchPath)) === patch.sha256,
        `${spec.packageId}: compatibility patch hash mismatch: ${patch.path}`);
    assert(['applied', 'already-integrated'].includes(patch.state),
        `${spec.packageId}: invalid compatibility patch state: ${patch.state}`);
  }
  if (identity.benchmark?.sourceDiff) {
    verifySourceDiff(packageRoot, identity.benchmark.sourceDiff, `${spec.packageId}: benchmark`);
  }
  if (identity.benchmark?.buildInputs) {
    assert(/^[0-9a-f]{64}$/.test(identity.benchmark.buildInputs.sha256 || ''),
        `${spec.packageId}: invalid benchmark build-input hash`);
    assert(Number.isInteger(identity.benchmark.buildInputs.fileCount) &&
        identity.benchmark.buildInputs.fileCount > 0,
    `${spec.packageId}: invalid benchmark build-input file count`);
  }
  return {
    commit,
    latest,
    remoteRef,
    clean: identity.sourceMode === 'head-snapshot' || status === '',
    sourceMode: identity.sourceMode || 'worktree',
    stale: identity.commit !== latest,
    sourceRoot: tgfxRoot,
  };
}

function verifyArtifact(spec, identity, name, expectedPath) {
  const artifact = identity.artifacts?.[name];
  assert(artifact && typeof artifact === 'object', `${spec.packageId}: missing ${name} artifact`);
  assert(artifact.path === expectedPath,
      `${spec.packageId}: ${name} path ${artifact.path} != ${expectedPath}`);
  return artifact;
}

async function verifyPackage(spec) {
  const packageRoot = path.join(packagesRoot, spec.packageId);
  const manifestPath = path.join(packageRoot, 'tgfx-build.json');
  const manifestDisk = fs.readFileSync(manifestPath);
  const manifestUrl = new URL(`/packages/${spec.packageId}/tgfx-build.json`, baseUrl).toString();
  const manifestHttp = await fetchBuffer(manifestUrl);
  assert(sha256(manifestDisk) === sha256(manifestHttp),
      `${spec.packageId}: served manifest differs from disk`);

  const identity = JSON.parse(manifestDisk.toString('utf8'));
  assert(identity.schemaVersion === 'tgfx-benchmark.build.v2',
      `${spec.packageId}: unsupported build manifest schema`);
  assert(identity.packageId === spec.packageId, `${spec.packageId}: package identity mismatch`);
  assert(identity.codegen === spec.codegen, `${spec.packageId}: codegen mismatch`);
  assert(identity.backend === spec.backend, `${spec.packageId}: backend mismatch`);
  assert((identity.packageRole || 'official') === spec.packageRole,
      `${spec.packageId}: package role mismatch`);
  assert(/^[0-9a-f]{40}$/.test(identity.commit || ''), `${spec.packageId}: invalid commit`);
  assert(identity.threadMode === 'st' && identity.buildType === 'release',
      `${spec.packageId}: expected release single-thread package`);
  if (identity.benchmark?.sourceDiff) {
    verifySourceDiff(packageRoot, identity.benchmark.sourceDiff, `${spec.packageId}: benchmark`);
  }

  const htmlDisk = fs.readFileSync(path.join(packageRoot, 'index.html'));
  const htmlUrl = new URL(`/packages/${spec.packageId}/index.html`, baseUrl).toString();
  const htmlHttp = await fetchBuffer(htmlUrl);
  assert(sha256(htmlDisk) === sha256(htmlHttp), `${spec.packageId}: served HTML differs from disk`);
  assert(htmlDisk.toString('utf8').includes(`./${spec.entry}`),
      `${spec.packageId}: HTML does not reference ${spec.entry}`);

  const files = {};
  for (const [name, expectedPath] of Object.entries({
    entry: spec.entry,
    glue: spec.glue,
    wasm: spec.wasm,
  })) {
    const artifact = verifyArtifact(spec, identity, name, expectedPath);
    const disk = fs.readFileSync(path.join(packageRoot, expectedPath));
    const url = new URL(`/packages/${spec.packageId}/${expectedPath}`, baseUrl).toString();
    const http = await fetchBuffer(url);
    const diskHash = sha256(disk);
    const httpHash = sha256(http);
    assert(diskHash === artifact.sha256, `${spec.packageId}: ${name} hash differs from manifest`);
    assert(disk.length === artifact.bytes, `${spec.packageId}: ${name} size differs from manifest`);
    assert(diskHash === httpHash, `${spec.packageId}: served ${name} differs from disk`);
    files[name] = {
      path: expectedPath,
      bytes: disk.length,
      sha256: diskHash,
      servedMatch: true,
      manifestMatch: true,
    };
  }

  const shaderBundle = identity.artifacts.shaderBundle;
  if (spec.codegen === 'aot') {
    assert(shaderBundle && shaderBundle.identityHash && shaderBundle.toolchainABI,
        `${spec.packageId}: AOT package is missing bundle identity`);
    assert(shaderBundle.shaderCount > 0 && shaderBundle.stageCount > 0,
        `${spec.packageId}: AOT package has an empty shader bundle`);
  } else {
    assert(shaderBundle === null, `${spec.packageId}: JIT package unexpectedly includes AOT bundle`);
  }

  return {
    codegen: spec.codegen,
    backend: spec.backend,
    commit: identity.commit,
    benchmarkBuild: identity.benchmark?.buildInputs || null,
    source: verifySourceIdentity(identity, spec, packageRoot),
    files,
    shaderBundle,
  };
}

async function main() {
  const packageIds = (process.env.BENCHMARK_PACKAGES ||
    'aot-webgl,aot-webgpu,jit-webgl,jit-webgpu')
      .split(',').map(value => value.trim()).filter(Boolean);
  const packageSpecs = packageIds.map(packageSpec);
  const result = {
    schemaVersion: 'tgfx-benchmark.package-preflight.v1',
    baseUrl,
    allowStale,
    packages: {},
    executionStatus: 'completed',
  };

  for (const spec of packageSpecs) {
    result.packages[spec.packageId] = await verifyPackage(spec);
  }
  for (const packageRole of ['official', 'candidate']) {
    for (const codegen of ['aot', 'jit']) {
      const commits = new Set(
          packageSpecs.filter(spec =>
            spec.packageRole === packageRole && spec.codegen === codegen)
              .map(spec => result.packages[spec.packageId].commit));
      assert(commits.size <= 1,
          `${packageRole}/${codegen}: backends were built from different commits`);
    }
    const benchmarkBuilds = packageSpecs
        .filter(spec => spec.packageRole === packageRole)
        .map(spec => ({
          packageId: spec.packageId,
          build: result.packages[spec.packageId].benchmarkBuild,
        }));
    if (benchmarkBuilds.length > 0) {
      for (const {packageId, build} of benchmarkBuilds) {
        assert(build && /^[0-9a-f]{64}$/.test(build.sha256 || '') &&
            Number.isInteger(build.fileCount) && build.fileCount > 0,
        `${packageId}: ${packageRole} packages require benchmark build identity`);
      }
      const benchmarkHashes =
          new Set(benchmarkBuilds.map(({build}) => `${build.sha256}:${build.fileCount}`));
      assert(benchmarkHashes.size === 1,
          `${packageRole}: packages were built from different benchmark sources`);
    }
  }
  const anyStale = Object.values(result.packages).some(entry => entry.source.stale);
  result.validityStatus = anyStale ? 'valid-with-stale' : 'valid';

  const outputPath = path.join(repoRoot, 'results', 'package-preflight.json');
  fs.mkdirSync(path.dirname(outputPath), {recursive: true});
  fs.writeFileSync(outputPath, `${JSON.stringify(result, null, 2)}\n`, 'utf8');
  console.log(JSON.stringify(result, null, 2));
}

main().catch(error => {
  console.error(error);
  process.exit(1);
});
