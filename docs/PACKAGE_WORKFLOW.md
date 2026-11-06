# Package Workflow

Date: 2026-10-28.

## Source layers

The benchmark separates three source layers:

1. **Benchmark platform**

   The benchmark UI, scenes, runners, reports, and documentation live in
   `tgfx-benchmark`. Changes here affect every package built from the current
   checkout.

2. **Official compatibility layer**

   Patches in `web/patches/compatibility/` are curated against the configured
   TGFX remote ref. They contain build or runtime compatibility fixes needed by
   the benchmark. Official packages apply these patches to a clean configured
   Git ref snapshot and never include arbitrary dirty-worktree changes.

3. **Candidate optimization layer**

   A candidate package captures the complete tracked and untracked source diff
   from a selected TGFX checkout. It is intended for active optimization,
   profiling, and A/B experiments.

The current TGFX working tree mixes all three kinds of local work: AOT build
compatibility, performance instrumentation, gated optimization experiments,
and standalone probe drivers. Only reviewed compatibility patches belong in the
official layer. The remaining work stays in candidate packages until it has
passed correctness and performance review.

## Official packages

Build the four stable packages with:

```sh
cd tgfx-benchmark/web
npm run build:packages
```

An official build:

- resolves the configured TGFX remote ref, independently of worktree `HEAD`;
- exports that exact ref into a temporary clean snapshot;
- applies every patch declared by
  `web/patches/compatibility/index.json`;
- builds the package in a staging directory;
- publishes the finished directory atomically;
- records the TGFX commit, compatibility patch hashes, benchmark source patch,
  and artifact hashes in `tgfx-build.json`.

Uncommitted optimization experiments and a stale worktree `HEAD` are ignored by
an official build. They do not need to be removed or fast-forwarded before
rebuilding an official package.

## Candidate packages

The default candidate command replaces one stable scratch slot atomically:

```sh
cd tgfx-benchmark/web
node script/build-package.js \
  --package=aot-webgpu \
  --tgfx-root=/path/to/tgfx \
  --candidate
```

Output:

```text
packages/candidate-aot-webgpu/
```

Use a tag when two candidate builds must remain available for comparison:

```sh
node script/build-package.js \
  --package=aot-webgpu \
  --tgfx-root=/path/to/tgfx \
  --candidate \
  --tag=batch-01
```

Output:

```text
packages/candidate-batch-01-aot-webgpu/
```

`--label` is an alias for `--tag`. Tags are normalized to lowercase and are
limited to letters, digits, `.`, `_`, and `-`. Builds acquire a workspace-wide
lock because the web compiler emits shared generated files.

Candidate manifests record:

- the package role and optional tag;
- the original TGFX commit and remote-ref position;
- the complete `candidate-source.patch` and its SHA-256;
- compatibility patch IDs, states, and hashes;
- the benchmark source patch and a separate hash of files that actually enter
  the benchmark build;
- generated Wasm, glue, entry, and shader-bundle hashes.

Candidate CMake/Ninja build trees are retained under `packages/.cache/build/`
by `codegen/backend`, separate from published package directories. Repeated
candidate builds therefore recompile changed TGFX sources instead of rebuilding
Tint and all unchanged objects. Official packages continue to use disposable
clean staging trees. The candidate cache is not part of a package and can be
removed with:

```sh
npm run clean:build-cache
```

## Package catalog and UI

The package catalog is generated from package manifests:

```sh
npm run catalog:packages
```

Outputs:

```text
packages/index.json
```

The development server also exposes a fresh `/api/packages` response. The UI
uses that endpoint, groups official and candidate packages, and can refresh the
list without restarting the server. Selecting a package always uses its full
package ID from the manifest; package directory names are not parsed for codegen
or backend identity.

## Comparison rules

- Official versus official: compare only packages built from the same benchmark
  source patch and compatibility patch set.
- Candidate versus candidate: use the same base commit and compatibility patch
  set. Change one optimization variable per pair and retain both tagged
  packages until reports are archived.
- Official versus candidate: useful for product-level direction, but not for
  attributing the difference to one optimization.
- Every report must retain `tgfxPackage`, TGFX commit, source patch hash, and
  artifact hashes.
