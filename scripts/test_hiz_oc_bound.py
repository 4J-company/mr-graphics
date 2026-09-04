#!/usr/bin/env python3
"""
Benchmark occlusion culling bounds (Box, Sphere, DynamicBest, Both) across scenes.

Runs batch_mr_graphics_bench.py with interleaved scene×bound order (fairer GPU clocks),
then merges accuracy (and optional perf) CSVs grouped by scene.

  DynamicBest — smaller screen-rect of box/sphere (one HiZ test)
  Both        — visible only if both box and sphere tests pass (old DynamicBest)

  ./scripts/test_hiz_oc_bound.py --with-perf --out bench_results/oc_bounds_full.csv --assets-base bin/models
  ./scripts/test_hiz_oc_bound.py --acc-scene default_scenes_and_kittens.json \\
      --perf-scene default_scenes_and_kittens_for_perf.json --with-perf \\
      --out bench_results/oc_bounds_full.csv --assets-base bin/models
"""

from __future__ import annotations

import argparse
import csv
import subprocess
import sys
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parent.parent
SCRIPTS = REPO_ROOT / "scripts"
sys.path.insert(0, str(SCRIPTS))
from batch_mr_graphics_bench import resolve_bench_config  # noqa: E402

BATCH_SCRIPT = SCRIPTS / "batch_mr_graphics_bench.py"
MERGE_SCRIPT = SCRIPTS / "merge_results.py"
BENCH_RESULTS = REPO_ROOT / "bench_results"

DEFAULT_ACC_SCENE = REPO_ROOT / "bench_configs/default_scenes_and_kittens.json"
DEFAULT_PERF_SCENE = REPO_ROOT / "bench_configs/default_scenes_and_kittens_for_perf.json"

OC_BOUNDS = ("Box", "Sphere", "DynamicBest", "Both")
PERF_FIELDS = ("mean_oc_culling_gpu_time_ms", "mean_gpu_rendering_time_ms")
ACC_FIELDS = (
    "mean_total_objects_number",
    "mean_objects_inside_frustum",
    "mean_visible_objects_number",
    "mean_really_visible_objects_number",
    "mean_occlusion_culling_accuracy",
)


def run_cmd(argv: list[str], label: str) -> None:
    print(f"  $ {' '.join(argv)}", file=sys.stderr)
    proc = subprocess.run(argv, check=False)
    if proc.returncode != 0:
        print(f"Error: {label} failed (exit {proc.returncode})", file=sys.stderr)
        raise SystemExit(proc.returncode)


def batch_common_args(args: argparse.Namespace, scenes: Path) -> list[str]:
    argv = [
        sys.executable,
        str(BATCH_SCRIPT),
        f"--scenes={scenes}",
        f"--exe={args.exe}",
        f"--assets-base={args.assets_base}",
        f"--warmup={args.warmup}",
        f"--measure={args.measure}",
    ]
    for bound in OC_BOUNDS:
        argv.append(f"--oc-bounds={bound}")
    if args.timeout is not None:
        argv.append(f"--timeout={args.timeout}")
    if not args.xvfb:
        argv.append("--no-xvfb")
    return argv


def run_batch(args: argparse.Namespace, scenes: Path, preset: str, out: Path) -> None:
    argv = batch_common_args(args, scenes)
    argv.extend([f"--preset={preset}", f"--out={out}"])
    run_cmd(argv, f"batch preset={preset} interleaved bounds")


def split_by_bound(src: Path, bounds: tuple[str, ...], prefix: str) -> list[tuple[str, Path]]:
    """Split interleaved CSV into one file per oc_bounds (drops oc_bounds column)."""
    by_bound: dict[str, list[dict[str, str]]] = {b: [] for b in bounds}
    with src.open(newline="", encoding="utf-8") as fin:
        reader = csv.DictReader(fin)
        if reader.fieldnames is None:
            raise ValueError(f"No header in {src}")
        fieldnames = [c for c in reader.fieldnames if c != "oc_bounds"]
        for row in reader:
            b = row.get("oc_bounds", "")
            if b not in by_bound:
                raise ValueError(f"Unexpected oc_bounds={b!r} in {src}")
            by_bound[b].append({k: row.get(k, "") for k in fieldnames})

    out_paths: list[tuple[str, Path]] = []
    for bound in bounds:
        out = BENCH_RESULTS / f"{prefix}_{bound}.csv"
        rows = by_bound[bound]
        if not rows:
            raise ValueError(f"No rows for oc_bounds={bound} in {src}")
        with out.open("w", newline="", encoding="utf-8") as fout:
            w = csv.DictWriter(fout, fieldnames=fieldnames, extrasaction="ignore")
            w.writeheader()
            w.writerows(rows)
        out_paths.append((bound, out))
    return out_paths


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


