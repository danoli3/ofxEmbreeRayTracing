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

# Linux: static, -fPIC, no TBB (scripts/build_embree_linux.sh), in Embree's link
# order; the GNU linker reads archives in order, so the ISA library is followed
# by libembree4.a again for the core symbols it uses
linux64:
	ADDON_LIBS = libs/embree/lib/linux64/libembree4.a
	ADDON_LIBS += libs/embree/lib/linux64/libembree_avx2.a
	ADDON_LIBS += libs/embree/lib/linux64/libembree4.a
	ADDON_LIBS += libs/embree/lib/linux64/libsys.a
	ADDON_LIBS += libs/embree/lib/linux64/libmath.a
	ADDON_LIBS += libs/embree/lib/linux64/libsimd.a
	ADDON_LIBS += libs/embree/lib/linux64/liblexers.a
	ADDON_LIBS += libs/embree/lib/linux64/libtasking.a
	ADDON_LDFLAGS = -lpthread -ldl

linuxaarch64:
	ADDON_LIBS = libs/embree/lib/linuxaarch64/libembree4.a
	ADDON_LIBS += libs/embree/lib/linuxaarch64/libsys.a
	ADDON_LIBS += libs/embree/lib/linuxaarch64/libmath.a
	ADDON_LIBS += libs/embree/lib/linuxaarch64/libsimd.a
	ADDON_LIBS += libs/embree/lib/linuxaarch64/liblexers.a
	ADDON_LIBS += libs/embree/lib/linuxaarch64/libtasking.a
	ADDON_LDFLAGS = -lpthread -ldl
