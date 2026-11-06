#!/usr/bin/env node
'use strict';

const assert = require('node:assert/strict');
const fs = require('node:fs');
const os = require('node:os');
const path = require('node:path');
const {
  candidatePackageId,
  normalizeTag,
  readPackageCatalog,
} = require('../../web/script/package-catalog');
const {
  hashBenchmarkBuildInputs,
  publishStagedPackage,
} = require('../../web/script/build-package');

function writeManifest(root, manifest) {
  const packageRoot = path.join(root, manifest.packageId);
  fs.mkdirSync(packageRoot, {recursive: true});
  fs.writeFileSync(path.join(packageRoot, 'tgfx-build.json'), `${JSON.stringify(manifest)}\n`);
}

function manifest(packageId, packageRole, codegen, backend, builtAt, tag = null) {
  return {
    schemaVersion: 'tgfx-benchmark.build.v2',
    packageId,
    packageRole,
    ...(tag ? {tag} : {}),
    codegen,
    backend,
    commit: 'a'.repeat(40),
    dirty: packageRole === 'candidate',
    sourceMode: packageRole === 'candidate' ? 'worktree' : 'head-snapshot',
    sourceName: 'tgfx-test',
    builtAt,
    benchmark: {commit: 'b'.repeat(40), dirty: true},
  };
}

function main() {
  assert.equal(normalizeTag('Opt Batch 01'), 'opt-batch-01');
  assert.equal(
      candidatePackageId('aot-webgpu', 'batch-01'),
      'candidate-batch-01-aot-webgpu');
  assert.equal(candidatePackageId('aot-webgpu'), 'candidate-aot-webgpu');
  assert.throws(() => normalizeTag('../escape'), /Invalid package tag/);
  const firstFingerprint = hashBenchmarkBuildInputs(path.resolve(__dirname, '../..'));
  const secondFingerprint = hashBenchmarkBuildInputs(path.resolve(__dirname, '../..'));
  assert.match(firstFingerprint.sha256, /^[0-9a-f]{64}$/);
  assert.ok(firstFingerprint.fileCount > 0);
  assert.deepEqual(secondFingerprint, firstFingerprint);

  const temporary = fs.mkdtempSync(path.join(os.tmpdir(), 'tgfx-catalog-test-'));
  try {
    writeManifest(temporary, manifest(
        'aot-webgl', 'official', 'aot', 'webgl', '2026-01-01T00:00:00.000Z'));
    writeManifest(temporary, manifest(
        'candidate-old-aot-webgl', 'candidate', 'aot', 'webgl',
        '2026-01-02T00:00:00.000Z', 'old'));
    writeManifest(temporary, manifest(
        'candidate-new-aot-webgl', 'candidate', 'aot', 'webgl',
        '2026-01-03T00:00:00.000Z', 'new'));
    fs.mkdirSync(path.join(temporary, 'not-a-package'));

    const catalog = readPackageCatalog(temporary);
    assert.deepEqual(catalog.packages.map(entry => entry.packageId), [
      'aot-webgl',
      'candidate-new-aot-webgl',
      'candidate-old-aot-webgl',
    ]);
    assert.equal(catalog.packages[1].tag, 'new');
    assert.equal(catalog.packages[1].sourceMode, 'worktree');

    const finalPackage = path.join(temporary, 'candidate-publish-aot-webgl');
    const stagingPackage = path.join(temporary, '.staging', 'publish');
    fs.mkdirSync(finalPackage, {recursive: true});
    fs.mkdirSync(stagingPackage, {recursive: true});
    fs.writeFileSync(path.join(finalPackage, 'version.txt'), 'old\n');
    fs.writeFileSync(path.join(stagingPackage, 'version.txt'), 'new\n');
    publishStagedPackage(stagingPackage, finalPackage);
    assert.equal(fs.readFileSync(path.join(finalPackage, 'version.txt'), 'utf8'), 'new\n');
    assert.equal(fs.existsSync(stagingPackage), false);
    console.log('package catalog: naming, discovery, metadata and ordering passed');
  } finally {
    fs.rmSync(temporary, {recursive: true, force: true});
  }
}

main();
