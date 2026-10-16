#!/usr/bin/env bash
#
# build.sh - Build script for compiling main.c using Clang.
#
# Description:
#   This script compiles main.c using clang.
#   - By default, it compiles in debug mode (-g -O0) and save the bin as dash.
#   - Passing -p compiles in production mode with optimizations (-O3).
#   - Passing -t defines the TEST macro (-D TEST) for compiling unit tests.
#   - Uses -Wno-nullability-completeness to suppress clang warnings requesting
#     _Nullable/_Nonnull pointer type specifiers.
#
# Usage:
#   ./build.sh [-p] [-t] [-o <output>] [-h]
#
# Options:
#   -p              Compile optimized for production (-O3) instead of debug mode
#   -t              Compile with -D TEST flag
#   -o <output>     Specify output binary name (default: a.out)
#   -h              Show this help message
#

set -euo pipefail

# Script directory (root of the project)
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "${SCRIPT_DIR}"

# Default configurations
BUILD_MODE="debug"
TEST_FLAG=false
OUTPUT="dash"

usage() {
    cat <<EOF
Usage: $0 [OPTIONS]

Options:
  -p              Compile optimized for production (-O3)
  -t              Compile with test flag (-D TEST)
  -o <output>     Specify output binary name (default: dash)
  -h              Show this help message
EOF
}

# Parse options
while getopts ":pto:h" opt; do
    case "${opt}" in
        p)
            BUILD_MODE="production"
            ;;
        t)
            TEST_FLAG=true
            ;;
        o)
            OUTPUT="${OPTARG}"
            ;;
        h)
            usage
            exit 0
            ;;
        \?)
            echo "Error: Invalid option -${OPTARG}" >&2
            usage >&2
            exit 1
            ;;
        :)
            echo "Error: Option -${OPTARG} requires an argument." >&2
            usage >&2
            exit 1
            ;;
    esac
done
shift $((OPTIND - 1))

# Base flags
# -Wno-nullability-completeness prevents clang from requiring _Nullable/_Nonnull on every pointer
CFLAGS=("-Wno-nullability-completeness")

if [ "${BUILD_MODE}" = "production" ]; then
    CFLAGS+=("-O3")
else
    CFLAGS+=("-g" "-O0" "-fsanitize=address,undefined" "-fno-omit-frame-pointer")
fi

if [ "${TEST_FLAG}" = true ]; then
    CFLAGS+=("-D" "TEST")
fi

# Append any additional arguments passed to the script
CFLAGS+=("$@")

echo "Compiling main.c [mode=${BUILD_MODE}, test=${TEST_FLAG}] -> ${OUTPUT}"
clang "${CFLAGS[@]}" main.c -o "${OUTPUT}"
