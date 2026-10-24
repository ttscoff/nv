#ifndef CMARK_GFM_EXPORT_H
#define CMARK_GFM_EXPORT_H

/* SPM build: export header for cmark-gfm static library.
 * When CMARK_GFM_STATIC_DEFINE is set (always for SPM), export macros expand to nothing. */

#ifdef CMARK_GFM_STATIC_DEFINE
#  define CMARK_GFM_EXPORT
#  define CMARK_GFM_NO_EXPORT
#else
#  if defined(__GNUC__) || defined(__clang__)
#    define CMARK_GFM_EXPORT __attribute__((visibility("default")))
#    define CMARK_GFM_NO_EXPORT __attribute__((visibility("hidden")))
#  else
#    define CMARK_GFM_EXPORT
#    define CMARK_GFM_NO_EXPORT
#  endif
#endif

#endif
