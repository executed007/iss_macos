/* macOS (Rosetta 2) guest memory layout: low maps stay below Rosetta's reserved range
 * [0xfc0000000, 0x7000000000), the shm pool aliases, and guest fixed maps inside the range fail. */
#define _GNU_SOURCE
#include "runtime.h"
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>

/* The settings menu of the GPU library restarts through probe.c, which tests do not link. */
void runtime_restart(void) { abort(); }

typedef int32_t (ABI *Allocate)(int64_t, int64_t, uint64_t, uint64_t, int, int64_t *);
typedef int32_t (ABI *Map)(void **, uint64_t, int, int, int64_t, uint64_t);
typedef int32_t (ABI *Release)(uint64_t, uint64_t);
#define GET(type, name) ((type)runtime_resolve(name, 0))

int main(void) {
    /* Host allocations stay out of the system window and the guest user range. */
    runtime_memory_reserve();
    /* 40 GiB: Darwin fills bottom-up, so without the placeholders these reach 0x700000000. */
    void *hosts[40];
    for (int i=0; i<40; ++i) {
        uintptr_t host=(uintptr_t)mmap(NULL,UINT64_C(1)<<30,PROT_READ|PROT_WRITE,MAP_PRIVATE|MAP_ANONYMOUS,-1,0);
        assert(host!=(uintptr_t)MAP_FAILED);
        assert(host+(UINT64_C(1)<<30)<=UINT64_C(0x700000000) || host>=UINT64_C(0x800000000));
        assert(host+(UINT64_C(1)<<30)<=UINT64_C(0x7000000000) || host>=UINT64_C(0xfc00000000));
        hosts[i]=(void *)host;
    }
    for (int i=0; i<40; ++i) munmap(hosts[i],UINT64_C(1)<<30);
    int got_null=0, mapped=0;
    for (int i=0; i<200; ++i) {
        uintptr_t p=(uintptr_t)runtime_low_map(256u<<20,PROT_READ|PROT_WRITE);
        if (!p) { got_null=1; continue; }
        assert(!got_null); /* exhausted stays exhausted */
        assert(p>=UINT64_C(0x800000000) && p+(256u<<20)<=UINT64_C(0xfc0000000));
        ++mapped;
    }
    assert(got_null && mapped>0);

    runtime_start(1); /* the resolver answers once started */
    Allocate allocate=GET(Allocate,"rTXw65xmLIA#libkernel");
    Map map=GET(Map,"L-Q3LEjIbgA#libkernel");
    assert(allocate && map);
    int64_t phys=-1;
    assert(allocate(0,INT64_MAX,65536,0,0,&phys)==0 && phys>=0);
    void *a=NULL, *b=NULL;
    assert(map(&a,65536,0x33,0,phys,0)==0 && map(&b,65536,0x33,0,phys,0)==0 && a!=b);
    ((volatile unsigned char *)a)[123]=0x5a;
    assert(((volatile unsigned char *)b)[123]==0x5a);

    /* Released direct memory reads as zero when allocated again. */
    Release release=GET(Release,"MBuItvba6z8#libkernel");
    assert(release && release((uint64_t)phys,65536)==0);
    int64_t again=-1; void *c=NULL;
    assert(allocate(0,INT64_MAX,65536,0,0,&again)==0 && again==phys);
    assert(map(&c,65536,0x33,0,again,0)==0 && ((volatile unsigned char *)c)[123]==0);

    void *fixed=(void *)UINT64_C(0x1000000000);
    assert((uint32_t)map(&fixed,65536,0x33,0x10,phys,0)==0x8002000c);
    printf("macos memory test passed (%d low maps of 256 MiB)\n",mapped);
    return 0;
}
