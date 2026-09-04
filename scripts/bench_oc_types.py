#!/usr/bin/env python3
"""
Benchmark HiZ vs tuned MSOC variants across scenes from a bench config.

Runs batch_mr_graphics_bench.py per variant (HiZ + 4 MSOC configs from
bench_configs/msoc_params_config.json + None with --disable-occlusion-culling),
optionally merges perf+accuracy (full preset),
then concatenates results grouped by scene (scene1 hiz, scene1 msoc, scene2 hiz, …).

Intermediate CSVs go to bench_results/ in the repo root.

  ./scripts/bench_oc_types.py default_scenes.json --preset=full --out bench_results/oc_types_full.csv
  ./scripts/bench_oc_types.py --acc-scene=default_scenes_and_kittens.json \\
      --perf-scene=default_scenes_and_kittens_for_perf.json --preset=full --out bench_results/oc_types_full.csv
  ./scripts/bench_oc_types.py default_scenes.json --preset=full --msoc-extend --out bench_results/oc_types_msoc.csv
"""

from __future__ import annotations

import argparse
import csv
import json
import subprocess
import sys
from dataclasses import dataclass
from pathlib import Path

SCRIPTS = Path(__file__).resolve().parent
REPO_ROOT = SCRIPTS.parent
sys.path.insert(0, str(SCRIPTS))

from batch_mr_graphics_bench import resolve_bench_config  # noqa: E402

BATCH_SCRIPT = SCRIPTS / "batch_mr_graphics_bench.py"
MERGE_SCRIPT = SCRIPTS / "merge_results.py"
BENCH_RESULTS = REPO_ROOT / "bench_results"
DEFAULT_MSOC_PARAMS_CONFIG = REPO_ROOT / "bench_configs/msoc_params_config.json"

PERF_FIELDS = ("mean_oc_culling_gpu_time_ms", "mean_gpu_rendering_time_ms")
ACC_FIELDS = (
    "mean_total_objects_number",
    "mean_objects_inside_frustum",
    "mean_visible_objects_number",
    "mean_really_visible_objects_number",
    "mean_occlusion_culling_accuracy",
)
MSOC_PERF_FIELDS = (
    "mean_msoc_tile_prep_gpu_time_ms",
    "mean_msoc_tile_scan_gpu_time_ms",
    "mean_msoc_tile_finalize_gpu_time_ms",
    "mean_msoc_tile_test_gpu_time_ms",
    "mean_msoc_tile_apply_gpu_time_ms",
)
MSOC_ACC_FIELDS = ("mean_msoc_tiles_number",)

_PRESET_ALIASES = {
    "acc": "acc",
    "accuracy": "acc",
    "perf": "perf",
    "performance": "perf",
    "full": "full",
}


@dataclass(frozen=True)
class OcVariant:
    name: str
    oc_type: str
    msoc_with_hiz_coarse: bool = False
    msoc_tile_size: str | None = None
    msoc_tiles_per_thread: int | None = None
    disable_occlusion_culling: bool = False


def effective_perf_fields(msoc_extend: bool) -> tuple[str, ...]:
    if msoc_extend:
        return PERF_FIELDS + MSOC_PERF_FIELDS
    return PERF_FIELDS


def effective_acc_fields(msoc_extend: bool) -> tuple[str, ...]:
    if msoc_extend:
        return ACC_FIELDS + MSOC_ACC_FIELDS
    return ACC_FIELDS


def parse_preset(value: str) -> str:
    key = value.lower()
    if key not in _PRESET_ALIASES:
        raise argparse.ArgumentTypeError(
            f"invalid preset {value!r} (expected acc, accuracy, perf, performance, or full)"
        )
    return _PRESET_ALIASES[key]


def run_cmd(argv: list[str], label: str) -> None:
    print(f"  $ {' '.join(argv)}", file=sys.stderr)
    proc = subprocess.run(argv, check=False)
    if proc.returncode != 0:
        print(f"Error: {label} failed (exit {proc.returncode})", file=sys.stderr)
        raise SystemExit(proc.returncode)


def load_oc_variants(config_path: Path) -> list[OcVariant]:
    with config_path.open(encoding="utf-8") as fin:
        data = json.load(fin)
    if not isinstance(data, dict):
        raise ValueError(f"{config_path}: expected JSON object at top level")

    variants: list[OcVariant] = [
        OcVariant(name="hiz", oc_type="hiz"),
    ]
    for name, params in data.items():
        if not isinstance(params, dict):
            raise ValueError(f"{config_path}: variant {name!r} must be an object")
        oc_type = params.get("oc-type")
        if not oc_type:
            raise ValueError(f"{config_path}: variant {name!r} missing 'oc-type'")
        tpt = params.get("msoc-tiles-per-thread")
        variants.append(
            OcVariant(
                name=name,
                oc_type=str(oc_type),
                msoc_with_hiz_coarse=bool(params.get("msoc-with-hiz-coarse", False)),
                msoc_tile_size=params.get("msoc-tile-size"),
                msoc_tiles_per_thread=int(tpt) if tpt is not None else None,
            )
        )
    variants.append(
        OcVariant(name="None", oc_type="None", disable_occlusion_culling=True),
    )
    return variants


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
    if args.resolution is not None:
        argv.append(f"--resolution={args.resolution}")
    return argv


