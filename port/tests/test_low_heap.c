/* macOS low heap (runtime_low_heap.c): the runtime's host objects handed to the guest stay below
 * 1 TiB (the guest packs pointers into 40 bits), with the malloc contracts the runtime relies on. */
#define TLG_SYSTEM_MALLOC /* malloc here is the system's: the test calls the low heap by name */
#include "runtime.h"
#include <assert.h>
#include <errno.h>
#include <pthread.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

/* The settings menu of the GPU library restarts through probe.c, which tests do not link. */
void runtime_restart(void) { abort(); }

#define LOW(p) ((uintptr_t)(p)<UINT64_C(0xfc0000000) && (uintptr_t)(p)>=UINT64_C(0x800000000))
static uint64_t seed=88172645463325252u;
static uint64_t next(void) { seed^=seed<<13; seed^=seed>>7; seed^=seed<<17; return seed; }

static void *worker(void *arg) {
    const unsigned char tag=(unsigned char)(uintptr_t)arg;
    unsigned char *live[64]={0}; size_t sizes[64]={0};
    uint64_t s=(uintptr_t)arg*2654435761u+1;
    for (int i=0; i<50000; ++i) {
        s^=s<<13; s^=s>>7; s^=s<<17;
        const int k=(int)(s%64);
        if (live[k]) {
            for (size_t j=0; j<sizes[k]; ++j) assert(live[k][j]==(unsigned char)(tag+j));
            runtime_low_free(live[k]);
        }
        sizes[k]=1+(size_t)(s>>8)%4096;
        live[k]=runtime_low_malloc(sizes[k]);
        assert(live[k] && LOW(live[k]));
        for (size_t j=0; j<sizes[k]; ++j) live[k][j]=(unsigned char)(tag+j);
    }
    for (int k=0; k<64; ++k) {
        for (size_t j=0; j<sizes[k]; ++j) assert(live[k][j]==(unsigned char)(tag+j));
        runtime_low_free(live[k]);
    }
    return NULL;
}

int main(void) {
    enum { N=10000 };
    static unsigned char *blocks[N]; static size_t sizes[N];
    for (int i=0; i<N; ++i) {
        sizes[i]=1+next()%(256*1024);
        blocks[i]=runtime_low_malloc(sizes[i]);
        assert(blocks[i] && LOW(blocks[i]) && !((uintptr_t)blocks[i]%16));
        blocks[i][0]=(unsigned char)i; blocks[i][sizes[i]-1]=(unsigned char)(i>>8);
    }
    for (int i=0; i<N; ++i) {
        assert(blocks[i][0]==(unsigned char)i && blocks[i][sizes[i]-1]==(unsigned char)(i>>8));
        runtime_low_free(blocks[i]);
    }

    /* realloc keeps the contents growing and shrinking, across size classes. */
    unsigned char *p=runtime_low_malloc(24);
    for (int i=0; i<24; ++i) p[i]=(unsigned char)i;
    p=runtime_low_realloc(p,300000);
    assert(p && LOW(p));
    for (int i=0; i<24; ++i) assert(p[i]==(unsigned char)i);
    memset(p+24,0x77,300000-24);
    p=runtime_low_realloc(p,40);
    assert(p && LOW(p));
    for (int i=0; i<24; ++i) assert(p[i]==(unsigned char)i);
    for (int i=24; i<40; ++i) assert(p[i]==0x77);
    runtime_low_free(p);
    assert(runtime_low_realloc(NULL,10) && LOW(runtime_low_realloc(NULL,10)));

    /* A freed block is reused; calloc zeroes reused blocks. */
    unsigned char *a=runtime_low_malloc(100);
    memset(a,0xff,100);
    runtime_low_free(a);
    unsigned char *b=runtime_low_calloc(10,10);
    assert(b==a);
    for (int i=0; i<100; ++i) assert(!b[i]);
    runtime_low_free(b);
    errno=0;
    assert(!runtime_low_calloc(SIZE_MAX/2,4) && errno==ENOMEM);
    runtime_low_free(NULL);

    /* Alignment, strdup. */
    void *aligned=runtime_low_aligned_alloc(4096,5000);
    assert(aligned && LOW(aligned) && !((uintptr_t)aligned%4096));
    memset(aligned,1,5000);
    aligned=runtime_low_realloc(aligned,9000);
    assert(aligned && ((unsigned char *)aligned)[4999]==1);
    runtime_low_free(aligned);
    void *m=NULL;
    assert(!runtime_low_posix_memalign(&m,256,10) && m && LOW(m) && !((uintptr_t)m%256));
    runtime_low_free(m);
    assert(runtime_low_posix_memalign(&m,3,10)==EINVAL);
    char *s=runtime_low_strdup("guest handle");
    assert(s && LOW(s) && !strcmp(s,"guest handle"));
    runtime_low_free(s);

    /* Memory from the system (libraries) is handed back to the system. */
    char *system=malloc(64);
    strcpy(system,"library");
    system=runtime_low_realloc(system,128);
    assert(!strcmp(system,"library"));
    runtime_low_free(system);
    runtime_low_free(strdup("library"));

    pthread_t threads[8];
    for (uintptr_t i=0; i<8; ++i) assert(!pthread_create(&threads[i],NULL,worker,(void *)i));
    for (int i=0; i<8; ++i) assert(!pthread_join(threads[i],NULL));
    puts("low heap: ok");
    return 0;
}
