
# Simple String
Simple string in C. Buffer is ideal for c-strings (null sentinel), UTF8, or binary.
Sometimes you just need a string. Whether c-string, UTF8, or binary.


## Build
For those unfamiliar with cmake:

        mkdir build && cd build
        cmake ..
        cmake --build .

For code coverage:

        cmake -DCODE_COVERAGE=ON ..
        cmake --build .

For installation:

        cd build
        sudo cmake --install .
        sudo ldconfig

Cleanup:

        rm -rf build


## Testing
I believe I've tested enough of the code that it is stable (~95% line coverage).
Build tests like you build the project.
Run tests with:

        ctest -VV

Download git submodules prior to code coverage or doxygen:

        git submodule update --init

Run code coverage:

        make ccov-prove


## Acknowledgements
Thanks to the author of bdd-for-c.
I have a slightly modified version in `test/`.


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
1. See operations in `ss.h`.
1. See code in `src/`.
1. See tests in `test/`.
1. See remaining comments or contribute.
1. Ok.


## Warning
I designed this for x86 arch.
Some architectures require pointer adresses to be sizeof(pointer) aligned.
So test for any other architecture.


## Design
The string length and other metadata are stored behind the string pointer (like malloc does).
This allows you to use c-string functions and easy indexing.
Each string automatically inserts and maintains a null character/sentinel at the end.

Additionally, there are constant cost empty strings, get rid of those NULL pointers.
This is acheived by checking length and allocating on modification functions.
So we can use a global reference for a static empty string.

Finally, I decided to omit returning error values for ENOMEM conditions.
Mostly because code littered with checks becomes ugly and hard to read.
There are times to check memory and times to not.
Really, just try to avoid allocations altogether and focus on freeing memory
when possible.


## Basic Examples
1. Include:

        #include "sstring.h"

1. Create a string:

        SS s = ss_new(0); /* Exact fit allocated string. */
        SS s2 = ss_newfrom(30, "hello", 5); /* Allocated with extra space. */
        SS s3 = ss_empty(): /* Non-allocated empty string. */
        ss_stack(s4, 32); /* Stack allocated, switches to heap if too long. */

        /* Do string operations here. */

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
        ss_setgrow(&s, SS_GROW100); /* Double reallocation length. */

1. Get string length in O(1) time:

        size_t len = ss_len(s);

1. Copy into a string:

        ss_copy(&s, "to copy", 7);

1. Empty string:

        s = ss_empty(); /* Static global variable for empty string, no allocation. */
        ss_copy(&s, "word", 4); /* Heap allocated now. */
        /* Free, even if still empty. Just in-case. */
        ss_free(&s);

1. Fun formatting functions:

        s = ss_empty();
        /* GCC systems will give you a compilation warning for bad formats. */
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

1. Better example, do this:

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


## TODO
- Getting weird implicit function declaration error (but I'm sure I've declared it...)
- Add documentation
- Add doxygen generation
- Add static library generation to build setup.

