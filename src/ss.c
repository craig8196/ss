
#include "ss.h"
#include "ss_util.h"

#include <ctype.h>
#include <errno.h>
#include <features.h>
#include <limits.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>

#ifdef __GNUC__
#define LIKELY(x)   __builtin_expect(!!(x), 1)
#define UNLIKELY(x) __builtin_expect(!!(x), 0)
#else
#define LIKELY(x)   (x)
#define UNLIKELY(x) (x)
#endif

#ifdef __GNUC__
#define INLINE __attribute__((always_inline)) inline
#define NOINLINE __attribute__((noinline))
#else
#define INLINE
#define NOINLINE
#endif

#ifdef SS_MEMCHECK
#define _SS_MEMRETVAL int
#define _SS_MEMRETZERO return 0
#else
#define _SS_MEMRETVAL void
#define _SS_MEMRETZERO return
#endif

#define SS_GROW_MAX (4)
#define SS_GROW_SHIFT (16)
#define SS_TYPE_MASK (0x0000FFFF)
#define SS_GROW_MASK (0xFFFF0000)
#define SS_HEAP_ALLOCATED (0x00000100)

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

/**
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

static _sstring_empty_t g_ss_empty[SS_GROW_MAX] =
{
    { { 0, 0, SS_GROW0   << SS_GROW_SHIFT }, 0 },
    { { 0, 0, SS_GROW25  << SS_GROW_SHIFT }, 0 },
    { { 0, 0, SS_GROW50  << SS_GROW_SHIFT }, 0 },
    { { 0, 0, SS_GROW100 << SS_GROW_SHIFT }, 0 },
};

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

INLINE static SS
_ss_string(_sstring_t *m)
{
    return (SS)(m + 1);
}

INLINE static size_t
_ss_len(const SS s)
{
    return _ss_cmeta(s)->len;
}

INLINE static uint32_t
_ss_type(SS s)
{
    return _ss_meta(s)->type;
}

INLINE static bool
_ss_is_type(SS s, enum _sstring_type t)
{
    return (_ss_meta(s)->type & SS_TYPE_MASK) == (uint32_t)t;
}

INLINE static size_t
_ss_cap_max(void)
{
    return ((size_t)INT_MAX) - 2 - sizeof(_sstring_t);
}

INLINE static bool
_ss_valid_cap(size_t cap)
{
    return !!(cap <= _ss_cap_max());
}

INLINE static _sstring_t *
_ss_realloc(_sstring_t *m, size_t cap)
{
    _sstring_t *m2;

    if (m->type & SS_GROW_MASK)
    {
        size_t growcap = 0;

        switch (m->type >> SS_GROW_SHIFT)
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

    if (m->type & SS_HEAP_ALLOCATED)
    {
        m2 = ss_rawrealloc(m, sizeof(_sstring_t) + cap + 1);
        if (m2)
        {
            m2->cap = cap;
        }
    }
    else
    {
        m2 = malloc(sizeof(_sstring_t) + cap + 1);
        if (m2)
        {
            m2->cap = cap;
            m2->len = m->len;
            m2->type =
                (m->type & SS_GROW_MASK)
                | SS_HEAP_ALLOCATED
                | _SSTRING_NORM;
            ss_memcopy((SS)(m2 + 1), (SS)(m + 1), m->len + 1);
        }
    }
    return m2;
}

const char *
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

SS
ss_empty(void)
{
    return _ss_string((_sstring_t *)&g_ss_empty[0]);
}

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
        m->type = SS_HEAP_ALLOCATED | _SSTRING_NORM;
        s = _ss_string(m);
        s[0] = 0;
    }

    return s;
}

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
        m->type = SS_HEAP_ALLOCATED | _SSTRING_NORM;
        s = _ss_string(m);
        if (len)
        {
            ss_memcopy(s, cs, len);
        }
        s[len] = 0;
    }

    return s;
}

SS
ss_dup(SS s)
{
    return ss_newfrom(0, s, ss_len(s));
}

void
ss_free(SS *s) 
{
    /* Empty string and stack string will not have flag set. */
    if ((_ss_type(*s)) & SS_HEAP_ALLOCATED)
    {
        ss_rawfree(_ss_meta(*s));
    }
    (*s) = NULL;
}

size_t
ss_len(const SS s)
{
    return _ss_len(s);
}

size_t
ss_cap(const SS s)
{
    return _ss_cmeta(s)->cap;
}

bool
ss_isempty(const SS s)
{
    return !(_ss_len(s));
}

