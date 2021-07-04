/*******************************************************************************
 * Copyright (c) 2019 Craig Jacobson
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 ******************************************************************************/
/**
 * @file ss.c
 * @author Craig Jacobson
 * @brief Simple String Library implementation.
 *
 * #### Prefixes:
 * - ss_ is for manipulating binary ss buffers.
 * - ssc_ is for manipulating ss buffers assumed to be c strings (US ASCII/UTF8).
 * - ssu_ is for Unicode code-point operations.
 * - ssu8_ is for UTF8 operations.
 * - sse_ is for exporting internal functions primarily for testing.
 */

#include "ss_util.h"
#include "ss.h"

#include <ctype.h>
#include <errno.h>
#include <features.h>
#include <limits.h>
#include <stdarg.h>
#include <stdio.h>

#ifndef LIKELY
#ifdef __GNUC__
#define LIKELY(x)   __builtin_expect(!!(x), 1)
#else
#define LIKELY(x)   (x)
#endif
#endif

#ifndef UNLIKELY
#ifdef __GNUC__
#define UNLIKELY(x) __builtin_expect(!!(x), 0)
#else
#define UNLIKELY(x) (x)
#endif
#endif

#ifndef INLINE
#ifdef __GNUC__
#define INLINE __attribute__((always_inline)) inline
#else
#define INLINE
#endif
#endif

#ifndef NOINLINE
#ifdef __GNUC__
#define NOINLINE __attribute__((noinline))
#else
#define NOINLINE
#endif
#endif

#define _SS_GROW_MAX (4)
#define _SS_GROW_SHIFT (16)
#define _SS_TYPE_MASK (0x0000FFFF)
#define _SS_GROW_MASK (0xFFFF0000)
#define _SS_HEAP_ALLOCATED (0x00000100)


static void
_ss_abort(bool is_malloc, size_t size)
{
    const char msg[] = "libss %s(%zu) failure";
    const char m[] = "malloc";
    const char r[] = "realloc";

    /* Note that 2**64 is actually 20 max chars, good to have space. */
    char buf[sizeof(msg) + sizeof(r) + 32];

    snprintf(buf, sizeof(buf), msg, is_malloc ? m : r, size);
    perror(buf);
    abort();
}

INLINE static void *
_ss_rawalloc_impl(size_t size)
{
    void *mem = malloc(size);
    if (UNLIKELY(!mem))
    {
        _ss_abort(true, size);
    }
    return mem;
}

INLINE static void *
_ss_rawrealloc_impl(void *oldmem, size_t size)
{
    void *mem = realloc(oldmem, size);
    if (UNLIKELY(!mem))
    {
        _ss_abort(false, size);
    }
    return mem;
}

/*
 * Don't check for NULL since ss_free does operations
 * that would cause segfault anyways.
 */
INLINE static void
_ss_rawfree_impl(void *mem)
{
    free(mem);
}

enum _sstring_type
{
    _SSTRING_EMPTY = 0,
    _SSTRING_STACK = 1,
    _SSTRING_NORM  = 2,
};

/// @cond DOXYGEN_IGNORE

typedef struct _sstring_s
{
    size_t cap;
    size_t len;
    uint32_t type;
} __attribute__((packed)) _sstring_t;

typedef struct _sstring_empty_s
{
    _sstring_t m;
    int s;
} __attribute__((packed)) _sstring_empty_t;

static _sstring_empty_t g_ss_empty[_SS_GROW_MAX] =
{
    { { 0, 0, SS_GROW0   << _SS_GROW_SHIFT }, 0 },
    { { 0, 0, SS_GROW25  << _SS_GROW_SHIFT }, 0 },
    { { 0, 0, SS_GROW50  << _SS_GROW_SHIFT }, 0 },
    { { 0, 0, SS_GROW100 << _SS_GROW_SHIFT }, 0 },
};

/// @endcond

INLINE static _sstring_t *
_ss_meta(SS s)
{
    _sstring_t *m = (_sstring_t *)s;
    return m - 1;
}

INLINE static const _sstring_t *
_ss_cmeta(const SS s)
{
    const _sstring_t *m = (const _sstring_t *)s;
    return m - 1;
}

/**
 * @internal
 * @brief Convert metadata ref to SS.
 */
INLINE static SS
_ss_string(_sstring_t *m)
{
    return (SS)(m + 1);
}

/**
 * @internal
 * @return Length of string.
 */
INLINE static size_t
_ss_len(const SS s)
{
    return _ss_cmeta(s)->len;
}

/**
 * @internal
 * @return Type field of string.
 */
INLINE static uint32_t
_ss_type(SS s)
{
    return _ss_meta(s)->type;
}

/**
 * @internal
 * @return True if string is of the given type.
 */
INLINE static bool
_ss_is_type(SS s, enum _sstring_type t)
{
    return (_ss_meta(s)->type & _SS_TYPE_MASK) == (uint32_t)t;
}

/**
 * @internal
 * @return The maximum capacity.
 */
INLINE static size_t
_ss_cap_max(void)
{
    return ((size_t)UINT_MAX) - 2 - sizeof(_sstring_t);
}

/**
 * @internal
 * @return True if the given capacity is in the valid range.
 */
INLINE static bool
_ss_valid_cap(size_t cap)
{
    return !!(cap <= _ss_cap_max());
}

/**
 * @internal
 * @brief Adjust the capacity of the string to that given.
 */
INLINE static _sstring_t *
_ss_realloc(_sstring_t *m, size_t cap)
{
    _sstring_t *m2;

    if (m->type & _SS_HEAP_ALLOCATED)
    {
        m2 = ss_rawrealloc(m, sizeof(_sstring_t) + cap + 1);
        if (cap < m2->cap)
        {
            _ss_string(m2)[cap] = 0;
        }
        m2->cap = cap;
    }
    else
    {
        m2 = ss_rawalloc(sizeof(_sstring_t) + cap + 1);
        m2->cap = cap;
        m2->len = m->len;
        m2->type =
            (m->type & _SS_GROW_MASK)
            | _SS_HEAP_ALLOCATED
            | _SSTRING_NORM;
        ss_memcopy((SS)(m2 + 1), (SS)(m + 1), m->len + 1);
    }

    return m2;
}

INLINE static int
_ss_getgrow(uint32_t type)
{
    return (int)(type >> _SS_GROW_SHIFT);
}

/**
 * @internal
 * @brief Adjust capactiy of the string applying growth values.
 */
INLINE static _sstring_t *
_ss_realloc_grow(_sstring_t *m, size_t cap)
{
    if (m->type & _SS_GROW_MASK)
    {
        size_t growcap = 0;

        switch (_ss_getgrow(m->type))
        {
            case SS_GROW25:
                growcap = cap/4;
                break;
            case SS_GROW50:
                growcap = cap/2;
                break;
            case SS_GROW100:
                growcap = cap;
                break;
            default:
                break;
        }


        if (growcap > SS_MAX_REALLOC)
        {
            growcap = SS_MAX_REALLOC;
        }

        if ((growcap + cap) < cap || !_ss_valid_cap(growcap + cap))
        {
            cap = _ss_cap_max();
        }
        else
        {
            cap = cap + growcap;
        }
    }

    return _ss_realloc(m, cap);
}

INLINE static const char *
_ss_memrchar(const char *buf, char find, size_t searchlen)
{
    const char *end = buf + searchlen;
    while (buf <= end)
    {
        if (*end == find)
        {
            return end;
        }
        --end;
    }

    return NULL;
}

/**
 * The empty string is great if you want to avoid NULL checks but have
 * O(1) time and space cost.
 * This reference is not allocated, but will become allocated once you 
 * add to it using the library functions.
 * @warning While you can duplicate the empty string reference safely, don't.
 * @return Empty string reference.
 */
SS
ss_empty(void)
{
    return _ss_string((_sstring_t *)&g_ss_empty[0]);
}

/**
 * @return String with capacity as specified.
 */
SS
ss_new(size_t cap)
{
    if (!cap || NPOS == cap)
    {
        return ss_empty();
    }

    if (!_ss_valid_cap(cap))
    {
        cap = _ss_cap_max();
    }

    _sstring_t *m = ss_rawalloc(sizeof(_sstring_t) + cap + 1);
    SS s = NULL;

    if (m)
    {
        m->cap = cap;
        m->len = 0;
        m->type = _SS_HEAP_ALLOCATED | _SSTRING_NORM;
        s = _ss_string(m);
        s[0] = 0;
    }

    return s;
}

/**
 * @return New string with the given capacity and copy of the given string.
 */
SS
ss_newfrom(size_t cap, const char *cs, size_t len)
{
    cap = cap > len ? cap : len;
    _sstring_t *m = ss_rawalloc(sizeof(_sstring_t) + cap + 1);
    SS s = NULL;

    if (m)
    {
        m->cap = cap;
        m->len = len;
        m->type = _SS_HEAP_ALLOCATED | _SSTRING_NORM;
        s = _ss_string(m);
        if (len)
        {
            ss_memcopy(s, cs, len);
        }
        s[len] = 0;
    }

    return s;
}

