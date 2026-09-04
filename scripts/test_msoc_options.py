#!/usr/bin/env python3
"""
Benchmark MSOC occlusion culling across tile-size and tiles-per-thread options.

Runs batch_mr_graphics_bench.py per MSOC parameter combo, optionally merges perf+accuracy
(full preset), then concatenates results grouped by scene.

Intermediate CSVs go to bench_results/ in the repo root.

  ./scripts/test_msoc_options.py default_scenes.json --out bench_results/msoc_options_full.csv --assets-base=bin/models
  ./scripts/test_msoc_options.py --acc-scene=default_scenes_and_kittens.json --preset=acc --out bench_results/msoc_options_acc.csv
  ./scripts/test_msoc_options.py --perf-scene=default_scenes_and_kittens_for_perf.json --preset=perf --out bench_results/msoc_options_perf.csv
  ./scripts/test_msoc_options.py --acc-scene=default_scenes_and_kittens.json --perf-scene=default_scenes_and_kittens_for_perf.json --preset=full --out bench_results/msoc_options_full.csv
"""

from __future__ import annotations

import argparse
import csv
import subprocess
import sys
from pathlib import Path

SCRIPTS = Path(__file__).resolve().parent
REPO_ROOT = SCRIPTS.parent
sys.path.insert(0, str(SCRIPTS))

from batch_mr_graphics_bench import resolve_bench_config  # noqa: E402

BATCH_SCRIPT = SCRIPTS / "batch_mr_graphics_bench.py"
MERGE_SCRIPT = SCRIPTS / "merge_results.py"
BENCH_RESULTS = REPO_ROOT / "bench_results"

TILE_SIZES = (4, 8, 16)
TILES_PER_THREAD = (4, 8, 16)
PERF_FIELDS = ("mean_oc_culling_gpu_time_ms", "mean_gpu_time_ms")
ACC_FIELDS = (
    "mean_total_objects_number",
    "mean_objects_inside_frustum",
    "mean_visible_objects_number",
    "mean_really_visible_objects_number",
    "mean_occlusion_culling_accuracy",
)

_PRESET_ALIASES = {
    "acc": "acc",
    "accuracy": "acc",
    "perf": "perf",
    "performance": "perf",
    "full": "full",
}


def parse_preset(value: str) -> str:
    key = value.lower()
    if key not in _PRESET_ALIASES:
        raise argparse.ArgumentTypeError(
            f"invalid preset {value!r} (expected acc, accuracy, perf, performance, or full)"
        )
    return _PRESET_ALIASES[key]


def combo_key(tile_size: int, tiles_per_thread: int) -> str:
    return f"ts{tile_size}_tpt{tiles_per_thread}"


def tile_size_cli(tile_size: int) -> str:
    return f"[{tile_size}, {tile_size}]"


def tile_size_label(tile_size: int) -> str:
    return f"{tile_size}x{tile_size}"


def run_cmd(argv: list[str], label: str) -> None:
    print(f"  $ {' '.join(argv)}", file=sys.stderr)
    proc = subprocess.run(argv, check=False)
    if proc.returncode != 0:
        print(f"Error: {label} failed (exit {proc.returncode})", file=sys.stderr)
        raise SystemExit(proc.returncode)


def batch_common_args(args: argparse.Namespace, scenes_path: Path) -> list[str]:
    argv = [
        sys.executable,
        str(BATCH_SCRIPT),
        f"--scenes={scenes_path}",
        f"--exe={args.exe}",
        f"--assets-base={args.assets_base}",
        f"--warmup={args.warmup}",
        f"--measure={args.measure}",
    ]
    if args.timeout is not None:
        argv.append(f"--timeout={args.timeout}")
    if not args.xvfb:
        argv.append("--no-xvfb")
    if args.oc_bounds is not None:
        argv.append(f"--oc-bounds={args.oc_bounds}")
    return argv


def run_batch(
    args: argparse.Namespace,
    scenes_path: Path,
    preset: str,
    tile_size: int,
    tiles_per_thread: int,
    out: Path,
) -> None:
    key = combo_key(tile_size, tiles_per_thread)
    argv = batch_common_args(args, scenes_path)
    argv.extend(
        [
            f"--preset={preset}",
            f"--oc-type={args.oc_type}",
            f"--msoc-tile-size={tile_size_cli(tile_size)}",
            f"--msoc-tiles-per-thread={tiles_per_thread}",
            f"--out={out}",
        ]
    )
    if args.msoc_with_hiz_coarse:
        argv.append("--msoc-with-hiz-coarse")
    if args.dry_run:
        argv.append("--dry-run")
    run_cmd(argv, f"batch preset={preset} {key}")


def project_fields(src: Path, fields: tuple[str, ...], prefix: str, out: Path) -> None:
    argv = [
        sys.executable,
        str(MERGE_SCRIPT),
        f"--input={src}",
        f"--fields={' '.join(fields)}",
        f"--prefix={prefix}",
        f"--out={out}",
    ]
    run_cmd(argv, f"project {src.name}")


