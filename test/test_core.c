/**
 * @file test_core.c
 *
 * Testing Guidelines:
 * 0. Any new string function needs tests.
 * 1. If you see an untested edge case then add a test for it.
 * 2. If you see an opportunity to make the test strings funnier
 *    but still appropriate, do it.
 *
 */
#include "sstring.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "test_macros.h"

void
assert_empty(const SS s)
{
    assert(ss_len(s) == 0);
    assert(ss_isempty(s));
    assert(s[0] == 0);
}

void
assert_eq(const SS s, const char *cs, size_t len)
{
    assert(ss_len(s) == len && "Length check");
    assert(s[len] == 0 && "Null terminator check");
    assert(!memcmp(s, cs, len) && "Equals check");
}

/**
 * Test basic string allocation.
 * Done.
 */
void
test_new_free(void)
{
    log_test();

    {
        /* Test allocation with no cap.
         */
        SS s = ss_new(0);
        assert_empty(s);
        assert(ss_cap(s) == 0);
        ss_free(&s);
        assert(!s);
    }

    {
        /* Test allocation with cap.
         */
        SS s = ss_new(20);
        assert_empty(s);
        assert(ss_cap(s) == 20);
        ss_free(&s);
        assert(!s);
    }

    {
        /* Test allocation from cstring with no cap.
         */
        SS s = ss_newfrom(0, "hello", 5);
        assert_eq(s, "hello", 5);
        assert(ss_cap(s) == 5);
        ss_free(&s);
        assert(!s);
    }

    {
        /* Test allocation from cstring with cap.
         */
        SS s = ss_newfrom(20, "hello", 5);
        assert_eq(s, "hello", 5);
        assert(ss_cap(s) == 20);
        ss_free(&s);
        assert(!s);
    }

    {
        /* Test duplicate empty.
         */
        SS s = ss_empty();
        SS s2 = ss_dup(s);
        assert_empty(s);
        assert_empty(s2);
        assert(ss_cap(s2) == 0);
        assert(ss_isheaptype(s2));
        ss_free(&s);
        ss_free(&s2);
        assert(!s);
        assert(!s2);
    }

    {
        /* Test duplicate.
         */
        SS s = ss_newfrom(20, "hello", 5);
        SS s2 = ss_dup(s);
        assert_eq(s, s2, ss_len(s2));
        assert(ss_cap(s) == 20);
        assert(ss_cap(s2) == ss_len(s));
        ss_free(&s);
        ss_free(&s2);
        assert(!s);
        assert(!s2);
    }
}

/**
 * Test equal and compare method.
 * Done.
 */
void
test_equal_compare(void)
{
    log_test();

    {
        /* Test equal and compare on empty strings.
         */
        SS s1 = ss_empty();
        SS s2 = ss_empty();

        assert(s1 == s2);

        assert(ss_equal(s1, s2));
        assert(0 == ss_compare(s1, s2));
    }

    {
        /* Test equal and compare on strings with different capacities.
         * Test equal and compare on unequal strings.
         */
        ss_stack(s3, 15);
        ss_stack(s4, 30);

        assert(s3 != s4);
        assert(ss_equal(s3, s4));
        assert(0 == ss_compare(s3, s4));

        char buf[] = "great";
        size_t len = strlen(buf);

        ss_copy(&s3, buf, len);
        assert(!ss_equal(s3, s4));
        assert(0 < ss_compare(s3, s4));
        assert(0 > ss_compare(s4, s3));
        ss_copy(&s4, buf, len);
        assert(ss_equal(s3, s4));
        assert(0 == ss_compare(s3, s4));
    }
}

void
test_grow(void)
{
    log_test();

    char buf[] = "1111";
    size_t len = strlen(buf);

    SS s = ss_new(0);
    ss_setgrow(&s, SS_GROW25);
    ss_copy(&s, buf, len);
    assert(ss_cap(s) > len);
    ss_free(&s);

    s = ss_new(0);
    ss_setgrow(&s, SS_GROW50);
    ss_copy(&s, buf, len);
    assert(ss_cap(s) > len);
    ss_free(&s);

    s = ss_new(0);
    ss_setgrow(&s, SS_GROW100);
    ss_copy(&s, buf, len);
    assert(ss_cap(s) > len);
    ss_free(&s);

    s = ss_empty();
    assert(ss_cap(s) == 0);
    assert(ss_len(s) == 0);
    assert(s[0] == 0);
    ss_setgrow(&s, SS_GROW25);
    ss_setgrow(&s, SS_GROW50);
    ss_setgrow(&s, SS_GROW100);
    ss_setgrow(&s, SS_GROW0);
    ss_free(&s);
}

/**
 * Make sure that strings are reallocated onto the heap.
 * Done.
 */
