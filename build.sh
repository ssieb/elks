#!/bin/bash

# Full build (including the cross tool chain)

# Arguments:
#   - 'auto' : continuous integration context

SCRIPTDIR="$(dirname "$0")"

clean_exit () {
	E="$1"
	test -z $1 && E=0
	if [ $E -eq 0 ]
		then echo "Build script has terminated successfully."
		else echo "Build script has terminated with error $E"
	fi
	exit $E
}

# Environment setup

. "$SCRIPTDIR/env.sh"
[ $? -ne 0 ] && clean_exit 1

# Build cross tools if not already

if [ "$1" != "auto" ]; then
	mkdir -p "$CROSSDIR"
	tools/build.sh || clean_exit 1
fi

# Configure all

if [ "$1" = "auto" ]; then
	echo "Invoking 'make defconfig'..."
	make defconfig || clean_exit 2
else
	echo
	echo "Now invoking 'make menuconfig' for you to configure the system."
	echo "The defaults should be OK for many systems, but you may want to review them."
	echo -n "Press ENTER to continue..."
	read
	make menuconfig || clean_exit 2
fi

test -e .config || clean_exit 3

# Clean kernel, user land and image

if [ "$1" != "auto" ]; then
	echo "Cleaning all..."
	make clean || clean_exit 4
	fi

# Build kernel, user land and image
# Forcing single threaded build because of dirty dependencies (see #273)

echo "Building all..."
make -j1 all || clean_exit 5

# Success

echo "Target image is in 'image' folder."
clean_exit 0
