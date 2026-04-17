#!/usr/bin/env bash
set -Eeuo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
APP_NAME="${APP_NAME:-lzc-player}"
APP_DISPLAY_NAME="${APP_DISPLAY_NAME:-Lzc Player}"
QT_ROOT="${QT_ROOT:-}"
BUILD_DIR="${BUILD_DIR:-${ROOT_DIR}/build/linux-release}"
APPDIR="${APPDIR:-${ROOT_DIR}/dist/linux/${APP_NAME}.AppDir}"
DIST_ROOT="${DIST_ROOT:-${ROOT_DIR}/dist/linux}"
LINUXDEPLOYQT_BIN="${LINUXDEPLOYQT_BIN:-$(command -v linuxdeployqt || true)}"
APPIMAGETOOL_BIN="${APPIMAGETOOL_BIN:-$(command -v appimagetool || true)}"

if [[ -z "${LINUXDEPLOYQT_BIN}" ]]; then
    echo "linuxdeployqt not found. Set LINUXDEPLOYQT_BIN or install it locally." >&2
    exit 1
fi

if [[ -z "${APPIMAGETOOL_BIN}" ]]; then
    echo "appimagetool not found. Set APPIMAGETOOL_BIN or install it locally." >&2
    exit 1
fi

if [[ -n "${QT_ROOT}" ]]; then
    export PATH="${QT_ROOT}/bin:${PATH}"
    export CMAKE_PREFIX_PATH="${QT_ROOT}:${CMAKE_PREFIX_PATH:-}"
    export LD_LIBRARY_PATH="${QT_ROOT}/lib:${LD_LIBRARY_PATH:-}"
fi

export VERSION="${VERSION:-0.1}"
export ARCH="${ARCH:-$(uname -m)}"
export QML_SOURCES_PATHS="${QML_SOURCES_PATHS:-${ROOT_DIR}/qml}"
export LANG="${LANG:-C.UTF-8}"
export LC_ALL="${LC_ALL:-C.UTF-8}"

qt_plugin_dir() {
    if [[ -n "${QT_ROOT}" && -d "${QT_ROOT}/plugins" ]]; then
        printf '%s\n' "${QT_ROOT}/plugins"
        return
    fi

    if command -v qmake >/dev/null 2>&1; then
        qmake -query QT_INSTALL_PLUGINS
    fi
}

MASKED_QT_SQLDRIVERS_DIR=""

mask_qt_sqldrivers() {
    local plugin_dir sqldrivers_dir masked_dir
    plugin_dir="$(qt_plugin_dir)"
    if [[ -z "${plugin_dir}" ]]; then
        return
    fi

    sqldrivers_dir="${plugin_dir}/sqldrivers"
    masked_dir="${plugin_dir}/sqldrivers.linuxdeployqt-disabled"
    if [[ ! -d "${sqldrivers_dir}" ]]; then
        return
    fi

    rm -rf "${masked_dir}"
    mv "${sqldrivers_dir}" "${masked_dir}"
    MASKED_QT_SQLDRIVERS_DIR="${masked_dir}"
}

restore_qt_sqldrivers() {
    if [[ -z "${MASKED_QT_SQLDRIVERS_DIR}" || ! -d "${MASKED_QT_SQLDRIVERS_DIR}" ]]; then
        return
    fi

    mv "${MASKED_QT_SQLDRIVERS_DIR}" "${MASKED_QT_SQLDRIVERS_DIR%.linuxdeployqt-disabled}"
    MASKED_QT_SQLDRIVERS_DIR=""
}

deploy_runtime() {
    "${LINUXDEPLOYQT_BIN}" "${APPDIR}/usr/bin/${APP_NAME}" \
        -executable="${APPDIR}/usr/bin/${APP_NAME}" \
        -bundle-non-qt-libs \
        -qmldir="${ROOT_DIR}/qml" \
        -unsupported-allow-new-glibc
}

