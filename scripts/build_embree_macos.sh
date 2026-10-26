#!/usr/bin/env bash
# Builds libs/embree for macOS: Embree static libraries, universal (arm64 +
# x86_64, macOS 10.15+), with Embree's internal task system (no TBB), plus the
# headers and license. Needs Xcode command line tools, CMake and Ninja.
#
#   scripts/build_embree_macos.sh
#
# Embree's static library allows one code path (ISA) per slice with Apple clang:
# arm64 uses double-pumped NEON (Embree's default on Apple Silicon), x86_64 AVX2
# (Intel Macs from 2013 on).
set -euo pipefail

EMBREE_VERSION=v4.4.1
ADDON=$(cd "$(dirname "$0")/.." && pwd)
WORK=$(mktemp -d)
trap 'rm -rf "$WORK"' EXIT

git -c advice.detachedHead=false clone -q --depth 1 --branch $EMBREE_VERSION https://github.com/RenderKit/embree "$WORK/src"

for arch in arm64 x86_64; do
	echo "Embree $EMBREE_VERSION: macOS $arch"
	isa=()
	if [[ $arch == x86_64 ]]; then
		isa=(-DEMBREE_MAX_ISA=NONE -DEMBREE_ISA_SSE2=OFF -DEMBREE_ISA_SSE42=OFF -DEMBREE_ISA_AVX=OFF -DEMBREE_ISA_AVX2=ON -DEMBREE_ISA_AVX512=OFF)
	fi
	cmake -S "$WORK/src" -B "$WORK/build-$arch" -G Ninja -DCMAKE_BUILD_TYPE=Release \
		-DCMAKE_POLICY_VERSION_MINIMUM=3.5 -DCMAKE_OSX_ARCHITECTURES=$arch -DCMAKE_OSX_DEPLOYMENT_TARGET=10.15 \
		-DEMBREE_STATIC_LIB=ON -DEMBREE_TASKING_SYSTEM=INTERNAL -DEMBREE_ISPC_SUPPORT=OFF \
		-DEMBREE_TUTORIALS=OFF -DEMBREE_INSTALL_DEPENDENCIES=OFF "${isa[@]}" \
		-DCMAKE_INSTALL_PREFIX="$WORK/install-$arch" > "$WORK/$arch.log" 2>&1 || { tail -20 "$WORK/$arch.log"; exit 1; }
	cmake --build "$WORK/build-$arch" > "$WORK/$arch.log" 2>&1 || { grep -m10 error "$WORK/$arch.log"; exit 1; }
	cmake --install "$WORK/build-$arch" > /dev/null
done

out="$ADDON/libs/embree/lib/osx"
rm -rf "$out" && mkdir -p "$out"
for lib in "$WORK"/install-arm64/lib/*.a; do
	name=$(basename "$lib")
	[[ -f "$WORK/install-x86_64/lib/$name" ]] || { echo "x86_64 has no $name" >&2; exit 1; }
	lipo -create "$lib" "$WORK/install-x86_64/lib/$name" -output "$out/$name"
done
rm -rf "$ADDON/libs/embree/include"
cp -R "$WORK/install-arm64/include" "$ADDON/libs/embree/include"
cp "$WORK/src/LICENSE.txt" "$ADDON/libs/embree/license/LICENSE.txt"
cp "$WORK/src/third-party-programs.txt" "$ADDON/libs/embree/license/" 2>/dev/null || true
for f in "$out"/*.a; do echo "$(basename "$f"): $(lipo -archs "$f")"; done
du -sh "$out"
