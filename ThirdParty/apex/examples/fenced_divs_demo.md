# Pandoc Fenced Divs Demo

This document demonstrates all features of the Pandoc fenced
divs extension.

## Basic Fenced Div

A simple fenced div with ID and class:

::::: {#special .sidebar}
Here is a paragraph.

And another paragraph with more content.
:::::

## Fenced Div with Multiple Classes

::::: {.warning .important .highlight}
This is a warning div with multiple classes.
:::::

## Fenced Div with Attributes

::::: {#mydiv .container key="value" data-id="123"}
This div has an ID, a class, and custom attributes.
:::::

## Single Unbraced Word (Treated as Class)

::: sidebar
This is a div with a single unbraced word treated as a class
name.
:::

## Fenced Div with Trailing Colons

According to the spec, attributes may optionally be followed
by another string of consecutive colons:

::::: {#special .sidebar} ::::
Here is a paragraph with trailing colons after the
attributes.

And another.
::::::::::::::::::

## Nested Divs

Fenced divs can be nested. Opening fences must have
attributes:

::: Warning ::::::
This is a warning.

::: Danger
This is a warning within a warning.
:::
::::::::::::::::::

## Complex Nested Example

::::: {#outer .container} ::::
Outer div content.

::: {.inner .nested}
First inner div.

::: {.deep}
Deeply nested div.
:::
:::

More outer content.
::::::::::::::::::

## Div with All Attribute Types

::::: {#complex-id .class1 .class2 .class3 key1="value1"
key2='value2' data-test="123"}
This div demonstrates:

- ID (#complex-id)
- Multiple classes (.class1, .class2, .class3)
- Custom attributes (key1, key2, data-test)

:::::

## Div Separated by Blank Lines

This paragraph is before the div.

::::: {.separated}
This div is properly separated by blank lines from the
preceding paragraph.
:::::

This paragraph is after the div.

## Minimal Div (3 Colons Minimum)

::: {.minimal}
This uses the minimum 3 colons for the fence.
:::

## Div with Quoted Values

::::: {#quoted .test attr1="quoted value" attr2='single
quoted'}
This div has attributes with quoted values containing
spaces.
:::::

## Multiple Divs in Sequence

::: {.first}
First div.
:::

::: {.second}
Second div.
:::

::: {.third}
Third div.
:::

## Div with Empty Content

::: {.empty}
:::

## Div with Only ID

::: {#only-id}
This div has only an ID, no classes.
:::

## Div with Only Classes

::: {.only-classes .multiple}
This div has only classes, no ID.
:::

## Div with Mixed Content

::::: {#mixed .content}
This div contains:

- A list item
- Another list item

And a paragraph.

> A blockquote

And more content.
:::::

