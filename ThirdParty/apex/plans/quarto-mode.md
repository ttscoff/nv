# Plan: `--mode quarto` for Apex

## Goal

Add a **Quarto/Pandoc markdown compatibility mode** optimized for HTML rendering — not a Quarto runtime. Users should be able to point Apex at `.qmd`-style documents and get sensible HTML without running the full Quarto toolchain.

**In scope:** Pandoc markdown syntax from [Quarto Markdown Basics](https://quarto.org/docs/authoring/markdown-basics.html)  
**Out of scope:** cell execution, cross-refs (`@fig-…`), CSL/bibliography pipeline, multi-format output (PDF/Typst/Word), project/site rendering, conditional content, computed shortcodes

---

## Design principle

**Quarto mode = unified + Pandoc preprocessors + Quarto defaults**

Rather than a from-scratch mode, inherit `APEX_MODE_UNIFIED` and layer Quarto-specific preprocessing behind a master flag. That keeps maintenance manageable and avoids duplicating unified’s feature matrix.

```c
typedef enum {
    /* ... existing ... */
    APEX_MODE_UNIFIED = 4,
    APEX_MODE_QUARTO  = 5   /* Pandoc/Quarto markdown for HTML */
} apex_mode_t;
```

Add a convenience flag:

```c
bool enable_quarto_extensions;  /* Master switch for Pandoc/Quarto preprocessors */
```

Quarto mode sets this `true`; individual preprocessors can still be toggled via metadata/CLI for testing.

---

## Mode preset: `apex_options_for_mode(APEX_MODE_QUARTO)`

Start from unified, then override:


| Option                         | Quarto value | Rationale                      |
| ------------------------------ | ------------ | ------------------------------ |
| `enable_quarto_extensions`     | `true`       | Enables new preprocessors      |
| `enable_quarto_callouts`       | `true`       | Core Quarto authoring          |
| `enable_divs` / `enable_spans` | `true`       | Pandoc div/span syntax         |
| `enable_alpha_lists`           | `true`       | `a.` / `A.` in Quarto examples |
| `allow_mixed_list_markers`     | `true`       | Nested list markers            |
| `enable_definition_lists`      | `true`       | Pandoc definition lists        |
| `enable_footnotes`             | `true`       | Reference + inline `^[...]`    |
| `enable_math`                  | `true`       | `$...$` / `$$...$$`            |
| `enable_smart_typography`      | `true`       | `--`, `---`                    |
| `enable_task_lists`            | `true`       | GFM task lists in Quarto       |
| `enable_image_captions`        | `true`       | Figure captions                |
| `unsafe`                       | `true`       | Raw HTML passthrough           |
| `hardbreaks`                   | `false`      | Pandoc soft breaks, not GFM    |
| `enable_wiki_links`            | `false`      | Not Quarto                     |
| `enable_marked_extensions`     | `false`      | Marked-specific syntax off     |
| `enable_py_callouts`           | `false`      | Quarto uses `:::`, not `!!!`   |
| `enable_indices`               | `false`      | Not Pandoc/Quarto              |
| `id_format`                    | GFM          | Quarto default header IDs      |


**Refactor needed:** Many code paths gate on `mode == APEX_MODE_UNIFIED` (fenced divs, spans, autolink, etc.). Introduce helpers:

```c
static inline bool apex_mode_is_unified_family(apex_mode_t m) {
    return m == APEX_MODE_UNIFIED || m == APEX_MODE_QUARTO;
}
```

Replace hard-coded unified checks for divs, spans, fenced-div preprocessing, and similar features.

---

## Preprocessing pipeline

New steps run **only when `enable_quarto_extensions`** is true. Proposed order (relative to existing pipeline in `apex.c`):

```
… existing preprocessors (metadata, includes, tables, def lists) …

1. apex_preprocess_raw_content()      # {=format} blocks + inline
2. apex_preprocess_code_fence_attrs() # ```{.python filename="..."}
3. apex_preprocess_example_lists()  # (@) example list markers
4. apex_preprocess_line_blocks()    # | line blocks
5. apex_preprocess_roman_lists()    # i) ii) iii)
6. apex_preprocess_quarto_shortcodes()  # {{< ... >}} (optional/plugin)
7. apex_preprocess_quarto_callouts()    # existing
8. apex_process_fenced_divs()           # existing (extend mode check)
… parse …
9. apex_postprocess_fig_alt()       # fig-alt → alt attribute
10. apex_postprocess_diagrams()     # mermaid/graphviz wrappers
11. apex_postprocess_semantic_spans() # .smallcaps, .underline, .mark CSS
```

Raw content and code-fence attribute normalization must run **before** cmark sees fenced code blocks.

---

## Phase 0 — Infrastructure (1–2 days)

**Deliverables**

- [ ] Add `APEX_MODE_QUARTO` to `apex.h`, Ruby bindings, docs
- [ ] `apex_options_for_mode(APEX_MODE_QUARTO)` preset
- [ ] CLI: `--mode quarto`; metadata `mode: quarto`
- [ ] `apex_mode_is_unified_family()` and update mode gates in `apex.c`, `ial.c`
- [ ] `tests/fixtures/quarto/` + smoke test: `apex --mode quarto fixtures/quarto/smoke.md`
- [ ] Wiki page: `Quarto-Mode.md`; update `Modes.md`

**Acceptance:** `apex --mode quarto doc.md` runs with quarto callouts enabled and divs/spans working.

---

## Phase 1 — Prerequisite fixes + low-hanging fruit (1–2 days)

### 1a. Fix class-only bracketed spans

Current bug: `[text]{.smallcaps}` → `<spanclass="smallcaps">`.

**Fix:** In `attributes_to_html()`, ensure a leading space before `class=` when it’s the first attribute and no `id` precedes it (likely in the `first_attr` branch for classes).

### 1b. `fig-alt` mapping

Quarto: `![Caption](img.png){fig-alt="Alt text"}`

**HTML output:**

```html
<figure>
  <img src="img.png" alt="Alt text" />
  <figcaption>Caption</figcaption>
</figure>
```

Implement in image caption post-processing (`html_renderer.c` or IAL image attrs).

### 1c. Semantic span classes

Map Pandoc classes to CSS (inline or class-only):


| Class        | HTML/CSS                                 |
| ------------ | ---------------------------------------- |
| `.smallcaps` | `font-variant: small-caps`               |
| `.underline` | `text-decoration: underline`             |
| `.mark`      | `background-color: yellow` (or `<mark>`) |


Could be a small post-render pass or bundled CSS in `--standalone` when quarto mode is active.

**Acceptance:** Quarto “Other Spans” examples render correctly.

---

## Phase 2 — Raw content `{=format}` (2–3 days)

**Syntax**

Block:

```markdown
```{=html}
<iframe src="..."></iframe>
```
```

Inline:

```markdown
`<a>html</a>`{=html}
```

**Behavior (HTML output)**


| Format                  | Action                                                            |
| ----------------------- | ----------------------------------------------------------------- |
| `html`                  | Passthrough raw (requires `unsafe: true`)                         |
| `latex`, `typst`, `tex` | Omit or wrap in `<!-- raw format=typst -->…<!-- /raw -->` comment |
| unknown                 | Same as non-html: preserve as HTML comment for debugging          |


**Implementation:** New `src/extensions/raw_content.c`

- Scan fenced blocks; if info string matches `^=\w+$`, extract body and emit raw HTML block (or placeholder replaced post-parse)
- Scan inline: backtick span immediately followed by `{=format}`

**Tests:** `{=html}`, `{=latex}`, `{=typst}`, inline `{=html}`, ensure no processing inside `{=html}` blocks

---

## Phase 3 — List extensions (2–3 days)

### 3a. Example lists `(@)`

Pandoc `example_lists` extension — `(@)` is a **list marker**, not a standalone continuation line. See [Pandoc example lists](https://pandoc.org/MANUAL.html#extension-example_lists).

```markdown
(@)  My first example will be numbered (1).
(@)  My second example will be numbered (2).

Explanation of examples.

(@)  My third example will be numbered (3).
```

Labeled markers `(@good)` are also supported; labels are stripped and items are numbered sequentially.

**Implementation:** Preprocessor converts `(@)` / `(@label)` line-start markers to `1.`, `2.`, `3.`, … with document-wide sequential numbering. Interrupted lists rely on cmark's `start` attribute on continued `<ol>` blocks.

**Not yet implemented:** repeating a prior example by reusing its label (`(@foo)` twice).

### 3b. Roman numeral markers `i)`, `ii)`, `I)`, etc.

Extend the alpha-list preprocessor (`apex.c` ~line 2649) with a third style:

```html
<ol style="list-style-type: lower-roman">
```

Detect `i)`, `ii)`, `iii)` (and uppercase) at list-item starts.

### 3c. Line blocks

```markdown
| Line one
|   preserved spaces
| Line three
```

Pandoc line blocks: lines starting with `|` + space, continuation lines indented or also `|`.

**Output:** `<div class="line-block">` with `<br />` or nested divs preserving spacing.

**Tests:** Quarto’s nested list example (`1. / i) / A.`), line block example, `(@)` continuation

---

## Phase 4 — Code fence attributes (2 days)

Quarto/Pandoc extended fences:

```markdown
```{.python filename="run.py" linenos=true}
print("hello")
```
```

**Preprocessor** normalizes to something cmark + Apex highlighting understand:

```markdown
```python
print("hello")
```
```

…plus metadata preserved for HTML output:

```html
<pre class="python" data-filename="run.py" data-linenos="true">
```

Or use existing code-line-numbers when `linenos` / `line-numbers` is present.

**Parse rules:** Info string `{.class key="val" key2=val2}` — class becomes language if `.python`-style; remaining keys become data attributes on `<pre>`/`<code>`.

---

## Phase 5 — Diagrams (2–3 days)

Quarto native diagram fences:

```markdown
```{mermaid}
flowchart LR
  A --> B
```
```

**HTML strategy (pragmatic):**

```html
<pre class="mermaid">
flowchart LR
  A --> B