/**
 * @return Duplicate of the given string.
 */
SS
ss_dup(SS s)
{
    return ss_newfrom(0, s, ss_len(s));
}

/**
 * @brief Free the given string.
 * @note Will properly check for empty and stack string types.
 * @param s
 */
void
ss_free(SS *s) 
{
    /* Empty string and stack string will not have flag set. */
    if ((_ss_type(*s)) & _SS_HEAP_ALLOCATED)
    {
        ss_rawfree(_ss_meta(*s));
    }
    (*s) = NULL;
}

/**
 * @brief Exposed method for internal
 * @warning Users should not use this function.
 * @param b - The buffer to initialize and turn into a stack string.
 * @return Initialized stack string.
 */
SS
ss_stack_init(void *b, size_t cap)
{
    _sstring_t *m = (_sstring_t *)b;
    m->cap = cap;
    m->len = 0;
    m->type = _SSTRING_STACK;

    SS s = _ss_string(m);
    s[0] = 0;

    return s;
}

/**
 * @warning Your system and available memory may restrict this more.
 * @note The value is adjusted for the metadata and NULL sentinel.
 * @return The maximum capacity value.
 */
size_t
ss_maxcap(void)
{
    return _ss_cap_max();
}

/**
 * @note If you have a c-string you're using and have modified the string
 *       so the end is different than before, call ssc_setlen before using.
 * @return Length of the string in O(1) time.
 */
size_t
ss_len(const SS s)
{
    return _ss_len(s);
}

/**
 * @note The length will always be less than or equal to capacity.
 * @return The capacity of the string.
 */
size_t
ss_cap(const SS s)
{
    return _ss_cmeta(s)->cap;
}

/**
 * @return True if the length is zero.
 */
bool
ss_isempty(const SS s)
{
    return !(_ss_len(s));
}

/**
 * @return True if this was a string returned from ss_empty.
 */
bool
ss_isemptytype(const SS s)
{
    return _ss_is_type(s, _SSTRING_EMPTY);
}

/**
 * @return True if this string is on the heap (malloc'd).
 */
bool
ss_isheaptype(const SS s)
{
    return ((_ss_type(s)) & _SS_HEAP_ALLOCATED);
}

/**
 * @return True if this string is on the stack.
 */
bool
ss_isstacktype(const SS s)
{
    return _ss_is_type(s, _SSTRING_STACK);
}

/**
 * @return True if both strings are equal (capacity can differ); false othewise.
 */
bool
ss_equal(const SS s1, const SS s2)
{
    if (s1 == s2)
    {
        return true;
    }

    const _sstring_t *m1 = _ss_cmeta(s1);
    const _sstring_t *m2 = _ss_cmeta(s2);

    if (m1->len != m2->len)
    {
        return false;
    }

    return 0 == ss_memcompare(s1, s2, m1->len);
}

/**
 * @note Uses memcmp by default. Make your own for local specific strings.
 * @return <1 if s1 < s2, >1 if s2 < s1, zero if equal.
 */
int
ss_compare(const SS s1, const SS s2)
{
    const _sstring_t *m1 = _ss_meta(s1);
    const _sstring_t *m2 = _ss_meta(s2);
    size_t len = m1->len < m2->len ? m1->len : m2->len;

    if (!len)
    {
        if (m1->len < m2->len)
        {
            return -1;
        }
        else if (m1->len > m2->len)
        {
            return 1;
        }
        else
        {
            return 0;
        }
    }

    return ss_memcompare(s1, s2, len);
}

/**
 * @return The index of the string to find; NPOS if not found.
 */
size_t
ss_find(const SS s, size_t index, const char *cs, size_t len)
{
    const _sstring_t *m = _ss_meta(s);

    if (len && m->len && index < m->len)
    {
        size_t searchlen = m->len - index;
        const char *cursor = s + index;
        for (;;)
        {
            if (*cs != *cursor)
            {
                cursor = ss_memchar(cursor, *cs, searchlen);
                if (!cursor)
                {
                    break;
                }
            }

            searchlen = m->len - (cursor - s);

            if (searchlen < len)
            {
                break;
            }

            if (0 == ss_memcompare(cursor, cs, len))
            {
                return cursor - s;
            }
            else
            {
                ++cursor;
                --searchlen;
            }
        }
    }

    return NPOS;
}

/**
 * @return The right-most index of the string to find; NPOS if not found.
 */
size_t
ss_rfind(const SS s, size_t index, const char *cs, size_t len)
{
    const _sstring_t *m = _ss_meta(s);

    if (index > m->len)
    {
        /* If m->len is zero is caught in the next statement. */
        index = m->len - 1;
    }

    if (len && m->len && index < m->len)
    {
        size_t last = len - 1;
        size_t searchlen = index + 1;
        const char *cursor = s + index;
        for (;;)
        {
            if (cs[last] != *cursor)
            {
                cursor = ss_memrchar(s, cs[last], searchlen - 1);
                if (!cursor)
                {
                    break;
                }
            }

            searchlen = (cursor - s) + 1;

            if (searchlen < len)
            {
                break;
            }

            if (0 == ss_memcompare(cursor - last, cs, len))
            {
                return (cursor - last) - s;
            }
            else
            {
                --cursor;
                --searchlen;
            }
        }
    }

    return NPOS;
}

/**
 * @return The count of the number of sub-strings found.
 */
size_t
ss_count(const SS s, size_t index, const char *cs, size_t len)
{
    size_t count = 0;

    if (len)
    {
        const _sstring_t *m = _ss_meta(s);

        if (index >= m->len)
        {
            return count;
        }

        size_t searchlen = m->len - index;
        const char *cursor = s + index;
        for (;;)
        {
            if (*cs != *cursor)
            {
                cursor = ss_memchar(cursor, *cs, searchlen);
                if (!cursor)
                {
                    break;
                }
            }

            searchlen = m->len - (cursor - s);

            if (searchlen < len)
            {
                break;
            }

            if (0 == ss_memcompare(cursor, cs, len))
            {
                ++count;
                cursor += len;
                searchlen -= len;
                
                if (searchlen < len)
                {
                    break;
                }
            }
            else
            {
                ++cursor;
                --searchlen;
            }
        }
    }

    return count;
}



/** @brief Pack 16-bit uint. */
#define packu16(BUF, I) (packi16((BUF), (I)))
/** @brief Pack 16-bit int. */
static void
packi16(unsigned char *buf, unsigned int i)
{
    *buf++ = i >> 8;
    *buf = i;
}

/** @brief Pack 32-bit uint. */
#define packu32(BUF, I) (packi32((BUF), (I)))
/** @brief Pack 32-bit int. */
static void
packi32(unsigned char *buf, uint32_t i)
{
    *buf++ = i >> 24;
    *buf++ = i >> 16;
    *buf++ = i >> 8;
    *buf = i;
}

/** @brief Pack 64-bit uint. */
#define packu64(BUF, I) (packi64((BUF), (I)))
/** @brief Pack 64-bit int. */
static void
packi64(unsigned char *buf, uint64_t i)
{
    *buf++ = i >> 56;
    *buf++ = i >> 48;
    *buf++ = i >> 40;
    *buf++ = i >> 32;
    *buf++ = i >> 24;
    *buf++ = i >> 16;
    *buf++ = i >> 8;
    *buf = i;
}

/** @return Unpack signed 16-bit int. */
static int16_t
unpacki16(const unsigned char *buf)
{
    unsigned int i2 = ((uint16_t)buf[0] << 8) | (uint16_t)buf[1];
    int i;

    /* Restore sign. */
    if (i2 <= 0x7fffu)
    {
        i = i2;
    }
    else
    {
        i = -1 - (unsigned int)(0xffffu - i2);
    }

    return (int16_t)i;
}

/** @return Unpack signed 16-bit uint. */
static uint16_t
unpacku16(const unsigned char *buf)
{
    return ((uint16_t)buf[0] << 8) | (uint16_t)buf[1];
}

/** @return Unpack signed 32-bit int. */
static int32_t
unpacki32(const unsigned char *buf)
{
    uint32_t i2 = ((uint32_t)buf[0] << 24) |
                           ((uint32_t)buf[1] << 16) |
                           ((uint32_t)buf[2] << 8)  |
                           (uint32_t)buf[3];
    int32_t i;

    /* Restore sign. */
    if (i2 <= 0x7fffffffu)
    {
        i = i2;
    }
    else
    {
        i = -1 - (int32_t)(0xffffffffu - i2);
    }

    return i;
}

/** @return Unpack signed 32-bit uint. */
static uint32_t
unpacku32(const unsigned char *buf)
{
    return ((uint32_t)buf[0] << 24) |
           ((uint32_t)buf[1] << 16) |
           ((uint32_t)buf[2] << 8)  |
           (uint32_t)buf[3];
}

