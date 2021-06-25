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
 * @file sstring.h
 * @author Craig Jacobson
 * @brief Simple string implementation.
 *
 * Prefixes:
 * ss_ is for manipulating binary ss buffers.
 * ssc_ is for manipulating ss buffers assumed to be c strings (US ASCII/UTF8).
 * ssu_ is for unicode operations, UTF8 is the unicode encoding used.
 * sse_ is for exporting internal functions primarily for testing.
 */
#ifndef SS_H_
#define SS_H_
#ifdef __cplusplus
extern "C" {
#endif


#include <stdbool.h>
#include <stdlib.h>
#include <stdint.h>


#ifndef NPOS
#define NPOS ((size_t)-1)
#endif

typedef char * SS;

enum ss_grow_opt
{
    SS_GROWFIT = 0,
    SS_GROW0   = 0,
    SS_GROW25  = 1,
    SS_GROW50  = 2,
    SS_GROW100 = 3,
};


/* Constructors */
SS
ss_empty(void);
SS
ss_new(size_t);
SS
ss_newfrom(size_t, const char *, size_t);
SS
ss_dup(SS);
void
ss_free(SS *);

SS
ssc_new(size_t, const char *);
SS
ssc_dup(const char *);

#define SS_HEADER_SIZE ((sizeof(size_t)*2) + (sizeof(uint32_t)))

#define ss_stack(NAME, CAP) \
    char _ ## NAME ## _internal_buf[SS_HEADER_SIZE + (CAP) + 1]; \
    SS NAME = (SS)(&_ ## NAME ## _internal_buf[SS_HEADER_SIZE]); \
    { \
        size_t *m = (size_t *)_ ## NAME ## _internal_buf; \
        *m = (CAP); \
        ++m; \
        *m = 0; \
        ++m; \
        *((uint32_t *)m) = 1; \
        NAME[0] = 0; \
    }


/* Queries */
size_t
ss_len(const SS);
size_t
ss_cap(const SS);
bool
ss_isempty(const SS);
bool
ss_isemptytype(const SS);
bool
ss_isheaptype(const SS);
bool
ss_isstacktype(const SS);
bool
ss_equal(const SS, const SS);
int
ss_compare(const SS, const SS);
size_t
ss_find(const SS, size_t, const char *, size_t);
size_t
ss_rfind(const SS, size_t, const char *, size_t);
size_t
ss_count(const SS, size_t, const char *, size_t);

/* Adjust Length */
void
ss_setlen(SS, size_t);
void
ssc_setlen(SS);

/* Modify Meta */
void
ss_setgrow(SS *, enum ss_grow_opt);
void
ss_heapify(SS *);
void
ss_swap(SS *, SS *);
void
ss_reserve(SS *, size_t);
void
ss_fit(SS *);
void
ss_resize(SS *, size_t);
void
ss_addcap(SS *, size_t);


/* Modify without Realloc */
void
ss_clear(SS);
void
ss_remove(SS, size_t, const char *, size_t);
void
ss_removerange(SS, size_t, size_t);
void
ss_reverse(SS);
void
ss_trunc(SS, size_t);
void
ss_trim(SS, const char *, size_t);
void
ss_trimrange(SS, size_t, size_t, const char *, size_t);

void
ssc_trim(SS, const char *);
void
ssc_upper(SS);
void
ssc_lower(SS);
void
ssc_unesc(SS);

/* Modify */
void
ss_copy(SS *, const char *, size_t);
void
ss_cat(SS *, const char *, size_t);
void
ss_lcat(SS *, const char *, size_t);
void
ss_replace(SS *, size_t, const char *, size_t, const char *, size_t);
void
ss_replacerange(SS *, size_t, size_t, const char *, size_t);
void
ss_insert(SS *, size_t, const char *, size_t);
void
ss_overlay(SS *, size_t, const char *, size_t);
#ifdef __GNU_LIBRARY__
int
ss_copyf(SS *, const char *fmt, ...)
         __attribute__((format(printf, 2, 3)));
int
ss_catf(SS *, const char *fmt, ...)
        __attribute__((format(printf, 2, 3)));
#else
int
ss_copyf(SS *, const char *fmt, ...);
int
ss_catf(SS *, const char *fmt, ...);
#endif
void
ss_catint64(SS *, int64_t);
void
ss_catuint64(SS *, uint64_t);

void
ssc_esc(SS *);

/* Unicode UTF-8 */
#define SS_UTF8_SEQ_MAX (5)
typedef uint32_t unicode_t;

bool
ssu_isvalid(unicode_t);
int
ssu8_cpseqlen(unicode_t);
int
ssu8_seqlen(const char *);
int
ssu8_cptoseq(unicode_t, char *);
int
ssu8_seqtocp(const char *, unicode_t *);

/* Exports */
int
sse_clz32(uint32_t n);
int
sse_msb32(uint32_t n);
const char *
sse_memrchar(const char *, char, size_t);

#ifdef __cplusplus
}
#endif
#endif /* SS_H_ */

