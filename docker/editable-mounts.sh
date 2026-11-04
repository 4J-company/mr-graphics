#!/bin/bash
# Print docker -v arguments that expose host Conan editables inside the
# dev image. The image keeps CONAN_HOME=/opt/conan and its binary cache.
# The host editable file is mounted over that cache's editable list, and
# each package directory is mounted at the same absolute path the file
# records, so Conan resolves those recipes from the host tree.
set -euo pipefail

file=${1:-}
if [ -z "${file}" ] || [ ! -f "${file}" ]; then
  exit 0
fi

exec python3 - "${file}" <<'PY'
import json, os, sys

path = sys.argv[1]
dest = "/opt/conan/editable_packages.json"
with open(path) as fh:
    data = json.load(fh)
if not isinstance(data, dict):
    sys.exit("editable_packages.json must be a JSON object")

seen = []
args = ["-v", f"{os.path.abspath(path)}:{dest}:ro"]

def add(folder):
    folder = os.path.abspath(folder)
    if not os.path.isdir(folder):
        sys.exit(f"editable path does not exist: {folder}")
    if folder in seen:
        return
    seen.append(folder)
    args.extend(["-v", f"{folder}:{folder}"])

refs = []
for ref, info in data.items():
    if not isinstance(info, dict) or not info.get("path"):
        sys.exit(f"editable entry {ref} has no path")
    src = info["path"]
    add(src if os.path.isdir(src) else os.path.dirname(src))
    out = info.get("output_folder")
    if out:
        add(out if os.path.isdir(out) else os.path.dirname(out))
    refs.append(ref)

if refs:
    print("Conan editables: " + ", ".join(refs), file=sys.stderr)
print("\n".join(args))
PY
