#!/bin/bash
# Quick script to test Linux builds locally using Docker

set -e

# Build the Docker image if it doesn't exist
if ! docker image inspect apex-linux-build >/dev/null 2>&1; then
	echo "Building Docker image..."
	docker build -f Dockerfile.linux-build -t apex-linux-build .
fi

# Run the build in the container
echo "Running Linux build in Docker..."
docker run --rm -v "$(pwd):/workspace" -w /workspace apex-linux-build bash -c "
    git submodule update --init --recursive
    rm -rf build-release
    make release-linux
"

echo "Build complete! Check the release/ directory."