bool
ss_isemptytype(const SS s)
{
    return _ss_is_type(s, _SSTRING_EMPTY);
}

bool
ss_isheaptype(const SS s)
{
    return ((_ss_type(s)) & SS_HEAP_ALLOCATED);
}

bool
ss_isstacktype(const SS s)
{
    return _ss_is_type(s, _SSTRING_STACK);
}

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

void
ssc_setlen(SS s)
{
    _sstring_t *m = _ss_meta(s);

    if (m->cap)
    {
        /* Don't modify empty string. */
        s[m->cap] = 0;
    }

    size_t len = strlen(s);

    if (len <= m->cap)
    {
        m->len = len;
        s[len] = 0;
    }
}

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
                    _ss_meta(*s)->type = (opt << SS_GROW_SHIFT) | ((t) & ~SS_GROW_MASK);
                }
                break;
        }
    }
    else
    {
        *s = _ss_string(((_sstring_t *)&g_ss_empty[opt]));
    }
}

_SS_MEMRETVAL
ss_heapify(SS *s)
{
    if (!(_ss_type(*s) & SS_HEAP_ALLOCATED))
    {
        const _sstring_t *m = _ss_cmeta(*s);
        _sstring_t *m2 = ss_rawalloc(sizeof(_sstring_t) + m->len + 1);
#ifdef SS_MEMCHECK
        if (!m2)
        {
            return ENOMEM;
        }
#endif

        SS s2 = _ss_string(m2);
        ss_memcopy(s2, *s, m->len + 1);
        m2->cap = m->len;
        m2->len = m->len;
        m2->type =
            SS_HEAP_ALLOCATED
            | _SSTRING_NORM
            | (SS_GROW_MASK & m->type);
        *s = s2;
    }

#ifdef SS_MEMCHECK
    return 0;
#endif
}

void
ss_swap(SS *s1, SS *s2)
{
    SS tmp = *s1;
    *s1 = *s2;
    *s2 = tmp;
}

_SS_MEMRETVAL
ss_reserve(SS *s, size_t res)
{
    _sstring_t *m = _ss_meta(*s);

    /* Empty string will realloc. */
    if (m->cap < res)
    {
        m = _ss_realloc(m, res);
#ifdef SS_MEMCHECK
        if (!m)
        {
            return ENOMEM;
        }
#endif

        *s = _ss_string(m);
    }

#ifdef SS_MEMCHECK
    return 0;
#endif
}

_SS_MEMRETVAL
ss_fit(SS *s)
{
    _sstring_t *m = _ss_meta(*s);

    /* We don't want to fit empty.
     * We don't want to fit stack allocated.
     */
    if (m->cap != m->len && (_ss_type(*s) & SS_HEAP_ALLOCATED))
    {
        uint32_t before = m->type;
        m->type = m->type & SS_TYPE_MASK;
        m = _ss_realloc(m, m->len);
#ifdef SS_MEMCHECK
        if (!m)
        {
            m = _ss_meta(*s);
            m->type = before;
            return ENOMEM;
        }
#endif
        m->type = before;

        *s = _ss_string(m);
    }

#ifdef SS_MEMCHECK
    return 0;
#endif
}

_SS_MEMRETVAL
ss_resize(SS *s, size_t res)
{
    _sstring_t *m = _ss_meta(*s);

    /* If stack allocated and has capacity, just return. */
    if (!(_ss_type(*s) & SS_HEAP_ALLOCATED))
    {
        if (m->cap >= res)
        {
#ifdef SS_MEMCHECK
            return 0;
#else
            return;
#endif
        }
    }

    if (m->cap != res)
    {
        bool truncate = res < m->len;

        m = _ss_realloc(m, res);
#ifdef SS_MEMCHECK
        if (!m)
        {
            return ENOMEM;
        }
#endif

        *s = _ss_string(m);

        if (truncate)
        {
            m->len = res;
            (*s)[res] = 0;
        }
    }

#ifdef SS_MEMCHECK
    return 0;
#endif
}

_SS_MEMRETVAL
ss_addcap(SS *s, size_t add)
{
    _sstring_t *m = _ss_meta(*s);

    
    size_t newcap = m->cap + add;
    if (newcap < m->cap)
    {
#ifdef SS_MEMCHECK
        return EINVAL;
#else
        newcap = _ss_cap_max();
#endif
    }

    m = _ss_realloc(m, newcap);
#ifdef SS_MEMCHECK
    if (!m)
    {
        return ENOMEM;
    }
#endif

    *s = _ss_string(m);

#ifdef SS_MEMCHECK
    return 0;
#endif
}

