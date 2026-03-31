/* Minimal string function implementations for bare-metal builds.
 *
 * These satisfy FreeRTOS (memset in tasks.c / heap_4.c) and any other
 * internal library call without depending on newlib's libc.
 *
 * Being part of the startup OBJECT library, the symbols are available
 * unconditionally and are seen by the linker before any static archive.
 */

#include <stddef.h>

/* GCC's -ftree-loop-distribute-patterns (-O3) can convert simple loops into
 * calls to memset/memcpy.  If we ARE the implementation, that would recurse.
 * The pragma disables that transform for this translation unit only.       */
#pragma GCC optimize("no-tree-loop-distribute-patterns")

void *memset(void *s, int c, size_t n)
{
    unsigned char *p = (unsigned char *)s;
    unsigned char v = (unsigned char)c;
    while (n--) *p++ = v;
    return s;
}

void *memcpy(void *dst, const void *src, size_t n)
{
    unsigned char *d = (unsigned char *)dst;
    const unsigned char *s = (const unsigned char *)src;
    while (n--) *d++ = *s++;
    return dst;
}

void *memmove(void *dst, const void *src, size_t n)
{
    unsigned char *d = (unsigned char *)dst;
    const unsigned char *s = (const unsigned char *)src;
    if (d < s) {
        while (n--) *d++ = *s++;
    } else if (d > s) {
        d += n; s += n;
        while (n--) *--d = *--s;
    }
    return dst;
}

int memcmp(const void *s1, const void *s2, size_t n)
{
    const unsigned char *a = (const unsigned char *)s1;
    const unsigned char *b = (const unsigned char *)s2;
    while (n--) {
        if (*a != *b) return (int)*a - (int)*b;
        ++a; ++b;
    }
    return 0;
}