void
test_heapify(void)
{
    log_test();

    {
        /* Global empty string to heap test.
         */
        SS s = ss_empty();
        SS s2 = s;
        assert(ss_isemptytype(s));
        ss_heapify(&s);
        assert(s != s2);
        assert(s != ss_empty());
        assert(ss_isheaptype(s));
        assert(ss_cap(s) == 0);
        ss_free(&s);
    }

    {
        /* Stack string to heap test.
         */
        ss_stack(s, 32);
        assert(ss_isstacktype(s));
        SS s2 = s;
        ss_heapify(&s);
        assert(s != s2);
        assert(ss_isheaptype(s));
        assert(ss_cap(s) == 0);
        ss_free(&s);
    }

    {
        /* Heap string should be unaffected.
         * Heap flag should be set after new, heapify, and reallocation.
         */
        SS s = ss_new(5);
        assert(ss_isheaptype(s));
        SS s2 = s;
        ss_heapify(&s);
        assert(s == s2);
        assert(ss_isheaptype(s));
        assert(ss_cap(s) == 5);
        ss_copy(&s, "abcabc", 6);
        assert(ss_isheaptype(s));
        assert(ss_cap(s) == 6);
        ss_free(&s);
    }
}

void
test_find(void)
{
    log_test();

    {
        char buf[] = "asdfasdfasdf";
        size_t len = strlen(buf);

        char needle[] = "asdf";
        size_t nlen = strlen(needle);

        SS s = ss_newfrom(0, buf, len);

        size_t index = ss_find(s, 0, needle, nlen);
        assert(index == 0);
        index = ss_find(s, 1, needle, nlen);
        assert(index == 4);
        index = ss_find(s, 5, needle, nlen);
        assert(index == 8);
        index = ss_find(s, 9, needle, nlen);
        assert(index == NPOS);

        index = ss_rfind(s, ss_len(s) + 1, needle, nlen);
        assert(index == 8);
        index = ss_rfind(s, 9, needle, nlen);
        assert(index == 4);
        index = ss_rfind(s, 3, needle, nlen);
        assert(index == 0);

        ss_free(&s);
    }

    {
        char buf[] = "aszfzz";
        size_t len = strlen(buf);

        char needle[] = "asdf";
        size_t nlen = strlen(needle);

        SS s = ss_newfrom(0, buf, len);

        size_t index = ss_find(s, 0, needle, nlen);
        assert(index == NPOS);

        index = ss_rfind(s, ss_len(s) + 1, needle, nlen);
        assert(index == NPOS);

        ss_free(&s);
    }
}

void
test_count(void)
{
    log_test();

    {
        char hay[] = "aaaaaaaaaa";
        size_t hlen = strlen(hay);

        SS s = ss_newfrom(0, hay, hlen);
        size_t count = ss_count(s, 0, "a", 1);
        assert(count == hlen);
        count = ss_count(s, 4, "a", 1);
        assert(count == (hlen - 4));
        ss_free(&s);
    }

    {
        char hay[] = "asdfzzzasdzzzasdfzzzzasdasdf";
        size_t hlen = strlen(hay);
        char needle[] = "asdf";
        size_t nlen = strlen(needle);
        size_t expect = 3;

        SS s = ss_newfrom(0, hay, hlen);
        size_t count = ss_count(s, 0, needle, nlen);
        assert(count == expect);
        count = ss_count(s, 4, needle, nlen);
        assert(count == (expect - 1));
        count = ss_count(s, 0, "hello", 5);
        assert(!count);
        ss_free(&s);
    }
}

/**
 * Test swap function.
 * Done.
 */
void
test_swap(void)
{
    log_test();

    {
        /* Test swap.
         */
        char buf1[] = "asdf";
        size_t len1 = strlen(buf1);
        char buf2[] = "fdsa";
        size_t len2 = strlen(buf2);

        SS s1 = ss_newfrom(0, buf1, len1);
        SS s2 = ss_newfrom(0, buf2, len2);

        ss_swap(&s1, &s2);
        assert(!strcmp(s1, buf2));
        assert(!strcmp(s2, buf1));

        ss_free(&s1);
        ss_free(&s2);
    }
}

/**
 * Test length functions.
 * Done.
 */
void
test_setlen(void)
{
    log_test();

    char buf[] = "asdf";
    size_t len = strlen(buf);

    {
        /* Test settings length to valid value.
         */
        SS s = ss_newfrom(0, buf, len);
        assert(ss_len(s) == len);
        ss_setlen(s, len/2);
        assert(ss_len(s) == (len/2));
        ss_free(&s);
    }

    {
        /* Test settings length past end of string.
         */
        SS s = ss_newfrom(0, buf, len);
        assert(ss_len(s) == len);
        ss_setlen(s, 2*len);
        assert(ss_len(s) == len);
        ss_free(&s);
    }

    {
        /* Test setting valid cstring len.
         */
        SS s = ss_newfrom(0, buf, len);
        s[2] = 0;
        ssc_setlen(s);
        assert(ss_len(s) == 2);
        ss_free(&s);
    }

    {
#if 0
        /* Test setting length past end of string.
         * Commented out for valgrind tests.
         */
        SS s = ss_newfrom(0, buf, len);
        s[len] = 0x43;
        ssc_setlen(s);
        assert(ss_len(s) == len);
        ss_free(&s);
#endif
    }

    {
        /* Test setting valid cstring len.
         */
        SS s = ss_newfrom(0, buf, len);
        ss_trunc(s, len/2);
        assert(ss_len(s) == (len/2));
        ss_free(&s);
    }
}

/**
 * Test basic resizing functions.
 * Done.
 */
