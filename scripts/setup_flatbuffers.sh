#!/usr/bin/env bash
set -euo pipefail

# 只安装到本项目 .deps，不使用 sudo，不替换系统库。
DART_ROOT="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
DART_DEPS="$DART_ROOT/.deps"
DART_ARCHIVE="$DART_DEPS/downloads/flatbuffers-v25.2.10.tar.gz"
DART_SHA256="b9c2df49707c57a48fc0923d52b8c73beb72d675f9d44b2211e4569be40a7421"

for DART_TOOL in curl tar sha256sum cmake ninja g++; do
    command -v "$DART_TOOL" >/dev/null || {
        printf '缺少工具：%s\n' "$DART_TOOL" >&2
        exit 1
    }
done

mkdir -p "$DART_DEPS/downloads" "$DART_DEPS/sources"
if [ ! -f "$DART_ARCHIVE" ]; then
    curl --fail --location --retry 2 \
        https://github.com/google/flatbuffers/archive/refs/tags/v25.2.10.tar.gz \
        -o "$DART_ARCHIVE"
fi
printf '%s  %s\n' "$DART_SHA256" "$DART_ARCHIVE" | sha256sum --check -
tar -xzf "$DART_ARCHIVE" -C "$DART_DEPS/sources"

cmake -S "$DART_DEPS/sources/flatbuffers-25.2.10" \
    -B "$DART_DEPS/flatbuffers-build" -G Ninja \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_INSTALL_PREFIX="$DART_DEPS/flatbuffers-25.2.10" \
    -DFLATBUFFERS_BUILD_TESTS=OFF \
    -DFLATBUFFERS_BUILD_FLATC=OFF \
    -DFLATBUFFERS_BUILD_FLATHASH=OFF \
    -DFLATBUFFERS_BUILD_GRPCTEST=OFF
cmake --build "$DART_DEPS/flatbuffers-build" --parallel 2
cmake --install "$DART_DEPS/flatbuffers-build"

printf 'FlatBuffers 25.2.10 已安装到 %s\n' "$DART_DEPS/flatbuffers-25.2.10"
printf '沿用仓库的生成头文件；本步骤未安装 flatc。\n'
