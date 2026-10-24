# Quarto smoke test

## Callouts

::: {.callout-note}
Quarto callout note body.
:::

## Spans

[smallcaps text]{.smallcaps}

[underlined text]{.underline}

[highlighted text]{.mark}

## Figure

![Caption](elephant.png){fig-alt="Alt text"}

## Div

::: {.border}
Bordered div content.
:::

## Raw content

```{=html}
<strong>raw html</strong>
```

Text `<span class="x">inline</span>`{=html} after.

## Example lists

(@)  My first example will be numbered (1).
(@)  My second example will be numbered (2).

Explanation of examples.

(@)  My third example will be numbered (3).

## Roman list

i) Alpha
ii) Beta

## Line block

| Line one
|   preserved spaces
| Line three

## Code fence attributes

```{.python filename="run.py"}
print("hello")
```

## Diagram fences

```{mermaid}
flowchart LR
  A --> B
```

```{dot}
digraph { A -> B; }
```

## Shortcodes

Page 1

{{< pagebreak >}}

Page 2

Press {{< kbd $@3 >}} to save.

