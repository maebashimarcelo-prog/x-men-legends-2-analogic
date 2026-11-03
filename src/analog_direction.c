/* Experimental targeted input getter: character movement callsites only. */
#include "xml2_analog.h"
#include <math.h>
#include <stdio.h>
#include <string.h>

uintptr_t g_original_direction_getter = 0;
static uintptr_t g_input_wrapper_cell = 0;
static unsigned g_input_pad = 0;
static float g_input_deadzone = 0.15f;
/* Full movement magnitude at 37% physical stick deflection. */
static const float g_full_speed_radius = 0.88f;
extern void analog_direction_getter(void);

/* Diagnostic only: record unique vertical-axis getter callers outside movement.
   No changes to returned values or menu behavior. */
static uintptr_t g_menu_callers[96];
static unsigned g_menu_caller_count = 0;
static void log_vertical_caller(uintptr_t caller) {
    unsigned i;
    char message[128];
    if (caller == 0x0042DC54u || caller == 0x0042DC62u) return;
    for (i = 0; i < g_menu_caller_count; ++i)
        if (g_menu_callers[i] == caller) return;
    if (g_menu_caller_count >= 96) return;
    g_menu_callers[g_menu_caller_count++] = caller;
    snprintf(message, sizeof message,
             "MenuProbe: vertical getter caller=0x%08X (diagnostic only)\r\n",
             (unsigned)caller);
    analog_log_event(message);
}

/* Multi-player investigation: capture the game input object (this/ECX) for
   movement callers. No change to movement values, speed or controller choice. */
static uintptr_t g_player_contexts[64];
static unsigned g_player_context_count = 0;
static void log_player_context(uintptr_t context, uintptr_t caller) {
    unsigned i;
    char message[160];
    if (!context) return;
    for (i = 0; i < g_player_context_count; ++i)
        if (g_player_contexts[i] == context) return;
    if (g_player_context_count >= 64) return;
    g_player_contexts[g_player_context_count++] = context;
    snprintf(message, sizeof message,
             "PlayerProbe: movement context=0x%08X caller=0x%08X\r\n",
             (unsigned)context, (unsigned)caller);
    analog_log_event(message);
}

int analog_direction_value(unsigned index, uintptr_t return_address, float *out, uintptr_t context) {
    uintptr_t wrapper;
    int32_t x, y;
    float fx, fy, radius, output;
    uint32_t active;
    unsigned selected_pad = g_input_pad;
    /* Experimental mapping for the observed contiguous player contexts.
       Contexts 0x79EBFC and 0x79EF8C differ by 0x390 bytes.
       Never map an unknown context to another player's stick: use the
       original game getter instead. */
    if (index > 1) return 0;
    if (return_address != 0x0042DC54u && return_address != 0x0042DC62u) {
        if (index == 1) log_vertical_caller(return_address);
        return 0;
    }
    if (index == 0) log_player_context(context, return_address);
    if (context >= 0x0079EBFCu && context <= 0x0079F6ACu &&
        ((context - 0x0079EBFCu) % 0x390u) == 0) {
        selected_pad = g_input_pad + (unsigned)((context - 0x0079EBFCu) / 0x390u);
        if (selected_pad >= 4) return 0;
    } else {
        return 0;
    }
    if (!g_input_wrapper_cell) return 0;
    wrapper = *(const uintptr_t *)g_input_wrapper_cell;
    if (!wrapper) return 0;
    active = *(const uint32_t *)(wrapper + 0x129CCu);
    if (!(active & (1u << selected_pad))) return 0;
    x = *(const int32_t *)(wrapper + 0x4F0u + selected_pad * 0x110u);
    y = *(const int32_t *)(wrapper + 0x4F4u + selected_pad * 0x110u);
    if (x < -1000 || x > 1000 || y < -1000 || y > 1000) return 0;
    fx = (float)x / 1000.0f;
    fy = (float)y / 1000.0f;
    radius = sqrtf(fx*fx + fy*fy);
    if (!isfinite(radius)) return 0;
    if (radius <= g_input_deadzone) { *out = 0.0f; return 1; }
    /* Radial deadzone and faster ramp to full magnitude; preserve 360-degree angle. */
    output = (radius - g_input_deadzone) / (g_full_speed_radius - g_input_deadzone);
    if (output > 1.0f) output = 1.0f;
    output /= radius;
    *out = (index == 0 ? fx : -fy) * output;
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
    snprintf(message,sizeof message,"Direction: MULTIPLAYER TEST mapping base=0x0079EBFC stride=0x390; deadzone=%.3f, first_pad=%u, target=0x%08X\r\n",
             config->deadzone, config->controller_index, (unsigned)target);
    analog_log_event(message);
    return 1;
}