void
test_resize(void)
{
    log_test();

    char buf[] = "asdf";
    size_t len = strlen(buf);

    {
        /* Test reserve.
         */
        SS s = ss_new(200);
        assert(ss_cap(s) == 200);
        ss_reserve(&s, 300);
        assert(ss_cap(s) == 300);
        ss_free(&s);
    }

    {
        /* Test fit.
         */
        SS s = ss_newfrom(200, buf, len);
        assert(ss_cap(s) == 200);
        ss_fit(&s);
        assert(ss_cap(s) == len);
        assert_eq(s, buf, len);
        ss_free(&s);
    }

    {
        /* Test resize same.
         */
        SS s = ss_newfrom(200, buf, len);
        SS s2 = s;
        assert(ss_cap(s) == 200);
        ss_resize(&s, 200);
        assert(ss_cap(s) == 200);
        assert_eq(s, buf, len);
        assert(s == s2);
        ss_free(&s);
    }

    {
        /* Test resize to larger.
         */
        SS s = ss_newfrom(200, buf, len);
        assert(ss_cap(s) == 200);
        ss_resize(&s, 500);
        assert(ss_cap(s) == 500);
        assert_eq(s, buf, len);
        ss_free(&s);
    }

    {
        /* Test resize to smaller.
         */
        SS s = ss_newfrom(200, buf, len);
        assert(ss_cap(s) == 200);
        ss_resize(&s, len/2);
        assert(ss_cap(s) == (len/2));
        assert(ss_len(s) == (len/2));
        ss_free(&s);
    }

    {
        /* Test add capacity.
         */
        SS s = ss_new(10);
        assert(ss_cap(s) == 10);
        ss_addcap(&s, 10);
        assert(ss_cap(s) == 20);
        ss_free(&s);
    }
}

void
test_stack()
{
    log_test();

    char buf[] = "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa";
    size_t len = strlen(buf);

    ss_stack(s, 32);
    assert_empty(s);
    assert(ss_cap(s) == 32);
    assert(s[0] == 0);
    ss_resize(&s, 31);
    assert(ss_cap(s) == 32);
    SS tmp = s;
    ss_copy(&s, buf, len);
    assert(s != tmp);
    assert(ss_cap(s) == len);
    assert(ss_len(s) == len);
    ss_free(&s);
}

void
test_clear(void)
{
    log_test();

    char buf[] = "asdf";
    size_t len = strlen(buf);

    SS s = ss_new(0);
    assert_empty(s);
    ss_copy(&s, buf, len);
    assert_eq(s, buf, len);
    ss_clear(s);
    assert_empty(s);
    ss_free(&s);
}

/**
 * Test unescape function.
 * Done.
 */
void
test_esc(void)
{
    log_test();

    {
        /* Test simple escapes.
         */
        char buf[] = "\\a\\b\\e\\f\\n\\r\\t\\v\\\\\\\'\\\"\\?";
        size_t len = strlen(buf);
        char ans[] = "\a\b\x1B\f\n\r\t\v\\\'\"\?";
        size_t alen = strlen(ans);

        SS s = ss_newfrom(0, buf, len);
        ssc_unesc(s);
        assert_eq(s, ans, alen);
        ss_free(&s);
    }

    {
        /* Test memmove.
         */
        char buf[] = "\\\\text to move\\\\";
        size_t len = sizeof(buf) - 1;
        char ans[] = "\\text to move\\";
        size_t alen = sizeof(ans) - 1;

        SS s = ss_newfrom(0, buf, len);
        ssc_unesc(s);
        assert_eq(s, ans, alen);
        ss_free(&s);
    }

    {
        /* Test hex escapes.
         */
        char buf[] = "\\xinvalid\\x7F\\x0\\x00\\xff\\x3D";
        size_t len = sizeof(buf) - 1;
        char ans[] = "\\xinvalid\x7F\x00\x00\xFF\x3D";
        size_t alen = sizeof(ans) - 1;

        SS s = ss_newfrom(0, buf, len);
        ssc_unesc(s);
        assert_eq(s, ans, alen);
        ss_free(&s);
    }

    {
        /* Test unicode 4 nibble escapes.
         */
        char buf[] = "\\uinvalid\\u1\\u22\\u333\\u4444\\u44444";
        size_t len = sizeof(buf) - 1;
        char ans[] = "\\uinvalid\x01\x22\u0333\u4444\u44444";
        size_t alen = sizeof(ans) - 1;

        SS s = ss_newfrom(0, buf, len);
        ssc_unesc(s);
        assert_eq(s, ans, alen);
        ss_free(&s);
    }

    {
        /* Test unicode 8 nibble escapes.
         */
        char buf[] = "\\Uinvalid\\U1\\U22\\U333\\U4444";
        size_t len = sizeof(buf) - 1;
        char ans[] = "\\Uinvalid\x01\x22\u0333\u4444";
        size_t alen = sizeof(ans) - 1;

        SS s = ss_newfrom(0, buf, len);
        ssc_unesc(s);
        assert_eq(s, ans, alen);
        ss_free(&s);
    }

    {
        /* Test octal escapes.
         */
        char buf[] = "\\0\\77\\007\\477\\377";
        size_t len = sizeof(buf) - 1;
        char ans[] = "\0\077\007\47""7\377";
        size_t alen = sizeof(ans) - 1;

        SS s = ss_newfrom(0, buf, len);
        ssc_unesc(s);
        assert_eq(s, ans, alen);
        ss_free(&s);
    }

    {
        /* Test false positive.
         */
        char buf[] = "\\z";
        size_t len = sizeof(buf) - 1;
        char ans[] = "\\z";
        size_t alen = sizeof(ans) - 1;

        SS s = ss_newfrom(0, buf, len);
        ssc_unesc(s);
        assert_eq(s, ans, alen);
        ss_free(&s);
    }

    {
        /* Test esc.
         */
        char buf[] = "\a\b\x1B\f\n\r\t\v\\\'\"asdf\x7F";
        size_t len = sizeof(buf) - 1;
        char ans[] = "\\a\\b\\e\\f\\n\\r\\t\\v\\\\\\\'\\\"asdf\\x7F";
        size_t alen = sizeof(ans) - 1;

        SS s = ss_newfrom(0, buf, len);
        ssc_esc(&s);
        assert_eq(s, ans, alen);
        ss_free(&s);
    }
}

