#pragma once

#if defined(__BIONIC__) || defined(ANDROID)
#include <setjmp.h>
#include <signal.h>
#include <stdint.h>

// Android Bionic does not implement getcontext/makecontext/swapcontext.
// For ARM64 stack switching and fibers, we provide standard setjmp/longjmp wrappers.

struct ucontext_t_bionic {
    jmp_buf jmp;
    stack_t uc_stack;
    struct ucontext_t_bionic* uc_link;
};

#endif
