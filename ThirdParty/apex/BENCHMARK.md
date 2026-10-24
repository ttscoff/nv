# Apex Markdown Processor - Performance Benchmark

## Test Document

- **File:** `/Users/ttscoff/Desktop/Code/apex/tests/fixtures/comprehensive_test.md`
- **Lines:**      619
- **Words:**     2581
- **Size:**    17008 bytes

## Output Modes

| Mode | Iterations | Average (ms) | Min (ms) | Max (ms) | Throughput (words/sec) |
|------|------------|--------------|---------|---------|------------------------|
| Fragment Mode (default HTML output) | 50 | 21 | 20 | 27 | 129050.00 |
| Pretty-Print Mode (formatted HTML) | 50 | 22 | 21 | 26 | 129050.00 |
| Standalone Mode (complete HTML document) | 50 | 21 | 20 | 24 | 129050.00 |
| Standalone + Pretty (full features) | 50 | 22 | 20 | 25 | 129050.00 |

## Mode Comparison

| Mode | Iterations | Average (ms) | Min (ms) | Max (ms) | Throughput (words/sec) |
|------|------------|--------------|---------|---------|------------------------|
| CommonMark Mode (minimal, spec-compliant) | 50 | 12 | 10 | 35 | 258100.00 |
| GFM Mode (GitHub Flavored Markdown) | 50 | 20 | 19 | 22 | 129050.00 |
| MultiMarkdown Mode (metadata, footnotes, tables) | 50 | 21 | 20 | 23 | 129050.00 |
| Kramdown Mode (attributes, definition lists) | 50 | 20 | 19 | 22 | 129050.00 |
| Unified Mode (all features enabled) | 50 | 21 | 20 | 24 | 129050.00 |
| Default Mode (unified, all features) | 50 | 21 | 20 | 24 | 129050.00 |

---

*Benchmark Complete*
