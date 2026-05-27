# Makefile for GravityGPU KEXT
# Note: Actual KEXT builds in macOS require Xcode/xcodebuild. This is a reference.

XCODEBUILD = xcodebuild

all:
	$(XCODEBUILD) -configuration Release -target GravityGPU SYMROOT=build OBJROOT=build

clean:
	$(XCODEBUILD) -configuration Release -target GravityGPU clean SYMROOT=build OBJROOT=build
	rm -rf build/

load: all
	sudo chown -R root:wheel build/Release/GravityGPU.kext
	sudo kextload build/Release/GravityGPU.kext

unload:
	sudo kextunload build/Release/GravityGPU.kext
