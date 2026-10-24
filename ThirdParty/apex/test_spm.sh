#!/bin/bash
# Quick test script for Apex SPM integration
# This creates a temporary test project to verify the package works

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
TEMP_DIR=$(mktemp -d)
TEST_PROJECT="$TEMP_DIR/ApexSPMTest"

echo "Creating test project in: $TEST_PROJECT"
mkdir -p "$TEST_PROJECT/Sources/ApexSPMTest"

# Create Package.swift for test project
cat >"$TEST_PROJECT/Package.swift" <<'EOF'
// swift-tools-version:5.7
import PackageDescription

let package = Package(
    name: "ApexSPMTest",
    platforms: [.macOS(.v10_13)],
    dependencies: [
        .package(path: "../apex")
    ],
    targets: [
        .executableTarget(
            name: "ApexSPMTest",
            dependencies: [.product(name: "Apex", package: "apex")]
        )
    ]
)
EOF

# Create test main.swift
cat >"$TEST_PROJECT/Sources/ApexSPMTest/main.swift" <<'EOF'
import Foundation
import Apex

print("Testing Apex Swift Package Manager Integration")
print(String(repeating: "=", count: 50))

// Test 1: Basic conversion
print("\n1. Basic conversion (unified mode):")
let markdown1 = "# Hello World\n\nThis is a test."
let html1 = markdown1.apexHTML()
print("Input: \(markdown1)")
print("Output: \(html1.prefix(100))...")

// Test 2: GFM mode
print("\n2. GFM mode:")
let markdown2 = "# Title\n\n- [ ] Task 1\n- [x] Task 2"
let html2 = markdown2.apexHTML(mode: .gfm)
print("Input: \(markdown2)")
print("Output: \(html2.prefix(100))...")

// Test 3: With options
print("\n3. With options (pretty print):")
var options = ApexOptions()
options.pretty = true
options.generateHeaderIDs = true
let html3 = markdown1.apexHTML(mode: .unified, options: options)
print("Output length: \(html3.count) characters")

// Test 4: Static converter
print("\n4. Static converter:")
let html4 = Apex.convert(markdown1, mode: .gfm)
print("Output length: \(html4.count) characters")

// Test 5: Standalone document
print("\n5. Standalone document:")
let html5 = markdown1.apexHTML(
    mode: .unified,
    standalone: true,
    stylesheet: nil,
    title: "Test Document"
)
print("Output length: \(html5.count) characters")
print("Has <html> tag: \(html5.contains("<html>"))")
print("Has <title> tag: \(html5.contains("<title>"))")

print("\n" + String(repeating: "=", count: 50))
print("✅ All tests completed successfully!")
EOF

# Create Sources directory first
mkdir -p "$TEST_PROJECT/Sources/ApexSPMTest"

# Update the path in Package.swift to point to the actual apex directory
sed -i '' "s|path: \"../apex\"|path: \"$SCRIPT_DIR\"|g" "$TEST_PROJECT/Package.swift"

echo "Cleaning Apex package build cache for fresh build..."
cd "$SCRIPT_DIR"
rm -rf .build 2>/dev/null || true

echo "Building test project (clean build)..."
cd "$TEST_PROJECT"
# Clean any existing build artifacts in test project
rm -rf .build 2>/dev/null || true
swift build

echo "Running test..."
swift run ApexSPMTest

echo ""
echo "✅ SPM integration test passed!"
echo "Cleaning up..."
rm -rf "$TEMP_DIR"
