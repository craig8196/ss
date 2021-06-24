
#include <stdbool.h>
#include <string.h>
#include "bdd.h"
#include "ss.h"


bool
is_empty(const SS s)
{
    return 0 == ss_len(s) && ss_isempty(s) && 0 == s[0];
}

bool
eq(const SS s, const char *cs, size_t len)
{
    return len == ss_len(s) && 0 == s[len] && !memcmp(s, cs, len);
}

spec("simple-string library")
{
    describe("ss_new")
    {
        it("should have no errors allocating with no cap")
        {
            SS s = ss_new(0);
            check(is_empty(s));
            check(0 == ss_cap(s));
            ss_free(&s);
            check(!s);
        }

        it("should allocate with a cap")
        {
            SS s = ss_new(20);
            check(is_empty(s));
            check(20 == ss_cap(s));
            ss_free(&s);
            check(!s);
        }

        it("should not allocate and return empty string if 0 or NPOS is used")
        {
            SS s = ss_new(0);
            SS e = ss_empty();
            check(e == s);

            SS n = ss_new(NPOS);
            check(e == n);

            ss_free(&s);
            ss_free(&n);
            ss_free(&e);
        }
    }

    describe("ss_newfrom")
    {
        it("should allocate from c-string with no cap")
        {
            SS s = ss_newfrom(0, "hello", 5);
            check(eq(s, "hello", 5));
            check(ss_cap(s) == 5);
            ss_free(&s);
            check(!s);
        }

        it("should allocate and copy c-string with cap")
        {
            SS s = ss_newfrom(20, "hello", 5);
            check(eq(s, "hello", 5));
            check(ss_cap(s) == 20);
            ss_free(&s);
            check(!s);
        }
    }

    describe("ss_dup")
    {
        it("should duplicate empty string")
        {
            SS s = ss_empty();
            SS s2 = ss_dup(s);
            check(is_empty(s));
            check(is_empty(s2));
            check(ss_cap(s2) == 0);
            check(ss_isheaptype(s2));
            ss_free(&s);
            ss_free(&s2);
            check(!s);
            check(!s2);
        }

        it("should duplicate a string")
        {
            SS s = ss_newfrom(20, "hello", 5);
            SS s2 = ss_dup(s);
            check(eq(s, s2, ss_len(s2)));
            check(ss_cap(s) == 20);
            check(ss_cap(s2) == ss_len(s));
            ss_free(&s);
            ss_free(&s2);
            check(!s);
            check(!s2);
        }
    }

    describe("ss_equal and ss_compare")
    {
        it("should work on empty strings")
        {
            SS s1 = ss_empty();
            SS s2 = ss_empty();

            check(s1 == s2);
            check(ss_equal(s1, s2));
            check(0 == ss_compare(s1, s2));
        }

        it("should eq/compare with different capacities and unequal strings")
        {
            ss_stack(s3, 15);
            ss_stack(s4, 30);

            check(s3 != s4);
            check(ss_equal(s3, s4));
            check(0 == ss_compare(s3, s4));

            char buf[] = "great";
            size_t len = strlen(buf);

            ss_copy(&s3, buf, len);
            check(!ss_equal(s3, s4));
            check(0 < ss_compare(s3, s4));
            check(0 > ss_compare(s4, s3));
            ss_copy(&s4, buf, len);
            check(ss_equal(s3, s4));
            check(0 == ss_compare(s3, s4));
        }
    }

    describe("ss_setgrow")
    {
        it("should grow according to the growth flag")
        {
            char buf[] = "1111";
            size_t len = strlen(buf);

            SS s = ss_new(0);
            ss_setgrow(&s, SS_GROW25);
            ss_copy(&s, buf, len);
            check(ss_cap(s) > len);
            ss_free(&s);

            s = ss_new(0);
            ss_setgrow(&s, SS_GROW50);
            ss_copy(&s, buf, len);
            check(ss_cap(s) > len);
            ss_free(&s);

            s = ss_new(0);
            ss_setgrow(&s, SS_GROW100);
            ss_copy(&s, buf, len);
            check(ss_cap(s) > len);
            ss_free(&s);

            s = ss_empty();
            check(ss_cap(s) == 0);
            check(ss_len(s) == 0);
            check(s[0] == 0);
            ss_setgrow(&s, SS_GROW25);
            ss_setgrow(&s, SS_GROW50);
            ss_setgrow(&s, SS_GROW100);
            ss_setgrow(&s, SS_GROW0);
            ss_free(&s);
        }
    }

    describe("ss_heapify")
    {
        it("should guarantee that the empty string is moved to the heap")
        {
            SS s = ss_empty();
            SS s2 = s;
            check(ss_isemptytype(s));
            ss_heapify(&s);
            check(s != s2);
            check(s != ss_empty());
            check(ss_isheaptype(s));
            check(ss_cap(s) == 0);
            ss_free(&s);
        }

        it("should move stack allocated strings to the heap")
        {
            ss_stack(s, 32);
            check(ss_isstacktype(s));
            SS s2 = s;
            ss_heapify(&s);
            check(s != s2);
            check(ss_isheaptype(s));
            check(ss_cap(s) == 0);
            ss_free(&s);
        }

        it("should not affect non-zero newly allocated types")
        {
            SS s = ss_new(5);
            check(ss_isheaptype(s));
            SS s2 = s;
            ss_heapify(&s);
            check(s == s2);
            check(ss_isheaptype(s));
            check(ss_cap(s) == 5);
            ss_copy(&s, "abcabc", 6);
            check(ss_isheaptype(s));
            check(ss_cap(s) == 6);
            ss_free(&s);
        }
    }

    describe("ss_find and ss_rfind")
    {
        it("should find the search string")
        {
            char buf[] = "asdfasdfasdf";
            size_t len = strlen(buf);

            char needle[] = "asdf";
            size_t nlen = strlen(needle);

            SS s = ss_newfrom(0, buf, len);

            size_t index = ss_find(s, 0, needle, nlen);
            check(index == 0);
            index = ss_find(s, 1, needle, nlen);
            check(index == 4);
            index = ss_find(s, 5, needle, nlen);
            check(index == 8);
            index = ss_find(s, 9, needle, nlen);
            check(index == NPOS);

            index = ss_rfind(s, ss_len(s) + 1, needle, nlen);
            check(index == 8);
            index = ss_rfind(s, 9, needle, nlen);
            check(index == 4);
            index = ss_rfind(s, 3, needle, nlen);
            check(index == 0);

            ss_free(&s);
        }

        it("should cover a find short-circuit case for code coverage")
        {
            char buf[] = "longstring";
            size_t len = strlen(buf);

            SS s = ss_newfrom(0, buf, len - 1);

            size_t index = ss_find(s, 0, buf, len);
            check(index == NPOS);

            ss_free(&s);
        }

        it("should find the search string in reverse")
        {
            char buf[] = "aszfzz";
            size_t len = strlen(buf);

            char needle[] = "asdf";
            size_t nlen = strlen(needle);

            SS s = ss_newfrom(0, buf, len);

            size_t index = ss_find(s, 0, needle, nlen);
            check(index == NPOS);

            index = ss_rfind(s, ss_len(s) + 1, needle, nlen);
            check(index == NPOS);

            ss_free(&s);
        }

        it("should cover a rfind short-circuit case for code coverage")
        {
            char buf[] = "longstring";
            size_t len = strlen(buf);

            SS s = ss_newfrom(0, buf + 1, len - 1);

            size_t index = ss_rfind(s, NPOS, buf, len);
            check(index == NPOS);

            ss_free(&s);
        }
    }

    describe("ss_count")
    {
        it("should return zero for zero length strings")
        {
            char hay[] = "aaaaaaaaaa";
            size_t hlen = strlen(hay);

            SS s = ss_newfrom(0, hay, hlen);
            size_t count = ss_count(s, 0, "", 0);
            check(0 == count);
            ss_free(&s);
        }

        it("should return zero when counting past the end of the string")
        {
            char hay[] = "aaaaaaaaaa";
            size_t hlen = strlen(hay);

            SS s = ss_newfrom(0, hay, hlen);
            size_t count = ss_count(s, hlen, "a", 1);
            check(0 == count);
            ss_free(&s);
        }

        it("should count single char strings fine")
        {
            char hay[] = "aaaaaaaaaa";
            size_t hlen = strlen(hay);

            SS s = ss_newfrom(0, hay, hlen);
            size_t count = ss_count(s, 0, "a", 1);
            check(count == hlen);
            count = ss_count(s, 4, "a", 1);
            check(count == (hlen - 4));
            ss_free(&s);
        }

        it("should count multi char strings fine")
        {
            char hay[] = "asdfzzzasdzzzasdfzzzzasdasdf";
            size_t hlen = strlen(hay);
            char needle[] = "asdf";
            size_t nlen = strlen(needle);
            size_t expect = 3;

            SS s = ss_newfrom(0, hay, hlen);
            size_t count = ss_count(s, 0, needle, nlen);
            check(count == expect);
            count = ss_count(s, 4, needle, nlen);
            check(count == (expect - 1));
            count = ss_count(s, 0, "hello", 5);
            check(!count);
            ss_free(&s);
        }

        it("should short-circuit when remaining chars are too short")
        {
            char hay[] = "longstrin";
            size_t hlen = strlen(hay);
            char needle[] = "longstring";
            size_t nlen = strlen(needle);
            size_t expect = 0;

            SS s = ss_newfrom(0, hay, hlen);
            size_t count = ss_count(s, 0, needle, nlen);
            check(count == expect);
            ss_free(&s);
        }
    }

    describe("ss_swap")
    {
        it("should swap two pointers")
        {
            char buf1[] = "asdf";
            size_t len1 = strlen(buf1);
            char buf2[] = "fdsa";
            size_t len2 = strlen(buf2);

            SS s1 = ss_newfrom(0, buf1, len1);
            SS s2 = ss_newfrom(0, buf2, len2);

            ss_swap(&s1, &s2);
            check(!strcmp(s1, buf2));
            check(!strcmp(s2, buf1));

            ss_free(&s1);
            ss_free(&s2);
        }
    }

    describe("ss_setlen")
    {
        char buf[] = "asdf";
        size_t len = strlen(buf);

        it("should set length to valid value less than length")
        {
            SS s = ss_newfrom(0, buf, len);
            check(ss_len(s) == len);
            ss_setlen(s, len/2);
            check(ss_len(s) == (len/2));
            ss_free(&s);
        }

        it("should not allow setting length past the end of the string")
        {
            SS s = ss_newfrom(0, buf, len);
            check(ss_len(s) == len);
            ss_setlen(s, 2*len);
            check(ss_len(s) == len);
            ss_free(&s);
        }
    }

    describe("ssc_setlen")
    {
        char buf[] = "asdf";
        size_t len = strlen(buf);

        it("should set c-string length when used correctly")
        {
            SS s = ss_newfrom(0, buf, len);
            s[2] = 0;
            ssc_setlen(s);
            check(ss_len(s) == 2);
            ss_free(&s);
        }

        it("should correctly set c-string length when sentinel is overwritten")
        {
            SS s = ss_newfrom(0, buf, len);
            s[len] = 0x43;
            ssc_setlen(s);
            check(ss_len(s) == len);
            ss_free(&s);
        }
    }

    describe("ss_trunc")
    {
        char buf[] = "asdf";
        size_t len = strlen(buf);

        it("should correctly truncate")
        {
            SS s = ss_newfrom(0, buf, len);
            ss_trunc(s, len/2);
            check(ss_len(s) == (len/2));
            ss_free(&s);
        }
    }

    describe("ss_reserve")
    {
        char buf[] = "asdf";
        size_t len = strlen(buf);

        it("should reallocate so the reserve amount is met")
        {
            SS s = ss_new(200);
            check(ss_cap(s) == 200);
            ss_reserve(&s, 300);
            check(ss_cap(s) == 300);
            ss_free(&s);
        }
    }

    describe("ss_fit")
    {
        char buf[] = "asdf";
        size_t len = strlen(buf);

        it("should reallocate so the capacity is also the length")
        {
            SS s = ss_newfrom(200, buf, len);
            check(ss_cap(s) == 200);
            ss_fit(&s);
            check(ss_cap(s) == len);
            check(eq(s, buf, len));
            ss_free(&s);
        }
    }

    describe("ss_resize")
    {
        char buf[] = "asdf";
        size_t len = strlen(buf);

        it("should not reallocate if resize capacity is the same")
        {
            SS s = ss_newfrom(200, buf, len);
            SS s2 = s;
            check(ss_cap(s) == 200);
            ss_resize(&s, 200);
            check(ss_cap(s) == 200);
            check(eq(s, buf, len));
            check(s == s2);
            ss_free(&s);
        }

        it("should resize to a large value")
        {
            SS s = ss_newfrom(200, buf, len);
            check(ss_cap(s) == 200);
            ss_resize(&s, 500);
            check(ss_cap(s) == 500);
            check(eq(s, buf, len));
            ss_free(&s);
        }

        it("should resize to a smaller value")
        {
            SS s = ss_newfrom(200, buf, len);
            check(ss_cap(s) == 200);
            ss_resize(&s, len/2);
            check(ss_cap(s) == (len/2));
            check(ss_len(s) == (len/2));
            ss_free(&s);
        }
    }

    describe("ss_addcap")
    {
        it("should not reallocate")
        {
            SS s = ss_new(10);
            SS save = s;
            ss_addcap(&s, 0);
            check(ss_cap(s) == 10);
            check(s == save);
            ss_free(&s);
        }

        it("should add the amount of capacity given")
        {
            SS s = ss_new(10);
            check(ss_cap(s) == 10);
            ss_addcap(&s, 10);
            check(ss_cap(s) == 20);
            ss_free(&s);
        }
    }

    describe("stack")
    {
        it("should allocate a string on the stack")
        {
            char buf[] = "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa";
            size_t len = strlen(buf);

            ss_stack(s, 32);
            check(is_empty(s));
            check(ss_cap(s) == 32);
            check(s[0] == 0);
            ss_resize(&s, 31);
            check(ss_cap(s) == 32);
            SS tmp = s;
            ss_copy(&s, buf, len);
            check(s != tmp);
            check(ss_cap(s) == len);
            check(ss_len(s) == len);
            ss_free(&s);
        }
    }

    describe("ss_clear")
    {
        it("should clear the buffer setting the length and sentinel correctly")
        {
            char buf[] = "asdf";
            size_t len = strlen(buf);

            SS s = ss_new(0);
            check(is_empty(s));
            ss_copy(&s, buf, len);
            check(eq(s, buf, len));
            ss_clear(s);
            check(is_empty(s));
            ss_free(&s);
        }
    }

    describe("ss_esc")
    {
        it("should esc basic escape sequences correctly")
        {
            char buf[] = "\\a\\b\\e\\f\\n\\r\\t\\v\\\\\\\'\\\"\\?";
            size_t len = strlen(buf);
            char ans[] = "\a\b\x1B\f\n\r\t\v\\\'\"\?";
            size_t alen = strlen(ans);

            SS s = ss_newfrom(0, buf, len);
            ssc_unesc(s);
            check(eq(s, ans, alen));
            ss_free(&s);
        }

        it("should move text correctly")
        {
            char buf[] = "\\\\text to move\\\\";
            size_t len = sizeof(buf) - 1;
            char ans[] = "\\text to move\\";
            size_t alen = sizeof(ans) - 1;

            SS s = ss_newfrom(0, buf, len);
            ssc_unesc(s);
            check(eq(s, ans, alen));
            ss_free(&s);
        }

        it("should correctly convert hex values")
        {
            char buf[] = "\\xinvalid\\x7F\\x0\\x00\\xff\\x3D";
            size_t len = sizeof(buf) - 1;
            char ans[] = "\\xinvalid\x7F\x00\x00\xFF\x3D";
            size_t alen = sizeof(ans) - 1;

            SS s = ss_newfrom(0, buf, len);
            ssc_unesc(s);
            check(eq(s, ans, alen));
            ss_free(&s);
        }

        it("should correctly handle 4 nibble escapes")
        {
            char buf[] = "\\uinvalid\\u1\\u22\\u333\\u4444\\u44444";
            size_t len = sizeof(buf) - 1;
            char ans[] = "\\uinvalid\x01\x22\u0333\u4444\u44444";
            size_t alen = sizeof(ans) - 1;

            SS s = ss_newfrom(0, buf, len);
            ssc_unesc(s);
            check(eq(s, ans, alen));
            ss_free(&s);
        }

        it("should correctly handle unicode 8 nibble escapes")
        {
            char buf[] = "\\Uinvalid\\U1\\U22\\U333\\U4444";
            size_t len = sizeof(buf) - 1;
            char ans[] = "\\Uinvalid\x01\x22\u0333\u4444";
            size_t alen = sizeof(ans) - 1;

            SS s = ss_newfrom(0, buf, len);
            ssc_unesc(s);
            check(eq(s, ans, alen));
            ss_free(&s);
        }

        it("should correctly escape octal values")
        {
            char buf[] = "\\0\\77\\007\\477\\377";
            size_t len = sizeof(buf) - 1;
            char ans[] = "\0\077\007\47""7\377";
            size_t alen = sizeof(ans) - 1;

            SS s = ss_newfrom(0, buf, len);
            ssc_unesc(s);
            check(eq(s, ans, alen));
            ss_free(&s);
        }

        it("should overcome a false positive")
        {
            char buf[] = "\\z";
            size_t len = sizeof(buf) - 1;
            char ans[] = "\\z";
            size_t alen = sizeof(ans) - 1;

            SS s = ss_newfrom(0, buf, len);
            ssc_unesc(s);
            check(eq(s, ans, alen));
            ss_free(&s);
        }

        it("should handle a hybrid test")
        {
            char buf[] = "\a\b\x1B\f\n\r\t\v\\\'\"asdf\x7F";
            size_t len = sizeof(buf) - 1;
            char ans[] = "\\a\\b\\e\\f\\n\\r\\t\\v\\\\\\\'\\\"asdf\\x7F";
            size_t alen = sizeof(ans) - 1;

            SS s = ss_newfrom(0, buf, len);
            ssc_esc(&s);
            check(eq(s, ans, alen));
            ss_free(&s);
        }
    }

    describe("ss_copy")
    {
        it("should copy the string and length correctly")
        {
            char buf[] = "asdfgh";
            size_t len = strlen(buf);

            SS s = ss_new(0);
            ss_copy(&s, buf, len);
            check(eq(s, buf, len));
            ss_free(&s);
        }
    }

    describe("ss_cat")
    {
        it("should concatenate the string")
        {
            char buf[] = "hello world";

            SS s = ss_new(0);
            ss_cat(&s, buf, strlen(buf));
            check(ss_len(s) == strlen(buf));
            check(s[ss_len(s)] == 0);
            ss_cat(&s, buf, strlen(buf));
            check(ss_len(s) == 2 * strlen(buf));
            check(s[ss_len(s)] == 0);
            check(!memcmp(s, buf, strlen(buf)));
            check(!memcmp(s + strlen(buf), buf, strlen(buf)));
            ss_free(&s);
        }
    }

    describe("ss_lcat")
    {
        it("should left-concatenate")
        {
            char buf[] = "hello";
            size_t blen = strlen(buf);
            char pre[] = "asdf";
            size_t plen = strlen(pre);

            SS s = ss_new(0);
            ss_lcat(&s, buf, blen);
            check(ss_len(s) == blen);
            check(s[ss_len(s)] == 0);
            check(!memcmp(s, buf, blen));
            ss_lcat(&s, pre, plen);
            check(ss_len(s) == (blen + plen));
            check(!memcmp(s, pre, plen));
            check(!memcmp(s + plen, buf, blen));
            check(s[ss_len(s)] == 0);
            ss_free(&s);
        }
    }

    describe("ss_replace")
    {
        it("should replace longer string with shorter, no reallocation")
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
            SS save = s;
            ss_replace(&s, 1, rep, rlen, wit, wlen);
            check(eq(s, ans, alen));
            check(save == s);
            ss_free(&s);
        }

        it("should replace string with the empty string")
        {
            char buf[] = "abcabcabcabc";
            size_t blen = strlen(buf);
            char rep[] = "abc";
            size_t rlen = strlen(rep);

            SS s = ss_newfrom(0, buf, blen);
            ss_replace(&s, 0, rep, rlen, "", 0);
            check(is_empty(s));
            ss_free(&s);
        }

        it("should do nothing when replacing empty string with string")
        {
            char buf[] = "empty";
            size_t blen = strlen(buf);

            SS s = ss_newfrom(0, buf, blen);
            SS save = s;
            ss_replace(&s, 0, "", 0, "", 0);
            check(save == s);
            ss_free(&s);
        }

        it("should replace same-sized strings")
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
            check(eq(s, ans, alen));
            ss_free(&s);
        }

        it("should replace with longer strings")
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
            check(eq(s, ans, alen));
            ss_free(&s);
        }
        
        it("should move text correctly when replacing shorter string")
        {
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
            check(eq(s, ans, alen));
            ss_free(&s);
        }

        it("should use char search correctly when replacing with longer")
        {
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
            check(eq(s, ans, alen));
            ss_free(&s);
        }

        it("should ignore false positive in char search")
        {
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
            check(eq(s, ans, alen));
            ss_free(&s);
        }
    }

    describe("ss_replacerange")
    {
        it("should replace with long string and empty string")
        {
            char replace[] = "something else";
            size_t rlen = strlen(replace);
            char buf[] = "aaaaaaaareplaceaaaaa";
            size_t blen = strlen(buf);
            size_t index = 8;
            size_t end = index + 7;
            
            SS s = ss_newfrom(0, buf, blen);
            ss_replacerange(&s, index, end, replace, rlen);
            check(ss_len(s) == (blen - (end - index) + rlen));
            check(!memcmp(s + index, replace, rlen));
            ss_replacerange(&s, 0, 8, "", 0);
            check(ss_len(s) == rlen + 5);
            check(s[ss_len(s)] == 0);
            check(!memcmp(s, replace, rlen));
            check(!memcmp(s + rlen, "aaaaa", 5));
            ss_free(&s);
        }

        it("should concatenate when length is at the end")
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
            check(eq(s, ans, alen));
            ss_free(&s);
        }
    }

    describe("ss_insert")
    {
        it("should insert")
        {
            char buf[] = "bbbbbb";
            size_t len = strlen(buf);
            size_t newlen = len;

            SS s = ss_newfrom(0, buf, len);
            ss_insert(&s, 6, "a", 1);
            ++newlen;
            check(ss_len(s) == newlen);
            
            ss_insert(&s, 3, "a", 1);
            ++newlen;
            check(ss_len(s) == newlen);

            ss_insert(&s, 0, "a", 1);
            ++newlen;
            check(ss_len(s) == newlen);

            char ans[] = "abbbabbba";
            check(!strcmp(s, ans));

            ss_insert(&s, 20, "a", 1);
            char ans2[] = "abbbabbbaa";
            check(!strcmp(s, ans2));

            ss_free(&s);
        }
    }

    describe("ss_overlay")
    {
        it("should overlay at the end correctly")
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
            check(ss_len(s) == alen);
            check(s[ss_len(s)] == 0);
            check(!memcmp(s, ans, alen));
            ss_overlay(&s, 0, over, olen);
            check(ss_len(s) == alen);
            check(!memcmp(s, over, olen));
            check(!memcmp(s + olen, ans + olen, alen - olen));
            ss_free(&s);
        }

        it("should concatenate when overlaying at the end")
        {
            char buf[] = "blah";
            size_t blen = strlen(buf);
            char over[] = "end";
            size_t olen = strlen(over);

            SS s = ss_newfrom(0, buf, blen);
            ss_overlay(&s, NPOS, over, olen);
            check(ss_len(s) == blen + olen);
            check(!memcmp(s, buf, blen));
            check(!memcmp(s + blen, over, olen));
            ss_free(&s);
        }
    }

    describe("ss_remove")
    {
        it("should not recursively remove")
        {
            char buf[] = "abczzzzabcababcc";
            size_t blen = strlen(buf);
            char rem[] = "abc";
            size_t rlen = strlen(rem);
            char ans[] = "zzzzabc";
            size_t alen = strlen(ans);

            SS s = ss_newfrom(0, buf, blen);
            ss_remove(s, 0, rem, rlen);
            check(ss_len(s) == alen);
            check(s[alen] == 0);
            check(!memcmp(s, ans, alen));
            ss_free(&s);
        }

        it("should start at the correct index while removing")
        {
            char buf[] = "abczzzzabcababcc";
            size_t blen = strlen(buf);
            char rem[] = "abc";
            size_t rlen = strlen(rem);
            char ans[] = "abczzzzabc";
            size_t alen = strlen(ans);

            SS s = ss_newfrom(0, buf, blen);
            ss_remove(s, 1, rem, rlen);
            check(ss_len(s) == alen);
            check(s[alen] == 0);
            check(!memcmp(s, ans, alen));
            ss_free(&s);
        }
    }

    describe("ss_removerange")
    {
        it("should remove from mid-string")
        {
            char buf[] = "abczzzzabcababcc";
            size_t blen = strlen(buf);
            size_t start = 3;
            size_t end = start + 4;
            char ans[] = "abcabcababcc";
            size_t alen = strlen(ans);

            SS s = ss_newfrom(0, buf, blen);
            ss_removerange(s, start, end);
            check(eq(s, ans, alen));
            ss_free(&s);
        }

        it("should remove from the end")
        {
            char buf[] = "abczzzzabcababcc";
            size_t blen = strlen(buf);
            size_t start = blen - 4;
            size_t end = blen + 4;
            char ans[] = "abczzzzabcab";
            size_t alen = strlen(ans);

            SS s = ss_newfrom(0, buf, blen);
            ss_removerange(s, start, end);
            check(eq(s, ans, alen));
            ss_free(&s);
        }
    }

    describe("ss_reverse")
    {
        it("should reverse correctly")
        {
            char buf[] = "abcd";
            char ans[] = "dcba";
            size_t len = strlen(buf);

            SS s = ss_newfrom(0, buf, len);
            ss_reverse(s);
            check(ss_len(s) == len);
            check(s[ss_len(s)] == 0);
            check(!memcmp(s, ans, len));
            ss_free(&s);
        }
    }

    describe("ss_trim")
    {
        it("should not trim if 0 len")
        {
            char buf[] = "howdy";
            size_t len = strlen(buf);

            SS s = ss_newfrom(0, buf, len);
            ss_trim(s, NULL, 0);
            check(eq(s, buf, len));
            ss_trim(s, "", 0);
            check(eq(s, buf, len));
            ss_free(&s);
        }

        it("should trim front and end")
        {
            char buf[] = "howdy";
            size_t len = strlen(buf);
            char trim[] = "hy";
            size_t tlen = strlen(trim);
            char ans[] = "owd";
            size_t alen = strlen(ans);

            SS s = ss_newfrom(0, buf, len);
            ss_trim(s, trim, tlen);
            check(eq(s, ans, alen));
            ss_free(&s);
        }

        it("should trim all")
        {
            char buf[] = "howdy";
            size_t len = strlen(buf);
            char trim[] = "howdy";
            size_t tlen = strlen(trim);
            char ans[] = "";
            size_t alen = strlen(ans);

            SS s = ss_newfrom(0, buf, len);
            ss_trim(s, trim, tlen);
            check(eq(s, ans, alen));
            ss_free(&s);
        }
    }

    describe("ss_trimrange")
    {
        it("should do nothing when trimming 0 len")
        {
            char buf[] = "asdfasdfasdf";
            size_t len = strlen(buf);

            SS s = ss_newfrom(0, buf, len);
            ss_trimrange(s, 1, len - 1, NULL, 0);
            check(eq(s, buf, len));
            ss_trimrange(s, 1, len - 1, "", 0);
            check(eq(s, buf, len));
            ss_free(&s);
        }

        it("should trim range")
        {
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
            check(eq(s, ans, alen));
            ss_free(&s);
        }

        it("should not trim if range is invalid")
        {
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
            check(eq(s, ans, alen));
            ss_free(&s);
        }
    }

    describe("ssc_trim")
    {
        it("should trim whitespace when NULL")
        {
            char buf[] = " \n\t\v\r\fasdf \n\t\v\r\f";
            size_t len = strlen(buf);
            char ans[] = "asdf";
            size_t alen = strlen(ans);

            SS s = ss_newfrom(0, buf, len);
            ssc_trim(s, NULL);
            check(eq(s, ans, alen));
            ss_free(&s);
        }

        it("should trim cstring chars")
        {
            char buf[] = "asdfasdfasdf";
            size_t len = strlen(buf);
            char trim[] = "af";
            char ans[] = "sdfasdfasd";
            size_t alen = strlen(ans);

            SS s = ss_newfrom(0, buf, len);
            ssc_trim(s, trim);
            check(eq(s, ans, alen));
            ss_free(&s);
        }

        it("should trim all")
        {
            char buf[] = "asdfasdfasdf";
            size_t len = strlen(buf);
            char trim[] = "asdf";
            char ans[] = "";
            size_t alen = strlen(ans);

            SS s = ss_newfrom(0, buf, len);
            ssc_trim(s, trim);
            check(eq(s, ans, alen));
            ss_free(&s);
        }

        it("should not modify empty")
        {
            SS s = ss_empty();
            ssc_trim(s, "asdf");
            check(is_empty(s));
            ss_free(&s);
        }
    }

    describe("ss_upper and ss_lower")
    {
        it("should uppercase")
        {
            char buf[] = "asdf";
            size_t len = strlen(buf);
            char ans[] = "ASDF";
            size_t alen = strlen(ans);

            SS s = ss_newfrom(0, buf, len);
            ssc_upper(s);
            check(eq(s, ans, alen));
            ss_free(&s);
        }

        it("should lowercase")
        {
            char buf[] = "ASDF";
            size_t len = strlen(buf);
            char ans[] = "asdf";
            size_t alen = strlen(ans);

            SS s = ss_newfrom(0, buf, len);
            ssc_lower(s);
            check(eq(s, ans, alen));
            ss_free(&s);
        }

        it("should not modify empty")
        {
            SS s = ss_empty();
            ssc_upper(s);
            ssc_lower(s);
            check(is_empty(s));
            ss_free(&s);
        }
    }

    describe("ss_copyf")
    {
        it("should copy a formatted string")
        {
            char buf[] = "asdf";
            size_t len = strlen(buf);

            SS s = ss_new(0);
            ss_copyf(&s, "%s", buf);
            check(ss_len(s) == len);
            check(!memcmp(s, buf, len));
            check(s[len] == 0);
            ss_free(&s);

            s = ss_empty();
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat"
#pragma GCC diagnostic ignored "-Wformat-extra-args"
            check(ss_copyf(&s, "%", buf));
#pragma GCC diagnostic pop
            check(is_empty(s));
            ss_free(&s);
        }
    }

    describe("ss_catf")
    {
        it("should concatenate a formatted string")
        {
            char buf[] = "hello world";
            size_t len = strlen(buf);

            SS s = ss_new(0);
            ss_catf(&s, "%s", buf);
            check(eq(s, buf, len));
            ss_catf(&s, "%s", buf);
            check(ss_len(s) == 2 * len);
            check(s[2 * len] == 0);
            check(!memcmp(s + len, buf, len));
            ss_free(&s);

            s = ss_empty();
            ss_catf(&s, "%s", buf);
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat"
#pragma GCC diagnostic ignored "-Wformat-extra-args"
            check(ss_catf(&s, "%", buf));
#pragma GCC diagnostic pop
            check(eq(s, buf, len));
            ss_free(&s);
        }
    }

    describe("ss_catint64")
    {
        it("should be zero")
        {
            char buf[] = "0";
            size_t len = strlen(buf);
            int64_t n = 0;
            SS s = ss_empty();
            ss_catint64(&s, n);
            check(eq(s, buf, len));
            ss_free(&s);
        }

        it("should be one")
        {
            char buf[] = "1";
            size_t len = strlen(buf);
            int64_t n = 1;
            SS s = ss_empty();
            ss_catint64(&s, n);
            check(eq(s, buf, len));
            ss_free(&s);
        }

        it("should be negative one")
        {
            char buf[] = "-1";
            size_t len = strlen(buf);
            int64_t n = -1;
            SS s = ss_empty();
            ss_catint64(&s, n);
            check(eq(s, buf, len));
            ss_free(&s);
        }

        it("should be max")
        {
            char buf[] = "9223372036854775807";
            size_t len = strlen(buf);
            int64_t n = 9223372036854775807LL;
            SS s = ss_empty();
            ss_catint64(&s, n);
            check(eq(s, buf, len));
            ss_free(&s);
        }

        it("should be neg max")
        {
            char buf[] = "-9223372036854775808";
            size_t len = strlen(buf);
            int64_t n = (-9223372036854775807LL) - 1;
            SS s = ss_empty();
            ss_catint64(&s, n);
            check(eq(s, buf, len));
            ss_free(&s);
        }
    }

    describe("ss_catuint64")
    {
        it("should be zero")
        {
            char buf[] = "0";
            size_t len = strlen(buf);
            uint64_t n = 0;
            SS s = ss_empty();
            ss_catuint64(&s, n);
            check(eq(s, buf, len));
            ss_free(&s);
        }

        it("should be one")
        {
            char buf[] = "1";
            size_t len = strlen(buf);
            uint64_t n = 1;
            SS s = ss_empty();
            ss_catuint64(&s, n);
            check(eq(s, buf, len));
            ss_free(&s);
        }

        it("should be max")
        {
            char buf[] = "18446744073709551615";
            size_t len = strlen(buf);
            uint64_t n = 0xFFFFFFFFFFFFFFFFULL;
            SS s = ss_empty();
            ss_catuint64(&s, n);
            check(eq(s, buf, len));
            ss_free(&s);
        }
    }

    describe("ssu_isvalid")
    {
        it("should pass valid points")
        {
            check(ssu_isvalid(0));
            check(ssu_isvalid(0x0000D7FF));
            check(ssu_isvalid(0x0000E000));
            check(ssu_isvalid(0x0010FFFF));
        }

        it("should not pass invalid points")
        {
            check(!ssu_isvalid(0x0000D800));
            check(!ssu_isvalid(0x0000DFFE));
            check(!ssu_isvalid(0x0000DFFF));
            check(!ssu_isvalid(0x00110000));
            check(!ssu_isvalid((unicode_t)-1));
        }
    }
        
    describe("ssu8_cpseqlen")
    {
        it("should return correct sequence lengths")
        {
            check(1 == ssu8_cpseqlen(0));
            check(1 == ssu8_cpseqlen(127));
            // 8 bits
            check(2 == ssu8_cpseqlen(128));
            // 11 bits
            check(2 == ssu8_cpseqlen(0x000007FF));
            // 12 bits
            check(3 == ssu8_cpseqlen(0x00000800));
            // 16 bits
            check(3 == ssu8_cpseqlen(0x0000FFFF));
            // 17 bits
            check(4 == ssu8_cpseqlen(0x00010000));
            // 21 bits
            check(4 == ssu8_cpseqlen(0x0010FFFF));
        }

        it("should return zero for these sequences")
        {
            // 21 bits
            check(0 == ssu8_cpseqlen(0x001FFFFF));
            // 22 bits
            check(0 == ssu8_cpseqlen(0x00200000));
            // 26 bits
            check(0 == ssu8_cpseqlen(0x03FFFFFF));
            // 27 bits
            check(0 == ssu8_cpseqlen(0x04000000));
            // 31 bits
            check(0 == ssu8_cpseqlen(0x7FFFFFFF));
            // 32 bits
            check(0 == ssu8_cpseqlen(0xFFFFFFFF));
        }
    }

    describe("ssu8_seqlen")
    {
        it("should return correct sequence lengths")
        {
            check(1 == ssu8_seqlen("\0"));
            check(1 == ssu8_seqlen("\x7F"));
            check(2 == ssu8_seqlen("\xC0"));
            check(3 == ssu8_seqlen("\xE0"));
            check(4 == ssu8_seqlen("\xF0"));
        }

        it("should return zero for invalid sequence bytes")
        {
            check(0 == ssu8_seqlen("\x80"));
            check(0 == ssu8_seqlen("\xF8"));
            check(0 == ssu8_seqlen("\xFC"));
            check(0 == ssu8_seqlen("\xFE"));
            check(0 == ssu8_seqlen("\xFF"));
        }
    }

    describe("ssu8_cptoseq and ssu8_seqtocp")
    {
        it("should treat us-ascii correctly")
        {
            char buf[SS_UTF8_SEQ_MAX];

            memset(buf, 0, sizeof(buf));
            check(1 == ssu8_cptoseq(0, buf));
            check(0 == *buf);

            memset(buf, 0, sizeof(buf));
            check(1 == ssu8_cptoseq(127, buf));
            check(127 == *buf);
        }

        it("should convert to and from code points/sequences")
        {
            char buf[SS_UTF8_SEQ_MAX];

            unicode_t cp[] =
            {
                0x00000000,
                0x0000007F,
                0x00000080,
                0x000007FF,
                0x00000800,
                0x0000FFFF,
                0x00010000,
                0x0010FFFF,
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
                "\xF4\x8F\xBF\xBF",
            };

            size_t len = sizeof(cp)/sizeof(cp[0]);

            size_t i;
            for (i = 0; i < len; ++i)
            {
                unicode_t c = cp[i];
                const char *a = ans[i];
                memset(buf, 0, sizeof(buf));
                check(ssu8_cpseqlen(c) == ssu8_cptoseq(c, buf));
                check(0 == memcmp(buf, a, strlen(a)));
            }

            for (i = 0; i < len; ++i)
            {
                unicode_t c = 0;
                unicode_t cpans = cp[i];
                const char *u = ans[i];

                check(ssu8_cpseqlen(cpans) == ssu8_seqtocp(u, &c));
                check(cpans == c);
            }
        }

        it("should fail on these code points")
        {
            char buf[SS_UTF8_SEQ_MAX];

            memset(buf, 0, sizeof(buf));
            check(1 == ssu8_cptoseq(0, buf));
            check(0 == *buf);

            memset(buf, 0, sizeof(buf));
            check(1 == ssu8_cptoseq(127, buf));
            check(127 == *buf);


            unicode_t cp[] =
            {
                0x00110000,
                0x00200000,
                0x03FFFFFF,
                0x04000000,
                0x7FFFFFFF,
            };

            size_t len = sizeof(cp)/sizeof(cp[0]);

            size_t i;
            for (i = 0; i < len; ++i)
            {
                unicode_t c = cp[i];
                check(0 == ssu8_cpseqlen(c));
                check(0 == ssu8_cptoseq(c, buf));
            }
        }

        it("should fail on these sequences")
        {
            const char *seq[] =
            {
                "\x80",
                "\xC2\xC0",
                "\xE0\xC0\xC0",
                "\xF0\xC0\xC0\xC0",
            };

            size_t len = sizeof(seq)/sizeof(seq[0]);

            size_t i;
            for (i = 0; i < len; ++i)
            {
                const char *s = seq[i];
                unicode_t c = 0;
                check(0 == ssu8_seqtocp(s, &c));
            }
        }
    }

    describe("sse_clz32")
    {
        it("should count leading zeros")
        {
            check(32 == sse_clz32(0));
            uint32_t n = 1;
            int count = 31;
            do
            {
                check(count == sse_clz32(n));
                n <<= 1;
                --count;
            } while (n);
        }
    }

    describe("sse_memrchar")
    {
        it("should find chars in reverse")
        {
            char buf[] = "asdfasdf";
            size_t len = strlen(buf);

            check(&buf[7] == sse_memrchar(buf, 'f', len));
            check(&buf[6] == sse_memrchar(buf, 'd', len));
            check(&buf[5] == sse_memrchar(buf, 's', len));
            check(&buf[4] == sse_memrchar(buf, 'a', len));
            check(&buf[3] == sse_memrchar(buf, 'f', len - 2));
        }
    }
}

