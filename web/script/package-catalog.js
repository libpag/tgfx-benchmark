#!/usr/bin/env node
'use strict';

const fs = require('fs');
const path = require('path');

const webDir = path.resolve(__dirname, '..');
const workspaceDir = path.resolve(webDir, '../..');
const defaultPackagesDir = path.join(workspaceDir, 'packages');
const packageIdPattern = /^(?:candidate(?:-[a-z0-9][a-z0-9._-]*)?-)?(?:aot|jit)-(?:webgl|webgpu)$/;
const tagPattern = /^[a-z0-9][a-z0-9._-]{0,47}$/;

function normalizeTag(value) {
  if (value == null || String(value).trim() === '') {
    return null;
  }
  const tag = String(value).trim().toLowerCase().replace(/\s+/g, '-');
  if (!tagPattern.test(tag)) {
    throw new Error(
        `Invalid package tag '${value}'. Use 1-48 lowercase letters, digits, '.', '_' or '-'.`);
  }
  return tag;
}

function candidatePackageId(basePackageId, tag = null) {
  if (!/^(?:aot|jit)-(?:webgl|webgpu)$/.test(basePackageId)) {
    throw new Error(`Invalid base package id '${basePackageId}'`);
  }
  const normalizedTag = normalizeTag(tag);
  return normalizedTag
    ? `candidate-${normalizedTag}-${basePackageId}`
    : `candidate-${basePackageId}`;
}

function packageSpecFromIdentity(identity) {
  if (!identity || !['aot', 'jit'].includes(identity.codegen) ||
      !['webgl', 'webgpu'].includes(identity.backend)) {
    throw new Error(`Package '${identity?.packageId || 'unknown'}' has an invalid codegen/backend`);
  }
  return {
    basePackageId: `${identity.codegen}-${identity.backend}`,
    codegen: identity.codegen,
    backend: identity.backend,
  };
}

function readPackageIdentity(packageRoot) {
  const manifestPath = path.join(packageRoot, 'tgfx-build.json');
  if (!fs.existsSync(manifestPath)) {
    return null;
  }
  const identity = JSON.parse(fs.readFileSync(manifestPath, 'utf8'));
  if (!identity || typeof identity !== 'object') {
    throw new Error(`${manifestPath} does not contain a package identity`);
  }
  if (!packageIdPattern.test(identity.packageId || '')) {
    throw new Error(`${manifestPath} has an invalid package id '${identity.packageId || ''}'`);
  }
  if (identity.packageId !== path.basename(packageRoot)) {
    throw new Error(`${manifestPath} package id does not match its directory`);
  }
  packageSpecFromIdentity(identity);
  return identity;
}

function catalogEntry(identity) {
  const spec = packageSpecFromIdentity(identity);
  const role = identity.packageRole === 'candidate' ? 'candidate' : 'official';
  return {
    packageId: identity.packageId,
    packageRole: role,
    tag: identity.tag || null,
    basePackageId: spec.basePackageId,
    codegen: spec.codegen,
    backend: spec.backend,
    commit: identity.commit,
    dirty: Boolean(identity.dirty),
    sourceMode: identity.sourceMode || 'worktree',
    sourceName: identity.sourceName || null,
    builtAt: identity.builtAt,
    benchmarkCommit: identity.benchmark?.commit || null,
    benchmarkDirty: Boolean(identity.benchmark?.dirty),
    hasShaderBundle: identity.codegen === 'aot',
  };
}

function compareEntries(left, right) {
  if (left.packageRole !== right.packageRole) {
    return left.packageRole === 'official' ? -1 : 1;
  }
  if (left.codegen !== right.codegen) {
    return left.codegen === 'aot' ? -1 : 1;
  }
  if (left.backend !== right.backend) {
    return left.backend === 'webgl' ? -1 : 1;
  }
  const leftTime = Date.parse(left.builtAt || 0) || 0;
  const rightTime = Date.parse(right.builtAt || 0) || 0;
  return rightTime - leftTime || left.packageId.localeCompare(right.packageId);
}

function readPackageCatalog(packagesRoot = defaultPackagesDir) {
  if (!fs.existsSync(packagesRoot)) {
    return {
      schemaVersion: 'tgfx-benchmark.package-catalog.v1',
      generatedAt: new Date().toISOString(),
      packages: [],
    };
  }
  const entries = [];
  const rejected = [];
  for (const dirent of fs.readdirSync(packagesRoot, {withFileTypes: true})) {
    if (!dirent.isDirectory() || dirent.name.startsWith('.')) {
      continue;
    }
    const packageRoot = path.join(packagesRoot, dirent.name);
    try {
      const identity = readPackageIdentity(packageRoot);
      if (identity != null) {
        entries.push(catalogEntry(identity));
      }
    } catch (error) {
      rejected.push({packageId: dirent.name, reason: error.message});
    }
  }
  entries.sort(compareEntries);
  return {
    schemaVersion: 'tgfx-benchmark.package-catalog.v1',
    generatedAt: new Date().toISOString(),
    packages: entries,
    ...(rejected.length > 0 ? {rejected} : {}),
  };
}

function writePackageCatalog(packagesRoot = defaultPackagesDir) {
  fs.mkdirSync(packagesRoot, {recursive: true});
  const catalog = readPackageCatalog(packagesRoot);
  const catalogPath = path.join(packagesRoot, 'index.json');
  const temporaryPath = `${catalogPath}.${process.pid}.tmp`;
  fs.writeFileSync(temporaryPath, `${JSON.stringify(catalog, null, 2)}\n`);
  fs.renameSync(temporaryPath, catalogPath);
  return catalog;
}

module.exports = {
  candidatePackageId,
  catalogEntry,
  defaultPackagesDir,
  normalizeTag,
  packageIdPattern,
  packageSpecFromIdentity,
  readPackageCatalog,
  readPackageIdentity,
  writePackageCatalog,
};

if (require.main === module) {
  const catalog = writePackageCatalog();
  console.log(JSON.stringify(catalog, null, 2));
}
