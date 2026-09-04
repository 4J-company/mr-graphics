#!/usr/bin/env bash
set -euo pipefail

REPO_ROOT="$(cd "$(dirname "$0")/.." && pwd)"
cd "$REPO_ROOT"

OUT_DIR="${OUT_DIR:-tuning_results}"
ACC_SCENE="${ACC_SCENE:-bench_configs/default_scenes_and_kittens.json}"
PERF_SCENE="${PERF_SCENE:-bench_configs/default_scenes_and_kittens_for_perf.json}"
ASSETS_BASE="${ASSETS_BASE:-bin/models}"

mkdir -p "$OUT_DIR"

COMMON=(
  --acc-scene="$ACC_SCENE"
  --perf-scene="$PERF_SCENE"
  --assets-base="$ASSETS_BASE"
  --preset=full
)

run_variant() {
  local name="$1"
  shift
  echo "=== tuning: $name ==="
  ./scripts/test_msoc_options.py "${COMMON[@]}" --out="$OUT_DIR/${name}.csv" "$@"
}

run_variant msoc --oc-type=msoc
run_variant msoc_hiz_coarse --oc-type=msoc --msoc-with-hiz-coarse
run_variant msoc_adaptive --oc-type=msoc-adaptive-tile-size
run_variant msoc_adaptive_coarse --oc-type=msoc-adaptive-tile-size --msoc-with-hiz-coarse

echo "Done. Results in $OUT_DIR/"
