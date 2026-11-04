#!/bin/bash
# Install Conan dependencies for one compiler and build type.
# Used while building the image. The project build command is docker/mr-build.sh
# and is not baked into the image.
set -euo pipefail

if [ "$#" -gt 0 ]; then
  exec "$@"
fi

compiler_bin="${COMPILER:-g++}"
case "${compiler_bin}" in
  gcc|g++) conan_compiler=gcc; dump_bin=g++ ;;
  clang|clang++) conan_compiler=clang; dump_bin=clang++ ;;
  *) echo "COMPILER must be g++ or clang++, got ${compiler_bin}" >&2; exit 1 ;;
esac
version="$("${dump_bin}" -dumpversion | cut -d. -f1)"
build_type="${BUILD_TYPE:-Release}"

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

exec conan install . -b missing \
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
