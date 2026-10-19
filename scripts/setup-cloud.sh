#!/usr/bin/env bash
# Debian cloud setup without root privileges; verified apt indexes remain enabled.
set -euo pipefail
setup_root=/workspace/toolchain
repo_root=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
mkdir -p "$setup_root/apt/lists/partial" "$setup_root/apt/cache/archives/partial" "$setup_root/sysroot" /workspace/deps
python3 -m pip install --no-cache-dir --upgrade --target "$setup_root/python" cmake==4.4.4
cat > "$setup_root/apt/sources.list" <<'SOURCES'
deb [signed-by=/usr/share/keyrings/debian-archive-keyring.gpg] https://deb.debian.org/debian trixie main
SOURCES
apt_options=(-o "Dir::Etc::sourcelist=$setup_root/apt/sources.list" -o Dir::Etc::sourceparts=- -o "Dir::State::lists=$setup_root/apt/lists")
apt-get "${apt_options[@]}" -o "Dir::Cache=$setup_root/apt/cache" update
download_dir=$(mktemp -d "$setup_root/apt/download.XXXXXX")
trap 'rm -rf -- "$download_dir"' EXIT
(
 cd "$download_dir"
 apt-get "${apt_options[@]}" download libfftw3-dev/trixie libfftw3-single3/trixie libasound2-dev/trixie libx11-dev/trixie libxext-dev/trixie libxinerama-dev/trixie libxrandr-dev/trixie libxcursor-dev/trixie libxrender-dev/trixie libxfixes-dev/trixie libfreetype-dev/trixie libfontconfig-dev/trixie libpng-dev/trixie libbrotli-dev/trixie libbz2-dev/trixie zlib1g-dev/trixie libxcb1-dev/trixie libxau-dev/trixie libxdmcp-dev/trixie x11proto-dev/trixie xorg-sgml-doctools/trixie xtrans-dev/trixie xvfb/trixie xauth/trixie xserver-common/trixie libxfont2/trixie libxkbfile1/trixie libfontenc1/trixie x11-xkb-utils/trixie
)
python3 - "$download_dir" <<'PY'
from pathlib import Path
import subprocess
import sys
root=Path('/workspace/toolchain/sysroot')
for archive in Path(sys.argv[1]).glob('*.deb'):
    subprocess.run(['dpkg-deb','-x',str(archive),str(root)],check=True)
for link in (root/'usr/lib/x86_64-linux-gnu').glob('*.so'):
    if link.is_symlink() and not link.exists():
        target=Path('/usr/lib/x86_64-linux-gnu')/link.readlink().name
        if target.exists():
            link.unlink()
            link.symlink_to(target)
PY
cat > "$setup_root/activate.sh" <<'ACTIVATE'
export PATH=/workspace/toolchain/python/cmake/data/bin:/workspace/toolchain/sysroot/usr/bin:$PATH
export PKG_CONFIG_PATH=/workspace/toolchain/sysroot/usr/lib/x86_64-linux-gnu/pkgconfig:/workspace/toolchain/sysroot/usr/share/pkgconfig
export PKG_CONFIG_SYSROOT_DIR=/workspace/toolchain/sysroot
export CPATH=/workspace/toolchain/sysroot/usr/include:/workspace/toolchain/sysroot/usr/include/freetype2
export LIBRARY_PATH=/workspace/toolchain/sysroot/usr/lib/x86_64-linux-gnu
export LD_LIBRARY_PATH=/workspace/toolchain/sysroot/usr/lib/x86_64-linux-gnu
export XDG_CACHE_HOME=/workspace/toolchain/cache
ACTIVATE
mkdir -p "$setup_root/cache"
juce_revision=51a8a6d7aeae7326956d747737ccf1575e61e209
if [[ ! -d /workspace/deps/JUCE/.git ]]; then
 git clone --depth 1 --branch 8.0.6 https://github.com/juce-framework/JUCE.git /workspace/deps/JUCE
fi
[[ $(git -C /workspace/deps/JUCE rev-parse HEAD) == "$juce_revision" ]] || { echo 'JUCE checkout revision mismatch' >&2; exit 1; }
git -C /workspace/deps/JUCE diff --quiet HEAD -- || { echo 'JUCE dependency has local modifications' >&2; exit 1; }
source "$setup_root/activate.sh"
cmake -S "$repo_root" -B "$repo_root/build" -G 'Unix Makefiles' -DCMAKE_BUILD_TYPE=Release -DFETCHCONTENT_SOURCE_DIR_JUCE=/workspace/deps/JUCE
cmake --build "$repo_root/build" --parallel 2