void
test_copy(void)
{
    log_test();

    char buf[] = "asdfgh";
    size_t len = strlen(buf);

    SS s = ss_new(0);
    ss_copy(&s, buf, len);
    assert_eq(s, buf, len);
    ss_free(&s);
}

void
test_cat(void)
{
    log_test();

    char buf[] = "hello world";

    SS s = ss_new(0);
    ss_cat(&s, buf, strlen(buf));
    assert(ss_len(s) == strlen(buf));
    assert(s[ss_len(s)] == 0);
    ss_cat(&s, buf, strlen(buf));
    assert(ss_len(s) == 2 * strlen(buf));
    assert(s[ss_len(s)] == 0);
    assert(!memcmp(s, buf, strlen(buf)));
    assert(!memcmp(s + strlen(buf), buf, strlen(buf)));
    ss_free(&s);
}

void
test_lcat(void)
{
    log_test();

    char buf[] = "hello";
    size_t blen = strlen(buf);
    char pre[] = "asdf";
    size_t plen = strlen(pre);

    SS s = ss_new(0);
    ss_lcat(&s, buf, blen);
    assert(ss_len(s) == blen);
    assert(s[ss_len(s)] == 0);
    assert(!memcmp(s, buf, blen));
    ss_lcat(&s, pre, plen);
    assert(ss_len(s) == (blen + plen));
    assert(!memcmp(s, pre, plen));
    assert(!memcmp(s + plen, buf, blen));
    assert(s[ss_len(s)] == 0);
    ss_free(&s);
}

void
test_replace(void)
{
    log_test();

    {
        char buf[] = "abcabcabcabc";
        size_t blen = strlen(buf);
        char rep[] = "abc";
        size_t rlen = strlen(rep);
        char wit[] = "gh";
        size_t wlen = strlen(wit);
        char ans[] = "abcghghgh";
        size_t alen = strlen(ans);

        SS s = ss_newfrom(0, buf, blen);
        ss_replace(&s, 1, rep, rlen, wit, wlen);
        assert_eq(s, ans, alen);
        ss_free(&s);
    }

    {
        char buf[] = "abcabcabcabc";
        size_t blen = strlen(buf);
        char rep[] = "abc";
        size_t rlen = strlen(rep);

        SS s = ss_newfrom(0, buf, blen);
        ss_replace(&s, 0, rep, rlen, "", 0);
        assert_empty(s);
        ss_free(&s);
    }

    {
        char buf[] = "abcabcabcabc";
        size_t blen = strlen(buf);
        char rep[] = "abc";
        size_t rlen = strlen(rep);
        char wit[] = "ghj";
        size_t wlen = strlen(wit);
        char ans[] = "ghjghjghjghj";
        size_t alen = strlen(ans);

        SS s = ss_newfrom(0, buf, blen);
        ss_replace(&s, 0, rep, rlen, wit, wlen);
        assert_eq(s, ans, alen);
        ss_free(&s);
    }

    {
        char buf[] = "abcabcabcabc";
        size_t blen = strlen(buf);
        char rep[] = "abc";
        size_t rlen = strlen(rep);
        char wit[] = "long";
        size_t wlen = strlen(wit);
        char ans[] = "longlonglonglong";
        size_t alen = strlen(ans);

        SS s = ss_newfrom(0, buf, blen);
        ss_replace(&s, 0, rep, rlen, wit, wlen);
        assert_eq(s, ans, alen);
        ss_free(&s);
    }
    
    {
        /* Test memmove when replacing with shorter string.
         */
        char buf[] = "aabbbbaa";
        size_t blen = strlen(buf);
        char rep[] = "aa";
        size_t rlen = strlen(rep);
        char wit[] = "c";
        size_t wlen = strlen(wit);
        char ans[] = "cbbbbc";
        size_t alen = strlen(ans);

        SS s = ss_newfrom(0, buf, blen);
        ss_replace(&s, 0, rep, rlen, wit, wlen);
        assert_eq(s, ans, alen);
        ss_free(&s);
    }

    {
        /* Test char search when replacing with longer string.
         */
        char buf[] = "aabbbbaa";
        size_t blen = strlen(buf);
        char rep[] = "aa";
        size_t rlen = strlen(rep);
        char wit[] = "ccc";
        size_t wlen = strlen(wit);
        char ans[] = "cccbbbbccc";
        size_t alen = strlen(ans);

        SS s = ss_newfrom(0, buf, blen);
        ss_replace(&s, 0, rep, rlen, wit, wlen);
        assert_eq(s, ans, alen);
        ss_free(&s);
    }

    {
        /* Test false positive when searching for char.
         */
        char buf[] = "aabbbbabbbbaa";
        size_t blen = strlen(buf);
        char rep[] = "aa";
        size_t rlen = strlen(rep);
        char wit[] = "ccc";
        size_t wlen = strlen(wit);
        char ans[] = "cccbbbbabbbbccc";
        size_t alen = strlen(ans);

        SS s = ss_newfrom(0, buf, blen);
        ss_replace(&s, 0, rep, rlen, wit, wlen);
        assert_eq(s, ans, alen);
        ss_free(&s);
    }
}

