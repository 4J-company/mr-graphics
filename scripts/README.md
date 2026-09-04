# Benchmark scripts

Scripts for running occlusion-culling benches (HiZ / MSOC) over scene sets: accuracy, timing, parameter sweeps, and OC-type comparisons.

Run from the **repo root**.

## What each file does

| File | Role |
|------|------|
| `batch_mr_graphics_bench.py` | Low-level runner: one scene → one binary run → one CSV row |
| `merge_results.py` | Join perf + accuracy CSVs, or project/trim columns |
| `test_msoc_options.py` | Grid over `tile_size × tiles_per_thread` for MSOC |
| `test_hiz_oc_bound.py` | Compare bounds: Box / Sphere / DynamicBest / Both |
| `bench_oc_types.py` | Compare OC types (HiZ vs MSOC variants vs no OC) |
| `tuning_msoc.sh` | Wrapper: four `test_msoc_options` runs (msoc / coarse / adaptive / adaptive+coarse) |

Orchestrators call `batch_mr_graphics_bench.py` and, for `full` / `--with-perf`, `merge_results.py`. Intermediate CSVs go under `bench_results/` at the repo root.

## Dependencies

- Built `build/Release/examples/mr-graphics-example` (and sibling `simple-bench` if scenes use `bench-instances-number`).
- Models: relative paths resolve from `--assets-base` (often `bin/models`).
- Headless: `xvfb` (`xvfb-run -a`). xvfb is **on** by default; disable with `--no-xvfb`.
- Scene and MSOC configs live under `bench_configs/` (see below).

## Scene configs (`bench_configs/*.json`)

JSON array of objects. Pass a full path or just a file name — scripts also look under `bench_configs/`.

Scene fields:

| Field | Required | Description |
|-------|----------|-------------|
| `model` | yes | Model path (absolute, or relative to `--assets-base`) |
| `name` | no | Scene id; defaults to `model` stem |
| `camera` | no | Camera string for the CLI |
| `proj` | no | Projection, e.g. `"[0.1, 1000]"` |
| `resolution` | no | Default `1920x1080` |
| `warmup` / `measure` | no | Override CLI `--warmup` / `--measure` |
| `bench-instances-number` | no | If set → run via `simple-bench` |

CSV row name: `{name}_{camera_hash8}`, where hash is SHA1 of `name|camera` (8 hex chars). Same model with different cameras → different `scene_name`.

Typical configs:

- `default_scenes.json` / `default_scenes_and_kittens.json` — accuracy (fewer frames).
- `*_for_perf.json` — same views, larger `warmup`/`measure` (e.g. 100 / 1000).

For a fair `full` run, prefer **separate** configs: `--acc-scene=...` and `--perf-scene=..._for_perf.json`.

### `bench_configs/msoc_params_config.json`

MSOC variants for `bench_oc_types.py`. Object: key = variant name, value:

```json
{
  "msoc": {
    "oc-type": "msoc",
    "msoc-with-hiz-coarse": false,
    "msoc-tile-size": "[4, 4]",
    "msoc-tiles-per-thread": 8
  }
}
```

The script always prepends `hiz` and appends `None` (OC off, `--disable-occlusion-culling`).

## Presets: accuracy vs perf

| Preset | Binary flag | Purpose |
|--------|-------------|---------|
| `accuracy` / `acc` | `--read-gbuf` | Ground truth for `occlusion_culling_accuracy` (heavier) |
| `perf` | no `--read-gbuf` | Cleaner GPU timing |
| `full` (orchestrators) | both runs + merge | One table with `perf_*` and `acc_*` columns |

Common binary flags (via batch): `--print-stat`, `--enable-culling-stat`, `--frames-number=warmup+measure`.

## How metrics are computed

1. Binary writes `stats.json` **next to the exe** (cwd = exe directory).
2. Batch deletes any old `stats.json`, runs the scene, reads the file.
3. First `warmup` frames are dropped; remaining frames use an arithmetic mean.
4. Bare `nan`/`inf` from C++ JSON are coerced to `null` so parsing does not fail.

Useful CSV columns: `mean_occlusion_culling_accuracy`, `mean_oc_culling_gpu_time_ms`, `mean_gpu_rendering_time_ms`, object counters, and (for MSOC) per-stage timings / `mean_msoc_tiles_number`.

---

## `batch_mr_graphics_bench.py` — base run

One scene config × one preset → one CSV.

```bash
./scripts/batch_mr_graphics_bench.py \
  --scenes default_scenes.json \
  --exe ./build/Release/examples/mr-graphics-example \
  --assets-base bin/models \
  --preset accuracy \
  --out mr_graphics_bench_acc.csv
```

Useful flags:

- `--oc-type` — `hiz` / `msoc` / `msoc-adaptive-tile-size` (and aliases).
- `--oc-bounds` — repeatable: **interleaved** scene×bound runs (fairer GPU clocks).
- `--msoc-tile-size='[8, 8]'`, `--msoc-tiles-per-thread=8`, `--msoc-with-hiz-coarse`.
- `--disable-occlusion-culling` — reference without late OC.
- `--resolution=1920x1080` — override JSON resolution.
- `--warmup` / `--measure` (default 50 / 150), `--timeout`, `--dry-run`, `--no-xvfb`.

Batch default `--assets-base` is `../assets`; orchestrators usually pass `bin/models` explicitly.

---

## `merge_results.py`

**Join** (perf + accuracy on `scene_name`; only scenes present in **both**):

```bash
./scripts/merge_results.py \
  --perf perf.csv --accuracy acc.csv --out merged.csv \
  --perf-fields mean_oc_culling_gpu_time_ms mean_gpu_rendering_time_ms \
  --accuracy-fields mean_occlusion_culling_accuracy mean_really_visible_objects_number
```