/** @return Unpack signed 64-bit int. */
static int64_t
unpacki64(const unsigned char *buf)
{
    uint64_t i2 = ((uint64_t)buf[0] << 56) |
                                ((uint64_t)buf[1] << 48) |
                                ((uint64_t)buf[2] << 40) |
                                ((uint64_t)buf[3] << 32) |
                                ((uint64_t)buf[4] << 24) |
                                ((uint64_t)buf[5] << 16) |
                                ((uint64_t)buf[6] << 8)  |
                                buf[7];
    int64_t i;

    /* Restore sign. */
    if (i2 <= 0x7fffffffffffffffu)
    {
        i = i2;
    }
    else
    {
        i = -1 -(int64_t)(0xffffffffffffffffu - i2);
    }

    return i;
}

/** @return Unpack signed 64-bit uint. */
static uint64_t
unpacku64(const unsigned char *buf)
{
    return ((uint64_t)buf[0] << 56) |
           ((uint64_t)buf[1] << 48) |
           ((uint64_t)buf[2] << 40) |
           ((uint64_t)buf[3] << 32) |
           ((uint64_t)buf[4] << 24) |
           ((uint64_t)buf[5] << 16) |
           ((uint64_t)buf[6] << 8)  |
           (uint64_t)buf[7];
}

/**
 * @internal
 * @see https://wiki.sei.cmu.edu/confluence/display/c/MSC39-C.+Do+not+call+va_arg()+on+a+va_list+that+has+an+indeterminate+value
 * @see https://stackoverflow.com/questions/7084857/what-are-the-automatic-type-promotions-of-variadic-function-arguments
 * @see https://stackoverflow.com/questions/63849830/default-argument-promotion-in-a-variadic-function
 * @return Number of bytes written; NPOS on error.
 */
static size_t
_ss_packBE(size_t *caplen, unsigned char *buf, const char *fmt, va_list *argp)
{
    va_list ap;
    va_copy(ap, *argp);

    /* char, short, int */
    unsigned int h;
    uint32_t i;
    uint64_t q;

    size_t len = 0;
    size_t blen = *caplen;
    *caplen = 0;

    for (; fmt[0]; ++fmt)
    {
        switch (fmt[0])
        {
            case 'c':
            case 'b':
            case 'B':
                ++len;
                if (len > blen)
                {
                    *caplen = 1;
                    len = NPOS;
                    goto _pack_end;
                }
                h = va_arg(ap, unsigned int);
                *buf = (unsigned char)h;
                ++buf;
                break;
            case '?':
                ++len;
                if (len > blen)
                {
                    *caplen = 1;
                    len = NPOS;
                    goto _pack_end;
                }
                h = va_arg(ap, unsigned int);
                *buf = (h) ? 1 : 0;
                ++buf;
                break;
            case 'h':
            case 'H':
                len += 2;
                if (len > blen)
                {
                    *caplen = len - blen;
                    len = NPOS;
                    goto _pack_end;
                }
                h = va_arg(ap, unsigned int);
                packi16(buf, h);
                buf += 2;
                break;
            case 'i':
            case 'I':
                len += 4;
                if (len > blen)
                {
                    *caplen = len - blen;
                    len = NPOS;
                    goto _pack_end;
                }
                i = va_arg(ap, uint32_t);
                packi32(buf, i);
                buf += 4;
                break;
            case 'q':
            case 'Q':
                len += 8;
                if (len > blen)
                {
                    *caplen = len - blen;
                    len = NPOS;
                    goto _pack_end;
                }
                q = va_arg(ap, uint64_t);
                packi64(buf, q);
                buf += 8;
                break;
            default:
                len = NPOS;
                goto _pack_end;
                break;
        }
    }

    _pack_end:
    va_end(ap);

    return len;
}

/**
 * @brief Packs/serializes to a buffer using a Python inspired pack format.
 * @see https://docs.python.org/3/library/struct.html
 * @warning Floats are excluded and there are requirements for byte sequences.
 * @warning Allocate enough space or set a growth flag; otherwise this may be a slow method to execute.
 * @param s
 * @param fmt - The packing format string.
 * @return Number of bytes written; NPOS on error.
 *
 * #### Format Specification
 * Note that you need to provide pointers to all of the following.
 * c - char
 * b - signed char
 * B - unsigned char
 * ? - bool
 * h - int16_t
 * H - uint16_t
 * i - int32_t
 * I - uint32_t
 * q - int64_t
 * Q - uint64_t
 */
size_t
ss_packBE(SS *s, const char *fmt, ...)
{
    va_list argp;

    ss_clear(*s);

    _sstring_t *m = _ss_meta(*s);

    size_t cap = 0;
    size_t written = 0;

    do
    {
        if (cap)
        {
            m = _ss_realloc_grow(m, m->cap + cap);
        }
        cap = m->cap;

        va_start(argp, fmt);
        written = _ss_packBE(&cap, (unsigned char *)(_ss_string(m)), fmt, &argp);
        va_end(argp);
    } while (NPOS == written && cap);

    if (LIKELY(NPOS != written))
    {
        m->len = written;
    }
    else
    {
        m->len = 0;
    }
    *s = _ss_string(m);
    (*s)[m->len] = 0;

    return written;
}

/**
 * @brief Packs/serializes to a buffer.
 * @warning Allocate enough space or set a growth flag; otherwise this may be a slow method to execute.
 * @param s
 * @param fmt - The packing format string.
 * @return Number of bytes written; NPOS on error.
 */
size_t
ss_catpackBE(SS *s, const char *fmt, ...)
{
    va_list argp;

    _sstring_t *m = _ss_meta(*s);

    size_t cap = 0;
    size_t olen = m->len;
    size_t written = 0;

    do
    {
        if (cap)
        {
            m = _ss_realloc_grow(m, m->cap + cap);
        }
        cap = m->cap - olen;

        va_start(argp, fmt);
        written = _ss_packBE(&cap, &((unsigned char *)(_ss_string(m)))[olen], fmt, &argp);
        va_end(argp);
    } while (NPOS == written && cap);

    if (NPOS != written)
    {
        m->len = olen + written;
    }
    *s = _ss_string(m);
    (*s)[m->len] = 0;

    return written;
}



/**
 * @return Number of bytes processed; NPOS on error.
 */
static size_t
_ss_unpackBE(size_t blen, const unsigned char *buf, const char *fmt, va_list *argp)
{
	va_list ap;
    va_copy(ap, *argp);

    /* 8-bit */
    char *c;
	signed char *b;
	unsigned char *B;

    /* bool */
    bool *qmark;

    /* 16-bit */
	int16_t *h;
	uint16_t *H;

    /* 32-bit */
	int32_t *i;
	uint32_t *I;

    /* 64-bit */
	int64_t *q;
	uint64_t *Q;

	size_t len = 0;

	for(; fmt[0]; fmt++)
    {
		switch (fmt[0])
        {
            case 'c':
                len++;
                if (len > blen)
                {
                    len = NPOS;
                    goto _unpack_end;
                }
                c = va_arg(ap, char *);
                *c = *((char *)buf);
                buf++;
                break;
            case 'b':
                len++;
                if (len > blen)
                {
                    len = NPOS;
                    goto _unpack_end;
                }
                b = va_arg(ap, signed char *);
                *b = *((signed char *)buf);
                buf++;
                break;
            case 'B':
                len++;
                if (len > blen)
                {
                    len = NPOS;
                    goto _unpack_end;
                }
                B = va_arg(ap, unsigned char *);
                *B = *buf;
                buf++;
                break;
            case '?':
                len++;
                if (len > blen)
                {
                    len = NPOS;
                    goto _unpack_end;
                }
                qmark = va_arg(ap, bool *);
                *qmark = (*buf) ? true : false;
                buf++;
                break;
            case 'h':
                len += 2;
                if (len > blen)
                {
                    len = NPOS;
                    goto _unpack_end;
                }
                h = va_arg(ap, int16_t *);
                *h = unpacki16(buf);
                buf += 2;
                break;
            case 'H':
                len += 2;
                if (len > blen)
                {
                    len = NPOS;
                    goto _unpack_end;
                }
                H = va_arg(ap, uint16_t *);
                *H = unpacku16(buf);
                buf += 2;
                break;
            case 'i':
                len += 4;
                if (len > blen)
                {
                    len = NPOS;
                    goto _unpack_end;
                }
                i = va_arg(ap, int32_t *);
                *i = unpacki32(buf);
                buf += 4;
                break;
            case 'I':
                len += 4;
                if (len > blen)
                {
                    len = NPOS;
                    goto _unpack_end;
                }
                I = va_arg(ap, uint32_t *);
                *I = unpacku32(buf);
                buf += 4;
                break;
            case 'q':
                len += 8;
                if (len > blen)
                {
                    len = NPOS;
                    goto _unpack_end;
                }
                q = va_arg(ap, int64_t *);
                *q = unpacki64(buf);
                buf += 8;
                break;
            case 'Q':
                len += 8;
                if (len > blen)
                {
                    len = NPOS;
                    goto _unpack_end;
                }
                Q = va_arg(ap, uint64_t *);
                *Q = unpacku64(buf);
                buf += 8;
                break;
            default:
                len = NPOS;
                goto _unpack_end;
                break;
		}
	}

    _unpack_end:
	va_end(ap);

    return len;
}

