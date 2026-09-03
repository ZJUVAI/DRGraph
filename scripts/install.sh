#!/usr/bin/env bash
set -euo pipefail

root_dir="$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)"
source_dir="$root_dir"
build_dir="$root_dir/build"
prefix="$root_dir/install"
jobs=""
run_tests=0

usage() {
    echo "Usage: $0 [--source-dir <dir>] [--prefix <dir>] [--build-dir <dir>] [--jobs <count>] [--test]" >&2
}

require_value() {
    if [ "$#" -lt 2 ]; then
        usage
        exit 64
    fi
}

while [ "$#" -gt 0 ]; do
    case "$1" in
        --source-dir)
            require_value "$@"
            source_dir="$2"
            shift 2
            ;;
        --prefix)
            require_value "$@"
            prefix="$2"
            shift 2
            ;;
        --build-dir)
            require_value "$@"
            build_dir="$2"
            shift 2
            ;;
        --jobs)
            require_value "$@"
            jobs="$2"
            case "$jobs" in ''|*[!0-9]*) echo "Error: --jobs must be a positive integer" >&2; exit 64 ;; esac
            if [ "$jobs" -eq 0 ]; then
                echo "Error: --jobs must be a positive integer" >&2
                exit 64
            fi
            shift 2
            ;;
        --test)
            run_tests=1
            shift
            ;;
        --help|-h)
            usage
            exit 0
            ;;
        *)
            usage
            exit 64
            ;;
    esac
done

if [ ! -f "$source_dir/CMakeLists.txt" ]; then
    echo "Error: source directory has no CMakeLists.txt: $source_dir" >&2
    exit 66
fi

if [ "$run_tests" -eq 1 ]; then
    build_testing=ON
else
    build_testing=OFF
fi

cmake -S "$source_dir" -B "$build_dir" \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_INSTALL_PREFIX="$prefix" \
    -DBUILD_TESTING="$build_testing"

build_command=(cmake --build "$build_dir" --parallel)
if [ -n "$jobs" ]; then
    build_command+=("$jobs")
fi
if [ "$run_tests" -eq 0 ]; then
    build_command+=(--target drgraph)
fi
"${build_command[@]}"

if [ "$run_tests" -eq 1 ]; then
    ctest --test-dir "$build_dir" --output-on-failure
fi

cmake --install "$build_dir" --component Unspecified
echo "Installed CPU drgraph: $prefix/bin/drgraph"
