// SPDX-License-Identifier: GPL-2.0-or-later
// Registers of a signal's ucontext: Linux gregs/fpregs, Darwin uc_mcontext->__ss/__fs.
// GPR indices use x86 numbering (rax rcx rdx rbx rsp rbp rsi rdi r8..r15), as IsaRegs.gpr does.
#pragma once
#include <signal.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#ifdef __APPLE__
#include <sys/ucontext.h>

static inline uint64_t *sig_gpr(ucontext_t *uc, int x86_index) {
    // Darwin's thread state is ordered rax rbx rcx rdx rdi rsi rbp rsp r8..r15.
#define SIG_SS(field) offsetof(_STRUCT_X86_THREAD_STATE64, field)
    static const size_t at[16] = {SIG_SS(__rax), SIG_SS(__rcx), SIG_SS(__rdx), SIG_SS(__rbx),
                                  SIG_SS(__rsp), SIG_SS(__rbp), SIG_SS(__rsi), SIG_SS(__rdi),
                                  SIG_SS(__r8),  SIG_SS(__r9),  SIG_SS(__r10), SIG_SS(__r11),
                                  SIG_SS(__r12), SIG_SS(__r13), SIG_SS(__r14), SIG_SS(__r15)};
#undef SIG_SS
    return (uint64_t *)((unsigned char *)&uc->uc_mcontext->__ss + at[x86_index]);
}
static inline uint64_t *sig_rip(ucontext_t *uc) { return &uc->uc_mcontext->__ss.__rip; }
static inline uint64_t *sig_rflags(ucontext_t *uc) { return &uc->uc_mcontext->__ss.__rflags; }
// XMM0..15 are consecutive 16-byte registers, as in the FXSAVE area.
static inline unsigned char *sig_xmm0(ucontext_t *uc) { return (unsigned char *)&uc->uc_mcontext->__fs.__fpu_xmm0; }
static inline void sig_xmm_commit(ucontext_t *uc) { (void)uc; }
#else
#include <ucontext.h>

static inline uint64_t *sig_gpr(ucontext_t *uc, int x86_index) {
    static const int reg[16] = {REG_RAX, REG_RCX, REG_RDX, REG_RBX, REG_RSP, REG_RBP, REG_RSI, REG_RDI,
                                REG_R8,  REG_R9,  REG_R10, REG_R11, REG_R12, REG_R13, REG_R14, REG_R15};
    return (uint64_t *)&uc->uc_mcontext.gregs[reg[x86_index]];
}
static inline uint64_t *sig_rip(ucontext_t *uc) { return (uint64_t *)&uc->uc_mcontext.gregs[REG_RIP]; }
static inline uint64_t *sig_rflags(ucontext_t *uc) { return (uint64_t *)&uc->uc_mcontext.gregs[REG_EFL]; }
// XMM0-15 of the signal frame (FXSAVE layout at 160); the kernel restores them on return.
static inline unsigned char *sig_xmm0(ucontext_t *uc) {
    unsigned char *fpu = (unsigned char *)uc->uc_mcontext.fpregs;
    return fpu ? fpu + 160 : NULL;
}
// XSAVE frame (FP_XSTATE_MAGIC1): XSTATE_BV bit 1 (SSE) set, or XRSTOR would reset the XMM
// registers instead of loading the changed ones. XSAVE wrote their real values.
static inline void sig_xmm_commit(ucontext_t *uc) {
    unsigned char *fpu = (unsigned char *)uc->uc_mcontext.fpregs;
    uint32_t magic;
    if (fpu && (memcpy(&magic, fpu + 464, 4), magic == 0x46505853u)) fpu[512] |= 2;
}
#endif

// GPR indices for readable reports.
enum { SIG_RAX, SIG_RCX, SIG_RDX, SIG_RBX, SIG_RSP, SIG_RBP, SIG_RSI, SIG_RDI };
