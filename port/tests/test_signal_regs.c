/* signal_regs.h: a SIGTRAP handler edits GPRs (x86 numbering) and XMM0 through the helpers, and
 * the interrupted code sees the edits on return (Linux kernel / Rosetta 2 on macOS). */
#define _GNU_SOURCE
#include "signal_regs.h"
#include <assert.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

static int rip_ok;
static void trap(int sig, siginfo_t *info, void *context) {
    (void)sig; (void)info;
    ucontext_t *uc = context;
    rip_ok = ((unsigned char *)(uintptr_t)*sig_rip(uc))[-1] == 0xcc; /* RIP is past the CC byte */
    /* rax rcx rdx rbx rsi rdi: x86 indices 0 1 2 3 6 7 (Darwin stores them in another order). */
    static const int index[6] = {0, 1, 2, 3, 6, 7};
    for (int i = 0; i < 6; ++i) *sig_gpr(uc, index[i]) = 0x50 + (uint64_t)index[i];
    memset(sig_xmm0(uc), 0xab, 16);
    memset(sig_xmm0(uc) + 15 * 16, 0xcd, 16); /* XMM15: the registers are consecutive */
    sig_xmm_commit(uc);
}

int main(void) {
    struct sigaction sa = {0};
    sa.sa_sigaction = trap;
    sa.sa_flags = SA_SIGINFO;
    sigemptyset(&sa.sa_mask);
    assert(!sigaction(SIGTRAP, &sa, NULL));
    uint64_t rax, rcx, rdx, rbx, rsi, rdi;
    unsigned char xmm[16], xmm15[16];
    __asm__ volatile("xor %%eax,%%eax\n\txor %%ecx,%%ecx\n\txor %%edx,%%edx\n\txor %%ebx,%%ebx\n\t"
                     "xor %%esi,%%esi\n\txor %%edi,%%edi\n\tpxor %%xmm0,%%xmm0\n\tpxor %%xmm15,%%xmm15\n\tint3\n\t"
                     "movdqu %%xmm0,%6\n\tmovdqu %%xmm15,%7"
                     : "=a"(rax), "=c"(rcx), "=d"(rdx), "=b"(rbx), "=S"(rsi), "=D"(rdi), "=m"(xmm), "=m"(xmm15)
                     :
                     : "xmm0", "xmm15", "memory");
    printf("rip_ok=%d rax=%llx rcx=%llx rdx=%llx rbx=%llx rsi=%llx rdi=%llx xmm0[0]=%02x xmm0[15]=%02x xmm15[0]=%02x\n", rip_ok,
           (unsigned long long)rax, (unsigned long long)rcx, (unsigned long long)rdx, (unsigned long long)rbx,
           (unsigned long long)rsi, (unsigned long long)rdi, xmm[0], xmm[15], xmm15[0]);
    assert(rip_ok);
    assert(rax == 0x50 && rcx == 0x51 && rdx == 0x52 && rbx == 0x53 && rsi == 0x56 && rdi == 0x57);
    for (int i = 0; i < 16; ++i) assert(xmm[i] == 0xab && xmm15[i] == 0xcd);
    puts("signal regs test passed");
    return 0;
}