def concat_with_bound(sources: list[tuple[str, Path]], out: Path, with_perf: bool) -> None:
    if with_perf:
        metric_cols = [f"perf_{c}" for c in PERF_FIELDS] + [f"acc_{c}" for c in ACC_FIELDS]
    else:
        metric_cols = [f"acc_{c}" for c in ACC_FIELDS]
    fieldnames = ["bound", "scene_name"] + metric_cols

    by_bound: dict[str, dict[str, dict[str, str]]] = {}
    scene_order: list[str] = []

    for bound, path in sources:
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
                if bound == sources[0][0]:
                    scene_order.append(sn)
            by_bound[bound] = rows

    out.parent.mkdir(parents=True, exist_ok=True)
    with out.open("w", newline="", encoding="utf-8") as fout:
        w = csv.DictWriter(fout, fieldnames=fieldnames, extrasaction="ignore")
        w.writeheader()
        total = 0
        for sn in scene_order:
            for bound, _ in sources:
                row = by_bound[bound].get(sn)
                if row is None:
                    raise ValueError(f"scene_name {sn!r} missing in bound={bound}")
                out_row: dict[str, str] = {"bound": bound, "scene_name": sn}
                for c in metric_cols:
                    out_row[c] = row.get(c, "")
                w.writerow(out_row)
                total += 1

    print(f"Wrote {out}  rows={total}", file=sys.stderr)


def main() -> int:
    p = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    p.add_argument("--out", type=Path, required=True, help="Final merged CSV path")
    p.add_argument(
        "--scenes",
        type=Path,
        default=None,
        help="Fallback JSON scene config when --acc-scene / --perf-scene omitted",
    )
    p.add_argument(
        "--acc-scene",
        type=Path,
        default=None,
        help="Bench config for accuracy (path or name under bench_configs/)",
    )
    p.add_argument(
        "--perf-scene",
        type=Path,
        default=None,
        help="Bench config for perf (path or name under bench_configs/)",
    )
    p.add_argument("--with-perf", action="store_true", help="Also run perf preset and merge with accuracy")
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
    p.add_argument(
        "--keep-temps",
        action="store_true",
        help="Keep intermediate CSVs in bench_results/ (default: keep anyway)",
    )
    args = p.parse_args()

    try:
        default_scenes = resolve_bench_config(args.scenes) if args.scenes is not None else None
        acc = args.acc_scene if args.acc_scene is not None else (default_scenes or DEFAULT_ACC_SCENE)
        perf = args.perf_scene if args.perf_scene is not None else (default_scenes or DEFAULT_PERF_SCENE)
        args.acc_scenes = resolve_bench_config(Path(acc))
        args.perf_scenes = resolve_bench_config(Path(perf))
    except FileNotFoundError as e:
        print(f"Error: {e}", file=sys.stderr)
        return 1

    BENCH_RESULTS.mkdir(parents=True, exist_ok=True)

    print(
        f"oc-bounds bench: {len(OC_BOUNDS)} bound types (interleaved), "
        f"{'accuracy+perf' if args.with_perf else 'accuracy only'}; "
        f"acc={args.acc_scenes.name}"
        + (f", perf={args.perf_scenes.name}" if args.with_perf else ""),
        file=sys.stderr,
    )

    raw_acc_all = BENCH_RESULTS / "raw_acc_all_bounds.csv"
    print(f"accuracy bench interleaved ({args.acc_scenes.name})", file=sys.stderr)
    run_batch(args, args.acc_scenes, "accuracy", raw_acc_all)
    acc_parts = split_by_bound(raw_acc_all, OC_BOUNDS, "raw_acc")

    per_bound: list[tuple[str, Path]] = []
    if args.with_perf:
        raw_perf_all = BENCH_RESULTS / "raw_perf_all_bounds.csv"
        print(f"perf bench interleaved ({args.perf_scenes.name})", file=sys.stderr)
        run_batch(args, args.perf_scenes, "perf", raw_perf_all)
        perf_parts = dict(split_by_bound(raw_perf_all, OC_BOUNDS, "raw_perf"))

        for bound, acc_path in acc_parts:
            merged = BENCH_RESULTS / f"merged_{bound}.csv"
            print(f"[{bound}] merge perf + accuracy", file=sys.stderr)
            merge_perf_acc(perf_parts[bound], acc_path, merged)
            per_bound.append((bound, merged))
    else:
        for bound, acc_path in acc_parts:
            trimmed = BENCH_RESULTS / f"trimmed_{bound}.csv"
            print(f"[{bound}] project accuracy fields", file=sys.stderr)
            project_fields(acc_path, ACC_FIELDS, "acc_", trimmed)
            per_bound.append((bound, trimmed))

    print("Concatenating results", file=sys.stderr)
    concat_with_bound(per_bound, args.out.resolve(), args.with_perf)

    if not args.keep_temps:
        print(f"Intermediate files kept in {BENCH_RESULTS}/", file=sys.stderr)

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
