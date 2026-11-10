#!/bin/bash
set -e

# 以脚本所在位置定位项目；配置文件使用 ../config/... 相对路径。
DART_ROOT="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
cd "$DART_ROOT/build" || {
    printf '未找到 build 目录，请先完成 CMake 配置和编译。\n' >&2
    exit 1
}

if [ ! -x ./Dart ]; then
    printf '未找到可执行文件 %s/build/Dart，请先完成编译。\n' "$DART_ROOT" >&2
    exit 1
fi

# 为当前进程补充 SDK 动态库路径，不改动系统环境配置。
if [ -d /opt/MVS/lib/64 ]; then
    export LD_LIBRARY_PATH="/opt/MVS/lib/64${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
fi
for DART_OPENVINO_LIB in /opt/intel/openvino/runtime/lib/intel64 /opt/intel/openvino_2025/runtime/lib/intel64 /opt/intel/openvino_*/runtime/lib/intel64; do
    if [ -d "$DART_OPENVINO_LIB" ]; then
        export LD_LIBRARY_PATH="$DART_OPENVINO_LIB${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
        break
    fi
done

# 继承当前图形桌面的 DISPLAY / WAYLAND_DISPLAY，日志直接显示在终端。
exec ./Dart
