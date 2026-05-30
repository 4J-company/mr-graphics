#!/usr/bin/env python3
"""
Merge two mr-graphics bench CSVs (perf vs accuracy runs) by scene_name.

Rows align on scene_name (same table as batch_mr_graphics_bench.py). Writes one row per
scene present in BOTH inputs. Output columns: scene_name only from join key, plus
perf_<col> / acc_<col> for columns listed in --perf-fields / --accuracy-fields (do not list
scene_name there — it is implicit).

Single-CSV project mode (--input): select and optionally prefix columns without joining.

Example (join):

  ./scripts/merge_results.py \\
    --perf perf.csv --accuracy acc.csv --out merged.csv \\
    --perf-fields mean_oc_culling_gpu_time_ms mean_gpu_rendering_time_ms \\
    --accuracy-fields mean_gpu_rendering_time_ms mean_occlusion_culling_accuracy

Example (project):

  ./scripts/merge_results.py \\
    --input acc.csv --out trimmed.csv \\
    --fields mean_occlusion_culling_accuracy --prefix acc_
"""

from __future__ import annotations

import argparse
import csv
import sys
from pathlib import Path


def read_csv_rows(path: Path) -> tuple[list[str], list[dict[str, str]]]:
    with path.open(newline="", encoding="utf-8") as f:
        reader = csv.DictReader(f)
        if reader.fieldnames is None:
            raise ValueError(f"No header row in {path}")
        fieldnames = list(reader.fieldnames)
        rows = list(reader)
    return fieldnames, rows


def index_by_scene_name(rows: list[dict[str, str]], path: Path) -> dict[str, dict[str, str]]:
    out: dict[str, dict[str, str]] = {}
    for r in rows:
        key = r.get("scene_name", "")
        if not key:
            raise ValueError(f"Row missing scene_name in {path}")
        if key in out:
            raise ValueError(f"Duplicate scene_name {key!r} in {path}")
        out[key] = r
    return out


def split_field_list(s: str) -> list[str]:
    return [x for x in s.replace(",", " ").split() if x]


def run_project(args: argparse.Namespace) -> int:
    fields = split_field_list(args.fields)
    if not fields:
        print("Error: --fields must list at least one column.", file=sys.stderr)
        return 1
    if "scene_name" in fields:
        print("Error: do not include scene_name in --fields; it is output automatically.", file=sys.stderr)
        return 1

    header, rows = read_csv_rows(args.input)
    if "scene_name" not in header:
        print(f"Error: input CSV must have a scene_name column: {args.input}", file=sys.stderr)
        return 1

    missing = [c for c in fields if c not in header]
    if missing:
        print(f"Error: input CSV missing columns: {missing}. Available: {header}", file=sys.stderr)
        return 1

    prefix = args.prefix or ""
    out_cols = ["scene_name"] + [f"{prefix}{c}" for c in fields]

    args.out.parent.mkdir(parents=True, exist_ok=True)
    with args.out.open("w", newline="", encoding="utf-8") as fout:
        w = csv.DictWriter(fout, fieldnames=out_cols, extrasaction="ignore")
        w.writeheader()
        for r in rows:
            sn = r.get("scene_name", "")
            if not sn:
                print(f"Error: row missing scene_name in {args.input}", file=sys.stderr)
                return 1
            row: dict[str, str] = {"scene_name": sn}
            for c in fields:
                row[f"{prefix}{c}"] = r.get(c, "")
            w.writerow(row)

    print(f"Wrote {args.out}  projected_rows={len(rows)}", file=sys.stderr)
    return 0