void
test_replacerange(void)
{
    log_test();

    {
        char replace[] = "something else";
        size_t rlen = strlen(replace);
        char buf[] = "aaaaaaaareplaceaaaaa";
        size_t blen = strlen(buf);
        size_t index = 8;
        size_t end = index + 7;
        
        SS s = ss_newfrom(0, buf, blen);
        ss_replacerange(&s, index, end, replace, rlen);
        assert(ss_len(s) == (blen - (end - index) + rlen));
        assert(!memcmp(s + index, replace, rlen));
        ss_replacerange(&s, 0, 8, "", 0);
        assert(ss_len(s) == rlen + 5);
        assert(s[ss_len(s)] == 0);
        assert(!memcmp(s, replace, rlen));
        assert(!memcmp(s + rlen, "aaaaa", 5));
        ss_free(&s);
    }

    {
        char replace[] = "something else";
        size_t rlen = strlen(replace);
        char buf[] = "aaaaaaaareplaceaaaaa";
        size_t blen = strlen(buf);
        size_t start = blen + 18;
        size_t end = start + 20;
        char ans[] = "aaaaaaaareplaceaaaaasomething else";
        size_t alen = strlen(ans);
        
        SS s = ss_newfrom(0, buf, blen);
        ss_replacerange(&s, start, end, replace, rlen);
        assert_eq(s, ans, alen);
        ss_free(&s);
    }
}

void
test_insert(void)
{
    log_test();

    char buf[] = "bbbbbb";
    size_t len = strlen(buf);
    size_t newlen = len;

    SS s = ss_newfrom(0, buf, len);
    ss_insert(&s, 6, "a", 1);
    ++newlen;
    assert(ss_len(s) == newlen);
    
    ss_insert(&s, 3, "a", 1);
    ++newlen;
    assert(ss_len(s) == newlen);

    ss_insert(&s, 0, "a", 1);
    ++newlen;
    assert(ss_len(s) == newlen);

    char ans[] = "abbbabbba";
    assert(!strcmp(s, ans));

    ss_insert(&s, 20, "a", 1);
    char ans2[] = "abbbabbbaa";
    assert(!strcmp(s, ans2));

    ss_free(&s);
}

void
test_overlay(void)
{
    log_test();

    {
        char buf[] = "aaaaaaaaaa";
        size_t blen = strlen(buf);
        char over[] = "hello";
        size_t olen = strlen(over);
        size_t index = 6;
        char ans[] = "aaaaaahello";
        size_t alen = strlen(ans);

        SS s = ss_newfrom(0, buf, blen);
        ss_overlay(&s, index, over, olen);
        assert(ss_len(s) == alen);
        assert(s[ss_len(s)] == 0);
        assert(!memcmp(s, ans, alen));
        ss_overlay(&s, 0, over, olen);
        assert(ss_len(s) == alen);
        assert(!memcmp(s, over, olen));
        assert(!memcmp(s + olen, ans + olen, alen - olen));
        ss_free(&s);
    }

    {
        char buf[] = "blah";
        size_t blen = strlen(buf);
        char over[] = "end";
        size_t olen = strlen(over);

        SS s = ss_newfrom(0, buf, blen);
        ss_overlay(&s, NPOS, over, olen);
        assert(ss_len(s) == blen + olen);
        assert(!memcmp(s, buf, blen));
        assert(!memcmp(s + blen, over, olen));
        ss_free(&s);
    }
}

void
test_remove(void)
{
    log_test();

    {
        char buf[] = "abczzzzabcababcc";
        size_t blen = strlen(buf);
        char rem[] = "abc";
        size_t rlen = strlen(rem);
        char ans[] = "zzzzabc";
        size_t alen = strlen(ans);

        SS s = ss_newfrom(0, buf, blen);
        ss_remove(s, 0, rem, rlen);
        assert(ss_len(s) == alen);
        assert(s[alen] == 0);
        assert(!memcmp(s, ans, alen));
        ss_free(&s);
    }

    {
        char buf[] = "abczzzzabcababcc";
        size_t blen = strlen(buf);
        char rem[] = "abc";
        size_t rlen = strlen(rem);
        char ans[] = "abczzzzabc";
        size_t alen = strlen(ans);

        SS s = ss_newfrom(0, buf, blen);
        ss_remove(s, 1, rem, rlen);
        assert(ss_len(s) == alen);
        assert(s[alen] == 0);
        assert(!memcmp(s, ans, alen));
        ss_free(&s);
    }
}

