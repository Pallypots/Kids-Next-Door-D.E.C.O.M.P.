// Calling something that may crash, and coming back with the registers intact. Windows x86 only; analysis tooling.
//
// RtlCaptureContext will not do for this: on x86 it captures its caller's frame, so a resume from it returns out of the capturing function with callee-saved registers as they stood at
// the crash. guarded() saves them by hand, and the exception handler of the oracle restores them through guard_resume.
#pragma once
#include <windows.h>

#include <cstdint>

static volatile bool g_expect_crash;
static volatile int g_crashes;
static uint32_t g_resume_seh;
static uint32_t g_saved_ebx, g_saved_esi, g_saved_edi, g_saved_ebp, g_saved_esp, g_saved_eip;

// In the oracle's vectored handler, for an exception it takes for a crash.
static bool guard_resume(EXCEPTION_POINTERS* e) {
    if (!g_expect_crash) return false;
    g_expect_crash = false;
    g_crashes = g_crashes + 1;
    __writefsdword(0, g_resume_seh);
    e->ContextRecord->Ebx = g_saved_ebx;
    e->ContextRecord->Esi = g_saved_esi;
    e->ContextRecord->Edi = g_saved_edi;
    e->ContextRecord->Ebp = g_saved_ebp;
    e->ContextRecord->Esp = g_saved_esp;
    e->ContextRecord->Eip = g_saved_eip;
    return true;
}

static __declspec(naked) void __cdecl guarded_call(void (*fn)(void*), void* arg) {
    __asm {
        mov g_saved_ebx, ebx
        mov g_saved_esi, esi
        mov g_saved_edi, edi
        mov g_saved_ebp, ebp
        mov g_saved_esp, esp
        lea eax, come_back
        mov g_saved_eip, eax
        mov eax, [esp + 4]
        mov ecx, [esp + 8]
        push ecx
        call eax
        add esp, 4
    come_back:
        ret
    }
}

template <class F> static void guard_trampoline(void* f) { (*(F*)f)(); }

// Runs f; a crash inside comes back here as a plain return. True if it crashed.
template <class F> static bool guarded(F f) {
    const int before = g_crashes;
    g_resume_seh = __readfsdword(0);
    g_expect_crash = true;
    guarded_call(&guard_trampoline<F>, &f);
    g_expect_crash = false;
    return g_crashes != before;
}
