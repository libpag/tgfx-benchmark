#!/usr/bin/env node
'use strict';

const assert = require('node:assert/strict');
const crypto = require('node:crypto');
const fs = require('node:fs');
const os = require('node:os');
const path = require('node:path');
const {execFileSync} = require('node:child_process');
const {
  parseArgs,
  verifyCandidateSource,
} = require('../../web/script/build-package');

function git(root, ...args) {
  return execFileSync('git', ['-C', root, ...args], {encoding: 'utf8'}).trim();
}

function main() {
  const temporary = fs.mkdtempSync(path.join(os.tmpdir(), 'tgfx-candidate-test-'));
  try {
    git(temporary, 'init');
    git(temporary, 'config', 'user.email', 'benchmark-test@example.com');
    git(temporary, 'config', 'user.name', 'Benchmark Test');
    fs.writeFileSync(path.join(temporary, '.gitignore'), 'ignored.txt\n');
    fs.writeFileSync(path.join(temporary, 'tracked.txt'), 'before\n');
    git(temporary, 'add', '.gitignore', 'tracked.txt');
    git(temporary, 'commit', '-m', 'initial');
    git(temporary, 'update-ref', 'refs/remotes/origin/main', 'HEAD');

    fs.writeFileSync(path.join(temporary, 'tracked.txt'), 'after\n');
    fs.writeFileSync(path.join(temporary, 'new-file.txt'), 'new file\n');
    fs.writeFileSync(path.join(temporary, 'ignored.txt'), 'ignored\n');

    const source = verifyCandidateSource(
        {remoteRef: 'refs/remotes/origin/main'},
        temporary,
    );
    assert.equal(source.dirty, true);
    assert.equal(source.atRemoteRef, true);
    assert.equal(source.remoteCommit, source.commit);
    assert.equal(
        source.sourceDiff.sha256,
        crypto.createHash('sha256').update(source.patch).digest('hex'),
    );
    const patch = source.patch.toString('utf8');
    assert.match(patch, /-before\n\+after/);
    assert.match(patch, /new-file\.txt/);
    assert.doesNotMatch(patch, /ignored\.txt/);

    assert.deepEqual(
        parseArgs([
          process.execPath,
          'build-package.js',
          '--package=aot-webgpu',
          '--candidate',
        ]),
        {
          all: false,
          candidate: true,
          packageId: 'aot-webgpu',
          tgfxRoot: null,
          tag: null,
        },
    );
    assert.deepEqual(
        parseArgs([
          process.execPath,
          'build-package.js',
          '--package=aot-webgpu',
          '--candidate',
          '--tag=Opt Batch 01',
        ]).tag,
        'opt-batch-01',
    );
    assert.throws(
        () => parseArgs([
          process.execPath,
          'build-package.js',
          '--package=aot-webgpu',
          '--tag=invalid',
        ]),
        /requires --candidate/,
    );
    assert.throws(
        () => parseArgs([process.execPath, 'build-package.js', '--all', '--candidate']),
        /requires one --package/,
    );
    console.log('candidate package: tracked and untracked source patch capture passed');
  } finally {
    fs.rmSync(temporary, {recursive: true, force: true});
  }
}

main();
