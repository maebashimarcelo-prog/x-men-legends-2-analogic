#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

extern void *controller_forward_table[32];
extern int controller_test_invoke(void *method, void *front, unsigned count,
                                  const uintptr_t *args, uintptr_t *result);

typedef struct { void **vtable; unsigned cookie; } Real;
typedef struct { void **vtable; Real *real; } Front;
static Real real;
static unsigned failures, calls;
static uintptr_t values[6] = {0x10203040, 0x50607080, 0x12345678,
                              0x87654321, 0x76543210, 0xabcdef01};

static uintptr_t check(void *object, unsigned count, const uintptr_t *args) {
    unsigned i;
    calls++;
    if (object != &real || ((Real *)object)->cookie != 0x4d434f4d) failures++;
    for (i = 0; i < count; i++) if (args[i] != values[i]) failures++;
    return 0x81234567;
}
static uintptr_t WINAPI call0(void *p) { return check(p, 0, NULL); }
static uintptr_t WINAPI call1(void *p, uintptr_t a) {
    uintptr_t args[] = {a}; return check(p, 1, args);
}
static uintptr_t WINAPI call2(void *p, uintptr_t a, uintptr_t b) {
    uintptr_t args[] = {a, b}; return check(p, 2, args);
}
static uintptr_t WINAPI call4(void *p, uintptr_t a, uintptr_t b, uintptr_t c, uintptr_t d) {
    uintptr_t args[] = {a, b, c, d}; return check(p, 4, args);
}
static uintptr_t WINAPI call6(void *p, uintptr_t a, uintptr_t b, uintptr_t c,
                             uintptr_t d, uintptr_t e, uintptr_t f) {
    uintptr_t args[] = {a, b, c, d, e, f}; return check(p, 6, args);
}

int main(void) {
    void *vtable[32] = {0};
    void *saved[32];
    void *methods[] = {(void *)call0, (void *)call1, (void *)call2, (void *)call4, (void *)call6};
    const unsigned counts[] = {0, 1, 2, 4, 6};
    Front front = {controller_forward_table, &real};
    unsigned slot, test;
    real.vtable = vtable; real.cookie = 0x4d434f4d;
    for (slot = 0; slot < 32; slot++) for (test = 0; test < 5; test++) {
        uintptr_t result = 0;
        memset(vtable, 0, sizeof vtable);
        vtable[slot] = methods[test];
        memcpy(saved, vtable, sizeof saved);
        if (!controller_test_invoke(front.vtable[slot], &front, counts[test], values, &result)) {
            fprintf(stderr, "FAIL: stack cleanup slot=%u arity=%u\n", slot, counts[test]);
            failures++;
        }
        if (result != 0x81234567 || real.vtable != vtable ||
            memcmp(saved, vtable, sizeof saved) || front.real != &real) failures++;
    }
    if (calls != 160 || failures) {
        fprintf(stderr, "FAIL: forwarding calls=%u failures=%u\n", calls, failures); return 1;
    }
    puts("PASS: 32 x86 COM slots, 5 method arities; original this, arguments, return, stack and vtable preserved.");
    return 0;
}
