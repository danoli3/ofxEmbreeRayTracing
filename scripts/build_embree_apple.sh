#!/usr/bin/env bash
# Builds libs/embree/lib/macos/embree.xcframework: Embree static, internal task
# system (no TBB), each slice one libembree.a, plus the headers and license.
# Needs Xcode, CMake and Ninja.
#
#   macos-arm64_x86_64          macOS 10.15+: arm64 NEON, x86_64 AVX2
#   ios-arm64                   iOS 15+: arm64 NEON
#   ios-arm64_x86_64-simulator  iOS Simulator 15+: arm64 NEON, x86_64 AVX2
#
#   scripts/build_embree_apple.sh
#
# Embree's static library allows one code path (ISA) per architecture with
# Apple clang: double-pumped NEON on arm64 (Embree's default on Apple Silicon),
# AVX2 on x86_64 (Intel Macs from 2013 on). Embree doesn't target iOS
# officially: its CMake files are told the CPU is aarch64 (otherwise they pick
# x86 code paths), and its hard-coded -mmacosx-version-min=10.7 is dropped.
set -euo pipefail

EMBREE_VERSION=v4.4.1
ADDON=$(cd "$(dirname "$0")/.." && pwd)
WORK=$(mktemp -d)
trap 'rm -rf "$WORK"' EXIT

git -c advice.detachedHead=false clone -q --depth 1 --branch $EMBREE_VERSION https://github.com/RenderKit/embree "$WORK/src"
sed -i '' '/-mmacosx-version-min=10.7/d' "$WORK/src/common/cmake/clang.cmake"

NEON=(-DCMAKE_SYSTEM_PROCESSOR=aarch64 -DEMBREE_MAX_ISA=NONE -DEMBREE_ISA_NEON=OFF -DEMBREE_ISA_NEON2X=ON)
AVX2=(-DEMBREE_MAX_ISA=NONE -DEMBREE_ISA_SSE2=OFF -DEMBREE_ISA_SSE42=OFF -DEMBREE_ISA_AVX=OFF -DEMBREE_ISA_AVX2=ON -DEMBREE_ISA_AVX512=OFF)
MACOS=(-DCMAKE_OSX_DEPLOYMENT_TARGET=10.15)
IOS=(-DCMAKE_SYSTEM_NAME=iOS -DCMAKE_OSX_DEPLOYMENT_TARGET=15.0 -DCMAKE_OSX_SYSROOT=iphoneos)
SIM=(-DCMAKE_SYSTEM_NAME=iOS -DCMAKE_OSX_DEPLOYMENT_TARGET=15.0 -DCMAKE_OSX_SYSROOT=iphonesimulator)

# name arch cmake-options...
build() {
	local name=$1 arch=$2; shift 2
	echo "Embree $EMBREE_VERSION: $name"
	cmake -S "$WORK/src" -B "$WORK/build-$name" -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_POLICY_VERSION_MINIMUM=3.5 \
		-DCMAKE_OSX_ARCHITECTURES=$arch "$@" \
		-DEMBREE_STATIC_LIB=ON -DEMBREE_TASKING_SYSTEM=INTERNAL -DEMBREE_ISPC_SUPPORT=OFF \
		-DEMBREE_TUTORIALS=OFF -DEMBREE_INSTALL_DEPENDENCIES=OFF \
		-DCMAKE_INSTALL_PREFIX="$WORK/install-$name" > "$WORK/$name.log" 2>&1 || { tail -20 "$WORK/$name.log"; exit 1; }
	cmake --build "$WORK/build-$name" > "$WORK/$name.log" 2>&1 || { grep -m10 error "$WORK/$name.log"; exit 1; }
	cmake --install "$WORK/build-$name" > /dev/null
	merge "$WORK/install-$name"
}