Output columns: `scene_name`, `perf_<field>`, `acc_<field>`. Do not list `scene_name` in the field lists.

**Project** (single CSV, select/prefix columns):

```bash
./scripts/merge_results.py \
  --input acc.csv --out trimmed.csv \
  --fields mean_occlusion_culling_accuracy --prefix acc_
```

---

## Orchestrators

### `test_msoc_options.py` — tile size / tiles-per-thread sweep

Grid: `tile_size ∈ {4,8,16}` × `tiles_per_thread ∈ {4,8,16}` (9 combos).

```bash
./scripts/test_msoc_options.py \
  --acc-scene=default_scenes_and_kittens.json \
  --perf-scene=default_scenes_and_kittens_for_perf.json \
  --preset=full \
  --assets-base=bin/models \
  --out bench_results/msoc_options_full.csv
```

- `--oc-type` (default `msoc`), `--msoc-with-hiz-coarse`.
- `--preset` — `acc` / `perf` / `full`.
- Final CSV: concat by scene with `msoc_tile_size`, `msoc_tiles_per_thread` columns.

### `tuning_msoc.sh` — full tuning of four modes

```bash
./scripts/tuning_msoc.sh
```

Optional env vars:

- `OUT_DIR` (default `tuning_results`)
- `ACC_SCENE`, `PERF_SCENE`, `ASSETS_BASE`

Writes four CSVs: `msoc`, `msoc_hiz_coarse`, `msoc_adaptive`, `msoc_adaptive_coarse`.

### `test_hiz_oc_bound.py` — OC bound type

Compares `Box`, `Sphere`, `DynamicBest`, `Both`:

- **DynamicBest** — single test using the bound with the smaller screen rect.
- **Both** — visible only if both box and sphere pass (old DynamicBest behavior).

```bash
./scripts/test_hiz_oc_bound.py \
  --with-perf \
  --acc-scene default_scenes_and_kittens.json \
  --perf-scene default_scenes_and_kittens_for_perf.json \
  --assets-base bin/models \
  --out bench_results/oc_bounds_full.csv
```

Without `--with-perf` — accuracy only. Bounds run interleaved in one batch pass (repeated `--oc-bounds`), then the CSV is split by bound.

Default scenes (if none given): `default_scenes_and_kittens.json` / `*_for_perf.json`.

### `bench_oc_types.py` — HiZ vs MSOC vs None

```bash
./scripts/bench_oc_types.py \
  --acc-scene=default_scenes_and_kittens.json \
  --perf-scene=default_scenes_and_kittens_for_perf.json \
  --preset=full \
  --out bench_results/oc_types_full.csv
```

- Variants from `--msoc-params-config` (default `bench_configs/msoc_params_config.json`) plus `hiz` and `None`.
- `--msoc-extend` — also emit MSOC stage timings and `mean_msoc_tiles_number`.
- `--preset` is required (`acc` / `perf` / `full`).
- Output grouped by scene: scene1×hiz, scene1×msoc, …

---

## Typical workflows

**Tune MSOC parameters (slow):**

```bash
./scripts/tuning_msoc.sh
# or one mode:
./scripts/test_msoc_options.py --acc-scene=... --perf-scene=... --preset=full \
  --oc-type=msoc --out tuning_results/msoc.csv
```

**Compare algorithms with fixed params:**

```bash
# edit bench_configs/msoc_params_config.json to the chosen tile/tpt first
./scripts/bench_oc_types.py --acc-scene=... --perf-scene=... --preset=full \
  --out bench_results/oc_types_full.csv
```

**Compare bounds (independent of MSOC tile grid):**

```bash
./scripts/test_hiz_oc_bound.py --with-perf --out bench_results/oc_bounds_full.csv
```

**Dry-run commands without GPU:**

```bash
./scripts/test_msoc_options.py default_scenes.json --preset=acc --out /tmp/x.csv --dry-run
```

---

## Gotchas

1. **Slow.** `full` × many scenes × 9 combos or × N OC types can take hours. Start with `hotel_only.json` / `--preset=acc`.
2. **Accuracy ≠ perf frame counts.** For timing comparisons use `*_for_perf.json` and `perf_*` columns, not the accuracy run.
3. **`stats.json` lives next to the exe.** Parallel batch jobs sharing one `build/Release/examples/` will race on that file — do not run two batches against the same binary at once.
4. **xvfb.** Needed without a display; with a monitor you can use `--no-xvfb`.
5. **Merge join** keeps only scenes present in **both** CSVs. Different scene sets for acc/perf → missing rows after merge.
6. **Intermediate files** under `bench_results/` (`raw_*`, `merged_*`) are not auto-cleaned — useful for debugging; do not commit them.
7. **Tile size** for fixed MSOC must be a power of two and (in the current impl) square.
8. Numbers in `msoc_params_config.json` and exe CLI defaults may differ — for a fair HiZ vs MSOC comparison, set the JSON to the tuned values.

## Minimal manual loop (no orchestrator)

```bash
./scripts/batch_mr_graphics_bench.py --scenes hotel_only.json --preset accuracy \
  --assets-base bin/models --oc-type msoc --msoc-tile-size='[8, 8]' \
  --out /tmp/acc.csv

./scripts/batch_mr_graphics_bench.py --scenes hotel_only_for_perf.json --preset perf \
  --assets-base bin/models --oc-type msoc --msoc-tile-size='[8, 8]' \
  --out /tmp/perf.csv

./scripts/merge_results.py --perf /tmp/perf.csv --accuracy /tmp/acc.csv --out /tmp/merged.csv \
  --perf-fields mean_oc_culling_gpu_time_ms mean_gpu_rendering_time_ms \
  --accuracy-fields mean_occlusion_culling_accuracy
```