/**
 * @brief Unpacks/deserializes a buffer. See ss_packBE documentation for more details.
 * @note The incoming bytes must be Big-Endian (network byte order).
 * @param s
 * @param fmt - The unpacking format string.
 * @return Number of bytes processed; NPOS on error.
 */
size_t
ss_unpackBE(const SS s, const char *fmt, ...)
{
    _sstring_t *m = _ss_meta(s);
    va_list argp;
    size_t n;

    if (!ss_isempty(s))
    {
        va_start(argp, fmt);
        n = _ss_unpackBE(ss_len(s), s, fmt, &argp);
        va_end(argp);

        return n;
    }
    else
    {
        return 0;
    }
}

/**
 * @brief Unpacks/deserializes a buffer. See ss_packBE documentation for more details.
 * @param blen - The length of the input buffer.
 * @param buf - The non-NULL buffer reference.
 * @param fmt - The unpacking format string.
 * @return Number of bytes processed; NPOS on error.
 */
size_t
ssb_unpackBE(size_t blen, unsigned char *buf, const char *fmt, ...)
{
    va_list argp;
    size_t n;

    if (blen)
    {
        va_start(argp, fmt);
        n = _ss_unpackBE(blen, buf, fmt, &argp);
        va_end(argp);

        return n;
    }
    else
    {
        return 0;
    }
}

/**
 * @brief Safely set the length of the string. Cannot exceed capacity.
 * @param s
 * @param len - The length to set the string to.
 */
void
ss_setlen(SS s, size_t len)
{
    _sstring_t *m = _ss_meta(s);

    /* Empty string will have cap of 0. */
    if (len <= m->cap)
    {
        s[len] = 0;
        m->len = len;
    }
}

/**
 * @brief Call to use strlen to set the length.
 * @note As a precaution the NULL sentinel is reset.
 * @param s
 */
void
ssc_setlen(SS s)
{
    _sstring_t *m = _ss_meta(s);

    /* Don't modify empty string. */
    if (m->cap)
    {
        s[m->cap] = 0;
    }

    size_t len = ss_cstrlen(s);

    if (len <= m->cap)
    {
        m->len = len;
        s[len] = 0;
    }
}

/**
 * @brief Set the growth rate option in the string's metadata.
 * @note It might seem odd to pass string by reference.
 *       This is done because of how empty string is implemented.
 * @param s
 * @param opt - The chosen growth rate.
 */
void
ss_setgrow(SS *s, enum ss_grow_opt opt)
{
    if (!_ss_is_type(*s, _SSTRING_EMPTY))
    {
        /* Stack string is mutable, so this is okay to set. */
        switch (opt)
        {
            case SS_GROW0:
            case SS_GROW25:
            case SS_GROW50:
            case SS_GROW100:
                {
                    uint32_t t = _ss_type(*s);
                    _ss_meta(*s)->type = (opt << _SS_GROW_SHIFT) | ((t) & ~_SS_GROW_MASK);
                }
                break;
        }
    }
    else
    {
        *s = _ss_string(((_sstring_t *)&g_ss_empty[opt]));
    }
}

/**
 * @brief Forces the string to be allocated onto the heap.
 * @param s
 */
void
ss_heapify(SS *s)
{
    if (!(_ss_type(*s) & _SS_HEAP_ALLOCATED))
    {
        const _sstring_t *m = _ss_cmeta(*s);
        _sstring_t *m2 = ss_rawalloc(sizeof(_sstring_t) + m->len + 1);

        SS s2 = _ss_string(m2);
        ss_memcopy(s2, *s, m->len + 1);
        m2->cap = m->len;
        m2->len = m->len;
        m2->type =
            _SS_HEAP_ALLOCATED
            | _SSTRING_NORM
            | (_SS_GROW_MASK & m->type);
        *s = s2;
    }
}

/**
 * @brief Swaps the two references.
 * @param s1
 * @param s2
 */
void
ss_swap(SS *s1, SS *s2)
{
    SS tmp = *s1;
    *s1 = *s2;
    *s2 = tmp;
}

/**
 * @brief Make sure the string can hold at least the given amount.
 * @note Growth disabled.
 * @param s
 * @param res - The amount to ensure the string can hold.
 */
void
ss_reserve(SS *s, size_t res)
{
    _sstring_t *m = _ss_meta(*s);

    /* Empty string will realloc. */
    if (m->cap < res)
    {
        m = _ss_realloc(m, res);
        *s = _ss_string(m);
    }
}

/**
 * @brief Reallocate to get rid of excess capacity.
 * @note Growth disabled.
 * @param s
 */
void
ss_fit(SS *s)
{
    _sstring_t *m = _ss_meta(*s);

    /* We don't want to fit empty.
     * We don't want to fit stack allocated.
     */
    if (m->cap != m->len && (_ss_type(*s) & _SS_HEAP_ALLOCATED))
    {
        m = _ss_realloc(m, m->len);
        *s = _ss_string(m);
    }
}

/**
 * @brief Set the capacity the amount given.
 * @note Differs from `ss_reserve` in that reserve will not reallocate
 *       if there is enough capacity.
 * @note Stack allocated strings will not switch to the heap if there is space.
 * @note Growth disabled.
 * @param s
 * @param res - The amount of capacity to have.
 */
void
ss_resize(SS *s, size_t res)
{
    _sstring_t *m = _ss_meta(*s);

    /* If stack allocated and has capacity, just return. */
    if (!(_ss_type(*s) & _SS_HEAP_ALLOCATED))
    {
        if (m->cap >= res)
        {
            return;
        }
    }

    if (m->cap != res)
    {
        bool truncate = res < m->len;

        m = _ss_realloc(m, res);
        *s = _ss_string(m);

        if (truncate)
        {
            m->len = res;
            (*s)[res] = 0;
        }
    }
}

/**
 * @brief Convenience function to add capacity to the string.
 * @note Growth disabled.
 * @param s
 * @param add - The amount to add.
 */
void
ss_addcap(SS *s, size_t add)
{
    _sstring_t *m = _ss_meta(*s);
    
    if (LIKELY(add))
    {
        size_t newcap = m->cap + add;
        if (newcap < m->cap)
        {
            newcap = _ss_cap_max();
        }

        m = _ss_realloc(m, newcap);
        *s = _ss_string(m);
    }
}

/**
 * @brief Clear the string (length to zero and sentinel in place).
 * @param s
 */
void
ss_clear(SS s)
{
    if (!_ss_is_type(s, _SSTRING_EMPTY))
    {
        _ss_meta(s)->len = 0;
        s[0] = 0;
    }
}

/**
 * @brief Remove the given substring from the string.
 * @param s
 * @param index - The place to start.
 * @param cs - The substring to remove.
 * @param len - The length of substring.
 */
void
ss_remove(SS s, size_t index, const char *cs, size_t len)
{
    _sstring_t *m = _ss_meta(s);

    if (len && m->len && index < m->len && (m->len - index) >= len)
    {
        size_t newlen = m->len;
        size_t searchlen = m->len - index;
        char *to = NULL;
        char *from = NULL;
        char *cursor = s + index;
        for (;;)
        {
            if (*cs != *cursor)
            {
                cursor = ss_mmemchar(cursor, *cs, searchlen);
                if (!cursor)
                {
                    break;
                }
            }

            searchlen = m->len - (cursor - s);

            if (searchlen < len)
            {
                break;
            }

            if (0 == ss_memcompare(cursor, cs, len))
            {
                newlen -= len;
                if (to)
                {
                    size_t movelen = cursor - from;
                    if (movelen)
                    {
                        ss_memmove(to, from, movelen);
                        to += movelen;
                    }
                }
                else
                {
                    to = cursor;
                }

                cursor += len;
                searchlen -= len;

                from = cursor;

                if (searchlen < len)
                {
                    break;
                }
            }
            else
            {
                ++cursor;
                --searchlen;
            }
        }

        if (to)
        {
            ss_memmove(to, from, ((s + m->len) - from) + 1);
        }

        m->len = newlen;
    }
}

/**
 * @brief Remove a section of string.
 * @param s
 * @param start - The starting point (inclusive).
 * @param end - The ending point (exclusive).
 */
void
ss_removerange(SS s, size_t start, size_t end)
{
    _sstring_t *m = _ss_meta(s);

    if (end > m->len)
    {
        end = m->len;
    }

    if (start >= end)
    {
        return;
    }

    size_t movelen = m->len - end + 1;
    ss_memmove(s + start, s + end, movelen);
    m->len -= (end - start);
}