def merge_perf_acc(perf: Path, acc: Path, out: Path) -> None:
    argv = [
        sys.executable,
        str(MERGE_SCRIPT),
        f"--perf={perf}",
        f"--accuracy={acc}",
        f"--out={out}",
        f"--perf-fields={' '.join(PERF_FIELDS)}",
        f"--accuracy-fields={' '.join(ACC_FIELDS)}",
    ]
    run_cmd(argv, f"merge {out.name}")


def concat_with_msoc_options(
    sources: list[tuple[int, int, Path]],
    out: Path,
    preset_mode: str,
) -> None:
    if preset_mode == "full":
        metric_cols = [f"perf_{c}" for c in PERF_FIELDS] + [f"acc_{c}" for c in ACC_FIELDS]
    elif preset_mode == "acc":
        metric_cols = [f"acc_{c}" for c in ACC_FIELDS]
    else:
        metric_cols = [f"perf_{c}" for c in PERF_FIELDS]
    fieldnames = ["msoc_tile_size", "msoc_tiles_per_thread", "scene_name"] + metric_cols

    by_combo: dict[tuple[int, int], dict[str, dict[str, str]]] = {}
    scene_order: list[str] = []

    for tile_size, tiles_per_thread, path in sources:
        combo = (tile_size, tiles_per_thread)
        with path.open(newline="", encoding="utf-8") as fin:
            reader = csv.DictReader(fin)
            if reader.fieldnames is None:
                raise ValueError(f"No header in {path}")
            rows: dict[str, dict[str, str]] = {}
            for row in reader:
                sn = row.get("scene_name", "")
                if not sn:
                    raise ValueError(f"Row missing scene_name in {path}")
                if sn in rows:
                    raise ValueError(f"Duplicate scene_name {sn!r} in {path}")
                rows[sn] = row
                if combo == (sources[0][0], sources[0][1]):
                    scene_order.append(sn)
            by_combo[combo] = rows

    out.parent.mkdir(parents=True, exist_ok=True)
    with out.open("w", newline="", encoding="utf-8") as fout:
        w = csv.DictWriter(fout, fieldnames=fieldnames, extrasaction="ignore")
        w.writeheader()
        total = 0
        for sn in scene_order:
            for tile_size in TILE_SIZES:
                for tiles_per_thread in TILES_PER_THREAD:
                    row = by_combo[(tile_size, tiles_per_thread)].get(sn)
                    if row is None:
                        raise ValueError(
                            f"scene_name {sn!r} missing for "
                            f"tile_size={tile_size_label(tile_size)} tiles_per_thread={tiles_per_thread}"
                        )
                    out_row: dict[str, str] = {
                        "msoc_tile_size": tile_size_label(tile_size),
                        "msoc_tiles_per_thread": str(tiles_per_thread),
                        "scene_name": sn,
                    }
                    for c in metric_cols:
                        out_row[c] = row.get(c, "")
                    w.writerow(out_row)
                    total += 1

    print(f"Wrote {out}  rows={total}", file=sys.stderr)


def process_combo(
    args: argparse.Namespace,
    acc_scenes_path: Path,
    perf_scenes_path: Path,
    tile_size: int,
    tiles_per_thread: int,
    preset_mode: str,
) -> Path | None:
    key = combo_key(tile_size, tiles_per_thread)
    if args.dry_run:
        if preset_mode == "full":
            run_batch(
                args,
                acc_scenes_path,
                "accuracy",
                tile_size,
                tiles_per_thread,
                BENCH_RESULTS / f"raw_acc_{key}.csv",
            )
            run_batch(
                args,
                perf_scenes_path,
                "perf",
                tile_size,
                tiles_per_thread,
                BENCH_RESULTS / f"raw_perf_{key}.csv",
            )
        else:
            batch_preset = "accuracy" if preset_mode == "acc" else "perf"
            scenes_path = acc_scenes_path if preset_mode == "acc" else perf_scenes_path
            run_batch(
                args,
                scenes_path,
                batch_preset,
                tile_size,
                tiles_per_thread,
                BENCH_RESULTS / f"raw_{preset_mode}_{key}.csv",
            )
        return None

    if preset_mode == "full":
        raw_acc = BENCH_RESULTS / f"raw_acc_{key}.csv"
        raw_perf = BENCH_RESULTS / f"raw_perf_{key}.csv"
        merged = BENCH_RESULTS / f"merged_{key}.csv"

        print(f"[{key}] accuracy bench ({acc_scenes_path.name})", file=sys.stderr)
        run_batch(args, acc_scenes_path, "accuracy", tile_size, tiles_per_thread, raw_acc)
        print(f"[{key}] perf bench ({perf_scenes_path.name})", file=sys.stderr)
        run_batch(args, perf_scenes_path, "perf", tile_size, tiles_per_thread, raw_perf)
        print(f"[{key}] merge perf + accuracy", file=sys.stderr)
        merge_perf_acc(raw_perf, raw_acc, merged)
        return merged

    batch_preset = "accuracy" if preset_mode == "acc" else "perf"
    prefix = "acc_" if preset_mode == "acc" else "perf_"
    fields = ACC_FIELDS if preset_mode == "acc" else PERF_FIELDS
    scenes_path = acc_scenes_path if preset_mode == "acc" else perf_scenes_path
    raw = BENCH_RESULTS / f"raw_{preset_mode}_{key}.csv"
    trimmed = BENCH_RESULTS / f"trimmed_{preset_mode}_{key}.csv"

    print(f"[{key}] {batch_preset} bench ({scenes_path.name})", file=sys.stderr)
    run_batch(args, scenes_path, batch_preset, tile_size, tiles_per_thread, raw)
    print(f"[{key}] project {preset_mode} fields", file=sys.stderr)
    project_fields(raw, fields, prefix, trimmed)
    return trimmed