def run_batch(
    args: argparse.Namespace,
    scenes_path: Path,
    preset: str,
    variant: OcVariant,
    out: Path,
) -> None:
    argv = batch_common_args(args, scenes_path)
    argv.extend(
        [
            f"--preset={preset}",
            f"--out={out}",
        ]
    )
    if variant.disable_occlusion_culling:
        argv.append("--disable-occlusion-culling")
    else:
        argv.append(f"--oc-type={variant.oc_type}")
        if variant.msoc_tile_size is not None:
            argv.append(f"--msoc-tile-size={variant.msoc_tile_size}")
        if variant.msoc_tiles_per_thread is not None:
            argv.append(f"--msoc-tiles-per-thread={variant.msoc_tiles_per_thread}")
        if variant.msoc_with_hiz_coarse:
            argv.append("--msoc-with-hiz-coarse")
    if args.dry_run:
        argv.append("--dry-run")
    run_cmd(argv, f"batch preset={preset} variant={variant.name}")


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


def merge_perf_acc(
    perf: Path,
    acc: Path,
    out: Path,
    perf_fields: tuple[str, ...],
    acc_fields: tuple[str, ...],
) -> None:
    argv = [
        sys.executable,
        str(MERGE_SCRIPT),
        f"--perf={perf}",
        f"--accuracy={acc}",
        f"--out={out}",
        f"--perf-fields={' '.join(perf_fields)}",
        f"--accuracy-fields={' '.join(acc_fields)}",
    ]
    run_cmd(argv, f"merge {out.name}")


def concat_with_oc_type(
    sources: list[tuple[str, Path]],
    out: Path,
    preset_mode: str,
    perf_fields: tuple[str, ...],
    acc_fields: tuple[str, ...],
) -> None:
    if preset_mode == "full":
        metric_cols = [f"perf_{c}" for c in perf_fields] + [f"acc_{c}" for c in acc_fields]
    elif preset_mode == "acc":
        metric_cols = [f"acc_{c}" for c in acc_fields]
    else:
        metric_cols = [f"perf_{c}" for c in perf_fields]
    fieldnames = ["oc_type", "scene_name"] + metric_cols

    by_type: dict[str, dict[str, dict[str, str]]] = {}
    scene_order: list[str] = []

    for variant_name, path in sources:
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
                if variant_name == sources[0][0]:
                    scene_order.append(sn)
            by_type[variant_name] = rows

    out.parent.mkdir(parents=True, exist_ok=True)
    with out.open("w", newline="", encoding="utf-8") as fout:
        w = csv.DictWriter(fout, fieldnames=fieldnames, extrasaction="ignore")
        w.writeheader()
        total = 0
        for sn in scene_order:
            for variant_name, _ in sources:
                row = by_type[variant_name].get(sn)
                if row is None:
                    raise ValueError(f"scene_name {sn!r} missing for oc_type={variant_name}")
                out_row: dict[str, str] = {"oc_type": variant_name, "scene_name": sn}
                for c in metric_cols:
                    out_row[c] = row.get(c, "")
                w.writerow(out_row)
                total += 1

    print(f"Wrote {out}  rows={total}", file=sys.stderr)