/**
 * @brief Reverse the characters in the string.
 * @param s
 */
void
ss_reverse(SS s)
{
    _sstring_t *m = _ss_meta(s);

    if (m->len)
    {
        char *left = s;
        char *right = (s - 1) + m->len;
        while (left < right)
        {
            char tmp = *right;

            *right = *left;
            --right;

            *left = tmp;
            ++left;
        }
    }
}

/**
 * @brief Chop off the end of the string.
 * @note Length is updated and sentinel is set.
 * @param s
 * @param index - The place to truncate the string.
 */
void
ss_trunc(SS s, size_t index)
{
    _sstring_t *m = _ss_meta(s);

    if (index < m->len)
    {
        m->len = index;
        s[index] = 0;
    }
}

/**
 * @internal
 * @brief Internal trim method.
 * @param rstart - VALID start index.
 * @param rend - VALID end index.
 */
INLINE static void
_ss_trim(SS s, size_t rstart, size_t rend, const char *cs, size_t len)
{
    _sstring_t *m = _ss_meta(s);
    size_t start = rstart;
    size_t end = rend;

    while (start < end && ss_memchar(cs, s[start], len))
    {
        ++start;
    }
    while (start < end && ss_memchar(cs, s[end -1], len))
    {
        --end;
    }

    size_t rmovelen = end - start;

    if (start != rstart && rmovelen)
    {
        ss_memmove(s + rstart, s + start, rmovelen);
    }

    if (start != rstart || end != rend)
    {
        size_t movelen = (m->len - rend) + 1;
        ss_memmove(s + rstart + rmovelen, s + rend, movelen);
    }

    m->len -= (rend - rstart) - rmovelen;
}

/**
 * @brief Trim the characters from the beginning and end of the string.
 * @param s
 * @param cs - The substring listing the characters to trim.
 * @param len - The length of substring.
 */
void
ss_trim(SS s, const char *cs, size_t len)
{
    if (len)
    {
        _ss_trim(s, 0, ss_len(s), cs, len);
    }
}

/**
 * @brief Trim the characters from the beginning and end in the range.
 * @param s
 * @param rstart - The start of the range (inclusive).
 * @param rend - The end of the range (exclusive).
 * @param cs - The substring of characters to trim.
 * @param len - The length of the substring.
 */
void
ss_trimrange(SS s, size_t rstart, size_t rend, const char *cs, size_t len)
{
    if (len)
    {
        _sstring_t *m = _ss_meta(s);

        if (rend > m->len)
        {
            rend = m->len;
        }

        if (rstart >= rend)
        {
            return;
        }

        _ss_trim(s, rstart, rend, cs, len);
    }
}

/**
 * @brief Like the other trim methods, but assumes a cstring.
 * @note This is done because strchr is likely highly optimized.
 * @param s
 * @param cs - The substring of characters to trim.
 *             Using NULL will default to trimming US-ASCII whitespace.
 */
void
ssc_trim(SS s, const char *cs)
{
    _sstring_t *m = _ss_meta(s);
    size_t start = 0;
    size_t end = m->len;

    if (cs)
    {
        while (start < end && ss_cstrchar(cs, s[start]))
        {
            ++start;
        }
        while (start < end && ss_cstrchar(cs, s[end - 1]))
        {
            --end;
        }
    }
    else
    {
        while (start < end && isspace(s[start]))
        {
            ++start;
        }
        while (start < end && isspace(s[end - 1]))
        {
            --end;
        }
    }

    if (end != start)
    {
        m->len = end - start;
        if (start)
        {
            ss_memmove(s, s + start, m->len + 1);
        }
    }
    else
    {
        m->len = 0;
    }
    s[m->len] = 0;
}

/**
 * @brief Convert US-ASCII characters to uppercase.
 * @param s
 */
void
ssc_upper(SS s)
{
    while (*s)
    {
        *s = toupper(*s);
        ++s;
    }
}

/**
 * @brief Convert US-ASSII characters to lowercase.
 * @param s
 */
void
ssc_lower(SS s)
{
    while (*s)
    {
        *s = tolower(*s);
        ++s;
    }
}

/**
 * @internal
 * @return The numerical value of the hexidecimal US-ASCII character
 *         code point ('0'-'F', upper/lower).
 * @param c - The hex character.
 */
INLINE static unsigned char
_ss_tohexval(unsigned char c)
{
    static const unsigned char map[] =
    {
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        /* 0 starting at first item */
        0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0, 0, 0, 0, 0, 0,
        /* A starting at second item */
        0, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        /* a starting at second item */
        0, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,

        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    };
    return map[c];
}

/**
 * @internal
 * @param n - The uint32_t to count bits of.
 * @return Number of bits set.
 */
INLINE static int
_ss_pop32(uint32_t n)
{
    n = n - ((n >> 1) & 0x55555555);
    n = (n & 0x33333333) + ((n >> 2) & 0x33333333);
    return (((n + (n >> 4)) & 0x0F0F0F0F) * 0x01010101) >> 24;
}

/*
 * @internal
 * @warning DO NOT use GCC clz impl because the result when n = 0 is undefined.
 */
INLINE static int
_ss_clz32_impl(uint32_t n)
{
    n = n | (n >> 1);
    n = n | (n >> 2);
    n = n | (n >> 4);
    n = n | (n >> 8);
    n = n | (n >>16);
#if UINT_MAX == 4294967295U && defined __GNUC__
    return __builtin_popcount(~n);
#else
    return _ss_pop32(~n);
#endif
}

/**
 * @internal
 * @note Uses de Bruijn algorithm for minimal perfect hashing.
 * @see https://en.wikipedia.org/wiki/Find_first_set
 * @return Return most significant bit index or zero if none.
 */
INLINE static int
_ss_msb32_impl(uint32_t c)
{
    static char map[] =
    {
        0, 9, 1, 10, 13, 21, 2, 29, 11, 14, 16, 18, 22, 25, 3, 30,
        8, 12, 20, 28, 15, 17, 24, 7, 19, 27, 23, 6, 26, 5, 4, 31
    };

    if (UNLIKELY(!c))
    {
        return 0;
    }

    c = c | (c >>  1);
    c = c | (c >>  2);
    c = c | (c >>  4);
    c = c | (c >>  8);
    c = c | (c >> 16);

    return map[(c * 0x07C4ACDD) >> 27] + 1;
}

/**
 * @internal
 * @brief Find the number of bytes to encode the number of bits.
 * 
 * Bytes   bits (diff)
 * 1  0- 7 bits
 * 2  8-11 bits (4)
 * 3 12-16 bits (5)
 * 4 17-21 bits (5)
 * 5 22-26 bits (5)
 * 6 27-31 bits (5)
 * 7 32-36 bits (5)
 *
 * Add three to the number of bits to align to fives.
 * Divide by 5 to get the number of bytes to encode.
 *
 * @param c - Valid Unicode code point.
 */
INLINE static int
_ss_utf8len(unicode_t c)
{
    return c < 128 ? 1 : ((_ss_msb32_impl(c) + 3)/5);
}

INLINE static int
_ss_unicodetoutf8(unicode_t c, unsigned char *buf)
{
    if (c < 128)
    {
        *buf = c;
        return 1;
#if 0
        /* If we want to support technically invalid utf8 output.
         * Only listed here as a form of documentation.
         * Stuff like this does exist in the wild.
         */
        if (c)
        {
            *buf = c;
            return 1;
        }
        else
        {
            /* Basically, null is output as multibyte so it
             * can be included in normal string processing stuff.
             */
            buf[0] = 0xC0;
            buf[1] = 0x80;
            return 2;
        }
#endif
    }
    else if (UNLIKELY(!ssu_isvalid(c)))
    {
        return 0;
    }
    else
    {
        int len = _ss_utf8len(c);
        int i = len - 1;
        do
        {
            buf[i] = (unsigned char)(c & 0x0000003F) | 0x80;
            c >>= 6;
            --i;
        } while (i);

        buf [0] = (0xFF ^ ((1 << (8 - len)) - 1)) | c;

        return len;
    }
}

/**
 * @brief Unescape the characters according to standard c-string escape sequences.
 * @param s
 */
