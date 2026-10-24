# GFM Header ID Generation Comparison

This document summarizes the differences between various
tools for generating GFM-compliant header IDs.

## Tools Tested

- **Pandoc**: General-purpose document converter
- **Comrak**: Rust-based GFM parser (likely most accurate)
- **Marked (JavaScript)**: JavaScript markdown parser with

  gfm-heading-id plugin

- **Apex**: Our implementation

## Key Differences

### 1. Multiple Spaces
**Comrak/Marked**: Convert to multiple dashes (`multiple---spaces---here`)

**Pandoc/Apex**: Collapse to single dash (`multiple-spaces-here`)

### 2. Underscores
**Comrak/Marked/Pandoc**: Preserve underscores (`heading_with_underscore`)

- **Apex**: Remove underscores (`headingwithunderscore`)

### 3. Em/En Dashes
- **Comrak/Marked/Pandoc**: Convert to double dashes

  (`em-dash--test`)

- **Apex**: Remove dashes (`em-dash-test`)

### 4. Diacritics
**Comrak/Marked/Pandoc**: Preserve diacritics (`diacritics-émoji-support`)

- **Apex**: Convert to ASCII (`diacritics-amoji-support`)

### 5. Non-Latin Characters
- **Comrak/Marked/Pandoc**: Preserve characters

  (`cyrillic-привет`)

- **Apex**: Convert to placeholders (`cyrillic-nn`)

### 6. Trailing Dashes
- **Comrak/Marked/Pandoc**: Preserve trailing dashes

  (`trailing-dash-test-`)

- **Apex**: Trim trailing dashes (`trailing-dash-test`)

### 7. Trailing Punctuation
- **Comrak/Marked/Pandoc**: Preserve trailing punctuation

  (`special-characters-`)

**Apex**: Remove trailing punctuation (`special-characters`)

### 8. Special Characters Only
- **Comrak**: Generates empty ID for `!@#$%^&*()`
- **Others**: Generate some ID

## Recommendations

Based on the comparison, **Comrak** and **Marked** appear to
follow GFM rules most closely and produce identical results
for most cases. To match GFM exactly, we should:

1. **Preserve underscores** (don't remove them)

**Convert em/en dashes to double dashes** (not remove them)

3. **Preserve diacritics** (don't convert to ASCII)
4. **Preserve non-Latin characters** (don't convert to

   placeholders)

5. **Preserve trailing dashes** (don't trim them)
6. **Preserve trailing punctuation** (don't remove it)
7. **Handle multiple spaces** - need to verify GFM behavior

   (Comrak/Marked use multiple dashes)

## Running the Comparison

Run the comparison script:
```bash
./tests/generate_gfm_ids.sh

```

This will show side-by-side comparison of all available
tools.

