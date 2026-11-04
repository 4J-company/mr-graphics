#!/bin/bash
# Build the mounted project. make passes this file as the container
# entrypoint, so edits here apply without rebuilding the image.
#
# Host profile /opt/conan/profiles/default is C++23. Build profile
# /opt/conan/profiles/build is C++20 (mold and other tool_requires).
# The -s flags below repeat that, so this script still works if the
# profile was not patched, and so the knobs are visible in one place.
set -euo pipefail

compiler_bin="${COMPILER:-g++}"
case "${compiler_bin}" in
  gcc|g++) conan_compiler=gcc; dump_bin=g++ ;;
  clang|clang++) conan_compiler=clang; dump_bin=clang++ ;;
  *) echo "COMPILER must be g++ or clang++, got ${compiler_bin}" >&2; exit 1 ;;
esac
version="$("${dump_bin}" -dumpversion | cut -d. -f1)"
build_type="${BUILD_TYPE:-Release}"

stamp="${MR_IMAGE_STAMP:-/etc/mr-graphics-image}"
if [ ! -f "${stamp}" ]; then
  echo "This image does not record its compilers and build types." >&2
  echo "Rebuild it with make image-build. A missing variant would be compiled in this container and deleted on exit." >&2
  exit 1
fi
# shellcheck disable=SC1090
source "${stamp}"
supported=0
for c in ${COMPILERS}; do
  [ "${c}" = "${conan_compiler}" ] && supported=1
done
if [ "${supported}" -ne 1 ]; then
  echo "This image supports COMPILERS=${COMPILERS}, not COMPILER=${compiler_bin}." >&2
  echo "Rebuild the image with that compiler. Building it here would be deleted on exit." >&2
  exit 1
fi
supported=0
for t in ${BUILD_TYPES}; do
  [ "${t}" = "${build_type}" ] && supported=1
done
if [ "${supported}" -ne 1 ]; then
  echo "This image supports BUILD_TYPES=${BUILD_TYPES}, not BUILD_TYPE=${build_type}." >&2
  echo "Rebuild the image with that build type. Building it here would be deleted on exit." >&2
  exit 1
fi

conan remote remove 4J-company >/dev/null 2>&1 || true
conan remote add 4J-company /opt/cci --type local-recipes-index

mkdir -p "${CPM_SOURCE_CACHE}"

extra=()
if [ "${conan_compiler}" = clang ]; then
  extra+=(-c "tools.build:compiler_executables={'c': 'clang', 'cpp': 'clang++'}")
fi
if [ -n "${JOBS:-}" ]; then
  extra+=(-c "tools.build:jobs=${JOBS}")
fi

exec conan build . -b missing \
  -s compiler="${conan_compiler}" \
  -s compiler.version="${version}" \
  -s compiler.libcxx=libstdc++11 \
  -s compiler.cppstd=23 \
  -s:b compiler.cppstd=20 \
  -s build_type="${build_type}" \
  -c tools.system.package_manager:mode=install \
  -c tools.system.package_manager:sudo=True \
  -c tools.files.download:retry=5 \
  -c tools.files.download:retry_wait=5 \
  -c "tools.cmake.cmaketoolchain:extra_variables={'CMAKE_CXX_SCAN_FOR_MODULES': 'OFF', 'CPPTRACE_DISABLE_CXX_20_MODULES': 'ON'}" \
  -c "ktx/*:tools.build:cxxflags=['-mno-avx', '-mno-avx2', '-mno-avx512f']" \
  "${extra[@]}"
