#!/bin/bash
# Remove only mr-graphics images and containers started from them.
# Stops at any other tagged image (the ubuntu base stays).
# Untagged parent and sibling layers of those images are included.
# DRY=1 prints the set and does not delete.
set -euo pipefail

IMAGE="${IMAGE:-mr-graphics-dev:local}"
REMOTE_IMAGE="${REMOTE_IMAGE:-ghcr.io/4j-company/mr-graphics-dev}"

mapfile -t all_ids < <(docker images -aq --no-trunc | sort -u)

declare -A ours=()

is_our_tag() {
  local tag
  for tag in "$@"; do
    case "${tag}" in
      mr-graphics-dev:*|"${REMOTE_IMAGE}"|"${REMOTE_IMAGE}":*) return 0 ;;
    esac
  done
  return 1
}

tags_of() {
  docker inspect -f '{{range .RepoTags}}{{.}} {{end}}' "$1" 2>/dev/null || true
}

# Containerd images omit Parent. {{.Parent}} then aborts the whole inspect.
parent_of() {
  local parent
  parent="$(docker image inspect "$1" --format '{{index . "Parent"}}' 2>/dev/null || true)"
  if [ -z "${parent}" ] || [ "${parent}" = "<no value>" ]; then
    return 0
  fi
  printf '%s\n' "${parent}"
}

for id in "${all_ids[@]}"; do
  # shellcheck disable=SC2086
  if is_our_tag $(tags_of "${id}"); then
    ours["${id}"]=1
  fi
done

changed=1
while [ "${changed}" -eq 1 ]; do
  changed=0
  for id in "${all_ids[@]}"; do
    [ -n "${ours[${id}]:-}" ] && continue
    # A foreign tag means this image is not ours.
    [ -n "$(tags_of "${id}")" ] && continue
    parent="$(parent_of "${id}")"
    if [ -n "${parent}" ] && [ -n "${ours[${parent}]:-}" ]; then
      ours["${id}"]=1
      changed=1
    fi
  done
  for id in "${!ours[@]}"; do
    parent="$(parent_of "${id}")"
    [ -z "${parent}" ] && continue
    [ -n "${ours[${parent}]:-}" ] && continue
    # Stop at a tagged image that is not mr-graphics (ubuntu:26.04).
    [ -n "$(tags_of "${parent}")" ] && continue
    if docker inspect "${parent}" >/dev/null 2>&1; then
      ours["${parent}"]=1
      changed=1
      all_ids+=("${parent}")
    fi
  done
done

if [ "${#ours[@]}" -eq 0 ]; then
  echo "image-clean: no mr-graphics images"
  exit 0
fi

echo "image-clean: mr-graphics images:"
for id in "${!ours[@]}"; do
  short="${id#sha256:}"
  echo "  ${short:0:12} $(tags_of "${id}")"
done

containers="$(
  for id in "${!ours[@]}"; do
    docker ps -aq --no-trunc --filter "ancestor=${id}"
  done | sort -u
)"

if [ -n "${containers}" ]; then
  echo "image-clean: containers:"
  while read -r cid; do
    [ -z "${cid}" ] && continue
    docker ps -a --no-trunc --filter "id=${cid}" --format '  {{.ID}} {{.Image}} {{.Names}}'
  done <<< "${containers}"
fi

if [ "${DRY:-0}" = 1 ]; then
  exit 0
fi

if [ -n "${containers}" ]; then
  # shellcheck disable=SC2086
  docker rm -f ${containers}
fi

# An id with several tags (local, :latest, image id) is not removed
# unless every tag is dropped. -f does that after containers are gone.
left="${!ours[*]}"
while [ -n "${left// /}" ]; do
  next=""
  progress=0
  for id in ${left}; do
    if err="$(docker rmi -f "${id}" 2>&1)"; then
      progress=1
    else
      next="${next} ${id}"
      printf '%s\n' "${err}" >&2
    fi
  done
  [ "${progress}" -eq 0 ] && break
  left="${next}"
done
if [ -n "${left// /}" ]; then
  echo "image-clean: failed to remove:${left}" >&2
  exit 1
fi