void
ssc_unesc(SS s)
{
    char buf[SS_UTF8_SEQ_MAX];
    char *p = s;
    char *to = NULL;
    char *from = NULL;
    _sstring_t *m = _ss_meta(s);
    size_t newlen = m->len;

    while (*p)
    {
        if ('\\' != *p)
        {
            ++p;
            p = ss_cstrchar(p, '\\');
            if (!p)
            {
                break;
            }
        }

        if (to)
        {
            if (to != from)
            {
                size_t movelen = p - from;
                if (movelen)
                {
                    ss_memmove(to, from, movelen);
                    to += movelen;
                }
            }
            else
            {
                to = p;
            }
        }
        else
        {
            to = p;
        }

        ++p;
        char ctest = *p;
        ++p;
        from = p;
        int written = 1;
        int removed = 2;
        switch (ctest)
        {
            case 'a':
                *to = 0x07;
                break;
            case 'b':
                *to = 0x08;
                break;
            case 'e':
                *to = 0x1B;
                break;
            case 'f':
                *to = 0x0C;
                break;
            case 'n':
                *to = 0x0A;
                break;
            case 'r':
                *to = 0x0D;
                break;
            case 't':
                *to = 0x09;
                break;
            case 'v':
                *to = 0x0B;
                break;
            case '\\':
                *to = 0x5C;
                break;
            case '\'':
                *to = 0x27;
                break;
            case '\"':
                *to = 0x22;
                break;
            case '?':
                *to = 0x3F;
                break;
            case 'x':
                {
                    if (isxdigit(*p))
                    {
                        unsigned char c = _ss_tohexval(*(unsigned char *)p);
                        ++p;
                        ++removed;
                        if (isxdigit(*p))
                        {
                            c <<= 4;
                            c ^= _ss_tohexval(*(unsigned char *)p);
                            ++p;
                            ++removed;
                        }
                        *to = (char)c;
                        from = p;
                    }
                    else
                    {
                        /* Ignore sequence */
                        from = p - 2;
                        written = 0;
                        removed = 0;
                    }
                }
                break;
            case 'u':
                {
                    if (isxdigit(*p))
                    {
                        unicode_t c = _ss_tohexval((unsigned char )*p);
                        ++p;
                        ++removed;

                        int i;
                        for (i = 0; i < 3; ++i, ++p, ++removed)
                        {
                            if (isxdigit(*p))
                            {
                                c <<= 4;
                                c ^= _ss_tohexval((unsigned char) *p);
                            }
                            else
                            {
                                break;
                            }
                        }
                        written = _ss_unicodetoutf8(c, (unsigned char *)buf);
                        ss_memcopy(to, buf, written);
                        from = p;
                    }
                    else
                    {
                        /* Ignore sequence */
                        from = p - 2;
                        written = 0;
                        removed = 0;
                    }
                }
                break;
            case 'U':
                {
                    if (isxdigit(*p))
                    {
                        unicode_t c = _ss_tohexval((unsigned char )*p);
                        ++p;
                        ++removed;

                        int i;
                        for (i = 0; i < 7; ++i, ++p, ++removed)
                        {
                            if (isxdigit(*p))
                            {
                                c <<= 4;
                                c ^= _ss_tohexval((unsigned char) *p);
                            }
                            else
                            {
                                break;
                            }
                        }
                        written = _ss_unicodetoutf8(c, (unsigned char *)buf);
                        ss_memcopy(to, buf, written);
                        from = p;
                    }
                    else
                    {
                        /* Ignore sequence */
                        from = p - 2;
                        written = 0;
                        removed = 0;
                    }
                }
                break;
            default:
                /* Handle octal sequences. */
                --p;
                if (isdigit(*p) && *p < 56)
                {
                    unsigned char c = _ss_tohexval(*p);
                    ++p;
                    // removed starts at 2
                    if (isdigit(*p) && *p < 56)
                    {
                        c <<= 3;
                        c ^= _ss_tohexval(*p);
                        ++p;
                        ++removed;
                        if (isdigit(*p) && *p < 56 && *(p - 2) < 52)
                        {
                            c <<= 3;
                            c ^= _ss_tohexval(*p);
                            ++p;
                            ++removed;
                        }
                    }

                    *to = c;
                    from = p;
                }
                else
                {
                    /* False positive. */
                    ++p;
                    from = p;
                    written = 2;
                    removed = 2;
                }
                break;
        }

        to += written;
        newlen -= (removed - written);
    }

    if (to && to != from)
    {
        ss_memmove(to, from, (s + m->len) - from + 1);
    }

    m->len = newlen;
}

/**
 * @brief Copy the substring into the string.
 * @param s
 * @param cs - The substring.
 * @param len - The lenght of substring.
 */
void
ss_copy(SS *s, const char *cs, size_t len)
{
    _sstring_t *m = _ss_meta(*s);

    if (len > m->cap)
    {
        m = _ss_realloc_grow(m, len);
        *s = _ss_string(m);
    }

    ss_memcopy(*s, cs, len);
    m->len = len;
    (*s)[m->len] = 0;
}

/**
 * @brief Concatenate the substring onto the string.
 * @param s
 * @param cs - The substring.
 * @param len - The lenght of substring.
 */
void
ss_cat(SS *s, const char *cs, size_t len)
{
    _sstring_t *m = _ss_meta(*s);

    if ((m->len + len) > m->cap)
    {
        m = _ss_realloc_grow(m, m->len + len);
        *s = _ss_string(m);
    }

    ss_memcopy(*s + m->len, cs, len);
    m->len += len;
    (*s)[m->len] = 0;
}

/**
 * @brief Left-concatenate the substring onto the string.
 * @param s
 * @param cs - The substring.
 * @param len - The lenght of substring.
 */
void
ss_lcat(SS *s, const char *cs, size_t len)
{
    _sstring_t *m = _ss_meta(*s);

    if ((m->len + len) > m->cap)
    {
        m = _ss_realloc_grow(m, m->len + len);
        *s = _ss_string(m);
    }

    ss_memmove(*s + len, *s, m->len + 1);
    ss_memcopy(*s, cs, len);
    m->len += len;
}

/**
 * @brief Replace a substring within the string with another value.
 * @param s
 * @param index - The index to start searching.
 * @param replace - The string to replace.
 * @param rlen - The length of `replace`.
 * @param with - The string to replace with.
 * @param wlen - The length of `with`.
 */
void
ss_replace(SS *s, size_t index, const char *replace, size_t rlen, const char *with, size_t wlen)
{
    if (!rlen)
    {
        return;
    }

    if (!wlen)
    {
        ss_remove(*s, index, replace, rlen);
        return;
    }

    _sstring_t *m = _ss_meta(*s);

    if (index >= m->len)
    {
        return;
    }

    if (wlen <= rlen)
    {
        /* Replace, inplace. Easy. */
        size_t newlen = m->len;
        size_t searchlen = m->len - index;
        char *to = NULL;
        char *from = NULL;
        char *cursor = *s + index;
        for (;;)
        {
            if (*replace != *cursor)
            {
                cursor = ss_mmemchar(cursor, *replace, searchlen);
                if (!cursor)
                {
                    break;
                }
            }

            searchlen = m->len - (cursor - *s);

            if (searchlen < rlen)
            {
                break;
            }

            if (0 == ss_memcompare(cursor, replace, rlen))
            {
                newlen -= (rlen - wlen);
                if (to)
                {
                    size_t movelen = cursor - from;
                    if (movelen && to != from)
                    {
                        ss_memmove(to, from, movelen);
                        to += movelen;
                    }
                }
                else
                {
                    to = cursor;
                }

                ss_memcopy(to, with, wlen);
                to += wlen;
                cursor += rlen;
                searchlen -= rlen;
                from = cursor;

                if (searchlen < rlen)
                {
                    break;
                }
            }
        }

        if (to && to != from)
        {
            ss_memmove(to, from, (*s + m->len) - from + 1);
        }

        m->len = newlen;
    }
    else
    {
        /* Replace with possible expansion. Hard. */
        size_t i = ss_find(*s, index, replace, rlen);
        if (i != NPOS)
        {
            size_t diff = wlen - rlen;
            size_t count = ss_count(*s, i + rlen, replace, rlen);
            ++count;
            size_t cap = (diff * count) + m->len;
            if (cap > m->cap)
            {
                m = _ss_realloc_grow(m, cap);
                *s = _ss_string(m);
            }

            /*
             * *s + i is start location of first replace
             *
             * Replace r with ww.
             * ccrccrcc
             *   ^
             * Memmove to end
             * Memcpy
             * ccwwfccrcc
             *   ^
             * we found the second one
             * ccwwfccrcc
             *        ^
             * ccwwccfrcc
             *       ^
             * ccwwccwwcc
             */
            size_t movelen = (m->len - (i + rlen)) + 1;
            char *cursor = *s + i;
            char *from = cursor + rlen + (diff * count);
            ss_memmove(from, cursor + rlen, movelen);
            ss_memcopy(cursor, with, wlen);

            char *to = cursor + wlen;
            cursor = from;

            m->len += diff * count;

            while (--count)
            {
                for (;;)
                {
                    if (*replace != *cursor)
                    {
                        ++cursor;
                        size_t searchlen = m->len - (cursor - *s);
                        cursor = ss_memchar(cursor + 1, *replace, searchlen);
                        /* Omit the NULL check since the count doesn't lie. */
                    }

                    if (0 == ss_memcompare(cursor, replace, rlen))
                    {
                        /* Move text to to space. */
                        movelen = cursor - from;
                        if (movelen)
                        {
                            ss_memmove(to, from, movelen);
                        }
                        /* Advance to space. */
                        to += movelen;
                        ss_memcopy(to, with, wlen);
                        /* Advance to space. */
                        to += wlen;
                        from = cursor + rlen;
                        cursor = from;
                        break;
                    }
                    else
                    {
                        ++cursor;
                    }

                }
            }
        }
    }
}

