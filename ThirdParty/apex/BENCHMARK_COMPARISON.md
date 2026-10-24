# Markdown Processor Comparison Benchmark

## Available Tools

Found 7 tools:
- apex
- cmark-gfm
- cmark
- pandoc
- multimarkdown
- kramdown
- marked

## Processor Comparison

**File:** `/Users/ttscoff/Desktop/Code/apex/tests/fixtures/comprehensive_test.md` (17008 bytes, 619 lines)

| Processor | Time (ms) | Relative |
|-----------|-----------|----------|
| apex | 14.57 | 1.00x |
| cmark-gfm | 3.13 | .21x |
| cmark | 2.39 | .16x |
| pandoc | 91.13 | 6.25x |
| multimarkdown | 2.31 | .15x |
| kramdown | 317.27 | 21.77x |
| marked | 86.59 | 5.94x |

## Processor Comparison

**File:** `/Users/ttscoff/Desktop/Code/apex/tests/fixtures/large_doc.md` (29275 bytes, 1094 lines)

| Processor | Time (ms) | Relative |
|-----------|-----------|----------|
| apex | 9.93 | 1.00x |
| cmark-gfm | 2.32 | .23x |
| cmark | 2.94 | .29x |
| pandoc | 114.57 | 11.53x |
| multimarkdown | 2.78 | .27x |
| kramdown | 319.61 | 32.18x |
| marked | 85.88 | 8.64x |

## Apex Mode Comparison

**Test File:** `/Users/ttscoff/Desktop/Code/apex/tests/fixtures/comprehensive_test.md`

| Mode | Time (ms) | Relative |
|------|-----------|----------|
| commonmark | 2.98 | 1.00x |
| gfm | 14.01 | 4.69x |
| mmd | 13.83 | 4.63x |
| kramdown | 12.09 | 4.05x |
| unified | 14.00 | 4.69x |
| default (unified) | 15.67 | 5.25x |

## Apex Feature Overhead

| Features | Time (ms) |
|----------|-----------|
| CommonMark (minimal) | 3.07 |
| + GFM tables/strikethrough | 13.78 |
| + All Apex features | 15.34 |
| + Pretty printing | 15.93 |
| + Standalone document | 16.08 |

---

*Benchmark Complete*
