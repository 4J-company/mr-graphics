#!/usr/bin/env python3
"""
Batch-run mr-graphics-example or simple-bench over scenes from a JSON config.

Reads per-frame stats from stats.json next to the executable (cwd = exe dir).
Warmup frames discarded; remaining frames averaged (arithmetic mean).

Preset accuracy adds --read-gbuf (needed for occlusion_culling_accuracy ground truth).
Preset perf omits --read-gbuf for lighter GPU/CPU work.

Scenes with bench-instances-number (in JSON or via --bench-instances-number) use simple-bench;
others use mr-graphics-example.

Virtual display (no visible window): install xvfb (e.g. xorg-server-xvfb on Arch/Manjaro),
use default --xvfb / xvfb-run -a.

  ./scripts/batch_mr_graphics_bench.py --scenes scripts/default_scenes.json \\
    --exe ./build/Release/examples/mr-graphics-example \\
    --assets-base bin/models --out mr_graphics_bench.csv --preset perf
"""

from __future__ import annotations

import argparse
import csv
import hashlib
import json
import math
import re
import shlex
import subprocess
import sys
from dataclasses import dataclass
from pathlib import Path
from typing import Any, Iterable

STATS_JSON = "stats.json"


@dataclass(frozen=True)
class Scene:
    scene_id: str
    model_file: str
    camera: str | None = None
    proj: str | None = None
    resolution: str = "1280x720"
    bench_instances_number: int | None = None
    warmup: int | None = None
    measure: int | None = None


def load_scenes(path: Path) -> list[Scene]:
    raw = json.loads(path.read_text(encoding="utf-8"))
    if not isinstance(raw, list):
        raise ValueError(f"{path}: expected JSON array of scenes")

    scenes: list[Scene] = []
    for i, item in enumerate(raw):
        if not isinstance(item, dict):
            raise ValueError(f"{path}: scene[{i}] must be an object")

        model = item.get("model")
        if not model or not isinstance(model, str):
            raise ValueError(f"{path}: scene[{i}] missing string field 'model'")

        name = item.get("name")
        if name is None:
            scene_id = Path(model).stem
        elif isinstance(name, str):
            scene_id = name
        else:
            raise ValueError(f"{path}: scene[{i}] field 'name' must be a string")

        camera = item.get("camera")
        if camera is not None and not isinstance(camera, str):
            raise ValueError(f"{path}: scene[{i}] field 'camera' must be a string")

        proj = item.get("proj")
        if proj is not None and not isinstance(proj, str):
            raise ValueError(f"{path}: scene[{i}] field 'proj' must be a string")

        resolution = item.get("resolution", "1280x720")
        if not isinstance(resolution, str):
            raise ValueError(f"{path}: scene[{i}] field 'resolution' must be a string")

        bench_n = item.get("bench-instances-number")
        if bench_n is not None:
            if isinstance(bench_n, bool) or not isinstance(bench_n, int):
                raise ValueError(f"{path}: scene[{i}] field 'bench-instances-number' must be an integer")
            if bench_n <= 0:
                raise ValueError(f"{path}: scene[{i}] field 'bench-instances-number' must be positive")

        warmup = item.get("warmup")
        if warmup is not None:
            if isinstance(warmup, bool) or not isinstance(warmup, int):
                raise ValueError(f"{path}: scene[{i}] field 'warmup' must be an integer")
            if warmup < 0:
                raise ValueError(f"{path}: scene[{i}] field 'warmup' must be non-negative")

        measure = item.get("measure")
        if measure is not None:
            if isinstance(measure, bool) or not isinstance(measure, int):
                raise ValueError(f"{path}: scene[{i}] field 'measure' must be an integer")
            if measure <= 0:
                raise ValueError(f"{path}: scene[{i}] field 'measure' must be positive")

        scenes.append(
            Scene(
                scene_id=scene_id,
                model_file=model,
                camera=camera,
                proj=proj,
                resolution=resolution,
                bench_instances_number=bench_n,
                warmup=warmup,
                measure=measure,
            )
        )
    return scenes


def resolve_model_path(scene: Scene, assets_base: Path) -> Path:
    p = Path(scene.model_file)
    return p if p.is_absolute() else (assets_base / scene.model_file).resolve()


def camera_hash8(scene_id: str, camera: str | None) -> str:
    return hashlib.sha1(f"{scene_id}|{camera or ''}".encode("utf-8")).hexdigest()[:8]


