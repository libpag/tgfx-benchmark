#!/usr/bin/env node
'use strict';

const fs = require('fs');
const path = require('path');

function parseArgs(argv) {
  const args = {input: null, output: null};
  for (let i = 2; i < argv.length; ++i) {
    if (argv[i] === '--input') {
      args.input = argv[++i];
    } else if (argv[i] === '--output') {
      args.output = argv[++i];
    }
  }
  return args;
}

function quantile(values, fraction) {
  if (values.length === 0) {
    return null;
  }
  const sorted = [...values].sort((a, b) => a - b);
  const index = Math.min(sorted.length - 1, Math.max(0, Math.ceil(sorted.length * fraction) - 1));
  return sorted[index];
}

function summarize(values) {
  if (values.length === 0) {
    return {count: 0, p50: null, p95: null, p99: null, max: null};
  }
  return {
    count: values.length,
    p50: quantile(values, 0.5),
    p95: quantile(values, 0.95),
    p99: quantile(values, 0.99),
    max: Math.max(...values),
  };
}

function formatMicroseconds(value) {
  if (value === null || value === undefined) {
    return 'n/a';
  }
  return `${(value / 1000).toFixed(3)} ms`;
}

function validateSummary(summary) {
  const errors = [];
  if (summary.schemaVersion !== 'tgfx-benchmark.summary.v1') {
    errors.push('schemaVersion must be tgfx-benchmark.summary.v1');
  }
  if (!Array.isArray(summary.samples)) {
    errors.push('samples must be an array');
    return errors;
  }
  if (summary.sampleCount !== summary.samples.length) {
    errors.push('sampleCount does not match samples.length');
  }
  for (const [index, sample] of summary.samples.entries()) {
    const required = [
      'frameId', 'recordingId', 'submittedFrameId', 'callbackBeginUs', 'callbackEndUs',
      'sceneDrawUs', 'flushUs', 'submitUs',
    ];
    for (const key of required) {
      if (typeof sample[key] !== 'number') {
        errors.push(`samples[${index}].${key} must be a number`);
      }
    }
  }
  return errors;
}

function generateReport(summary) {
  const intervals = summary.samples.slice(1).map((sample, index) =>
    sample.callbackBeginUs - summary.samples[index].callbackBeginUs);
  const callbackDurations = summary.samples.map(sample =>
    sample.callbackEndUs - sample.callbackBeginUs);
  const sceneDraw = summary.samples.map(sample => sample.sceneDrawUs);
  const flush = summary.samples.map(sample => sample.flushUs);
  const submit = summary.samples.map(sample => sample.submitUs);

  const intervalSummary = summarize(intervals);
  const callbackSummary = summarize(callbackDurations);
  const sceneDrawSummary = summarize(sceneDraw);
  const flushSummary = summarize(flush);
  const submitSummary = summarize(submit);

  const completeness = summary.sampleCount < 30 ? 'insufficient-samples' : 'complete';
  const validity = summary.overflowed ? 'invalid-overflow' : 'valid';

  return [
    '# TGFX Benchmark Stage 1 Report',
    '',
    `- Schema: ${summary.schemaVersion}`,
    `- Execution: ${summary.executionStatus}`,
    `- Completeness: ${completeness}`,
    `- Validity: ${validity}`,
    `- Sample count: ${summary.sampleCount}`,
    `- Overflowed: ${summary.overflowed}`,
    `- First submit: ${formatMicroseconds(summary.firstSubmitUs)}`,
    `- Drain after measurement: ${formatMicroseconds(summary.drainAfterMeasurementUs)}`,
    '',
    '## Frame intervals',
    '',
    `- p50: ${formatMicroseconds(intervalSummary.p50)}`,
    `- p95: ${formatMicroseconds(intervalSummary.p95)}`,
    `- p99: ${formatMicroseconds(intervalSummary.p99)}`,
    `- max: ${formatMicroseconds(intervalSummary.max)}`,
    '',
    '## CPU phases',
    '',
    '| Phase | Count | p50 | p95 | p99 | Max |',
    '| --- | ---: | ---: | ---: | ---: | ---: |',
    `| Callback | ${callbackSummary.count} | ${formatMicroseconds(callbackSummary.p50)} | ${formatMicroseconds(callbackSummary.p95)} | ${formatMicroseconds(callbackSummary.p99)} | ${formatMicroseconds(callbackSummary.max)} |`,
    `| Scene draw | ${sceneDrawSummary.count} | ${formatMicroseconds(sceneDrawSummary.p50)} | ${formatMicroseconds(sceneDrawSummary.p95)} | ${formatMicroseconds(sceneDrawSummary.p99)} | ${formatMicroseconds(sceneDrawSummary.max)} |`,
    `| Flush | ${flushSummary.count} | ${formatMicroseconds(flushSummary.p50)} | ${formatMicroseconds(flushSummary.p95)} | ${formatMicroseconds(flushSummary.p99)} | ${formatMicroseconds(flushSummary.max)} |`,
    `| Submit | ${submitSummary.count} | ${formatMicroseconds(submitSummary.p50)} | ${formatMicroseconds(submitSummary.p95)} | ${formatMicroseconds(submitSummary.p99)} | ${formatMicroseconds(submitSummary.max)} |`,
    '',
  ].join('\n');
}

function main() {
  const args = parseArgs(process.argv);
  if (!args.input || !args.output) {
    console.error('Usage: node generate-report.js --input summary.json --output report.md');
    process.exitCode = 1;
    return;
  }
  const summary = JSON.parse(fs.readFileSync(path.resolve(args.input), 'utf8'));
  const errors = validateSummary(summary);
  if (errors.length !== 0) {
    console.error('Invalid summary:');
    for (const error of errors) {
      console.error(`- ${error}`);
    }
    process.exitCode = 1;
    return;
  }
  const report = generateReport(summary);
  fs.writeFileSync(path.resolve(args.output), report, 'utf8');
  console.log(`report written: ${args.output}`);
}

main();
