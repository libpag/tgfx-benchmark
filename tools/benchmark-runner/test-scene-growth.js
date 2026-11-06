#!/usr/bin/env node
'use strict';

const assert = require('node:assert/strict');
const fs = require('node:fs');
const os = require('node:os');
const path = require('node:path');
const {execFileSync} = require('node:child_process');

const root = path.resolve(__dirname, '../..');
const source = fs.readFileSync(path.join(root, 'src/benchmark/MotionMarkBenches.cpp'), 'utf8');
const header = fs.readFileSync(path.join(root, 'src/benchmark/MotionMarkBenches.h'), 'utf8');
const shaderMixMotifCount = source.match(/constexpr size_t kShaderMixMotifCount = (\d+);/)?.[1];
assert.ok(shaderMixMotifCount);
const shaderMixHelpersStart = source.indexOf('constexpr std::array<float, kShaderMixMotifCount> ShaderMixScales');
const shaderMixHelpersEnd = source.indexOf('float Unit(', shaderMixHelpersStart);
assert.ok(shaderMixHelpersStart >= 0 && shaderMixHelpersEnd > shaderMixHelpersStart);
const shaderMixHelpers = source.slice(shaderMixHelpersStart, shaderMixHelpersEnd);
const types = [...header.matchAll(/struct MotionMark\w+ \{[\s\S]*?\n\};/g),
  ...header.matchAll(/struct MultiplyTile \{[\s\S]*?\n\};/g),
  ...header.matchAll(/struct ShaderMixTile \{[\s\S]*?\n\};/g)]
    .map(match => match[0]).join('\n');
const names = ['MultiplyBench', 'CanvasArcsBench', 'LeavesBench',
  'CanvasLinesBench', 'ImagesBench', 'SuitsBench', 'ShaderMixBench'];
const members = ['std::vector<MultiplyTile> tiles;',
  'std::vector<MotionMarkArc> arcs;',
  'std::vector<MotionMarkLeaf> particles; float canvasWidth=1000, canvasHeight=600;',
  'std::vector<MotionMarkLine> lines;',
  'std::vector<MotionMarkImageTile> tiles;',
  'std::vector<MotionMarkSuit> particles; float canvasWidth=1000, canvasHeight=600;',
  'std::vector<ShaderMixTile> tiles;'];
const vectors = ['tiles', 'arcs', 'particles', 'lines', 'tiles', 'particles', 'tiles'];
const signatures = [
  'ensureTileCapacity(size_t)', 'ensureArcCapacity(size_t)',
  'ensureParticleCapacity(size_t, float, float)', 'ensureLineCapacity(size_t)',
  'ensureTileCapacity(size_t)', 'ensureParticleCapacity(size_t, float, float)',
  'ensureTileCapacity(size_t)',
];
function method(name, prefix) {
  const start = source.indexOf(`void ${name}::${prefix}`);
  const end = source.indexOf('\n}\n', start) + 3;
  assert.ok(start >= 0 && end > start);
  return source.slice(start, end);
}
let methods = names.map((name, index) => method(name, signatures[index].split('(')[0])).join('\n');
methods += '\n' + method('LeavesBench', 'resetParticle') + '\n' +
  method('SuitsBench', 'resetParticle');
if (process.argv.includes('--negative-control')) {
  methods = methods.replace(/if \((\w+)\.capacity\(\) < newCount\) \{/g, 'if (true) {');
}
const harness = `
#include <algorithm>
#include <array>
#include <cmath>
#include <vector>
#include <iostream>
#include <stdexcept>
namespace tgfx {
struct Color { float red, green, blue, alpha; static Color Black(){return {};}
static Color Red(){return {};} };
struct Point { float x, y; };
}
constexpr float PI = 3.14159265f;
constexpr size_t kShaderMixMotifCount = ${shaderMixMotifCount};
constexpr std::array<int, 13> ImageNames = {};
${shaderMixHelpers}
float Unit(size_t index, size_t salt) {
  const auto value = std::sin(float(index)*127.1f + float(salt)*311.7f)*43758.5453f;
  return value - std::floor(value);
}
tgfx::Color Hsl(float,float,float){return {};}
${types}
${names.map((name, i) => `class ${name} { public: ${members[i]}
double animationFrame=0;
void ${signatures[i]};
${i === 2 ? 'void resetParticle(MotionMarkLeaf&,size_t,float,float) const;' : ''}
${i === 5 ? 'void resetParticle(MotionMarkSuit&,size_t,float,float) const;' : ''}
};`).join('\n')}
${methods}
int main() {
${names.map((name, i) => `{ ${name} scene;
for(size_t count=1;count<=512;++count) {
  scene.${signatures[i].split('(')[0]}(count${i === 2 || i === 5 ? ',1000,600' : ''});
  if(scene.${vectors[i]}.capacity()>count*2)
    throw std::runtime_error("${name}: capacity doubled without reaching capacity");
}
${i === 2 || i === 5 ? `for(const auto& item:scene.particles)
  if(item.x<0 || item.x>1000 || item.y<0 || item.y>600)
    throw std::runtime_error("${name}: newly added particle is outside canvas");` : ''}
}`).join('\n')}
std::cout<<"scene growth: seven incremental buffers and visible particle spawning passed\\n";
}`;
const temporary = fs.mkdtempSync(path.join(os.tmpdir(), 'tgfx-scene-growth-'));
try {
  const file = path.join(temporary, 'test.cpp');
  const binary = path.join(temporary, 'test');
  fs.writeFileSync(file, harness);
  execFileSync(process.env.CXX || 'clang++', ['-std=c++17', file, '-o', binary], {stdio: 'inherit'});
  if (process.argv.includes('--negative-control')) {
    assert.throws(() => execFileSync(binary, {stdio: 'pipe'}), error =>
      error.stderr.toString().includes('capacity doubled without reaching capacity'));
    console.log('negative control: unguarded allocation growth correctly fails');
  } else {
    execFileSync(binary, {stdio: 'inherit'});
  }
} finally {
  fs.rmSync(temporary, {recursive: true, force: true});
}
