#!/usr/bin/env bash
# Builds libs/embree/lib/linux/64 (x86_64), linux/arm64 and linux/aarch64
# (64-bit ARM; one build for both: generic and Raspberry Pi) in
# Docker: Embree static libraries, -fPIC (apps link as PIE), with Embree's
# internal task system (no TBB). Built on Ubuntu 22.04 (glibc 2.35).
#
#   scripts/build_embree_linux.sh            # both
#   scripts/build_embree_linux.sh arm64      # or amd64
#
# x86_64 has the SSE2 and AVX2 code paths (Embree picks at runtime), aarch64 NEON.
set -euo pipefail

EMBREE_VERSION=v4.4.1
ADDON=$(cd "$(dirname "$0")/.." && pwd)

for arch in ${*:-amd64 arm64}; do
	case $arch in
		amd64) dirs="linux/64"; isa="-DEMBREE_MAX_ISA=NONE -DEMBREE_ISA_SSE2=ON -DEMBREE_ISA_SSE42=OFF -DEMBREE_ISA_AVX=OFF -DEMBREE_ISA_AVX2=ON -DEMBREE_ISA_AVX512=OFF" ;;
		arm64) dirs="linux/arm64 linux/aarch64"; isa="" ;;
		*) echo "unknown arch: $arch (amd64 or arm64)" >&2; exit 1 ;;
	esac
	echo "Embree $EMBREE_VERSION: linux/$arch -> libs/embree/lib/{${dirs// /,}}"
	docker run --rm --platform "linux/$arch" -v "$ADDON:/addon" ubuntu:22.04 bash -euo pipefail -c "
		export DEBIAN_FRONTEND=noninteractive
		apt-get -o Acquire::Retries=5 update -qq && apt-get -o Acquire::Retries=5 install -y -qq git cmake g++ make > /dev/null
		cd /tmp && git -c advice.detachedHead=false clone -q --depth 1 --branch $EMBREE_VERSION https://github.com/RenderKit/embree src
		cmake -S src -B build -DCMAKE_BUILD_TYPE=Release -DCMAKE_POSITION_INDEPENDENT_CODE=ON \
			-DEMBREE_STATIC_LIB=ON -DEMBREE_TASKING_SYSTEM=INTERNAL -DEMBREE_ISPC_SUPPORT=OFF \
			-DEMBREE_TUTORIALS=OFF -DEMBREE_INSTALL_DEPENDENCIES=OFF $isa \
			-DCMAKE_INSTALL_PREFIX=/tmp/install -DCMAKE_INSTALL_LIBDIR=lib > /tmp/cmake.log 2>&1 || { tail -20 /tmp/cmake.log; exit 1; }
		cmake --build build -j\$(nproc) > /tmp/build.log 2>&1 || { grep -m10 error /tmp/build.log; exit 1; }
		cmake --install build > /dev/null
		for dir in $dirs; do
			rm -rf /addon/libs/embree/lib/\$dir && mkdir -p /addon/libs/embree/lib/\$dir
			cp /tmp/install/lib/*.a /addon/libs/embree/lib/\$dir/
		done
		echo '  link interface:' \$(grep -h -o 'INTERFACE_LINK_LIBRARIES \"[^\"]*\"' /tmp/install/lib/cmake/embree-*/embree-targets.cmake | head -3)
		ls -la /addon/libs/embree/lib/${dirs%% *} | awk 'NR>1{print \"  \", \$5, \$9}'
		chown -R \$(stat -c %u:%g /addon) /addon/libs/embree/lib/linux
	"
done
