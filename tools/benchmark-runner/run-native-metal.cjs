#!/usr/bin/env node
'use strict';

const fs = require('fs');
const path = require('path');
const {spawnSync} = require('child_process');

function parseArgs(argv) {
  const args = {
    binary: path.resolve(__dirname, '../../build-bench-metal-13/Benchmark'),
    caseId: 'motionmark.multiply',
    count: 600,
    warmupMs: 500,
    measurementMs: 500,
    output: null,
  };
  for (let i = 2; i < argv.length; ++i) {
    const key = argv[i];
    const value = argv[++i];
    switch (key) {
      case '--binary': args.binary = path.resolve(value); break;
      case '--case': args.caseId = value; break;
      case '--count': args.count = Number(value); break;
      case '--warmup': args.warmupMs = Number(value); break;
      case '--measurement': args.measurementMs = Number(value); break;
      case '--output': args.output = value; break;
      default: throw new Error(`Unknown argument: ${key}`);
    }
  }
  return args;
}

function main() {
  const args = parseArgs(process.argv);
  const result = spawnSync(args.binary, [], {
    env: {
      ...process.env,
      TGFX_BENCH_JSON: '1',
      TGFX_BENCH_SCENE: args.caseId,
      TGFX_BENCH_COUNT: String(args.count),
      TGFX_BENCH_WARMUP_MS: String(args.warmupMs),
      TGFX_BENCH_MEASUREMENT_MS: String(args.measurementMs),
    },
    encoding: 'utf8',
    stdio: ['ignore', 'pipe', 'pipe'],
  });

  if (result.error) {
    throw result.error;
  }
  if (result.status !== 0) {
    throw new Error(`Benchmark exited with ${result.status}: ${result.stderr}`);
  }

  const jsonLine = result.stdout
      .split(/\r?\n/)
      .find(line => line.startsWith('BENCH_JSON '));
  if (jsonLine === undefined) {
    throw new Error('Benchmark did not emit BENCH_JSON');
  }

  const summary = JSON.parse(jsonLine.slice('BENCH_JSON '.length));
  const output = {
    schemaVersion: 'tgfx-benchmark.stage8-native.v1',
    backend: 'metal',
    caseId: args.caseId,
    count: args.count,
    warmupMs: args.warmupMs,
    measurementMs: args.measurementMs,
    summary,
  };
  const json = JSON.stringify(output, null, 2) + '\n';
  if (args.output !== null) {
    fs.mkdirSync(path.dirname(path.resolve(args.output)), {recursive: true});
    fs.writeFileSync(args.output, json, 'utf8');
  }
  console.log(json);
}

main();