prune_problematic_runtime_libs() {
    local lib_dir="${APPDIR}/usr/lib"
    local plugin_dir="${APPDIR}/usr/plugins"
    local patterns=(
        "libGL.so*"
        "libEGL.so*"
        "libGLX.so*"
        "libOpenGL.so*"
        "libGLdispatch.so*"
        "libdrm.so*"
        "libgbm.so*"
        "libva.so*"
        "libva-*.so*"
        "libvdpau.so*"
        "libvulkan.so*"
        "libX11.so*"
        "libX11-xcb.so*"
        "libXau.so*"
        "libXdmcp.so*"
        "libXext.so*"
        "libXfixes.so*"
        "libXv.so*"
        "libxcb.so*"
        "libxcb-*.so*"
        "libxkbcommon.so*"
        "libxkbcommon-x11.so*"
    )

    rm -rf "${plugin_dir}/sqldrivers"

    if [[ ! -d "${lib_dir}" ]]; then
        return
    fi

    echo "Pruning bundled system graphics/display libraries from AppDir:"
    for pattern in "${patterns[@]}"; do
        find "${lib_dir}" -maxdepth 1 -type f -name "${pattern}" -print -delete
    done
}

build_appimage() {
    (cd "${DIST_ROOT}" && "${APPIMAGETOOL_BIN}" "${APPDIR}")
}

rm -rf "${BUILD_DIR}" "${APPDIR}"
mkdir -p "${DIST_ROOT}"

cmake -S "${ROOT_DIR}" -B "${BUILD_DIR}" -G Ninja \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_INSTALL_PREFIX=/usr
cmake --build "${BUILD_DIR}" --parallel
DESTDIR="${APPDIR}" cmake --install "${BUILD_DIR}"

cat > "${APPDIR}/${APP_NAME}.desktop" <<DESKTOP
[Desktop Entry]
Type=Application
Name=${APP_DISPLAY_NAME}
Exec=${APP_NAME} %f
Icon=${APP_NAME}
Categories=AudioVideo;Player;Video;
MimeType=video/mp4;video/x-matroska;video/x-msvideo;video/quicktime;audio/mpeg;audio/flac;
Terminal=false
DESKTOP

mkdir -p "${APPDIR}/usr/share/applications"
cp "${APPDIR}/${APP_NAME}.desktop" "${APPDIR}/usr/share/applications/${APP_NAME}.desktop"

python3 - "${APPDIR}/${APP_NAME}.png" <<'PY'
import struct
import sys
import zlib

path = sys.argv[1]
width = height = 256

def rgba(x, y):
    t = (x + y) / (2 * (width - 1))
    r = int(15 + (110 - 15) * (1 - t))
    g = int(23 + (231 - 23) * (1 - t))
    b = int(42 + (249 - 42) * (1 - t))
    cx = x - width / 2
    cy = y - height / 2
    if 76 <= x <= 180 and 70 <= y <= 180:
        left = x >= 96
        upper = y >= -0.6 * (x - 96) + 76
        lower = y <= 0.6 * (x - 96) + 180
        if left and upper and lower:
            return (248, 250, 252, 235)
    if abs(y - 196) <= 7 and 54 <= x <= 202:
        return (224, 242, 254, 200)
    if abs(y - 196) <= 7 and 54 <= x <= 128:
        return (248, 250, 252, 255)
    if cx * cx + cy * cy > 126 * 126:
        return (0, 0, 0, 0)
    return (r, g, b, 255)

raw = bytearray()
for y in range(height):
    raw.append(0)
    for x in range(width):
        raw.extend(rgba(x, y))

def chunk(kind, data):
    return struct.pack(">I", len(data)) + kind + data + struct.pack(">I", zlib.crc32(kind + data) & 0xFFFFFFFF)

png = b"\x89PNG\r\n\x1a\n"
png += chunk(b"IHDR", struct.pack(">IIBBBBB", width, height, 8, 6, 0, 0, 0))
png += chunk(b"IDAT", zlib.compress(bytes(raw), 9))
png += chunk(b"IEND", b"")
with open(path, "wb") as f:
    f.write(png)
PY

pushd "${APPDIR}" >/dev/null
ln -sf "usr/bin/${APP_NAME}" AppRun
popd >/dev/null

pushd "${DIST_ROOT}" >/dev/null
rm -f "${APP_NAME}"*.AppImage "${APP_NAME}"*.zsync
trap restore_qt_sqldrivers EXIT
mask_qt_sqldrivers
deploy_runtime
prune_problematic_runtime_libs
build_appimage
restore_qt_sqldrivers
trap - EXIT
popd >/dev/null

echo "Linux package created."
find "${DIST_ROOT}" -maxdepth 1 -type f \( -name "*.AppImage" -o -name "*.zsync" \) -print
