/* Experimental targeted input getter: character movement callsites only. */
#include "xml2_analog.h"
#include <math.h>
#include <stdio.h>
#include <string.h>

uintptr_t g_original_direction_getter = 0;
static uintptr_t g_input_wrapper_cell = 0;
static unsigned g_input_pad = 0;
static float g_input_deadzone = 0.15f;
extern void analog_direction_getter(void);

int analog_direction_value(unsigned index, uintptr_t return_address, float *out) {
    uintptr_t wrapper;
    int32_t x, y;
    float fx, fy, radius, output;
    uint32_t active;
    if (index > 1 || (return_address != 0x0042DC54u && return_address != 0x0042DC62u)) return 0;
    if (!g_input_wrapper_cell) return 0;
    wrapper = *(const uintptr_t *)g_input_wrapper_cell;
    if (!wrapper) return 0;
    active = *(const uint32_t *)(wrapper + 0x129CCu);
    if (!(active & (1u << g_input_pad))) return 0;
    x = *(const int32_t *)(wrapper + 0x4F0u + g_input_pad * 0x110u);
    y = *(const int32_t *)(wrapper + 0x4F4u + g_input_pad * 0x110u);
    if (x < -1000 || x > 1000 || y < -1000 || y > 1000) return 0;
    fx = (float)x / 1000.0f;
    fy = (float)y / 1000.0f;
    radius = sqrtf(fx*fx + fy*fy);
    if (!isfinite(radius)) return 0;
    if (radius <= g_input_deadzone) { *out = 0.0f; return 1; }
    /* Radial deadzone and linear remapping; retain continuous angle and radius. */
    output = (radius - g_input_deadzone) / (1.0f - g_input_deadzone);
    if (output > 1.0f) output = 1.0f;
    output /= radius;
    *out = (index == 0 ? fx : fy) * output;
    return 1;
}

int analog_direction_install(const XtaBuild *build, const XtaConfig *config) {
    DWORD old_protect, restored;
    uintptr_t *entry;
    uintptr_t expected = 0x00550B10u;
    uintptr_t target = (uintptr_t)&analog_direction_getter;
    char message[160];
    if (build->status != XTA_SUPPORTED ||
        strcmp(build->sha256,"8AE56DB5E3739E05977991380A6CF14165A4CF4D993D9A968369E332AFD7638C") ||
        build->preferred_base != 0x00400000u ||
        (uintptr_t)GetModuleHandleW(NULL) != 0x00400000u) {
        analog_log_event("Direction: NOT installed (unsupported build or relocated image)\r\n");
        return 0;
    }
    entry = (uintptr_t *)0x0069A104u;
    if (*entry != expected) {
        analog_log_event("Direction: NOT installed (vtable getter differs)\r\n");
        return 0;
    }
    g_input_wrapper_cell = build->wrapper_cell;
    g_input_pad = config->controller_index;
    g_input_deadzone = config->deadzone;
    g_original_direction_getter = expected;
    if (!VirtualProtect(entry,sizeof(*entry),PAGE_READWRITE,&old_protect)) return 0;
    *entry = target;
    if (!VirtualProtect(entry,sizeof(*entry),old_protect,&restored)) {
        analog_log_event("Direction: installed, but page protection restore failed\r\n");
    }
    snprintf(message,sizeof message,"Direction: EXPERIMENTAL getter active; deadzone=%.3f, pad=%u, target=0x%08X\r\n",
             config->deadzone, config->controller_index, (unsigned)target);
    analog_log_event(message);
    return 1;
}
