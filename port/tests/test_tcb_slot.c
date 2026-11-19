/* Guest TCB slot: `mov rax, gs:[runtime_thread_tcb_offset()]` (the loader's rewritten 9-byte TCB
 * load) reads the value stored in the slot, per thread. */
#define _GNU_SOURCE
#include "runtime.h"
#include <assert.h>
#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/mman.h>

#ifdef TCB_TEST_STANDALONE
/* Links runtime_thread.c alone until all of src/runtime*.c builds on macOS (Task 6). */
#include <stdlib.h>
void *runtime_low_map(size_t size, int prot) { (void)size; (void)prot; abort(); }
void runtime_thread_keys_cleanup(void) { abort(); }
#endif

uint32_t runtime_thread_tcb_offset(void);
typedef uint64_t (*Load)(void);
static Load load;
static pthread_key_t key_of(uint32_t offset) {
#ifdef __APPLE__
    return (pthread_key_t)(offset / 8); /* GS base = the pthread TSD array */
#else
#error "test_tcb_slot covers the macOS pthread-key scheme"
#endif
}
static void *other(void *unused) {
    (void)unused;
    assert(!pthread_setspecific(key_of(runtime_thread_tcb_offset()), (void *)0x5678ef01));
    return (void *)(uintptr_t)load();
}

int main(void) {
    uint32_t off = runtime_thread_tcb_offset();
    assert(off == runtime_thread_tcb_offset());
    unsigned char code[10] = {0x65, 0x48, 0x8b, 0x04, 0x25, 0, 0, 0, 0, 0xc3};
    memcpy(code + 5, &off, 4);
    unsigned char *page = mmap(NULL, 4096, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    assert(page != MAP_FAILED);
    memcpy(page, code, sizeof code);
    assert(!mprotect(page, 4096, PROT_READ | PROT_EXEC));
    load = (Load)(void *)page;
    assert(!pthread_setspecific(key_of(off), (void *)0x1234abcd));
    assert(load() == 0x1234abcd);
    pthread_t t;
    void *got;
    assert(!pthread_create(&t, NULL, other, NULL) && !pthread_join(t, &got));
    assert((uintptr_t)got == 0x5678ef01);
    assert(load() == 0x1234abcd);
    printf("tcb slot test passed (offset 0x%x)\n", off);
    return 0;
}