void
test_removerange(void)
{
    log_test();

    {
        char buf[] = "abczzzzabcababcc";
        size_t blen = strlen(buf);
        size_t start = 3;
        size_t end = start + 4;
        char ans[] = "abcabcababcc";
        size_t alen = strlen(ans);

        SS s = ss_newfrom(0, buf, blen);
        ss_removerange(s, start, end);
        assert_eq(s, ans, alen);
        ss_free(&s);
    }

    {
        char buf[] = "abczzzzabcababcc";
        size_t blen = strlen(buf);
        size_t start = blen - 4;
        size_t end = blen + 4;
        char ans[] = "abczzzzabcab";
        size_t alen = strlen(ans);

        SS s = ss_newfrom(0, buf, blen);
        ss_removerange(s, start, end);
        assert_eq(s, ans, alen);
        ss_free(&s);
    }
}

void
test_reverse(void)
{
    log_test();

    char buf[] = "abcd";
    char ans[] = "dcba";
    size_t len = strlen(buf);

    SS s = ss_newfrom(0, buf, len);
    ss_reverse(s);
    assert(ss_len(s) == len);
    assert(s[ss_len(s)] == 0);
    assert(!memcmp(s, ans, len));
    ss_free(&s);

}

/**
 * Test trim functions.
 * Done.
 */
void
test_trim(void)
{
    log_test();

    {
        /* Test NULL.
         */
        char buf[] = "howdy";
        size_t len = strlen(buf);

        SS s = ss_newfrom(0, buf, len);
        ss_trim(s, NULL, 0);
        assert_eq(s, buf, len);
        ss_free(&s);
    }

    {
        /* Test trim some.
         */
        char buf[] = "howdy";
        size_t len = strlen(buf);
        char trim[] = "hy";
        size_t tlen = strlen(trim);
        char ans[] = "owd";
        size_t alen = strlen(ans);

        SS s = ss_newfrom(0, buf, len);
        ss_trim(s, trim, tlen);
        assert_eq(s, ans, alen);
        ss_free(&s);
    }

    {
        /* Test trim all.
         */
        char buf[] = "howdy";
        size_t len = strlen(buf);
        char trim[] = "howdy";
        size_t tlen = strlen(trim);
        char ans[] = "";
        size_t alen = strlen(ans);

        SS s = ss_newfrom(0, buf, len);
        ss_trim(s, trim, tlen);
        assert_eq(s, ans, alen);
        ss_free(&s);
    }

    {
        /* Test trimrange NULL.
         */
        char buf[] = "asdfasdfasdf";
        size_t len = strlen(buf);

        SS s = ss_newfrom(0, buf, len);
        ss_trimrange(s, 1, len - 1, NULL, 0);
        assert_eq(s, buf, len);
        ss_free(&s);
    }

    {
        /* Test trimrange some.
         */
        char buf[] = "asdfasdfasdf";
        size_t len = strlen(buf);
        size_t start = 0;
        size_t end = len - 2;
        char trim[] = "as";
        size_t tlen = strlen(trim);
        char ans[] = "dfasdfdf";
        size_t alen = strlen(ans);

        SS s = ss_newfrom(0, buf, len);
        ss_trimrange(s, start, end, trim, tlen);
        assert_eq(s, ans, alen);
        ss_free(&s);
    }

    {
        /* Test trimrange invalid.
         */
        char buf[] = "asdfasdfasdf";
        size_t len = strlen(buf);
        size_t start = len + 12;
        size_t end = len + 10;
        char trim[] = "as";
        size_t tlen = strlen(trim);
        char ans[] = "asdfasdfasdf";
        size_t alen = strlen(ans);

        SS s = ss_newfrom(0, buf, len);
        ss_trimrange(s, start, end, trim, tlen);
        assert_eq(s, ans, alen);
        ss_free(&s);
    }

    {
        /* Test trim cstring NULL/whitespace.
         */
        char buf[] = " \n\t\v\r\fasdf \n\t\v\r\f";
        size_t len = strlen(buf);
        char ans[] = "asdf";
        size_t alen = strlen(ans);

        SS s = ss_newfrom(0, buf, len);
        ssc_trim(s, NULL);
        assert_eq(s, ans, alen);
        ss_free(&s);
    }

    {
        /* Test trim cstring chars.
         */
        char buf[] = "asdfasdfasdf";
        size_t len = strlen(buf);
        char trim[] = "af";
        char ans[] = "sdfasdfasd";
        size_t alen = strlen(ans);

        SS s = ss_newfrom(0, buf, len);
        ssc_trim(s, trim);
        assert_eq(s, ans, alen);
        ss_free(&s);
    }

    {
        /* Test trim cstring all.
         */
        char buf[] = "asdfasdfasdf";
        size_t len = strlen(buf);
        char trim[] = "asdf";
        char ans[] = "";
        size_t alen = strlen(ans);

        SS s = ss_newfrom(0, buf, len);
        ssc_trim(s, trim);
        assert_eq(s, ans, alen);
        ss_free(&s);
    }

    {
        /* Test empty.
         */
        SS s = ss_empty();
        ssc_trim(s, "asdf");
        assert_empty(s);
        ss_free(&s);
    }
}

/**
 * Test upper and lower functions.
 * Done.
 */
