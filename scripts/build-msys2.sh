#!/usr/bin/env bash
set -Eeuo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
CONFIGURE_PRESET="${CONFIGURE_PRESET:-msys2-mingw64}"
BUILD_PRESET="${BUILD_PRESET:-build-msys2-mingw64}"
QT_ROOT="${QT_ROOT:-${MSYSTEM_PREFIX:-${MINGW_PREFIX:-}}}"
MPV_ROOT="${MPV_ROOT:-${ROOT_DIR}/third_party/mpv}"
RUN_WINDEPLOYQT="${RUN_WINDEPLOYQT:-1}"
CMAKE_BIN="${CMAKE_BIN:-}"

find_first_existing() {
    local path
    for path in "$@"; do
        if [[ -e "${path}" ]]; then
            printf '%s\n' "${path}"
            return 0
        fi
    done
    return 1
}

copy_if_exists() {
    local source="$1"
    local target_dir="$2"
    if [[ -f "${source}" ]]; then
        cp -f "${source}" "${target_dir}/"
    fi
}

find_qt_plugin_root() {
    find_first_existing \
        "${QT_ROOT}/plugins" \
        "${QT_ROOT}/share/qt6/plugins" \
        "${QT_ROOT}/lib/qt6/plugins"
}

copy_qt_svg_runtime() {
    local target_dir="$1"
    local plugin_root="$2"
    local imageformats_dir="${target_dir}/imageformats"

    mkdir -p "${imageformats_dir}"

    copy_if_exists "${plugin_root}/imageformats/qsvg.dll" "${imageformats_dir}"
    copy_if_exists "${plugin_root}/imageformats/qsvgicon.dll" "${imageformats_dir}"
    copy_if_exists "${QT_ROOT}/bin/Qt6Svg.dll" "${target_dir}"
    copy_if_exists "${QT_ROOT}/bin/Qt6SvgWidgets.dll" "${target_dir}"
    copy_if_exists "${QT_ROOT}/bin/Qt6Xml.dll" "${target_dir}"
}

if [[ -z "${QT_ROOT}" ]]; then
    echo "QT_ROOT is not set and MSYSTEM_PREFIX is unavailable. Run this from an MSYS2 MinGW shell or export QT_ROOT." >&2
    exit 1
fi

if [[ ! -d "${QT_ROOT}" ]]; then
    echo "Qt root does not exist: ${QT_ROOT}" >&2
    exit 1
fi

if [[ ! -f "${MPV_ROOT}/include/mpv/client.h" ]]; then
    echo "libmpv headers not found under ${MPV_ROOT}/include/mpv." >&2
    echo "Extract mpv-dev-x86_64-20260417-git-c865008.7z to ${ROOT_DIR}/third_party/mpv or export MPV_ROOT." >&2
    exit 1
fi

MPV_LIBRARY="$(find_first_existing \
    "${MPV_ROOT}/libmpv.dll.a" \
    "${MPV_ROOT}/lib/libmpv.dll.a" \
    "${MPV_ROOT}/lib/libmpv-2.dll.a")"
MPV_RUNTIME_DLL="$(find_first_existing \
    "${MPV_ROOT}/libmpv-2.dll" \
    "${MPV_ROOT}/bin/libmpv-2.dll" \
    "${MPV_ROOT}/libmpv.dll" \
    "${MPV_ROOT}/bin/libmpv.dll")"

if [[ -z "${MPV_LIBRARY:-}" ]]; then
    echo "libmpv import library not found in ${MPV_ROOT}." >&2
    exit 1
fi

if [[ -z "${MPV_RUNTIME_DLL:-}" ]]; then
    echo "libmpv runtime DLL not found in ${MPV_ROOT}." >&2
    exit 1
fi

if [[ -z "${CMAKE_BIN}" && -x "${QT_ROOT}/bin/cmake.exe" ]]; then
    CMAKE_BIN="${QT_ROOT}/bin/cmake.exe"
fi

if [[ -z "${CMAKE_BIN}" && -x "${MSYSTEM_PREFIX:-}/bin/cmake.exe" ]]; then
    CMAKE_BIN="${MSYSTEM_PREFIX}/bin/cmake.exe"
fi

if [[ -z "${CMAKE_BIN}" ]]; then
    CMAKE_BIN="$(command -v cmake || true)"
fi

if [[ -z "${CMAKE_BIN}" ]]; then
    echo "cmake not found. Install mingw-w64-x86_64-cmake in the MSYS2 MinGW environment." >&2
    exit 1
fi

case "${CMAKE_BIN}" in
    /usr/bin/cmake|/usr/bin/cmake.exe)
        echo "Detected MSYS cmake at ${CMAKE_BIN}, which generates /c/... paths that native Ninja cannot build." >&2
        echo "Install mingw-w64-x86_64-cmake and rerun from the MSYS2 MinGW shell." >&2
        echo "Expected cmake path example: /mingw64/bin/cmake.exe" >&2
        exit 1
        ;;
esac

export PATH="${QT_ROOT}/bin:${PATH}"

"${CMAKE_BIN}" --preset "${CONFIGURE_PRESET}" \
    -DCMAKE_PREFIX_PATH="${QT_ROOT}" \
    -DMPV_ROOT="${MPV_ROOT}" \
    -DMPV_LIBRARY="${MPV_LIBRARY}" \
    -DMPV_RUNTIME_DLL="${MPV_RUNTIME_DLL}"

"${CMAKE_BIN}" --build --preset "${BUILD_PRESET}" --parallel

OUTPUT_DIR="${ROOT_DIR}/build/msys2-mingw64"
EXECUTABLE_PATH="${OUTPUT_DIR}/lzc-player.exe"
QT_PLUGIN_ROOT="$(find_qt_plugin_root || true)"

if [[ "${RUN_WINDEPLOYQT}" != "0" ]]; then
    if command -v windeployqt >/dev/null 2>&1; then
        windeployqt --release "${EXECUTABLE_PATH}"
        if [[ -n "${QT_PLUGIN_ROOT}" ]]; then
            copy_qt_svg_runtime "${OUTPUT_DIR}" "${QT_PLUGIN_ROOT}"
        else
            echo "Qt plugin root not found under ${QT_ROOT}; skipping manual SVG runtime copy." >&2
        fi
    else
        echo "windeployqt not found in PATH; skipping Qt runtime deployment." >&2
    fi
fi

echo "Build completed."
echo "Executable: ${EXECUTABLE_PATH}"
echo "Bundled libmpv DLL: ${OUTPUT_DIR}/libmpv-2.dll"
