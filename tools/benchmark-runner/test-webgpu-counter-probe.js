#!/usr/bin/env node
'use strict';

const assert = require('node:assert/strict');
const {installWebGPUCounterProbe} = require('./webgpu-counter-runner');

class GPUCommandEncoder {
  beginRenderPass() {
    return new GPURenderPassEncoder();
  }

  copyBufferToBuffer(source, sourceOffset, destination, destinationOffset, size) {
    void sourceOffset;
    void destinationOffset;
    void size;
  }

  clearBuffer(buffer, offset, size) {
    void buffer;
    void offset;
    void size;
  }

  finish() {
    return {};
  }
}

class GPURenderPassEncoder {
  draw(vertices, instances) {
    void vertices;
    void instances;
  }

  drawIndexed(indices, instances) {
    void indices;
    void instances;
  }

  setPipeline(pipeline) {
    void pipeline;
  }

  setBindGroup(index, bindGroup) {
    void index;
    void bindGroup;
  }

  setVertexBuffer(slot, buffer) {
    void slot;
    void buffer;
  }

  setIndexBuffer(buffer) {
    void buffer;
  }
}

class GPUComputePassEncoder {}

class GPUDevice {
  createBuffer(descriptor) {
    const buffer = new GPUBuffer();
    buffer.size = descriptor.size;
    return buffer;
  }

  createTexture(descriptor) {
    const texture = new GPUTexture();
    Object.assign(texture, descriptor);
    return texture;
  }
}

class GPUBuffer {
  destroy() {}
}

class GPUTexture {
  destroy() {}
}

class GPUQueue {
  writeBuffer() {}
  writeTexture() {}
  copyExternalImageToTexture() {}
  submit() {}
}

class GPUCanvasContext {
  getCurrentTexture() {
    return new GPUTexture();
  }
}

globalThis.GPUCommandEncoder = GPUCommandEncoder;
globalThis.GPURenderPassEncoder = GPURenderPassEncoder;
globalThis.GPUComputePassEncoder = GPUComputePassEncoder;
globalThis.GPUDevice = GPUDevice;
globalThis.GPUBuffer = GPUBuffer;
globalThis.GPUTexture = GPUTexture;
globalThis.GPUQueue = GPUQueue;
globalThis.GPUCanvasContext = GPUCanvasContext;

installWebGPUCounterProbe();

const device = new GPUDevice();
const queue = new GPUQueue();
const encoder = new GPUCommandEncoder();
const buffer = device.createBuffer({size: 64});
const destinationBuffer = device.createBuffer({size: 32});
const texture = device.createTexture({
  width: 4,
  height: 4,
  format: 'rgba8unorm',
  mipLevelCount: 1,
});
const pipelineA = {};
const pipelineB = {};
const pass = encoder.beginRenderPass();
pass.setPipeline(pipelineA);
pass.setBindGroup(0, {});
pass.draw(3, 2);
pass.setPipeline(pipelineB);
pass.setBindGroup(0, {});
pass.drawIndexed(6, 1);
pass.setVertexBuffer(0, buffer);
pass.setIndexBuffer(buffer);
queue.writeBuffer(buffer, 0, new Uint8Array(16));
queue.writeTexture(
    {texture, origin: {x: 0, y: 0}},
    new Uint8Array(8),
    {bytesPerRow: 8, rowsPerImage: 1},
    {width: 2, height: 1, depthOrArrayLayers: 1},
);
queue.copyExternalImageToTexture(
    {},
    {texture},
    {width: 1, height: 1, depthOrArrayLayers: 1},
);
encoder.copyBufferToBuffer(buffer, 0, destinationBuffer, 0, 16);
encoder.clearBuffer(buffer);
encoder.finish();
encoder.finish();
queue.submit([]);
(new GPUCanvasContext()).getCurrentTexture();
buffer.destroy();

const snapshot = globalThis.__tgfxGPUCounterProbe.snapshot();
assert.equal(snapshot.records.length, 1);
assert.equal(snapshot.totals.commandBuffers, 1);
assert.equal(snapshot.totals.renderPasses, 1);
assert.equal(snapshot.totals.drawCalls, 2);
assert.equal(snapshot.totals.indexedDrawCalls, 1);
assert.equal(snapshot.totals.instancesDrawn, 3);
assert.equal(snapshot.totals.verticesDrawn, 6);
assert.equal(snapshot.totals.indicesDrawn, 6);
assert.equal(snapshot.totals.pipelineBinds, 2);
assert.equal(snapshot.totals.uniquePipelines, 2);
assert.equal(snapshot.totals.bindGroupBinds, 2);
assert.equal(snapshot.totals.uploadOperations, 3);
assert.equal(snapshot.totals.uploadedBytes, 28);
assert.equal(snapshot.totals.uploadedPayloadBytes, 12);
assert.equal(snapshot.totals.copyOperations, 2);
assert.equal(snapshot.totals.copiedBytes, 80);
assert.equal(snapshot.totals.commandBufferSubmits, 1);
assert.equal(snapshot.totals.logicalResourceBytesCreated, 160);
assert.equal(snapshot.totals.logicalResourceBytesReleased, 64);
assert.equal(snapshot.liveResourceBytes, 96);
assert.equal(snapshot.peakLiveResourceBytes, 160);
assert.equal(snapshot.unknownTextureSizeCount, 0);
assert.ok(snapshot.records[0].at > 0);
assert.equal(snapshot.records[0].counters.drawCalls, 2);

console.log('webgpu counters: command-buffer attribution and resource accounting passed');
