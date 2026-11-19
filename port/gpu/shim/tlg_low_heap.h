// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
// tlg: macOS: host objects whose address reaches the guest live below 1 TiB (the guest packs
// pointers into 40 bits; Darwin's malloc zones sit near 0x6000_0000_0000). Class-scoped
// operators, so new/delete/make_unique of the class use runtime_low_heap.c.
#ifdef __APPLE__
#include <cstddef>
#include <new>
extern "C" void* runtime_low_malloc(std::size_t size);
extern "C" void runtime_low_free(void* p);
#define TLG_LOW_HEAP_OPERATORS                                                                    \
    static void* operator new(std::size_t size) {                                                 \
        if (void* p = runtime_low_malloc(size)) return p;                                         \
        throw std::bad_alloc();                                                                   \
    }                                                                                             \
    static void operator delete(void* p) noexcept { runtime_low_free(p); }
#else
#define TLG_LOW_HEAP_OPERATORS
#endif