def effective_bench_instances(scene: Scene, global_bench_instances: int | None) -> int | None:
    return scene.bench_instances_number if scene.bench_instances_number is not None else global_bench_instances


def effective_frames(scene: Scene, global_warmup: int, global_measure: int) -> tuple[int, int]:
    warmup = scene.warmup if scene.warmup is not None else global_warmup
    measure = scene.measure if scene.measure is not None else global_measure
    return warmup, measure


def sanitize_cpp_stats_json(text: str) -> str:
    """RenderStat::write_to_json can emit nan/-nan/inf bare — invalid JSON; coerce to null."""
    text = re.sub(r"(:\s*)[+-]?nan\b", r"\1null", text, flags=re.IGNORECASE)
    text = re.sub(r"(:\s*)[+-]?inf(?:inity)?\b", r"\1null", text, flags=re.IGNORECASE)
    return text


def parse_concat_json_objects(text: str) -> list[dict[str, Any]]:
    text = sanitize_cpp_stats_json(text)
    decoder = json.JSONDecoder()
    objs: list[dict[str, Any]] = []
    i = 0
    n = len(text)
    while i < n:
        while i < n and text[i].isspace():
            i += 1
        if i >= n:
            break
        obj, end = decoder.raw_decode(text, i)
        if not isinstance(obj, dict):
            raise ValueError(f"Expected JSON object at offset {i}, got {type(obj)}")
        objs.append(obj)
        i = end
    return objs


def mean(xs: Iterable[float]) -> float:
    xs_list = list(xs)
    if not xs_list:
        return math.nan
    return sum(xs_list) / len(xs_list)


def format_csv_numeric(v: float) -> str:
    """Round to 4 decimal places for CSV (compact trailing-zero strip)."""
    if math.isnan(v) or math.isinf(v):
        return ""
    r = round(float(v), 4)
    if r == 0:
        r = 0.0
    s = f"{r:.4f}".rstrip("0").rstrip(".")
    return s if s else "0"


# Keys emitted by RenderStat::write_to_json (excluding frame_number from aggregates).
_AGG_KEYS_IN_ORDER: tuple[str, ...] = (
    "cpu_fps",
    "cpu_time_ms",
    "gpu_fps",
    "gpu_time_ms",
    "cpu_rendering_time_ms",
    "culling_gpu_time_ms",
    "build_depth_pyramid_gpu_time_ms",
    "late_culling_gpu_time_ms",
    "gpu_rendering_time_ms",
    "gpu_models_time_ms",
    "gpu_shading_time_ms",
    "triangles_per_second",
    "triangles_per_second_millions",
    "triangles_number",
    "vertexes_number",
    "total_objects_number",
    "outside_frustum_objects_number",
    "visible_objects_number",
    "occluded_objects_number",
    "really_visible_objects_number",
    "not_occluded_in_frustum_objects",
    "occlusion_culling_accuracy",
)


def _json_number_or_nan(v: Any) -> float:
    if v is None:
        return math.nan
    if isinstance(v, bool):
        return float(v)
    if isinstance(v, (int, float)):
        return float(v)
    return math.nan


def aggregate_frames(records: list[dict[str, Any]], warmup: int, measure: int) -> dict[str, float]:
    total_need = warmup + measure
    if len(records) < total_need:
        raise ValueError(f"Expected at least {total_need} frames, got {len(records)}")
    slice_rec = records[warmup : warmup + measure]

    out: dict[str, float] = {}
    for key in _AGG_KEYS_IN_ORDER:
        vals = []
        for r in slice_rec:
            if key not in r:
                vals.append(math.nan)
            else:
                vals.append(_json_number_or_nan(r[key]))
        out[f"mean_{key}"] = mean(vals)

    oc_sum_per_frame = []
    for r in slice_rec:
        bd = _json_number_or_nan(r.get("build_depth_pyramid_gpu_time_ms", math.nan))
        lt = _json_number_or_nan(r.get("late_culling_gpu_time_ms", math.nan))
        if math.isnan(bd) or math.isnan(lt):
            oc_sum_per_frame.append(math.nan)
        else:
            oc_sum_per_frame.append(bd + lt)
    out["mean_oc_culling_gpu_time_ms"] = mean(oc_sum_per_frame)

    inside_vals: list[float] = []
    for r in slice_rec:
        total_o = _json_number_or_nan(r.get("total_objects_number", math.nan))
        outside = _json_number_or_nan(r.get("outside_frustum_objects_number", math.nan))
        if math.isnan(total_o) or math.isnan(outside):
            inside_vals.append(math.nan)
        else:
            inside_vals.append(total_o - outside)
    out["mean_objects_inside_frustum"] = mean(inside_vals)

    return out


