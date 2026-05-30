#!/usr/bin/env python3
"""
Benchmark occlusion culling bounds (Box, Sphere, DynamicBest) across fixed scenes.

Runs batch_mr_graphics_bench.py for each bound type, trims columns to match bench.sh,
then concatenates results with a bound column grouped by scene (scene1 Box/Sphere/DynamicBest, scene2 …).

Default: accuracy preset only. Use --with-perf to also run perf and merge perf+accuracy per bound.

Intermediate CSVs go to bench_results/ in the repo root.

  ./scripts/test_hiz_oc_bound.py --scenes scripts/default_scenes.json --out bench_results/oc_bounds.csv --assets-base bin/models
  ./scripts/test_hiz_oc_bound.py --scenes scripts/default_scenes.json --with-perf --out bench_results/oc_bounds_full.csv
"""

from __future__ import annotations

import argparse
import csv
import subprocess
import sys
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parent.parent
SCRIPTS = REPO_ROOT / "scripts"
BATCH_SCRIPT = SCRIPTS / "batch_mr_graphics_bench.py"
MERGE_SCRIPT = SCRIPTS / "merge_results.py"
BENCH_RESULTS = REPO_ROOT / "bench_results"

OC_BOUNDS = ("Box", "Sphere", "DynamicBest")
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


def batch_common_args(args: argparse.Namespace) -> list[str]:
    argv = [
        sys.executable,
        str(BATCH_SCRIPT),
        f"--scenes={args.scenes}",
        f"--exe={args.exe}",
        f"--assets-base={args.assets_base}",
        f"--warmup={args.warmup}",
        f"--measure={args.measure}",
    ]
    if args.timeout is not None:
        argv.append(f"--timeout={args.timeout}")
    if not args.xvfb:
        argv.append("--no-xvfb")
    return argv


def run_batch(args: argparse.Namespace, preset: str, oc_bound: str, out: Path) -> None:
    argv = batch_common_args(args)
    argv.extend(
        [
            f"--preset={preset}",
            f"--oc-bounds={oc_bound}",
            f"--out={out}",
        ]
    )
    run_cmd(argv, f"batch preset={preset} oc-bounds={oc_bound}")


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


def process_bound(args: argparse.Namespace, bound: str) -> Path:
    raw_acc = BENCH_RESULTS / f"raw_acc_{bound}.csv"
    trimmed = BENCH_RESULTS / f"trimmed_{bound}.csv"

    print(f"[{bound}] accuracy bench", file=sys.stderr)
    run_batch(args, "accuracy", bound, raw_acc)

    if args.with_perf:
        raw_perf = BENCH_RESULTS / f"raw_perf_{bound}.csv"
        merged = BENCH_RESULTS / f"merged_{bound}.csv"
        print(f"[{bound}] perf bench", file=sys.stderr)
        run_batch(args, "perf", bound, raw_perf)
        print(f"[{bound}] merge perf + accuracy", file=sys.stderr)
        merge_perf_acc(raw_perf, raw_acc, merged)
        return merged

    print(f"[{bound}] project accuracy fields", file=sys.stderr)
    project_fields(raw_acc, ACC_FIELDS, "acc_", trimmed)
    return trimmed


def main() -> int:
    p = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    p.add_argument("--out", type=Path, required=True, help="Final merged CSV path")
    p.add_argument(
        "--scenes",
        type=Path,
        required=True,
        help="JSON scene config passed to batch_mr_graphics_bench.py",
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

    BENCH_RESULTS.mkdir(parents=True, exist_ok=True)

    print(
        f"oc-bounds bench: {len(OC_BOUNDS)} bound types, "
        f"{'accuracy+perf' if args.with_perf else 'accuracy only'}",
        file=sys.stderr,
    )

    per_bound: list[tuple[str, Path]] = []
    for bound in OC_BOUNDS:
        result_path = process_bound(args, bound)
        per_bound.append((bound, result_path))

    print("Concatenating results", file=sys.stderr)
    concat_with_bound(per_bound, args.out.resolve(), args.with_perf)

    if not args.keep_temps:
        print(f"Intermediate files kept in {BENCH_RESULTS}/", file=sys.stderr)

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
