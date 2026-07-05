# Copyright 2026 Warren Moore
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

IOS_VERSION_MIN = 18.0

ifdef DEVICE
SDK    = $(shell xcrun --sdk iphoneos --show-sdk-path)
TARGET = arm64-apple-ios$(IOS_VERSION_MIN)
ifeq ($(strip $(PROVISIONING_PROFILE)),)
$(error DEVICE builds require PROVISIONING_PROFILE=/path/to/your.mobileprovision)
endif
# Embed the profile and sign with the entitlements it carries. An empty
# CODESIGN_IDENTITY lets codesign pick, but that fails if several certs match.
CODESIGN_IDENTITY ?=
CODESIGN = cp $(PROVISIONING_PROFILE) $@/embedded.mobileprovision && \
	_ent=$$(mktemp) && \
	security cms -D -i $(PROVISIONING_PROFILE) | plutil -extract Entitlements xml1 -o $$_ent - && \
	codesign --force --timestamp=none --generate-entitlement-der \
	         --sign "$(CODESIGN_IDENTITY)" --entitlements $$_ent $@ && \
	rm -f $$_ent
else
SDK    = $(shell xcrun --sdk iphonesimulator --show-sdk-path)
TARGET = arm64-apple-ios$(IOS_VERSION_MIN)-simulator
# Simulator apps only need an ad-hoc signature.
CODESIGN = codesign --force --sign - $@
endif

STORYBOARD = learn-metal/ios-shared/Base.lproj/LaunchScreen.storyboard
INFO_PLIST = learn-metal/ios-shared/Info.plist

# Each app is identified by its source subdirectory. That name is reused for the
# bundle, executable, bundle identifier, and CFBundleName, so the built bundles
# sort in build/ the same way the sources sort on disk.
APPS := \
    00-window \
    01-primitive \
    02-argbuffers \
    03-animation \
    04-instancing \
    05-perspective \
    06-lighting \
    07-texturing \
    08-compute \
    09-compute-to-render \
    10-frame-debugging

# Human-readable display names (CFBundleDisplayName). These may contain spaces.
DISPLAY_NAME_00-window            = Window
DISPLAY_NAME_01-primitive         = Primitive
DISPLAY_NAME_02-argbuffers        = Arg Buffers
DISPLAY_NAME_03-animation         = Animation
DISPLAY_NAME_04-instancing        = Instancing
DISPLAY_NAME_05-perspective       = Perspective
DISPLAY_NAME_06-lighting          = Lighting
DISPLAY_NAME_07-texturing         = Texturing
DISPLAY_NAME_08-compute           = Compute
DISPLAY_NAME_09-compute-to-render = Compute Render
DISPLAY_NAME_10-frame-debugging   = Debugging

BUNDLES := $(foreach a,$(APPS),build/$a.app)
OBJECTS := $(foreach a,$(APPS),learn-metal/$a/$a.o)

ifdef DEBUG
DBG_OPT_FLAGS=-g
else
DBG_OPT_FLAGS=-O2
endif

ifdef ASAN
ASAN_FLAGS=-fsanitize=address
else
ASAN_FLAGS=
endif

CC=clang++
CFLAGS=-Wall -std=c++17 -I./metal-cpp -I./metal-cpp-extensions -fno-objc-arc -isysroot $(SDK) -target $(TARGET) -mios-version-min=$(IOS_VERSION_MIN) $(DBG_OPT_FLAGS) $(ASAN_FLAGS)
LDFLAGS=-framework Metal -framework Foundation -framework UIKit -framework CoreGraphics -framework MetalKit
VPATH=./metal-cpp
PLISTBUDDY = /usr/libexec/PlistBuddy

.PHONY: all clean
all: $(BUNDLES)

%.o: %.cpp
	$(CC) -c $(CFLAGS) $< -o $@

define build_app
build/$(1).app: learn-metal/$(1)/$(1).o Makefile
	# Lay down a fresh app bundle
	rm -rf $$@ && mkdir -p $$@
	# Copy Info.plist
	cp $(INFO_PLIST) $$@/Info.plist
	# Fix up Info.plist
	$(PLISTBUDDY) \
	  -c "Set :CFBundleExecutable $(1)" \
	  -c "Set :CFBundleIdentifier com.metalbyexample.learn-metal-cpp.$(1)" \
	  -c "Set :CFBundleName $(1)" \
	  -c "Set :CFBundleDisplayName $(DISPLAY_NAME_$(1))" $$@/Info.plist
	# Compile launch storyboard
	ibtool $(STORYBOARD) --compilation-directory $$@/Base.lproj
	# Build
	$(CC) $(CFLAGS) $(LDFLAGS) $$< -o $$@/$(1)
	# Code sign (ad-hoc for Simulator, real identity + profile for device)
	$$(CODESIGN)

.PHONY: $(1)
$(1): build/$(1).app
endef

$(foreach a,$(APPS),$(eval $(call build_app,$a)))

clean:
	rm -rf $(BUNDLES) $(OBJECTS)
