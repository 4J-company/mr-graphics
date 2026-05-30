#!/usr/bin/env bash
set -euo pipefail

SCENES="${1:?Usage: $0 scenes.json}"

echo "======================"
echo "= Accuracy tests     ="
echo "======================"
./scripts/batch_mr_graphics_bench.py --scenes="$SCENES" --preset=accuracy --out=mr_graphics_bench_acc.csv --assets-base=bin/models

echo "======================"
echo "= Perf bench         ="
echo "======================"
./scripts/batch_mr_graphics_bench.py --scenes="$SCENES" --preset=perf --out=mr_graphics_bench_perf.csv --assets-base=bin/models

./scripts/merge_results.py \
--perf mr_graphics_bench_perf.csv \
--accuracy mr_graphics_bench_acc.csv \
--out mr_graphics_bench.csv \
--perf-fields "mean_oc_culling_gpu_time_ms mean_gpu_rendering_time_ms" \
--accuracy-fields "mean_total_objects_number mean_objects_inside_frustum mean_visible_objects_number mean_really_visible_objects_number mean_occlusion_culling_accuracy"