</pre>
```

When `--standalone` + quarto mode, auto-suggest/include mermaid.js (you already have `--script mermaid`).

Graphviz `{dot}` / `{graphviz}`: similar, or convert to `<img>` if a dot renderer is unavailable (document limitation).

**Optional:** `enable_quarto_diagrams` flag; off = plain code block (current behavior).

---

## Phase 6 — Shortcodes (plugin-first, 2–4 days)

Quarto shortcodes are a large surface area. Recommend **plugin, not core**:


| Shortcode           | Apex approach                                       |
| ------------------- | --------------------------------------------------- |
| `{{< kbd >}}`       | Extend `apex-plugin-kbd` to accept Quarto syntax    |
| `{{< pagebreak >}}` | Map to existing `{::pagebreak /}` or `<!--BREAK-->` |
| `{{< video >}}`     | New small plugin → `<video>` / embed iframe         |
| `{{< include >}}`   | Delegate to existing includes if possible           |


**Core shim:** Optional lightweight preprocessor converts `{{< name args >}}` → plugin hook or known expansions; unknown shortcodes pass through with a warning in verbose mode.

---

## Phase 7 — Polish & compatibility toggles (ongoing)


| Feature                       | Approach                                                                                     |
| ----------------------------- | -------------------------------------------------------------------------------------------- |
| `::: {.hidden}`               | Add `.hidden { display: none }` in quarto standalone CSS; math macros stay user’s problem    |
| Empty div list reset `::: {}` | Verify list numbering; fix if cmark merges lists incorrectly                                 |
| Strict blank-line-before-list | Optional `quarto_strict_lists` flag (off by default; Apex stays permissive unless requested) |
| Cross-ref syntax `@fig-x`     | Pass through as text, or wrap in `<span class="quarto-xref">` for future filter              |
| Citations `@key`              | Out of scope unless CSL integration is planned                                               |


---

## File layout

```
src/extensions/
  raw_content.c / .h
  quarto_lists.c / .h      # (@), roman lists, line blocks
  code_fence_attrs.c / .h
  quarto_diagrams.c / .h   # optional
  quarto_shortcodes.c / .h # optional thin shim

