/* macOS (Rosetta 2) guest memory layout: low maps stay below Rosetta's reserved range
 * [0xfc0000000, 0x7000000000), the shm pool aliases, and guest fixed maps inside the range fail. */
#define _GNU_SOURCE
#include "runtime.h"
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>

#ifdef MEMORY_TEST_STANDALONE
/* Links runtime_memory.c alone until all of src/runtime*.c builds on macOS (Task 6). */
int32_t *runtime_errno(void) { static int32_t e; return &e; }
uintptr_t runtime_lookup(const RuntimeExport *table, size_t count, const char *nid) { (void)table; (void)count; (void)nid; return 0; }
void runtime_wait_begin(const char *kind, const void *object, const char *name, const void *const *owner, uintptr_t frame) {
    (void)kind; (void)object; (void)name; (void)owner; (void)frame;
}
void runtime_wait_end(void) {}
uintptr_t runtime_resolve(const char *name, int is_data) { (void)is_data; return runtime_memory_resolve(name); }
#else
/* The settings menu of the GPU library restarts through probe.c, which tests do not link. */
void runtime_restart(void) { abort(); }
#endif

typedef int32_t (ABI *Allocate)(int64_t, int64_t, uint64_t, uint64_t, int, int64_t *);
typedef int32_t (ABI *Map)(void **, uint64_t, int, int, int64_t, uint64_t);
#define GET(type, name) ((type)runtime_resolve(name, 0))

int main(void) {
    int got_null=0, mapped=0;
    for (int i=0; i<200; ++i) {
        uintptr_t p=(uintptr_t)runtime_low_map(256u<<20,PROT_READ|PROT_WRITE);
        if (!p) { got_null=1; continue; }
        assert(!got_null); /* exhausted stays exhausted */
        assert(p>=UINT64_C(0x800000000) && p+(256u<<20)<=UINT64_C(0xfc0000000));
        ++mapped;
    }
    assert(got_null && mapped>0);

    Allocate allocate=GET(Allocate,"rTXw65xmLIA#libkernel");
    Map map=GET(Map,"L-Q3LEjIbgA#libkernel");
    assert(allocate && map);
    int64_t phys=-1;
    assert(allocate(0,INT64_MAX,65536,0,0,&phys)==0 && phys>=0);
    void *a=NULL, *b=NULL;
    assert(map(&a,65536,0x33,0,phys,0)==0 && map(&b,65536,0x33,0,phys,0)==0 && a!=b);
    ((volatile unsigned char *)a)[123]=0x5a;
    assert(((volatile unsigned char *)b)[123]==0x5a);

    void *fixed=(void *)UINT64_C(0x1000000000);
    assert((uint32_t)map(&fixed,65536,0x33,0x10,phys,0)==0x8002000c);
    printf("macos memory test passed (%d low maps of 256 MiB)\n",mapped);
    return 0;
}