void
test_upper_lower(void)
{
    log_test();

    {
        /* Test upper.
         */
        char buf[] = "asdf";
        size_t len = strlen(buf);
        char ans[] = "ASDF";
        size_t alen = strlen(ans);

        SS s = ss_newfrom(0, buf, len);
        ssc_upper(s);
        assert_eq(s, ans, alen);
        ss_free(&s);
    }

    {
        /* Test lower.
         */
        char buf[] = "ASDF";
        size_t len = strlen(buf);
        char ans[] = "asdf";
        size_t alen = strlen(ans);

        SS s = ss_newfrom(0, buf, len);
        ssc_lower(s);
        assert_eq(s, ans, alen);
        ss_free(&s);
    }

    {
        /* Test empty.
         */
        SS s = ss_empty();
        ssc_upper(s);
        ssc_lower(s);
        assert_empty(s);
        ss_free(&s);
    }
}

void
test_copyf(void)
{
    log_test();

    char buf[] = "asdf";
    size_t len = strlen(buf);

    SS s = ss_new(0);
    ss_copyf(&s, "%s", buf);
    assert(ss_len(s) == len);
    assert(!memcmp(s, buf, len));
    assert(s[len] == 0);
    ss_free(&s);

    s = ss_empty();
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat"
#pragma GCC diagnostic ignored "-Wformat-extra-args"
    assert(ss_copyf(&s, "%", buf));
#pragma GCC diagnostic pop
    assert_empty(s);
    ss_free(&s);
}

void
test_catf(void)
{
    log_test();

    char buf[] = "hello world";
    size_t len = strlen(buf);

    SS s = ss_new(0);
    ss_catf(&s, "%s", buf);
    assert_eq(s, buf, len);
    ss_catf(&s, "%s", buf);
    assert(ss_len(s) == 2 * len);
    assert(s[2 * len] == 0);
    assert(!memcmp(s + len, buf, len));
    ss_free(&s);

    s = ss_empty();
    ss_catf(&s, "%s", buf);
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat"
#pragma GCC diagnostic ignored "-Wformat-extra-args"
    assert(ss_catf(&s, "%", buf));
#pragma GCC diagnostic pop
    assert_eq(s, buf, len);
    ss_free(&s);
}

/**
 * Test int64 and uint64 concatentation methods.
 * Done.
 */
void
test_catint(void)
{
    log_test();

    /* Test catint64.
     */
    {
        /* Test zero.
         */
        char buf[] = "0";
        size_t len = strlen(buf);
        int64_t n = 0;
        SS s = ss_empty();
        ss_catint64(&s, n);
        assert_eq(s, buf, len);
        ss_free(&s);
    }

    {
        /* Test one.
         */
        char buf[] = "1";
        size_t len = strlen(buf);
        int64_t n = 1;
        SS s = ss_empty();
        ss_catint64(&s, n);
        assert_eq(s, buf, len);
        ss_free(&s);
    }

    {
        /* Test neg one.
         */
        char buf[] = "-1";
        size_t len = strlen(buf);
        int64_t n = -1;
        SS s = ss_empty();
        ss_catint64(&s, n);
        assert_eq(s, buf, len);
        ss_free(&s);
    }

    {
        /* Test max.
         */
        char buf[] = "9223372036854775807";
        size_t len = strlen(buf);
        int64_t n = 9223372036854775807LL;
        SS s = ss_empty();
        ss_catint64(&s, n);
        assert_eq(s, buf, len);
        ss_free(&s);
    }

    {
        /* Test neg max.
         */
        char buf[] = "-9223372036854775808";
        size_t len = strlen(buf);
        int64_t n = (-9223372036854775807LL) - 1;
        SS s = ss_empty();
        ss_catint64(&s, n);
        assert_eq(s, buf, len);
        ss_free(&s);
    }

    /* Test catuint64.
     */
    {
        /* Test zero.
         */
        char buf[] = "0";
        size_t len = strlen(buf);
        uint64_t n = 0;
        SS s = ss_empty();
        ss_catuint64(&s, n);
        assert_eq(s, buf, len);
        ss_free(&s);
    }

    {
        /* Test one.
         */
        char buf[] = "1";
        size_t len = strlen(buf);
        uint64_t n = 1;
        SS s = ss_empty();
        ss_catuint64(&s, n);
        assert_eq(s, buf, len);
        ss_free(&s);
    }

    {
        /* Test max.
         */
        char buf[] = "18446744073709551615";
        size_t len = strlen(buf);
        uint64_t n = 0xFFFFFFFFFFFFFFFFULL;
        SS s = ss_empty();
        ss_catuint64(&s, n);
        assert_eq(s, buf, len);
        ss_free(&s);
    }
}

/**
 * Test unicode functions.
 * Done.
 */