tests/
  fixtures/quarto/
    raw-content.md
    lists.md
    code-attrs.md
    diagrams.md
    callouts.md
    spans.md
  test_quarto_mode.c         # or section in test_extensions.c
```

---

## CLI & metadata surface

```bash
apex doc.qmd --mode quarto
apex doc.qmd --mode quarto --standalone --script mermaid
```

Metadata (in `.qmd` YAML):

```yaml
---
title: My Doc
mode: quarto
quarto-callouts: true
---
```

Individual toggles (for debugging / partial compatibility):

```yaml
quarto-raw: true
quarto-list-continuation: true
quarto-line-blocks: true
quarto-roman-lists: true
quarto-code-attrs: true
quarto-diagrams: true
quarto-shortcodes: false   # plugin handles it
```

---

## Testing strategy

1. **Fixture suite** — one `.md` per Quarto basics section; golden HTML snapshots
2. **Regression** — existing unified tests must not break when quarto flags are off
3. **Round-trip** — optional: quarto fixtures through `--to markdown` (future)
4. **Live samples** — copy 2–3 snippets from quarto.org docs into fixtures

Example acceptance test for Phase 2:

```bash
build/apex --mode quarto - <<'EOF'
```{=html}
<strong>raw</strong>
```

EOF

# expect: **raw** not ```

```

```

---

## Suggested release milestones

| Version | Phases | User-visible value |
|---------|--------|-------------------|
| **v1.2** | 0 + 1 | `--mode quarto` exists; callouts, spans, fig-alt work |
| **v1.3** | 2 + 3 | Raw `{=typst}`/`{=html}`, `(@)`, roman lists, line blocks |
| **v1.4** | 4 + 5 | Code fence attrs, mermaid fences |
| **v1.5** | 6 + 7 | Shortcode plugins, polish |

---

## Risks & decisions

1. **cmark limitations** — Line blocks and list continuation may need AST surgery, not just preprocessing. Budget time for post-parse fixes.
2. **Raw `{=typst}` in HTML** — Document clearly: Apex preserves/emits comments; it does not compile Typst. Users targeting Typst still need Quarto/Pandoc.
3. **Mode proliferation** — Keep quarto as unified-family; avoid a separate options matrix per feature.
4. **Shortcodes vs plugins** — Core should not reimplement Quarto’s shortcode engine; plugins + thin shim is the right boundary.
5. **Span bug** — Fix in unified too; not quarto-only.

---

## Recommended first PR (minimal viable quarto mode)

Single PR touching:

1. `APEX_MODE_QUARTO` + CLI + metadata
2. `apex_mode_is_unified_family()` refactor
3. Enable `--quarto-callouts` in quarto preset
4. Fix bracketed span `class=` bug
5. `fig-alt` mapping
6. `tests/fixtures/quarto/smoke.md` + tests

That gives a real `--mode quarto` users can try immediately, with raw content and list features as follow-up PRs.
```