/**
 * @brief Replace a range with a substring.
 * @param s
 * @param start - The start of the range (inclusive).
 * @param end - End of the range (exclusive).
 * @param cs - The substring.
 * @param len - The length of substring.
 */
void
ss_replacerange(SS *s, size_t start, size_t end, const char *cs, size_t len)
{
    _sstring_t *m = _ss_meta(*s);

    if (end > m->len)
    {
        end = m->len;
    }

    if (start > end)
    {
        start = end;
    }

    size_t rlen = end - start;

    if (len > rlen)
    {
        if ((m->len + (len - rlen)) > m->cap)
        {
            m = _ss_realloc_grow(m , m->len + (len - rlen));
            *s = _ss_string(m);
        }
    }

    if (rlen != len)
    {
        size_t endlen = m->len - end;
        ss_memmove(*s + start + len, *s + end, endlen + 1);
    }

    ss_memcopy(*s + start, cs, len);
    m->len = (m->len - rlen) + len;
}

/**
 * @brief Insert the string at the given index.
 * @param s
 * @param index - Position to insert substring.
 * @param cs - Substring.
 * @param len - Substring length.
 */
void
ss_insert(SS *s, size_t index, const char *cs, size_t len)
{
    _sstring_t *m = _ss_meta(*s);

    if (index > m->len)
    {
        index = m->len;
    }

    if ((m->len + len) > m->cap)
    {
        m = _ss_realloc_grow(m, m->len + len);
        *s = _ss_string(m);
    }

    char *dest = *s + index + len;
    char *src = *s + index;
    size_t movelen = m->len - index + 1;
    ss_memmove(dest, src, movelen);
    ss_memcopy(src, cs, len);
    m->len += len;
    (*s)[m->len] = 0;
}

/**
 * @brief Places the substring over the existing string starting at the given position.
 * @param s
 * @param index - The start position.
 * @param cs - Substring.
 * @param len - Substring length.
 */
void
ss_overlay(SS *s, size_t index, const char *cs, size_t len)
{
    _sstring_t *m = _ss_meta(*s);

    if (index > m->len)
    {
        index = m->len;
    }

    size_t overend = index + len;

    if (overend > m->cap)
    {
        m = _ss_realloc_grow(m, overend);
        *s = _ss_string(m);
    }

    ss_memcopy(*s + index, cs, len);
    if (overend > m->len)
    {
        m->len = overend;
        (*s)[overend] = 0;
    }
}

/**
 * @internal
 */
int
_ss_catf(SS *s, const char *fmt, va_list *argp)
{
    va_list ap;
    _sstring_t *m = _ss_meta(*s);

    if (m->cap == 0)
    {
        m = _ss_realloc_grow(m, ss_cstrlen(fmt) + 1);
        *s = _ss_string(m);
    }

    for (;;)
    {
        va_copy(ap, *argp);
        int n = ss_vsnprintf(*s + m->len, m->cap - m->len + 1, fmt, ap);
        va_end(ap);

        if (n < 0)
        {
            /*
             * In pre glibc v2.1 libraries,
             * -1 indicates not all bytes were written.
             * @see https://linux.die.net/man/3/vsnprintf
             */
#ifdef __GNU_LIBRARY__
#if (__GLIBC__ < 2) || (__GLIBC__ == 2 && __GLIBC_MINOR__ == 0)
            if (-1 != n)
            {
#endif
#endif
            (*s)[m->len] = 0;
            return EINVAL;
#ifdef __GNU_LIBRARY__
#if (__GLIBC__ < 2) || (__GLIBC__ == 2 && __GLIBC_MINOR__ == 0)
            }
#endif
#endif
        }

        /* n could be -1, but this should be converted to largest value. */
        if ((size_t)n < (m->cap - m->len))
        {
            m->len += (size_t)n;
            break;
        }

        m = _ss_realloc_grow(m, m->cap + 1);
        *s = _ss_string(m);
    }

    return 0;
}

/**
 * @brief A very useful function to write information to the string using a
 *        format string.
 * @warning The contents of the buffer are not guaranteed if EINVAL returned.
 * @note If using GCC you should get warnings for bad format strings.
 * @param s
 * @param fmt - Format string.
 * @return EINVAL if bad format string; zero otherwise.
 */
int
ss_copyf(SS *s, const char *fmt, ...)
{
    va_list argp;
    int retval;

    ss_clear(*s);

    va_start(argp, fmt);
    retval = _ss_catf(s, fmt, &argp);
    va_end(argp);

    return retval;
#if 0
    _sstring_t *m = _ss_meta(*s);
    va_list argp;

    if (m->cap == 0)
    {
        m = _ss_realloc_grow(m, ss_cstrlen(fmt) + 1);
        *s = _ss_string(m);
    }

    for (;;)
    {
        va_start(argp, fmt);
        int n = ss_vsnprintf(*s, m->cap, fmt, argp);
        va_end(argp);

        if (n < 0)
        {
            /*
             * In pre glibc v2.1 libraries,
             * -1 indicates not all bytes were written.
             * @see https://linux.die.net/man/3/vsnprintf
             */
#ifdef __GNU_LIBRARY__
#if (__GLIBC__ < 2) || (__GLIBC__ == 2 && __GLIBC_MINOR__ == 0)
            if (-1 != n)
            {
#endif
#endif
            (*s)[0] = 0;
            return EINVAL;
#ifdef __GNU_LIBRARY__
#if (__GLIBC__ < 2) || (__GLIBC__ == 2 && __GLIBC_MINOR__ == 0)
            }
#endif
#endif
        }

        /* n could be -1, but this should be converted to largest value. */
        if ((size_t)n < m->cap)
        {
            m->len = (size_t)n;
            break;
        }

        m = _ss_realloc_grow(m, m->cap + 1);
        *s = _ss_string(m);
    }

    return 0;
#endif
}

/**
 * @brief A very useful function to write information to the end of a string 
 *        using a format string.
 * @note If using GCC you should get warnings for bad format strings.
 * @param s
 * @param fmt - Format string.
 * @return EINVAL if bad format string; zero otherwise.
 */
int
ss_catf(SS *s, const char *fmt, ...)
{
    va_list argp;
    int retval;

    va_start(argp, fmt);
    retval = _ss_catf(s, fmt, &argp);
    va_end(argp);

    return retval;
#if 0
    _sstring_t *m = _ss_meta(*s);
    va_list argp;

    if (m->cap == 0)
    {
        m = _ss_realloc_grow(m, ss_cstrlen(fmt) + 1);
        *s = _ss_string(m);
    }

    for (;;)
    {
        va_start(argp, fmt);
        int n = ss_vsnprintf(*s + m->len, m->cap - m->len + 1, fmt, argp);
        va_end(argp);

        if (n < 0)
        {
            /*
             * In pre glibc v2.1 libraries,
             * -1 indicates not all bytes were written.
             * @see https://linux.die.net/man/3/vsnprintf
             */
#ifdef __GNU_LIBRARY__
#if (__GLIBC__ < 2) || (__GLIBC__ == 2 && __GLIBC_MINOR__ == 0)
            if (-1 != n)
            {
#endif
#endif
            (*s)[m->len] = 0;
            return EINVAL;
#ifdef __GNU_LIBRARY__
#if (__GLIBC__ < 2) || (__GLIBC__ == 2 && __GLIBC_MINOR__ == 0)
            }
#endif
#endif
        }

        /* n could be -1, but this should be converted to largest value. */
        if ((size_t)n < (m->cap - m->len))
        {
            m->len += (size_t)n;
            break;
        }

        m = _ss_realloc_grow(m, m->cap + 1);
        *s = _ss_string(m);
    }

    return 0;
#endif
}

/**
 * @brief Concatenate the signed integral value in decimal.
 * @param s
 * @param val - The value to convert.
 */
void
ss_catint64(SS *s, int64_t val)
{
    char buf[32];
    char *p = buf;

    _sstring_t *m = _ss_meta(*s);

    /* Previously I was going to use the following to determine
     * the length in advance.
     * However, it may not be accurate/precise.
     *
     * int digits = (ss_bitscanforward(val) / 3) + 1;
     */

    if (val >= 0)
    {
        do
        {
            *p = '0' + (val % 10);
            ++p;
            val = val / 10;
        } while (val);
    }
    else
    {
        do
        {
            *p = '0' + (-1)*(val % 10);
            ++p;
            val = val / 10;
        } while (val);

        *p = '-';
        ++p;
        *p = 0;
    }

    size_t len = p - buf;
    if ((m->len + len) > m->cap)
    {
        m = _ss_realloc_grow(m, m->len + len);
        *s = _ss_string(m);
    }

    m->len += len;
    char *ss = *s;
    ss[m->len] = 0;
    do
    {
        --p;
        *ss = *p;
        ++ss;
    } while (p != buf);
}