# One libembree.a from Embree's libraries. On Apple, Embree's lowest ISA is the
# same as its ISA library's (NEON2X or AVX2), so most objects of
# libembree_avx2.a repeat ones in libembree4.a; only those defining something
# new are kept, or the linker can pull both copies (duplicate symbols).
merge() {
	local lib=$1/lib objects=$1/objects
	mkdir -p "$objects"
	nm -g --defined-only "$lib/libembree4.a" 2> /dev/null | awk 'NF==3{print $3}' | sort -u > "$1/core.symbols"
	if [[ -f $lib/libembree_avx2.a ]]; then
		(cd "$objects" && ar x "$lib/libembree_avx2.a")
		for object in "$objects"/*.o; do
			local new
			new=$(nm -g --defined-only "$object" 2> /dev/null | awk 'NF==3{print $3}' | sort -u | comm -23 - "$1/core.symbols")
			[[ -n $new ]] || rm "$object"
		done
	fi
	libtool -static -o "$1/libembree.a" "$lib/libembree4.a" "$objects"/*.o "$lib/libsys.a" "$lib/libmath.a" "$lib/libsimd.a" \
		"$lib/liblexers.a" "$lib/libtasking.a" 2> /dev/null
}
build macos-arm64 arm64 "${MACOS[@]}" "${NEON[@]}"
build macos-x86_64 x86_64 "${MACOS[@]}" "${AVX2[@]}"
build ios-arm64 arm64 "${IOS[@]}" "${NEON[@]}"
build sim-arm64 arm64 "${SIM[@]}" "${NEON[@]}"
build sim-x86_64 x86_64 "${SIM[@]}" "${AVX2[@]}"

for platform in macos sim; do
	mkdir -p "$WORK/$platform"
	lipo -create "$WORK/install-$platform-arm64/libembree.a" "$WORK/install-$platform-x86_64/libembree.a" -output "$WORK/$platform/libembree.a"
done
out="$ADDON/libs/embree/lib/macos/embree.xcframework"
rm -rf "$out" && mkdir -p "$(dirname "$out")"
xcodebuild -create-xcframework -library "$WORK/macos/libembree.a" -library "$WORK/install-ios-arm64/libembree.a" \
	-library "$WORK/sim/libembree.a" -output "$out" > /dev/null

rm -rf "$ADDON/libs/embree/include"
cp -R "$WORK/install-macos-arm64/include" "$ADDON/libs/embree/include"
cp "$WORK/src/LICENSE.txt" "$ADDON/libs/embree/license/LICENSE.txt"
cp "$WORK/src/third-party-programs.txt" "$ADDON/libs/embree/license/" 2>/dev/null || true
# check: each slice links a program that traces a ray (no missing or duplicate symbols)
cat > "$WORK/check.cpp" << 'CPP'
#include <embree4/rtcore.h>
int main() {
	RTCDevice device = rtcNewDevice(nullptr);
	RTCScene scene = rtcNewScene(device);
	rtcCommitScene(scene);
	RTCRayHit rayhit {};
	rtcIntersect1(scene, &rayhit);
	rtcReleaseScene(scene);
	rtcReleaseDevice(device);
}
CPP
check() {
	xcrun --sdk $1 clang++ -std=c++17 -target $2 -I"$ADDON/libs/embree/include" "$WORK/check.cpp" "$out/$3/libembree.a" \
		-o "$WORK/check" > "$WORK/check.log" 2>&1 || { echo "$2 doesn't link:"; grep -m10 -E "Undefined|duplicate|\"" "$WORK/check.log"; exit 1; }
}
check macosx arm64-apple-macos10.15 macos-arm64_x86_64
check macosx x86_64-apple-macos10.15 macos-arm64_x86_64
check iphoneos arm64-apple-ios15.0 ios-arm64
check iphonesimulator arm64-apple-ios15.0-simulator ios-arm64_x86_64-simulator
check iphonesimulator x86_64-apple-ios15.0-simulator ios-arm64_x86_64-simulator
for slice in "$out"/*/; do echo "$(basename "$slice"): $(lipo -archs "$slice/libembree.a"), $(du -h "$slice/libembree.a" | cut -f1)"; done