void
ss_clear(SS s)
{
    if (!_ss_is_type(s, _SSTRING_EMPTY))
    {
        _ss_meta(s)->len = 0;
        s[0] = 0;
    }
}

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

void
ss_trim(SS s, const char *cs, size_t len)
{
    if (len)
    {
        _sstring_t *m = _ss_meta(s);

        size_t start = 0;
        size_t end = m->len;

        while (start < end && ss_memchar(cs, s[start], len))
        {
            ++start;
        }
        while (start < end && ss_memchar(cs, s[end - 1], len))
        {
            --end;
        }

        if (start != end)
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
}

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

        if (rstart > rend)
        {
            rstart = rend;
        }

        if (rstart == rend)
        {
            return;
        }

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
}

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

void
ssc_upper(SS s)
{
    while (*s)
    {
        *s = toupper(*s);
        ++s;
    }
}

void
ssc_lower(SS s)
{
    while (*s)
    {
        *s = tolower(*s);
        ++s;
    }
}

INLINE static unsigned char
_ss_tohexval(unsigned char c)
{
    static unsigned char map[] =
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

INLINE static int
_ss_pop32(uint32_t n)
{
    n = n - ((n >> 1) & 0x55555555);
    n = (n & 0x33333333) + ((n >> 2) & 0x33333333);
    return (((n + (n >> 4)) & 0x0F0F0F0F) * 0x01010101) >> 24;
}

/**
 * Note that we don't use GCC clz impl
 * because the result when n = 0 is undefined.
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
 * Uses de Bruijn algorithm for minimal perfect hashing.
 * @see https://en.wikipedia.org/wiki/Find_first_set
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
 * Find the number of bytes to encode the number of bits.
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

_SS_MEMRETVAL
ss_copy(SS *s, const char *cs, size_t len)
{
    _sstring_t *m = _ss_meta(*s);

    if (len > m->cap)
    {
        m = _ss_realloc(m, len);
#ifdef SS_MEMCHECK
        if (!m)
        {
            return ENOMEM;
        }
#endif

        *s = _ss_string(m);
    }

    ss_memcopy(*s, cs, len);
    m->len = len;
    (*s)[m->len] = 0;

#ifdef SS_MEMCHECK
    return 0;
#endif
}

_SS_MEMRETVAL
ss_cat(SS *s, const char *cs, size_t len)
{
    _sstring_t *m = _ss_meta(*s);

    if ((m->len + len) > m->cap)
    {
        m = _ss_realloc(m, m->len + len);
#ifdef SS_MEMCHECK
        if (!m)
        {
            return ENOMEM;
        }
#endif

        *s = _ss_string(m);
    }

    ss_memcopy(*s + m->len, cs, len);
    m->len += len;
    (*s)[m->len] = 0;

#ifdef SS_MEMCHECK
    return 0;
#endif
}

_SS_MEMRETVAL
ss_lcat(SS *s, const char *cs, size_t len)
{
    _sstring_t *m = _ss_meta(*s);

    if ((m->len + len) > m->cap)
    {
        m = _ss_realloc(m, m->len + len);
#ifdef SS_MEMCHECK
        if (!m)
        {
            return ENOMEM;
        }
#endif

        *s = _ss_string(m);
    }

    ss_memmove(*s + len, *s, m->len + 1);
    ss_memcopy(*s, cs, len);
    m->len += len;

#ifdef SS_MEMCHECK
    return 0;
#endif
}

_SS_MEMRETVAL
ss_replace(SS *s, size_t index, const char *replace, size_t rlen, const char *with, size_t wlen)
{
    if (!rlen)
    {
        return;
    }

    if (!wlen)
    {
        ss_remove(*s, index, replace, rlen);
#ifdef SS_MEMCHECK
        return 0;
#else
        return;
#endif
    }

    _sstring_t *m = _ss_meta(*s);

    if (index >= m->len)
    {
#ifdef SS_MEMCHECK
        return 0;
#else
        return;
#endif
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
                m = _ss_realloc(m, cap);
#ifdef SS_MEMCHECK
                if (!m)
                {
                    return ENOMEM;
                }
#endif

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

#ifdef SS_MEMCHECK
    return 0;
#endif
}

_SS_MEMRETVAL
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
            m = _ss_realloc(m , m->len + (len - rlen));
#ifdef SS_MEMCHECK
            if (!m)
            {
                return ENOMEM;
            }
#endif

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

#ifdef SS_MEMCHECK
    return 0;
#endif
}

_SS_MEMRETVAL
ss_insert(SS *s, size_t index, const char *cs, size_t len)
{
    _sstring_t *m = _ss_meta(*s);

    if (index > m->len)
    {
        index = m->len;
    }

    if ((m->len + len) > m->cap)
    {
        m = _ss_realloc(m, m->len + len);
#ifdef SS_MEMCHECK
        if (!m)
        {
            return ENOMEM;
        }
#endif

        *s = _ss_string(m);
    }

    char *dest = *s + index + len;
    char *src = *s + index;
    size_t movelen = m->len - index + 1;
    ss_memmove(dest, src, movelen);
    ss_memcopy(src, cs, len);
    m->len += len;
    (*s)[m->len] = 0;

#ifdef SS_MEMCHECK
    return 0;
#endif
}

_SS_MEMRETVAL
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
        m = _ss_realloc(m, overend);
#ifdef SS_MEMCHECK
        if (!m)
        {
            return ENOMEM;
        }
#endif

        *s = _ss_string(m);
    }

    ss_memcopy(*s + index, cs, len);
    if (overend > m->len)
    {
        m->len = overend;
        (*s)[overend] = 0;
    }

#ifdef SS_MEMCHECK
    return 0;
#endif
}

int
ss_copyf(SS *s, const char *fmt, ...)
{
    _sstring_t *m = _ss_meta(*s);
    va_list argp;

    if (m->cap == 0)
    {
        m = _ss_realloc(m, ss_cstrlen(fmt) + 1);
#ifdef SS_MEMCHECK
        if (!m)
        {
            return ENOMEM;
        }
#endif

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

        m = _ss_realloc(m, m->cap + 1);
#ifdef SS_MEMCHECK
        if (!m)
        {
            _ss_meta(*s)->len = 0;
            (*s)[0] = 0;
            return ENOMEM;
        }
#endif

        *s = _ss_string(m);
    }

    return 0;
}

int
ss_catf(SS *s, const char *fmt, ...)
{
    _sstring_t *m = _ss_meta(*s);
    va_list argp;

    if (m->cap == 0)
    {
        m = _ss_realloc(m, ss_cstrlen(fmt) + 1);
#ifdef SS_MEMCHECK
        if (!m)
        {
            return ENOMEM;
        }
#endif

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

        m = _ss_realloc(m, m->cap + 1);
#ifdef SS_MEMCHECK
        if (!m)
        {
            (*s)[ss_len(*s)] = 0;
            return ENOMEM;
        }
#endif

        *s = _ss_string(m);
    }

    return 0;
}

_SS_MEMRETVAL
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
        m = _ss_realloc(m, m->len + len);
#ifdef SS_MEMCHECK
        if (!m)
        {
            return ENOMEM;
        }
#endif

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

#ifdef SS_MEMCHECK
    return 0;
#endif
}

_SS_MEMRETVAL
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
        m = _ss_realloc(m, m->len + len);
#ifdef SS_MEMCHECK
        if (!m)
        {
            return ENOMEM;
        }
#endif

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

#ifdef SS_MEMCHECK
    return 0;
#endif
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

_SS_MEMRETVAL
ssc_esc(SS *s)
{
    _sstring_t *m = _ss_meta(*s);

    if (!m->len)
    {
#ifdef SS_MEMCHECK
        return 0;
#else
        return;
#endif
    }

    SS t = ss_new(m->len * 2);
#ifdef SS_MEMCHECK
    if (!t)
    {
        return ENOMEM;
    }
#endif
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

#ifdef SS_MEMCHECK
    int code = ss_copy(s, t, _ss_len(t));

    ss_free(&t);

    return code;
#else
    ss_copy(s, t, _ss_len(t));
    ss_free(&t);
#endif

}

bool
ssu_isvalid(unicode_t c)
{
    /* The range 0xD800 to 0xDFFF is reserved for UTF-16 surrogate pairs.
     * Anything above 0x10FFFF is outside the valid code range.
     */
    return c < 0x0000D800 || (c > 0x0000DFFF && c <= 0x0010FFFF);
}

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
 * @warn This will encode invalid code points. Use ssu_isvalid to check.
 * @return Sequence length, zero if invalid.
 */
int
ssu8_cptoseq(unicode_t c, char *seq)
{
    return _ss_unicodetoutf8(c, (unsigned char *)seq);
}

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

int
sse_clz32(uint32_t n)
{
    return _ss_clz32_impl(n);
}

int
sse_msb32(uint32_t n)
{
    return _ss_msb32_impl(n);
}

const char *
sse_memrchar(const char *buf, char find, size_t len)
{
    return _ss_memrchar(buf, find, len);
}

