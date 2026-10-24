# Width/Height Conversion Test

This test verifies that width and height attributes are
correctly converted:

- Percentages → `style` attribute
- `Xpx` → integer `width`/`height` attribute (strips `px`)
- Bare integers → `width`/`height` attribute
- Other units (em, rem, etc.) → `style` attribute

## Test 1: Percentage width
Expected: `style="width: 50%"`
![](img.jpg){ width=50% }

## Test 2: Pixel width
Expected: `width="300"`
![](img.jpg){ width=300px }

## Test 3: Bare integer width
Expected: `width="300"`
![](img.jpg){ width=300}

## Test 4: Percentage height
Expected: `style="height: 75%"`
![](img.jpg){ height=75% }

## Test 5: Pixel height
Expected: `height="200"`
![](img.jpg){ height=200px }

## Test 6: Both width and height with percentages
Expected: `style="width: 50%; height: 75%"`
![](img.jpg){ width=50% height=75% }

## Test 7: Mixed - pixel width, percentage height
Expected: `width="300" style="height: 50%"`
![](img.jpg){ width=300px height=50% }

## Test 8: Mixed - percentage width, pixel height
Expected: `height="200" style="width: 50%"`
![](img.jpg){ width=50% height=200px }

## Test 9: Bare integers for both
Expected: `width="300" height="200"`
![](img.jpg){ width=300 height=200 }

## Test 10: Other units (should go to style)
Expected: `style="width: 5em; height: 10rem"`
![](img.jpg){ width=5em height=10rem }

## Test 11: Inline image with percentage width
Expected: `style="width: 75%"`
![alt text](img.jpg){ width=75% }

## Test 12: Inline image with pixel width
Expected: `width="400"`
![alt text](img.jpg){ width=400px }

## Test 13: Reference image with percentage
Expected: `style="width: 60%"`
![ref image][img1]

[img1]: img.jpg "Title" { width=60% }

## Test 14: Reference image with pixel dimensions
Expected: `width="500" height="300"`
![ref image][img2]

[img2]: img.jpg { width=500px height=300px }

## Test 15: Mixed with other attributes
Expected: `id="test" class="image" width="250" style="height: 80%"`
![test](img.jpg){#test .image width=250px height=80% }

## Test 16: Zero pixel value
Expected: `width="0"`
![](img.jpg){ width=0px }

## Test 17: Decimal pixel value (should go to style)
Expected: `style="width: 100.5px"`
![](img.jpg){ width=100.5px }

## Test 18: Viewport units (should go to style)
Expected: `style="width: 50vw; height: 30vh"`
![](img.jpg){ width=50vw height=30vh }

## Test 19: Percentage with decimal
Expected: `style="width: 33.33%"`
![](img.jpg){ width=33.33% }

## Test 20: Multiple style values (width percentage, existing style)
Expected: `style="float: left; width: 50%"`
![](img.jpg){ style="float: left" width=50% }