def run_join(args: argparse.Namespace) -> int:
    perf_fields = split_field_list(args.perf_fields)
    acc_fields = split_field_list(args.accuracy_fields)
    if not perf_fields or not acc_fields:
        print("Error: --perf-fields and --accuracy-fields must list at least one column each.", file=sys.stderr)
        return 1
    if "scene_name" in perf_fields or "scene_name" in acc_fields:
        print("Error: do not include scene_name in --perf-fields/--accuracy-fields; it is output automatically.", file=sys.stderr)
        return 1

    perf_header, perf_rows = read_csv_rows(args.perf)
    acc_header, acc_rows = read_csv_rows(args.accuracy)

    if "scene_name" not in perf_header or "scene_name" not in acc_header:
        print("Error: both CSVs must have a scene_name column.", file=sys.stderr)
        return 1

    for name, cols, hdr in (
        ("perf", perf_fields, perf_header),
        ("accuracy", acc_fields, acc_header),
    ):
        missing = [c for c in cols if c not in hdr]
        if missing:
            print(f"Error: {name} CSV missing columns: {missing}. Available: {hdr}", file=sys.stderr)
            return 1

    perf_ix = index_by_scene_name(perf_rows, args.perf)
    acc_ix = index_by_scene_name(acc_rows, args.accuracy)

    keys_perf = set(perf_ix)
    keys_acc = set(acc_ix)
    common = sorted(keys_perf & keys_acc)
    only_perf = sorted(keys_perf - keys_acc)
    only_acc = sorted(keys_acc - keys_perf)

    if only_perf:
        print(f"Warning: {len(only_perf)} scene(s) only in perf (skipped): {only_perf[:8]}{'...' if len(only_perf) > 8 else ''}", file=sys.stderr)
    if only_acc:
        print(f"Warning: {len(only_acc)} scene(s) only in accuracy (skipped): {only_acc[:8]}{'...' if len(only_acc) > 8 else ''}", file=sys.stderr)

    if not common:
        print("Error: no matching scene_name rows between perf and accuracy.", file=sys.stderr)
        return 1

    if "preset" in perf_header:
        bad = [k for k in common if perf_ix[k].get("preset") != "perf"]
        if bad:
            print(f"Warning: perf CSV rows without preset=perf for scenes: {bad[:5]}...", file=sys.stderr)
    if "preset" in acc_header:
        bad = [k for k in common if acc_ix[k].get("preset") != "accuracy"]
        if bad:
            print(f"Warning: accuracy CSV rows without preset=accuracy for scenes: {bad[:5]}...", file=sys.stderr)

    out_cols = ["scene_name"] + [f"perf_{c}" for c in perf_fields] + [f"acc_{c}" for c in acc_fields]

    args.out.parent.mkdir(parents=True, exist_ok=True)
    with args.out.open("w", newline="", encoding="utf-8") as fout:
        w = csv.DictWriter(fout, fieldnames=out_cols, extrasaction="ignore")
        w.writeheader()
        for sn in common:
            pr, ar = perf_ix[sn], acc_ix[sn]
            row: dict[str, str] = {"scene_name": sn}
            for c in perf_fields:
                row[f"perf_{c}"] = pr.get(c, "")
            for c in acc_fields:
                row[f"acc_{c}"] = ar.get(c, "")
            w.writerow(row)

    print(f"Wrote {args.out}  merged_rows={len(common)}", file=sys.stderr)
    return 0


def main() -> int:
    p = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    p.add_argument("--out", type=Path, required=True)

    join = p.add_argument_group("join mode (perf + accuracy)")
    join.add_argument("--perf", type=Path, help="CSV from bench --preset perf")
    join.add_argument("--accuracy", "--acc", type=Path, dest="accuracy", help="CSV from bench --preset accuracy")
    join.add_argument(
        "--perf-fields",
        type=str,
        help="Whitespace-separated column names to copy from perf CSV (perf_<name> in output)",
    )
    join.add_argument(
        "--accuracy-fields",
        "--acc-fields",
        type=str,
        dest="accuracy_fields",
        help="Whitespace-separated column names from accuracy CSV (acc_<name> in output)",
    )

    project = p.add_argument_group("project mode (single CSV)")
    project.add_argument("--input", type=Path, help="Single input CSV")
    project.add_argument(
        "--fields",
        type=str,
        help="Whitespace-separated column names to copy from input CSV",
    )
    project.add_argument(
        "--prefix",
        type=str,
        default="",
        help="Optional prefix for output column names (e.g. acc_, perf_)",
    )

    args = p.parse_args()

    if args.input is not None:
        if args.perf or args.accuracy or args.perf_fields or args.accuracy_fields:
            print("Error: --input (project mode) cannot be combined with --perf/--accuracy.", file=sys.stderr)
            return 1
        if not args.fields:
            print("Error: project mode requires --fields.", file=sys.stderr)
            return 1
        return run_project(args)

    if not args.perf or not args.accuracy or not args.perf_fields or not args.accuracy_fields:
        print(
            "Error: join mode requires --perf, --accuracy, --perf-fields, --accuracy-fields "
            "(or use project mode with --input --fields).",
            file=sys.stderr,
        )
        return 1
    return run_join(args)


if __name__ == "__main__":
    raise SystemExit(main())