def default_exe_path() -> Path:
    return (Path(__file__).resolve().parent.parent / "build/Release/examples/mr-graphics-example").resolve()


def default_simple_bench_exe(example_exe: Path) -> Path:
    return (example_exe.parent / "simple-bench").resolve()


def _append_common_bench_flags(
    argv: list[str],
    scene: Scene,
    preset: str,
    frames: int,
    oc_bounds: str | None,
) -> None:
    argv.extend(
        [
            "--print-stat",
            "--enable-culling-stat",
            f"--frames-number={frames}",
        ]
    )
    if scene.camera is not None:
        argv.append(f"--camera={scene.camera}")
    if scene.proj is not None:
        argv.append(f"--proj={scene.proj}")
    if preset == "accuracy":
        argv.append("--read-gbuf")
    elif preset != "perf":
        raise ValueError(f"Unknown preset: {preset}")
    if oc_bounds is not None:
        argv.append(f"--oc-bounds={oc_bounds}")


def build_argv(
    example_exe: Path,
    simple_bench_exe: Path,
    scene: Scene,
    model_path: Path,
    preset: str,
    warmup: int,
    measure: int,
    xvfb: bool,
    global_bench_instances: int | None,
    oc_bounds: str | None = None,
) -> tuple[list[str], Path]:
    frames = warmup + measure
    bench_instances = effective_bench_instances(scene, global_bench_instances)
    use_simple_bench = bench_instances is not None

    argv: list[str] = []
    if xvfb:
        argv.extend(["xvfb-run", "-a"])

    if use_simple_bench:
        exe = simple_bench_exe
        argv.extend([str(exe), str(model_path)])
        _append_common_bench_flags(argv, scene, preset, frames, oc_bounds)
        argv.append(f"--bench-instances-number={bench_instances}")
        argv.append(f"--resolution={scene.resolution}")
    else:
        exe = example_exe
        argv.extend(
            [
                str(exe),
                "--mode=default",
                str(model_path),
            ]
        )
        _append_common_bench_flags(argv, scene, preset, frames, oc_bounds)
        argv.append(f"--resolution={scene.resolution}")

    return argv, exe


