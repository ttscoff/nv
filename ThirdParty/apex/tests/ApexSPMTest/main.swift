#!/usr/bin/env swift
// Test script for Apex Swift Package Manager integration

import Foundation
import Apex

print("Testing Apex Swift Package Manager Integration")
print(String(repeating: "=", count: 50))

// Test 1: Basic conversion
print("\n1. Basic conversion (unified mode):")
let markdown1 = "# Hello World\n\nThis is a test."
let html1 = markdown1.apexHTML()
print("Input: \(markdown1)")
print("Output: \(html1)")

// Test 2: GFM mode
print("\n2. GFM mode:")
let markdown2 = "# Title\n\n- [ ] Task 1\n- [x] Task 2"
let html2 = markdown2.apexHTML(mode: .gfm)
print("Input: \(markdown2)")
print("Output: \(html2)")

// Test 3: With options
print("\n3. With options (pretty print):")
var options = ApexOptions()
options.pretty = true
options.generateHeaderIDs = true
let html3 = markdown1.apexHTML(mode: .unified, options: options)
print("Output (pretty): \(html3)")

// Test 4: Static converter
print("\n4. Static converter:")
let html4 = Apex.convert(markdown1, mode: .gfm)
print("Output: \(html4)")

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
print("All tests completed!")