void
test_unicode(void)
{
    log_test();

    {
        /* Test unicode point validity.
         */
        assert(ssu_isvalid(0));
        assert(ssu_isvalid(0x0000D7FF));
        assert(!ssu_isvalid(0x0000D800));
        assert(!ssu_isvalid(0x0000DFFE));
        assert(!ssu_isvalid(0x0000DFFF));
        assert(ssu_isvalid(0x0000E000));
        assert(ssu_isvalid(0x0010FFFF));
        assert(!ssu_isvalid(0x00110000));
        assert(!ssu_isvalid((unicode_t)-1));
    }
    
    {
        /* Test unicode point sequence length.
         */
        assert(1 == ssu8_cpseqlen(0));
        assert(1 == ssu8_cpseqlen(127));
        // 8 bits
        assert(2 == ssu8_cpseqlen(128));
        // 11 bits
        assert(2 == ssu8_cpseqlen(0x000007FF));
        // 12 bits
        assert(3 == ssu8_cpseqlen(0x00000800));
        // 16 bits
        assert(3 == ssu8_cpseqlen(0x0000FFFF));
        // 17 bits
        assert(4 == ssu8_cpseqlen(0x00010000));
        // 21 bits
        assert(4 == ssu8_cpseqlen(0x001FFFFF));
        // Higher than 21 aren't technically valid unicode points, but why not?
        // 22 bits
        assert(5 == ssu8_cpseqlen(0x00200000));
        // 26 bits
        assert(5 == ssu8_cpseqlen(0x03FFFFFF));
        // 27 bits
        assert(6 == ssu8_cpseqlen(0x04000000));
        // 31 bits
        assert(6 == ssu8_cpseqlen(0x7FFFFFFF));
        // 32 bits
        assert(7 == ssu8_cpseqlen(0xFFFFFFFF));
    }

    {
        /* Test sequence length.
         */
        assert(1 == ssu8_seqlen("\0"));
        assert(1 == ssu8_seqlen("\x7F"));
        assert(1 == ssu8_seqlen("\x80"));
        assert(2 == ssu8_seqlen("\xC0"));
        assert(3 == ssu8_seqlen("\xE0"));
        assert(4 == ssu8_seqlen("\xF0"));
        assert(5 == ssu8_seqlen("\xF8"));
        assert(6 == ssu8_seqlen("\xFC"));
        assert(7 == ssu8_seqlen("\xFE"));
        assert(8 == ssu8_seqlen("\xFF"));
    }

    {
        /* Test code point to utf8 sequence.
         * Test utf8 sequence to code point.
         */
        char buf[SS_UTF8_SEQ_MAX];

        memset(buf, 0, sizeof(buf));
        assert(1 == ssu8_cptoseq(0, buf));
        assert(0 == *buf);

        memset(buf, 0, sizeof(buf));
        assert(1 == ssu8_cptoseq(127, buf));
        assert(127 == *buf);


        unicode_t cp[] =
        {
            0x00000000,
            0x0000007F,
            0x00000080,
            0x000007FF,
            0x00000800,
            0x0000FFFF,
            0x00010000,
            0x001FFFFF,
            0x00200000,
            0x03FFFFFF,
            0x04000000,
            0x7FFFFFFF,
        };

        const char *ans[] =
        {
            "\x00",
            "\x7F",
            "\xC2\x80",
            "\xDF\xBF",
            "\xE0\xA0\x80",
            "\xEF\xBF\xBF",
            "\xF0\x90\x80\x80",
            "\xF7\xBF\xBF\xBF",
            "\xF8\x88\x80\x80\x80",
            "\xFB\xBF\xBF\xBF\xBF",
            "\xFC\x84\x80\x80\x80\x80",
            "\xFD\xBF\xBF\xBF\xBF\xBF",
        };

        size_t len = sizeof(cp)/sizeof(cp[0]);

        size_t i;
        for (i = 0; i < len; ++i)
        {
            unicode_t c = cp[i];
            const char *a = ans[i];
            memset(buf, 0, sizeof(buf));
            assert(ssu8_cpseqlen(c) == ssu8_cptoseq(c, buf));
            assert(0 == memcmp(buf, a, strlen(a)));
        }

        for (i = 0; i < len; ++i)
        {
            unicode_t c = 0;
            unicode_t cpans = cp[i];
            const char *u = ans[i];

            assert(ssu8_cpseqlen(cpans) == ssu8_seqtocp(u, &c));
            assert(cpans == c);
        }
    }
}

/**
 * Test internal functions that are made available for testing.
 * Done.
 */
void
test_external(void)
{
    log_test();

    {
        /* Test clz zero.
         */
        assert(32 == sse_clz32(0));
        uint32_t n = 1;
        int count = 31;
        do
        {
            assert(count == sse_clz32(n));
            n <<= 1;
            --count;
        } while (n);
    }

    {
        /* Test memrchar.
         */
        char buf[] = "asdfasdf";
        size_t len = strlen(buf);

        assert(&buf[7] == sse_memrchar(buf, 'f', len));
        assert(&buf[6] == sse_memrchar(buf, 'd', len));
        assert(&buf[5] == sse_memrchar(buf, 's', len));
        assert(&buf[4] == sse_memrchar(buf, 'a', len));
        assert(&buf[3] == sse_memrchar(buf, 'f', len - 2));
    }
}

int
main(void)
{
    test_new_free();
    test_equal_compare();
    test_grow();
    test_heapify();
    test_find();
    test_count();
    test_swap();
    test_setlen();
    test_resize();
    test_stack();
    test_clear();
    test_esc();
    test_copy();
    test_cat();
    test_lcat();
    test_replace();
    test_replacerange();
    test_insert();
    test_overlay();
    test_remove();
    test_removerange();
    test_reverse();
    test_trim();
    test_upper_lower();
    test_copyf();
    test_catf();
    test_catint();
    test_unicode();
    test_external();
    return 0;
}

