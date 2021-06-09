
# Simple String
Simple string in C. Buffer is ideal for c-strings (null sentinel), UTF8, or binary.
Sometimes you just need a string. Whether c-string, UTF8, or binary.


## Tested
I believe I've tested enough of the code that it is stable (~99% line coverage).
Run tests with:

        make test

Run code coverage report with:

        make test prof=coverage


## Design Philosophy
I believe that to write a good library the following must be done in order:
1. Identify the problem.
1. Write the interface.
1. Define the algorithms.
1. Test the code.
1. Optimize.
1. Enjoy.

So...
1. Effective, convenient, and reasonably efficient string handling.
1. See operations in `sstring.h`.
1. See code in `src/`.
1. See tests in `test/`.
1. See remaining comments or contribute.
1. Ok.


## Warning
I designed this for x86 arch.
Some architectures require pointer adresses to be sizeof pointer aligned.
So test for any other architecture.


## Design
The string length and other metadata are stored behind the string pointer (like malloc does).
This allows you to use c-string functions and easy indexing.
Each string automatically inserts and maintains a null character/sentinel at the end.

Additionally, there are constant cost empty strings, get rid of those NULL pointers.
This is acheived by checking length and allocating on modification functions.
So we can use a global reference for a static empty string.


## Basic Examples
1. Include:

        #include "sstring.h"

1. Create a string:

        SS s = ss_new(0); /* Exact fit allocated string. */
        SS s2 = ss_newfrom(30, "hello", 5); /* Allocated with extra space. */
        SS s3 = ss_empty(): /* Non-allocated empty string. */
        ss_stack(s4, 32); /* Stack allocated, switches to heap if too long. */
        /* Always free your string. */
        ss_free(&s);
        ss_free(&s2);
        ss_free(&s3);
        ss_free(&s4);

1. Move string to heap:

        ss_stack(s, 10);
        /* Do something. */
        ss_heapify(&s);
        /* Now string is definitely on the heap. */
        /* Free or pass the string around. */

1. Make string grow faster for fewer reallocations:

        s = ss_empty();
        ss_set_growth(SS_GROW_OPT_100); /* Double reallocation length. */

1. Get string length in O(1) time:

        size_t len = ss_len(s);

1. Copy into a string:

        if (ss_copy(&s, "to copy", 7))
        {
            return ENOMEM;
        }

1. Empty string:

        s = ss_empty(); /* Static global variable for empty string, no allocation. */
        ss_copy(&s, "word", 4); /* Heap allocated now. */
        /* Free, even if still empty. Just in-case. */
        ss_free(&s);

1. Fun formatting functions:

        s = ss_empty();
        ss_copyf(&s, %d: ", 123);
        ss_catf(&s, "%s\n", "hello");
        /* s = "123: hello\n" */
        ss_free(&s); /* Good memory citizens. */


## Cautionary Examples
If you duplicate a pointer you may cause invalid memory.
Store the string pointer in a struct or pass by reference.


1. Bad example, don't do this:

        void
        function do_something(SS s)
        {
            ss_copy(&s, "badbadbad", 9);
        }

        int
        main()
        {
            SS s = ss_new(0);
            do_something(s);
            // Now s points to invalid memory.
            ss_free(&s); // Error
            return 1;
        }

1. Better example, you can do this:

        void
        function do_something(SS *s)
        {
            ss_copy(s, "good", 4);
        }

        int
        main()
        {
            SS s = ss_new(0);
            do_something(&s);
            ss_free(&s);
            return 0;
        }