/**
 * @brief Concatenate the unsigned integral value in decimal.
 * @param s
 * @param val - The value to convert.
 */
void
ss_catuint64(SS *s, uint64_t val)
{
    char buf[32];
    char *p = buf;

    _sstring_t *m = _ss_meta(*s);

    /* Previously I was going to use the following to determine
     * the length in advance.
     * However, it may not be accurate/precise.
     *
     * int digits = (ss_bitscanforward(val) / 3) + 1;
     */

    do
    {
        *p = '0' + (val % 10);
        ++p;
        val = val / 10;
    } while (val);

    size_t len = p - buf;
    if ((m->len + len) > m->cap)
    {
        m = _ss_realloc_grow(m, m->len + len);
        *s = _ss_string(m);
    }

    m->len += len;
    char *ss = *s;
    ss[m->len] = 0;
    do
    {
        --p;
        *ss = *p;
        ++ss;
    } while (p != buf);
}

INLINE static char
_ss_tohexchar(unsigned char nibble)
{
    static char map[] =
    {
        '0', '1', '2', '3',
        '4', '5', '6', '7',
        '8', '9', 'A', 'B',
        'C', 'D', 'E', 'F',
    };
    return map[nibble];
}

/**
 * @brief Escape the characters in the given string.
 * @param s
 */
void
ssc_esc(SS *s)
{
    _sstring_t *m = _ss_meta(*s);

    if (!m->len)
    {
        return;
    }

    SS t = ss_new(m->len * 2);
    ss_setgrow(&t, SS_GROW100);

    char *p = *s;
    while (*p)
    {
        switch (*p)
        {
            case 0x07:
                ss_cat(&t, "\\a", 2);
                break;
            case 0x08:
                ss_cat(&t, "\\b", 2);
                break;
            case 0x1B:
                ss_cat(&t, "\\e", 2);
                break;
            case 0x0C:
                ss_cat(&t, "\\f", 2);
                break;
            case 0x0A:
                ss_cat(&t, "\\n", 2);
                break;
            case 0x0D:
                ss_cat(&t, "\\r", 2);
                break;
            case 0x09:
                ss_cat(&t, "\\t", 2);
                break;
            case 0x0B:
                ss_cat(&t, "\\v", 2);
                break;
            case 0x5C:
                ss_cat(&t, "\\\\", 2);
                break;
            case 0x27:
                ss_cat(&t, "\\\'", 2);
                break;
            case 0x22:
                ss_cat(&t, "\\\"", 2);
                break;
            default:
                if (iscntrl(*p))
                {
                    char buf[4] = "\\x";
                    buf[2] = _ss_tohexchar(((unsigned char)*p) >> 4);
                    buf[3] = _ss_tohexchar(((unsigned char)*p) & 0x0F);
                    ss_cat(&t, buf, 4);
                }
                else
                {
                    ss_cat(&t, p, 1);
                }
                break;
        }
        ++p;
    }

    ss_copy(s, t, _ss_len(t));
    ss_free(&t);
}

/**
 * @param c - Unicode code point.
 * @return True if the code point is a valid Unicode code point.
 */
bool
ssu_isvalid(unicode_t c)
{
    /* The range 0xD800 to 0xDFFF is reserved for UTF-16 surrogate pairs.
     * Anything above 0x10FFFF is outside the valid code range.
     */
    return c < 0x0000D800 || (c > 0x0000DFFF && c <= 0x0010FFFF);
}

/**
 * @param c - Unicode code point.
 * @return Length of the UTF-8 sequence that would result from the given code point.
 */
int
ssu8_cpseqlen(unicode_t c)
{
    return ssu_isvalid(c) ? _ss_utf8len(c) : 0;
}

INLINE static int
_ss_seqlen(const char *seq)
{
    /* There are only 21 bits used for the Unicode standard.
     * This means only 4 bytes are needed, max.
     * So everything above 111110xx are not needed.
     */
    unsigned char c = *((const unsigned char *)seq);
    if (LIKELY(c < 0x80))
    {
        return 1;
    }
    if (UNLIKELY(c > 0xF7 || (c & 0xC0) == 0x80))
    {
        return 0;
    }
    else
    {
        /* We know that at least the leading bit is set.
         * Flip bits in c.
         */
        uint32_t z = (uint32_t)0x000000FF ^ (uint32_t)c;
        return _ss_clz32_impl(z) - 24;
    }
}

/**
 * @return Zero for invalid; number of bytes otherwise;
 *         continuation bytes will return 0.
 */
int
ssu8_seqlen(const char *seq)
{
    return _ss_seqlen(seq);
}

/**
 * @warning This will encode invalid code points. Use ssu_isvalid to check.
 * @return Sequence length, zero if invalid.
 */
int
ssu8_cptoseq(unicode_t c, char *seq)
{
    return _ss_unicodetoutf8(c, (unsigned char *)seq);
}

/**
 * @brief Convert the given sequence to a Unicode point in `cpout`.
 * @param seq - Pointer to the UTF-8 sequence.
 * @param cpout - Pointer to Unicode code point storage.
 * @return The length of the sequence; zero on failure.
 */
int
ssu8_seqtocp(const char *seq, unicode_t *cpout)
{
    const unsigned char *s = (const unsigned char *)seq;

    /* z should always be > 0. */
    int z = _ss_seqlen(seq);

    unicode_t c;
    /* 0xxxxxxx
     * 10xxxxxx invalid
     * 110xxxxx
     * 1110xxxx
     * 11110xxx
     * 111110xx invalid
     * 1111110x invalid
     * 11111110 invalid
     */
    switch (z)
    {
#if 0
        /* Moved to default case for code coverage/catch-all. */
        case 0:
            return 0;
#endif
        case 1:
            *cpout = *s;
            return 1;
        case 2:
            if (UNLIKELY((s[1] & 0xC0) != 0x80))
            {
                return 0;
            }
            c = ((0x1F & s[0]) << 6) | (0x3F & s[1]);
            *cpout = c;
            return 2;
        case 3:
            if (UNLIKELY(((s[1] & 0xC0) != 0x80)
                         && ((s[2] & 0xC0) != 0x80)))
            {
                return 0;
            }
            c = ((0x0F & s[0]) << 12)
                | ((0x3F & s[1]) << 6)
                | ((0x3F & s[2]));
            *cpout = c;
            return 3;
        case 4:
            if (UNLIKELY(((s[1] & 0xC0) != 0x80)
                         && ((s[2] & 0xC0) != 0x80)
                         && ((s[3] & 0xC0) != 0x80)))
            {
                return 0;
            }
            c = ((0x07 & s[0]) << 18)
                | ((0x3F & s[1]) << 12)
                | ((0x3F & s[2]) << 6)
                | ((0x3F & s[3]));
            *cpout = c;
            return 4;
#if 0
        case 5:
            if (UNLIKELY(((s[1] & 0xC0) != 0x80)
                         && ((s[2] & 0xC0) != 0x80)
                         && ((s[3] & 0xC0) != 0x80)
                         && ((s[4] & 0xC0) != 0x80)))
            {
                return 0;
            }
            c = ((0x03 & s[0]) << 24)
                | ((0x3F & s[1]) << 18)
                | ((0x3F & s[2]) << 12)
                | ((0x3F & s[3]) << 6)
                | ((0x3F & s[4]));
            *cpout = c;
            return 5;
        case 6:
            if (UNLIKELY(((s[1] & 0xC0) != 0x80)
                         && ((s[2] & 0xC0) != 0x80)
                         && ((s[3] & 0xC0) != 0x80)
                         && ((s[4] & 0xC0) != 0x80)
                         && ((s[5] & 0xC0) != 0x80)))
            {
                return 0;
            }
            c = ((0x01 & s[0]) << 30)
                | ((0x3F & s[1]) << 24)
                | ((0x3F & s[2]) << 18)
                | ((0x3F & s[3]) << 12)
                | ((0x3F & s[4]) << 6)
                | ((0x3F & s[5]));
            *cpout = c;
            return 6;
        case 7:
            return 0;
        case 8:
            return 0;
#endif
        default:
            break;
    }

    return 0;
}

/**
 * @return Number of leading zeros; returns 32 when 0.
 */
int
sse_clz32(uint32_t n)
{
    return _ss_clz32_impl(n);
}

/**
 * @return Index of most significant bit; zero when input is 0.
 */
int
sse_msb32(uint32_t n)
{
    return _ss_msb32_impl(n);
}

/**
 * @return Pointer to the right-most character in the buffer; NULL if not found.
 */
const char *
sse_memrchar(const char *buf, char find, size_t len)
{
    return _ss_memrchar(buf, find, len);
}

