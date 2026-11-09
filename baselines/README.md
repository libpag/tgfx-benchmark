# Baselines

Two kinds of artifacts live here, and they have very different guarantees.

| File | What it is | Portable? |
| --- | --- | --- |
| `spec.json` | Snapshot of the scene catalogue: every scene id, category, description, module flags, parameter list and the four tier values | **Yes.** It is the contract between the benchmark and any comparison, and it changes only when scenes change. |
| `<host>-<backend>-<suite>.json` | A full measurement result recorded on one machine | **No.** Timings are specific to that machine, backend and thermal state. Useful as a reference and as a local regression baseline, meaningless as an absolute target for anyone else. |

## What is committed

- `spec.json` — regenerate with `Benchmark --dump-spec --out=baselines/spec.json` after changing any
  scene, tier or parameter, and commit the diff. A diff here means every previously recorded
  baseline has to be re-recorded, because `(scene, tier, param_hash)` is the comparison key.
- `macos-m4pro-metal-core.json` — reference run on an Apple M4 Pro, Metal backend, `core` suite,
  M and L tiers, two repeats, 200 measured frames. It was recorded while the machine reported a
  `fair` thermal state, so it is on the slow side of what that machine can do. Treat it as an
  example of the format and a smoke test for the tooling, not as a performance target.

## Recording your own baseline

Record on an idle machine, on the same power profile you will use later, and with nothing else
compiling in the background.

```bash
# full core suite (M and L), isolated per case, without the raw sample arrays
./mac/Release-metal/Benchmark --mode=measure --suite=core --isolate --no-samples \
    --out=baselines/$(hostname -s)-metal-core.json

# a narrower baseline for a subsystem you are actively changing
./mac/Release-metal/Benchmark --mode=measure --isolate --no-samples \
    --filter='pag.*,ui.*' --tier=M,L --repeat=3 \
    --out=baselines/$(hostname -s)-metal-pag-ui.json
```

Conventions worth keeping:

- one file per `host` + `backend` + suite, so a later comparison can never mix a Metal number with
  an OpenGL one;
- `--isolate` for anything you intend to keep, because a single process lets one scene's GPU cache
  inflate the next scene's memory readings;
- `--no-samples` keeps the file around a hundred kilobytes instead of tens of megabytes. Drop it
  only when you need the raw distributions for variance analysis;
- record the thermal state that ended up in `env.thermal_start` / `env.thermal_end`. If a run ends
  in `serious` or `critical`, throw it away; it is not comparable with anything.

## Comparing against a baseline

```bash
./mac/Release-metal/Benchmark --mode=measure --suite=core --isolate --no-samples --out=/tmp/today.json

python3 tools/bench_compare.py baselines/macos-m4pro-metal-core.json /tmp/today.json \
    --base-checkpoints baselines/ckpt --head-checkpoints /tmp/ckpt \
    --markdown /tmp/report.md --html /tmp/report.html
```

The comparison reports `incomparable` instead of a number when the scene, tier or parameter hash
does not match, so a stale baseline fails loudly rather than silently producing nonsense.

For anything where a single digit of difference matters, do not compare against a stored file at
all: build both revisions, keep two binaries, and use `tools/bench_run.py`, which interleaves them
per case and primes the driver cache on both sides. A stored baseline cannot control for the
machine having changed since it was recorded.
