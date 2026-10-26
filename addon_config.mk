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
	# the macOS slice of embree.xcframework: universal (arm64 + x86_64), 10.15+
	# (scripts/build_embree_apple.sh)
	ADDON_LIBS = libs/embree/lib/macos/embree.xcframework/macos-arm64_x86_64/libembree.a

ios:
	# iOS 15+ and the Simulator
	ADDON_LIBS =
	ADDON_XCFRAMEWORKS = libs/embree/lib/macos/embree.xcframework

# Linux (openFrameworks 0.12.2+ layout: lib/linux/<arch>): static, -fPIC, no TBB,
# Embree's libraries merged into one libembree.a (scripts/build_embree_linux.sh):
# its core and its AVX2 code refer to each other, which the GNU linker only
# resolves inside one archive
linux/64:
	ADDON_LIBS = libs/embree/lib/linux/64/libembree.a
	ADDON_LDFLAGS = -lpthread -ldl

# 64-bit ARM: generic (servers, desktops, Jetson)
linux/arm64:
	ADDON_LIBS = libs/embree/lib/linux/arm64/libembree.a
	ADDON_LDFLAGS = -lpthread -ldl

# 64-bit Raspberry Pi OS (the same build)
linux/aarch64:
	ADDON_LIBS = libs/embree/lib/linux/aarch64/libembree.a
	ADDON_LDFLAGS = -lpthread -ldl
