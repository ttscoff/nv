# Pandoc/Kramdown IAL Image Compatibility Test

## Test 1: Reference definition with IAL after title
[ref]: foo.jpg "optional title" {#id .class key=val
key2="val 2"}

## Test 2: Inline image with IAL
An inline ![image](foo.jpg){#id .class width=30 height=20px}

## Test 3: Reference image with attributes
![image][ref]

## Test 4: Pandoc-style width with percentage
![](file.jpg){ width=50% }

## Test 5: Width and height with units
![](file2.jpg){ width=300px height=200px }

## Test 6: Mixed IAL and attributes
![test](test.jpg){#myid .myclass width=50% height=100px}

## Test 7: Reference with title and IAL
[imgref]: image.png "Image Title" {#refimg .imageclass
width=75%}

![ref image][imgref]

