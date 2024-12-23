#!/bin/bash

# Assuming the script is intended to run in a Unix-like environment

# Run cmake if the directory is set correctly
"${PS5_PAYLOAD_SDK}/bin/prospero-cmake"
if ! "${PS5_PAYLOAD_SDK}/bin/prospero-cmake" -S . -B .; then
    echo "Failed to run cmake. Check if PS5_PAYLOAD_SDK is set correctly and cmake is installed."
    exit 1
fi

make -j30