def resolve_scene_path(path: Path) -> Path:
    return resolve_bench_config(path)


def main() -> int:
    p = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    p.add_argument(
        "config",
        type=Path,
        nargs="?",
        default=None,
        help="Default bench config when --acc-scene / --perf-scene omitted",
    )
    p.add_argument(
        "--acc-scene",
        type=Path,
        default=None,
        help="Bench config for accuracy preset (path or name under bench_configs/)",
    )
    p.add_argument(
        "--perf-scene",
        type=Path,
        default=None,
        help="Bench config for perf preset (path or name under bench_configs/)",
    )
    p.add_argument("--preset", type=parse_preset, default="full", help="acc, perf, or full (default: full)")
    p.add_argument("--out", type=Path, required=True, help="Final merged CSV path")
    p.add_argument(
        "--oc-bounds",
        choices=("Box", "Sphere", "DynamicBest", "Both"),
        default=None,
        help="Occlusion culling bounds type (default: exe default)",
    )
    p.add_argument(
        "--oc-type",
        default="msoc",
        help="Occlusion culling type passed to mr-graphics-example (default: msoc)",
    )
    p.add_argument(
        "--msoc-with-hiz-coarse",
        action="store_true",
        default=False,
        help="Enable coarse HiZ prep pass before MSOC tile tests",
    )
    p.add_argument(
        "--exe",
        type=Path,
        default=REPO_ROOT / "build/Release/examples/mr-graphics-example",
        help="mr-graphics-example binary",
    )
    p.add_argument(
        "--assets-base",
        type=Path,
        default=Path("bin/models"),
        help="Base dir for relative model paths",
    )
    p.add_argument("--warmup", type=int, default=50)
    p.add_argument("--measure", type=int, default=150)
    p.add_argument("--timeout", type=float, default=None)
    p.add_argument("--xvfb", action=argparse.BooleanOptionalAction, default=True)
    p.add_argument("--dry-run", action="store_true", help="Print batch commands without running benches")
    args = p.parse_args()

    try:
        default_config = resolve_scene_path(args.config) if args.config is not None else None
        acc_scenes_path = resolve_scene_path(args.acc_scene) if args.acc_scene is not None else default_config
        perf_scenes_path = resolve_scene_path(args.perf_scene) if args.perf_scene is not None else default_config
    except FileNotFoundError as e:
        print(f"Error: {e}", file=sys.stderr)
        return 1

    if args.preset in ("acc", "full") and acc_scenes_path is None:
        print("Error: accuracy bench requires config positional arg or --acc-scene", file=sys.stderr)
        return 1
    if args.preset in ("perf", "full") and perf_scenes_path is None:
        print("Error: perf bench requires config positional arg or --perf-scene", file=sys.stderr)
        return 1

    BENCH_RESULTS.mkdir(parents=True, exist_ok=True)

    n_combos = len(TILE_SIZES) * len(TILES_PER_THREAD)
    scene_desc = []
    if args.preset in ("acc", "full"):
        scene_desc.append(f"acc={acc_scenes_path.name}")
    if args.preset in ("perf", "full"):
        scene_desc.append(f"perf={perf_scenes_path.name}")
    coarse_desc = ", coarse-hiz" if args.msoc_with_hiz_coarse else ""
    print(
        f"msoc-options bench: {n_combos} combos "
        f"(tile-size {', '.join(tile_size_label(ts) for ts in TILE_SIZES)}; "
        f"tiles-per-thread {', '.join(str(t) for t in TILES_PER_THREAD)}), "
        f"oc-type={args.oc_type}{coarse_desc}, preset={args.preset}, "
        f"scenes: {', '.join(scene_desc)}",
        file=sys.stderr,
    )

    per_combo: list[tuple[int, int, Path]] = []
    for tile_size in TILE_SIZES:
        for tiles_per_thread in TILES_PER_THREAD:
            result_path = process_combo(
                args,
                acc_scenes_path,
                perf_scenes_path,
                tile_size,
                tiles_per_thread,
                args.preset,
            )
            if result_path is not None:
                per_combo.append((tile_size, tiles_per_thread, result_path))

    if args.dry_run:
        print("Dry run complete (skipped merge/concat)", file=sys.stderr)
        return 0

    print("Concatenating results", file=sys.stderr)
    concat_with_msoc_options(per_combo, args.out.resolve(), args.preset)

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
