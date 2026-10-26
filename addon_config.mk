meta:
	ADDON_NAME = ofxEmbreeRayTracing
	ADDON_DESCRIPTION = Ray tracing on Intel Embree 4: ray / mesh queries, mouse picking and a progressive CPU ray tracer
	ADDON_AUTHOR = Dan Rosser
	ADDON_TAGS = "ray tracing" "embree" "rendering" "picking"
	ADDON_URL = https://github.com/danoli3/ofxEmbreeRayTracing

common:
	# Only libs/embree/include: Embree's headers are included as <embree4/rtcore.h>
	ADDON_INCLUDES = libs/embree/include
	ADDON_INCLUDES += src
	ADDON_INCLUDES_EXCLUDE = libs/embree/include/embree4

osx:
	# Embree static, universal (arm64 + x86_64), no TBB (scripts/build_embree_macos.sh)
	ADDON_LIBS = libs/embree/lib/osx/libembree4.a
	ADDON_LIBS += libs/embree/lib/osx/libembree_avx2.a
	ADDON_LIBS += libs/embree/lib/osx/libsys.a
	ADDON_LIBS += libs/embree/lib/osx/libmath.a
	ADDON_LIBS += libs/embree/lib/osx/libsimd.a
	ADDON_LIBS += libs/embree/lib/osx/liblexers.a
	ADDON_LIBS += libs/embree/lib/osx/libtasking.a

# Linux (openFrameworks 0.12.2+ layout: lib/linux/<arch>): static, -fPIC, no TBB
# (scripts/build_embree_linux.sh), in Embree's link order; the GNU linker reads
# archives in order, so the ISA library is followed by libembree4.a again for
# the core symbols it uses
linux/64:
	ADDON_LIBS = libs/embree/lib/linux/64/libembree4.a
	ADDON_LIBS += libs/embree/lib/linux/64/libembree_avx2.a
	ADDON_LIBS += libs/embree/lib/linux/64/libembree4.a
	ADDON_LIBS += libs/embree/lib/linux/64/libsys.a
	ADDON_LIBS += libs/embree/lib/linux/64/libmath.a
	ADDON_LIBS += libs/embree/lib/linux/64/libsimd.a
	ADDON_LIBS += libs/embree/lib/linux/64/liblexers.a
	ADDON_LIBS += libs/embree/lib/linux/64/libtasking.a
	ADDON_LDFLAGS = -lpthread -ldl

# 64-bit ARM: generic (servers, desktops, Jetson)
linux/arm64:
	ADDON_LIBS = libs/embree/lib/linux/arm64/libembree4.a
	ADDON_LIBS += libs/embree/lib/linux/arm64/libsys.a
	ADDON_LIBS += libs/embree/lib/linux/arm64/libmath.a
	ADDON_LIBS += libs/embree/lib/linux/arm64/libsimd.a
	ADDON_LIBS += libs/embree/lib/linux/arm64/liblexers.a
	ADDON_LIBS += libs/embree/lib/linux/arm64/libtasking.a
	ADDON_LDFLAGS = -lpthread -ldl

# 64-bit Raspberry Pi OS (the same build)
linux/aarch64:
	ADDON_LIBS = libs/embree/lib/linux/aarch64/libembree4.a
	ADDON_LIBS += libs/embree/lib/linux/aarch64/libsys.a
	ADDON_LIBS += libs/embree/lib/linux/aarch64/libmath.a
	ADDON_LIBS += libs/embree/lib/linux/aarch64/libsimd.a
	ADDON_LIBS += libs/embree/lib/linux/aarch64/liblexers.a
	ADDON_LIBS += libs/embree/lib/linux/aarch64/libtasking.a
	ADDON_LDFLAGS = -lpthread -ldl