def process_variant(
    args: argparse.Namespace,
    acc_scenes_path: Path,
    perf_scenes_path: Path,
    variant: OcVariant,
    preset_mode: str,
    perf_fields: tuple[str, ...],
    acc_fields: tuple[str, ...],
) -> Path | None:
    key = variant.name
    if args.dry_run:
        if preset_mode == "full":
            run_batch(args, acc_scenes_path, "accuracy", variant, BENCH_RESULTS / f"raw_acc_{key}.csv")
            run_batch(args, perf_scenes_path, "perf", variant, BENCH_RESULTS / f"raw_perf_{key}.csv")
        else:
            batch_preset = "accuracy" if preset_mode == "acc" else "perf"
            scenes_path = acc_scenes_path if preset_mode == "acc" else perf_scenes_path
            run_batch(args, scenes_path, batch_preset, variant, BENCH_RESULTS / f"raw_{preset_mode}_{key}.csv")
        return None

    if preset_mode == "full":
        raw_acc = BENCH_RESULTS / f"raw_acc_{key}.csv"
        raw_perf = BENCH_RESULTS / f"raw_perf_{key}.csv"
        merged = BENCH_RESULTS / f"merged_{key}.csv"

        print(f"[{key}] accuracy bench ({acc_scenes_path.name})", file=sys.stderr)
        run_batch(args, acc_scenes_path, "accuracy", variant, raw_acc)
        print(f"[{key}] perf bench ({perf_scenes_path.name})", file=sys.stderr)
        run_batch(args, perf_scenes_path, "perf", variant, raw_perf)
        print(f"[{key}] merge perf + accuracy", file=sys.stderr)
        merge_perf_acc(raw_perf, raw_acc, merged, perf_fields, acc_fields)
        return merged

    batch_preset = "accuracy" if preset_mode == "acc" else "perf"
    prefix = "acc_" if preset_mode == "acc" else "perf_"
    fields = acc_fields if preset_mode == "acc" else perf_fields
    scenes_path = acc_scenes_path if preset_mode == "acc" else perf_scenes_path
    raw = BENCH_RESULTS / f"raw_{preset_mode}_{key}.csv"
    trimmed = BENCH_RESULTS / f"trimmed_{preset_mode}_{key}.csv"

    print(f"[{key}] {batch_preset} bench ({scenes_path.name})", file=sys.stderr)
    run_batch(args, scenes_path, batch_preset, variant, raw)
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
    p.add_argument("--preset", type=parse_preset, required=True, help="acc, perf, or full")
    p.add_argument("--out", type=Path, required=True, help="Final merged CSV path")
    p.add_argument(
        "--msoc-params-config",
        type=Path,
        default=DEFAULT_MSOC_PARAMS_CONFIG,
        help=f"MSOC tuned params JSON (default: {DEFAULT_MSOC_PARAMS_CONFIG.relative_to(REPO_ROOT)})",
    )
    p.add_argument(
        "--oc-bounds",
        choices=("Box", "Sphere", "DynamicBest", "Both"),
        default=None,
        help="Occlusion culling bounds type (default: exe default)",
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
    p.add_argument(
        "--resolution",
        type=str,
        default=None,
        help="Override scene resolution (e.g. 1920x1080); default: per-scene or 1920x1080",
    )
    p.add_argument("--warmup", type=int, default=50)
    p.add_argument("--measure", type=int, default=150)
    p.add_argument("--timeout", type=float, default=None)
    p.add_argument("--xvfb", action=argparse.BooleanOptionalAction, default=True)
    p.add_argument("--dry-run", action="store_true", help="Print batch commands without running benches")
    p.add_argument(
        "--msoc-extend",
        action="store_true",
        help="Add MSOC phase GPU times (perf) and mean_msoc_tiles_number (accuracy) to output",
    )
    args = p.parse_args()

    try:
        default_config = resolve_scene_path(args.config) if args.config is not None else None
        acc_scenes_path = resolve_scene_path(args.acc_scene) if args.acc_scene is not None else default_config
        perf_scenes_path = resolve_scene_path(args.perf_scene) if args.perf_scene is not None else default_config
        msoc_params_path = args.msoc_params_config.resolve()
        variants = load_oc_variants(msoc_params_path)
    except FileNotFoundError as e:
        print(f"Error: {e}", file=sys.stderr)
        return 1
    except (OSError, json.JSONDecodeError, ValueError, TypeError) as e:
        print(f"Error: {e}", file=sys.stderr)
        return 1

    if args.preset in ("acc", "full") and acc_scenes_path is None:
        print("Error: accuracy bench requires config positional arg or --acc-scene", file=sys.stderr)
        return 1
    if args.preset in ("perf", "full") and perf_scenes_path is None:
        print("Error: perf bench requires config positional arg or --perf-scene", file=sys.stderr)
        return 1

    BENCH_RESULTS.mkdir(parents=True, exist_ok=True)

    perf_fields = effective_perf_fields(args.msoc_extend)
    acc_fields = effective_acc_fields(args.msoc_extend)

    scene_desc = []
    if args.preset in ("acc", "full"):
        scene_desc.append(f"acc={acc_scenes_path.name}")
    if args.preset in ("perf", "full"):
        scene_desc.append(f"perf={perf_scenes_path.name}")
    variant_names = ", ".join(v.name for v in variants)
    print(
        f"oc-type bench: {len(variants)} variants ({variant_names}), preset={args.preset}, "
        f"scenes: {', '.join(scene_desc)}, msoc_extend={args.msoc_extend}, "
        f"msoc_params={msoc_params_path.name}",
        file=sys.stderr,
    )

    per_type: list[tuple[str, Path]] = []
    for variant in variants:
        result_path = process_variant(
            args,
            acc_scenes_path,
            perf_scenes_path,
            variant,
            args.preset,
            perf_fields,
            acc_fields,
        )
        if result_path is not None:
            per_type.append((variant.name, result_path))

    if args.dry_run:
        print("Dry run complete (skipped merge/concat)", file=sys.stderr)
        return 0

    print("Concatenating results", file=sys.stderr)
    concat_with_oc_type(per_type, args.out.resolve(), args.preset, perf_fields, acc_fields)

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
