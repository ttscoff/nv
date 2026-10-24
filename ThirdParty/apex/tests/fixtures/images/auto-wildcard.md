
## Wildcard extension (*) - same as auto

Using `*` as the extension (e.g. `![](image.*)`) is equivalent to `![](image.png)` with the `auto` attribute when base_directory is set.

Apex scans for jpg, png, gif, webp, avif (1x, 2x, 3x) for images and mp4, webm, ogg, mov, m4v for videos.

![Profile menu wildcard](img/app-pass-1-profile-menu.*)