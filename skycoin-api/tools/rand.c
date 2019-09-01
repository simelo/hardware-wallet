/**
 * This file is part of the Skycoin project, https://skycoin.net/
 * This file is part of Trezor, https://trezor.com/
 *
 * Copyright (C) 2018-2019 Skycoin Project
 * Copyright (c) 2013-2014 Tomas Dzetkulic
 * Copyright (c) 2013-2014 Pavol Rusnak
 *
 * Permission is hereby granted, free of charge, to any person obtaining
 * a copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES
 * OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 */

#include <string.h>

#include "rand.h"
#include "entropypool.h"

#ifndef RAND_PLATFORM_INDEPENDENT

#include <stdio.h>
#ifdef _WIN32
#include <time.h>
#else
#include <assert.h>
#endif

// This function is not compiled in firmware
uint32_t __attribute__((weak)) _random32(void)
{
#ifdef _WIN32
    static int initialized = 0;
    if (!initialized) {
        srand((unsigned)time(NULL));
        initialized = 1;
    }
    return ((rand() % 0xFF) | ((rand() % 0xFF) << 8) | ((rand() % 0xFF) << 16) | ((rand() % 0xFF) << 24));
#else
    static FILE* frand = NULL;
    if (!frand) {
        frand = fopen("/dev/urandom", "r");
    }
    uint32_t r;
    size_t len_read = fread(&r, 1, sizeof(r), frand);
    assert(len_read == sizeof(r));
    return r;
#endif
}

#endif /* RAND_PLATFORM_INDEPENDENT */

//
// The following code is platform independent
//

#ifdef __CYGWIN__
#pragma weak _random_buffer
void _random_buffer(uint8_t* buf, size_t len)
#else
void __attribute__((weak)) _random_buffer(uint8_t* buf, size_t len)
#endif
{
    uint32_t *ptr = (uint32_t *) buf;
    size_t remaining = len;

    for (; remaining >= 4; ++ptr, remaining -= 4) {
        *ptr = _random32();
    }
    if (remaining > 0) {
        uint32_t r = _random32();
        memcpy((void *) ptr, (void *) &r, remaining);
    }
}

uint32_t random32(void)
{
    return random32_salted();
}

void random_buffer(uint8_t* buf, size_t len)
{
    return random_salted_buffer(buf, len);
}

uint32_t random_uniform(uint32_t n)
{
    uint32_t x, max = 0xFFFFFFFF - (0xFFFFFFFF % n);
    while ((x = random32()) >= max) {}
    return x / (max / n);
}

void random_permute(char* str, size_t len)
{
    for (int i = len - 1; i >= 1; i--) {
        int j = random_uniform(i + 1);
        char t = str[j];
        str[j] = str[i];
        str[i] = t;
    }
}
