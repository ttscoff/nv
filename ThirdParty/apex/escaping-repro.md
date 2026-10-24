# Apex: escaping & Unicode repro cases

Use this file in the Apex test harness or `Apex.convert(..., mode: .unified)` to verify behavior and drive fixes.

**Automated coverage:** the same scenarios are exercised in `tests/test_escaping_repro.c`. From the build directory run `./apex_test_runner escaping` (or `escaping_repro`).

**Reference:** [CommonMark spec — Escaping](https://spec.commonmark.org/0.31.2/#backslash-escapes) (inlines), [Images](https://spec.commonmark.org/0.31.2/#images).

---

## 1. Backslash before `[` after `!` (false image opener)

**Intent:** `!\[` should render as literal `!` + `[`, not start an image.

### 1a — Escaped bracket (spec expectation)

```markdown
Not an image: !\[literal bracket after bang]
```

**Expected (typical cmark):** Plain text: `Not an image: ![literal bracket after bang]` (no `<img>`, no swallowed tail).

---

### 1b — Same as blog HTML→MD glitch (sentence + link)

```markdown
I'm so glad you're all here![ Now that the update is slowing down, I will continue.
```

**Problem:** If `![` is parsed as an image, the parser may consume until a far `]` and drop or merge paragraphs.

**Workaround in consumers:** Insert a separator between `!` and `[` (e.g. narrow no-break space U+202F) so `![` is not contiguous.

---

## 2. Unicode double prime U+2033 (`″`) after ASCII (inches)

**Note:** The inch mark below is **U+2033 DOUBLE PRIME** (not ASCII `0x22`).

```markdown
Height in parentheses: (He's 5'7″ if you're wondering.) More text after the parens.
```

**Things to check:**

- Full sentence through `More text after the parens.` appears in HTML.
- No truncation immediately after `″` or after the word `if`.
- If output stops early, compare with the same line using ASCII inch mark only:

```markdown
Height in parentheses: (He's 5'7" if you're wondering.) More text after the parens.
```

---

## 3. Minimal line — prime only

**U+2033 only:**

```markdown
X″ Y
```

**ASCII control:**

```markdown
X" Y
```

If `X″ Y` truncates or errors but `X" Y` does not, isolate UTF-8 / lexer / serializer around U+2033.

---

## 4. True image (sanity check)

Should still parse as an image:

```markdown
![alt text](https://example.com/image.png)
```

---

## 5. Glued bold after paragraph (optional stress test)

Some pipelines truncate when `**` is glued without a newline:

```markdown
End of paragraph with a closing paren. (He's 5'7″ if you're wondering.) Hope this helps.**Next section** starts here.
```

**Check:** Entire line preserved; `**Next section**` renders as strong, not broken emphasis across the whole block.

---

## Summary for implementers

| # | Topic | What to verify / fix |
|---|--------|----------------------|
| 1 | `!\[` | Matches CommonMark escape rules in unified mode. |
| 2 | `″` (U+2033) | No truncation vs ASCII `"` for otherwise identical strings. |
| 3 | False `![` | Document that unescaped `![` is an image opener; optional lenient mode is non-standard. |
