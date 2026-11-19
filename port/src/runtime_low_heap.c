/* macOS: the runtime's malloc family (runtime.h macros). Host objects handed to the guest
 * (thread handles, TLS blocks, sync objects...) must sit below 1 TiB: the guest packs pointers
 * into 40 bits. Linux gets that from the non-PIE brk heap (probe.c mallopt); Darwin's malloc
 * zones sit near 0x6000_0000_0000. One region from runtime_low_map holds power-of-two blocks
 * with per-class free lists; a pointer outside the region belongs to the system allocator. */
#define TLG_SYSTEM_MALLOC /* this file is the heap: malloc/free here are the system's */
#include "runtime.h"
#ifdef __APPLE__
#include <errno.h>
#include <pthread.h>
#include <string.h>
#include <sys/mman.h>

/* Address space only (pages are touched on use); the low range is 31 GiB. */
#define REGION_SIZE (UINT64_C(8)<<30)
#define CHUNK (UINT64_C(1)<<20)
#define PAGE UINT64_C(16384)
#define HEADER 16
#define MIN_CLASS 5     /* 32-byte blocks: 16 header + 16 user */
#define CARVED_MAX 16   /* blocks up to 64 KiB (32 KiB + header) are carved from 1 MiB chunks */
#define CLASSES 34      /* largest block 8 GiB */

typedef struct { uint32_t cls, back; uint64_t unused; } Header; /* back: block start to user pointer */
typedef struct Free { struct Free *next; } Free;
// shortcut: one global lock, no chunk return to the OS; upgrade if profiles show contention
static pthread_mutex_t heap_lock=PTHREAD_MUTEX_INITIALIZER;
static pthread_once_t region_once=PTHREAD_ONCE_INIT;
static uintptr_t region, region_next, region_end;
static Free *free_lists[CLASSES];

/* runtime_low_map takes the memory table lock: called once, outside heap_lock, so the two locks
 * are never nested (runtime_memory.c itself uses the system malloc). */
static void region_init(void) {
    region=(uintptr_t)runtime_low_map(REGION_SIZE,PROT_READ|PROT_WRITE);
    region_next=region; region_end=region ? region+REGION_SIZE : 0;
}
static int is_low(const void *p) { return region && (uintptr_t)p>=region && (uintptr_t)p<region_end; }
static unsigned class_of(size_t block) { return block<=32 ? MIN_CLASS : 64-(unsigned)__builtin_clzll(block-1); }

/* A block of 1<<c bytes, or NULL when the region is exhausted. */
static char *take(unsigned c) {
    const uint64_t block=UINT64_C(1)<<c, n=c<=CARVED_MAX ? CHUNK : (block+PAGE-1)&~(PAGE-1);
    pthread_mutex_lock(&heap_lock);
    char *b=(char *)free_lists[c];
    if (b) free_lists[c]=((Free *)b)->next;
    else if (region_end-region_next>=n) {
        b=(char *)region_next; region_next+=n;
        for (uint64_t o=block; o+block<=n; o+=block) { Free *f=(Free *)(b+o); f->next=free_lists[c]; free_lists[c]=f; }
    }
    pthread_mutex_unlock(&heap_lock);
    return b;
}
static void *low_alloc(size_t size, size_t align) {
    pthread_once(&region_once,region_init);
    const size_t extra=align>HEADER ? align : 0;
    if (size>(UINT64_C(1)<<(CLASSES-1))-HEADER-extra) return NULL;
    const unsigned c=class_of(size+HEADER+extra);
    char *b=take(c);
    if (!b) return NULL;
    char *p=extra ? (char *)(((uintptr_t)b+HEADER+align-1)&~(uintptr_t)(align-1)) : b+HEADER;
    ((Header *)p)[-1]=(Header){c,(uint32_t)(p-b),0};
    return p;
}
static size_t usable(const void *p) { const Header *h=(const Header *)p-1; return ((size_t)1<<h->cls)-h->back; }
/* The region is full (or was never mapped). Small blocks are the guest-visible objects (thread
 * handles, sync objects, TLS): a pointer above 40 bits would be truncated by the guest, so stop.
 * A large host buffer may still come from the system heap. */
static void exhausted(size_t size) {
    if (!region || size<=(UINT64_C(1)<<CARVED_MAX)-HEADER) {
        fprintf(stderr,"STOP: low heap exhausted (%llu GiB at %#llx, request %zu bytes): guest-visible objects must stay below 1 TiB\n",
            (unsigned long long)(REGION_SIZE>>30),(unsigned long long)region,size);
        exit(21);
    }
    fprintf(stderr,"Runtime: low heap full, %zu-byte host buffer from the system heap (above 1 TiB)\n",size);
}
void *runtime_low_malloc(size_t size) {
    void *p=low_alloc(size,0);
    if (!p) { exhausted(size); p=malloc(size); }
    return p;
}
void runtime_low_free(void *p) {
    if (!p) return;
    if (!is_low(p)) { free(p); return; }
    const Header *h=(const Header *)p-1;
    const unsigned c=h->cls;
    char *b=(char *)p-h->back;
    /* Large blocks give their pages back but keep the address range (runtime_low_map never reuses it). */
    if (c>CARVED_MAX) madvise(b+PAGE,((size_t)1<<c)-PAGE,MADV_FREE);
    pthread_mutex_lock(&heap_lock);
    ((Free *)b)->next=free_lists[c]; free_lists[c]=(Free *)b;
    pthread_mutex_unlock(&heap_lock);
}
void *runtime_low_calloc(size_t count, size_t size) {
    size_t total;
    if (__builtin_mul_overflow(count,size,&total)) { errno=ENOMEM; return NULL; }
    void *p=low_alloc(total,0);
    if (!p) { exhausted(total); return calloc(count,size); }
    return memset(p,0,total);
}
void *runtime_low_realloc(void *p, size_t size) {
    if (!p) return runtime_low_malloc(size);
    if (!is_low(p)) return realloc(p,size);
    if (!size) { runtime_low_free(p); return NULL; }
    const size_t old=usable(p);
    if (size<=old) return p; // shortcut: shrinking keeps the block; upgrade if large shrinks waste memory
    void *q=runtime_low_malloc(size);
    if (!q) return NULL;
    memcpy(q,p,old);
    runtime_low_free(p);
    return q;
}
int runtime_low_posix_memalign(void **out, size_t align, size_t size) {
    if (!align || align&(align-1) || align%sizeof(void *)) return EINVAL;
    void *p=low_alloc(size,align);
    if (!p) { exhausted(size+align); return posix_memalign(out,align,size); }
    *out=p;
    return 0;
}
void *runtime_low_aligned_alloc(size_t align, size_t size) {
    if (!align || align&(align-1)) { errno=EINVAL; return NULL; }
    void *p=NULL;
    const int error=runtime_low_posix_memalign(&p,align<sizeof(void *) ? sizeof(void *) : align,size);
    if (error) { errno=error; return NULL; }
    return p;
}
char *runtime_low_strdup(const char *s) {
    const size_t n=strlen(s)+1;
    char *p=runtime_low_malloc(n);
    return p ? memcpy(p,s,n) : NULL;
}
#endif
