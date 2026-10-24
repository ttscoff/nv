# Media Format Handling - Images and Videos

This fixture tests WebP, AVIF, and video format attributes for images and videos.

## Images with WebP

![Hero image](img/hero.png "Hero!" webp)

## Images with AVIF

![Hero AVIF](img/hero.png avif)

## Images with WebP and @2x

![Retina WebP](img/hero.png webp @2x)

## Images with AVIF and @2x

![Retina AVIF](img/hero.png avif @2x )

## Images with both WebP and AVIF

![Modern formats](img/banner.jpg webp avif)

## Video - basic MP4

![Demo video](media/demo.mp4)

## Video - MP4 with WebM alternative

![Demo with WebM](media/demo.mp4 webm)

## Video - MP4 with OGG alternative

![Demo with OGG](media/intro.mp4 ogg)

## Video - WebM with MP4 fallback

![WebM primary](media/clip.webm mp4)

## Video - MOV format

![QuickTime](assets/trailer.mov)

## Video - M4V format

![M4V](assets/sample.m4v)

## Auto - discover formats from filesystem

When `auto` is specified and base_directory is set, Apex discovers existing
variants (2x, 3x, webp, avif for images; webm, ogg, mp4, mov, m4v for videos)
and generates appropriate picture/video elements.

![Profile menu](img/app-pass-1-profile-menu.jpg auto)

## Wildcard extension (*) - same as auto

Using `*` as the extension (e.g. `![](image.*)`) is equivalent to `![](image.png)` with the `auto` attribute when base_directory is set.

Apex scans for jpg, png, gif, webp, avif (1x, 2x, 3x) for images and mp4, webm, ogg, mov, m4v for videos.

![Profile menu wildcard](img/app-pass-1-profile-menu.*)