def main() -> int:
    p = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    p.add_argument(
        "--scenes",
        type=Path,
        required=True,
        help="JSON file with scene definitions (see scripts/default_scenes.json)",
    )
    p.add_argument("--exe", type=Path, default=default_exe_path(), help="mr-graphics-example binary")
    p.add_argument(
        "--simple-bench-exe",
        type=Path,
        default=None,
        help="simple-bench binary (default: sibling of --exe)",
    )
    p.add_argument(
        "--bench-instances-number",
        type=int,
        default=None,
        help="Default bench instances; scenes without bench-instances-number use simple-bench when set",
    )
    p.add_argument(
        "--assets-base",
        type=Path,
        default=Path("../assets"),
        help="Base dir for relative model paths",
    )
    p.add_argument("--out", type=Path, default=Path("mr_graphics_bench.csv"))
    p.add_argument("--preset", choices=("perf", "accuracy"), required=True)
    p.add_argument("--warmup", type=int, default=50)
    p.add_argument("--measure", type=int, default=150)
    p.add_argument("--timeout", type=float, default=None)
    p.add_argument("--dry-run", action="store_true")
    p.add_argument("--xvfb", action=argparse.BooleanOptionalAction, default=True, help="Wrap with xvfb-run -a")
    p.add_argument(
        "--oc-bounds",
        choices=("Box", "Sphere", "DynamicBest"),
        default=None,
        help="Occlusion culling bounds type",
    )
    args = p.parse_args()

    example_exe = args.exe.resolve()
    simple_bench_exe = (
        args.simple_bench_exe.resolve()
        if args.simple_bench_exe is not None
        else default_simple_bench_exe(example_exe)
    )

    try:
        bench_scenes = load_scenes(args.scenes.resolve())
    except (OSError, json.JSONDecodeError, ValueError) as e:
        print(f"Error loading scenes from {args.scenes}: {e}", file=sys.stderr)
        return 1

    base_columns = ["scene_name", "preset", "camera_hash8", "camera", "model_path"]

    mean_columns = [f"mean_{k}" for k in _AGG_KEYS_IN_ORDER]
    ix_outside = mean_columns.index("mean_outside_frustum_objects_number") + 1
    mean_columns.insert(ix_outside, "mean_objects_inside_frustum")
    mean_columns.append("mean_oc_culling_gpu_time_ms")
    fieldnames = base_columns + mean_columns

    args.out.parent.mkdir(parents=True, exist_ok=True)
    n_ok = 0
    n_fail = 0
    total_scenes = len(bench_scenes)

    with args.out.open("w", newline="", encoding="utf-8") as fcsv:
        w = csv.DictWriter(fcsv, fieldnames=fieldnames, extrasaction="ignore")
        w.writeheader()

        if not args.dry_run:
            print(
                f"mr-graphics bench: {total_scenes} scenes, preset={args.preset}, "
                f"{args.warmup + args.measure} frames per scene "
                f"(warmup {args.warmup}, measure {args.measure})",
                file=sys.stderr,
            )

        for idx, s in enumerate(bench_scenes, start=1):
            model = resolve_model_path(s, args.assets_base.resolve())
            cam_h = camera_hash8(s.scene_id, s.camera)
            scene_name = f"{s.scene_id}_{cam_h}"
            warmup, measure = effective_frames(s, args.warmup, args.measure)

            argv, run_exe = build_argv(
                example_exe,
                simple_bench_exe,
                s,
                model,
                args.preset,
                warmup,
                measure,
                args.xvfb,
                args.bench_instances_number,
                args.oc_bounds,
            )
            cwd = run_exe.parent
            stats_path = cwd / STATS_JSON

            if args.dry_run:
                print(" ".join(shlex.quote(x) for x in argv))
                continue

            print(
                f"[{idx}/{total_scenes}] run: {scene_name} ({model.name}), "
                f"warmup={warmup}, measure={measure}, please wait",
                file=sys.stderr,
            )
            stats_path.unlink(missing_ok=True)

            try:
                proc = subprocess.run(
                    argv,
                    cwd=str(cwd),
                    capture_output=True,
                    text=True,
                    timeout=args.timeout,
                    check=False,
                )
            except subprocess.TimeoutExpired:
                n_fail += 1
                print(f"[{idx}/{total_scenes}] TIMEOUT preset={args.preset} {scene_name}", file=sys.stderr)
                continue

            if proc.returncode != 0:
                n_fail += 1
                err_tail = (proc.stderr or "")[-800:]
                print(
                    f"[{idx}/{total_scenes}] FAIL rc={proc.returncode} preset={args.preset} {scene_name}\n{err_tail}",
                    file=sys.stderr,
                )
                continue

            if not stats_path.is_file():
                n_fail += 1
                print(
                    f"[{idx}/{total_scenes}] FAIL missing {stats_path} preset={args.preset} {scene_name}",
                    file=sys.stderr,
                )
                continue

            raw = stats_path.read_text(encoding="utf-8")
            try:
                records = parse_concat_json_objects(raw)
                agg = aggregate_frames(records, warmup, measure)
            except ValueError as e:
                n_fail += 1
                print(
                    f"[{idx}/{total_scenes}] FAIL stats preset={args.preset} {scene_name}: {e}",
                    file=sys.stderr,
                )
                continue

            print(
                f"[{idx}/{total_scenes}] done: {scene_name} ({model.name}), "
                f"{total_scenes - idx} scene(s) left",
                file=sys.stderr,
            )

            row: dict[str, Any] = {
                "scene_name": scene_name,
                "preset": args.preset,
                "camera_hash8": cam_h,
                "camera": s.camera or "",
                "model_path": str(model),
            }
            row.update({k: format_csv_numeric(agg[k]) for k in mean_columns})
            w.writerow(row)
            fcsv.flush()
            n_ok += 1

    if not args.dry_run:
        print(f"Wrote {args.out}  ok={n_ok}  fail={n_fail}", file=sys.stderr)
        if args.xvfb and n_fail:
            print(
                "Hint: if startup fails under Wayland/Xvfb, try --no-xvfb or ensure XWayland/xvfb-run works.",
                file=sys.stderr,
            )
    return 0 if n_fail == 0 else 1


if __name__ == "__main__":
    raise SystemExit(main())
