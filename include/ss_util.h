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
 * @file ss_util.h
 * @author Craig Jacobson
 * @brief Lower level utilities for simple tasks.
 *
 * These can be replaced if there are better implementations that the user
 * prefers.
 * For example, maybe you prefer to use SSE instructions for memchr.
 */
#ifndef SS_UTIL_H_
#define SS_UTIL_H_
#ifdef __cplusplus
extern "C" {
#endif


#include <string.h>

/// @cond DOXYGEN_IGNORE

#define ss_rawalloc _ss_rawalloc_impl
#define ss_rawrealloc _ss_rawrealloc_impl
#define ss_rawfree _ss_rawfree_impl

#define ss_cstrlen strlen
#define ss_cstrchar strchr
#define ss_memcopy memcpy
#define ss_memmove memmove
#define ss_memchar memchr
#define ss_mmemchar memchr
#define ss_memrchar _ss_memrchar
#define ss_memcompare memcmp
#define ss_vsnprintf vsnprintf

/* Maximum growth rate ~1MB (2**20). */
#define SS_MAX_REALLOC (0x100000)

/// @endcond

#ifdef __cplusplus
}
#endif
#endif /* SS_UTIL_H_ */


