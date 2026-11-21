/* macOS (Rosetta 2) guest memory layout: low maps stay below Rosetta's reserved range
 * [0xfc0000000, 0x7000000000), the shm pool aliases, and guest fixed maps inside the range fail. */
#define _GNU_SOURCE
#include "runtime.h"
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>

/* The settings menu of the GPU library restarts through probe.c, which tests do not link. */
void runtime_restart(void) { abort(); }

typedef int32_t (ABI *Allocate)(int64_t, int64_t, uint64_t, uint64_t, int, int64_t *);
typedef int32_t (ABI *Map)(void **, uint64_t, int, int, int64_t, uint64_t);
typedef int32_t (ABI *Release)(uint64_t, uint64_t);
typedef int32_t (ABI *MapFlexible)(void **, uint64_t, int, int);
#define GET(type, name) ((type)runtime_resolve(name, 0))
/* The GPU's view of guest memory (tlggpu, BufferCache without sparse binding). */
void *runtime_memory_backing_view(uintptr_t address, uint64_t size);
uint8_t *runtime_memory_pool(uint64_t *size);
int runtime_memory_read_backing(uintptr_t address, void *data, uint64_t size);

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

    /* Only direct memory is imported (Task 8c): direct mappings resolve to their pool offset, flexible
     * ones have no backing view (the GPU copies those). */
    uint64_t pool_size=0;
    uint8_t *pool=runtime_memory_pool(&pool_size);
    assert(pool && pool_size>=(UINT64_C(1)<<30));
    assert(runtime_memory_backing_view((uintptr_t)c,65536)==pool+again);
    assert(runtime_memory_backing_view((uintptr_t)c+16384,16384)==pool+again+16384);
    assert(!runtime_memory_backing_view((uintptr_t)c,65536+16384)); /* past the mapping */
    MapFlexible flexible=GET(MapFlexible,"IWIBBdTHit4#libkernel");
    void *f=NULL;
    assert(flexible && flexible(&f,32768,3,0)==0);
    assert(!runtime_memory_backing_view((uintptr_t)f,32768));
    /* A protection change splits the mapping; its second half keeps its pages (b0118f7). */
    ((volatile unsigned char *)f)[5]=0xa7;
    ((volatile unsigned char *)f)[16384+5]=0x3c;
    typedef int32_t (ABI *Mprotect)(const void *, uint64_t, int);
    Mprotect protect=GET(Mprotect,"vSMAm3cxYTY#libkernel");
    assert(protect && protect((char *)f+16384,16384,1)==0);
    unsigned char got=0;
    assert(runtime_memory_read_backing((uintptr_t)f+16384+5,&got,1) && got==0x3c);

    /* A fixed map inside Rosetta's reserved range fails, and the message names the address. */
    void *fixed=(void *)UINT64_C(0x1000000000);
    FILE *err=tmpfile();
    assert(err);
    fflush(stderr);
    int saved=dup(2);
    assert(saved>=0 && dup2(fileno(err),2)==2);
    assert((uint32_t)map(&fixed,65536,0x33,0x10,phys,0)==0x8002000c);
    fflush(stderr);
    assert(dup2(saved,2)==2);
    close(saved);
    char text[1024]={0};
    rewind(err);
    assert(fread(text,1,sizeof(text)-1,err)>0);
    fclose(err);
    assert(strstr(text,"guest asked for 0x1000000000 inside Rosetta's reserved range"));
    printf("macos memory test passed (%d low maps of 256 MiB)\n",mapped);
    return 0;
}
