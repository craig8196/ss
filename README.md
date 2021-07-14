
# libss: Simple String Library
Simple string in C.
Buffer is ideal for c-strings (null sentinel), UTF8, or binary.


## Design Choices
The string length and other metadata are stored behind the string pointer (like malloc does).
This allows you to use c-string functions and easy indexing.
This also gives O(1) time to retrieve the length, as opposed to O(n) with strlen.
Each string automatically inserts and maintains a null character/sentinel at the end.

The empty strings are O(1) cost, but are compatible with all functions.
This allows you to write code that doesn't need to check for NULL pointers.
This is done by pointing to a global empty string and checking length before modification.
The empty string will heap allocate when needed.

The stack string will place the needed data on the stack.
When needed the string will switch to heap allocation if length gets too long.
This is nice if you have a situation where most strings are expected to be
quite short, but not guaranteed.

I decided to omit returning error values for ENOMEM conditions.
The default memory allocation functions will print an error and abort.
Mostly because code littered with checks becomes ugly and hard to read.
I kept some of the code from when errors were reported, some work is needed
to make it so you can enable it from cmake.

A static library isn't built because of some complications that arise
with building both shared and static libraries.
With static builds you could use git submodules and include the source and
headers directly.

The library is compiled with -fPIC.


## Basic Examples
1. Include:

        #include "ss.h"

1. Create a string:

        SS s = ss_new(10); /* Exact fit allocated string. */
        SS s2 = ss_newfrom(30, "hello", 5); /* Allocated with extra space. */
        SS s3 = ss_empty(): /* Non-allocated, non-null, empty string. */
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
I believe I've tested enough of the code that it is stable
(~98% line/function coverage, only malloc fail code not tested).
Build tests like you build the project.
Run tests with:

        ctest -VV

Download git submodules and utilities prior to code coverage or doxygen:

        git submodule update --init
        apt install doxygen graphviz lcov

Run code coverage:

        make ccov-prove


## Acknowledgements
Thanks to the author of bdd-for-c for a very light-weight test framework.
I have a slightly modified version in `test/`.


## Compatibility Warning
I designed this for x86 arch.
Some architectures require pointer adresses to be sizeof(pointer) aligned.
So test for any other architecture.


## TODO
- Re-run code-coverage
- Fix code-coverage holes
- Fix ss.pc.in, the requires section is wrong



